/* * Copyright (C) 2007-2008 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include <string.h>

#include <glib/gi18n-lib.h>

#include <telepathy-glib/enums.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/gtypes.h>

#include <libmissioncontrol/mission-control.h>
#include <libmissioncontrol/mc-account.h>

#include <extensions/extensions.h>

#include "empathy-dispatcher.h"
#include "empathy-utils.h"
#include "empathy-tube-handler.h"
#include "empathy-account-manager.h"
#include "empathy-contact-factory.h"
#include "empathy-tp-file.h"
#include "empathy-chatroom-manager.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_DISPATCHER
#include <libempathy/empathy-debug.h>

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyDispatcher)
typedef struct
{
  EmpathyAccountManager *account_manager;
  MissionControl *mc;
  /* connection to connection data mapping */
  GHashTable *connections;
  /* accounts to connection mapping */
  GHashTable *accounts;
  gpointer token;
  GSList *tubes;

  /* channels which the dispatcher is listening "invalidated" */
  GList *channels;
} EmpathyDispatcherPriv;

G_DEFINE_TYPE (EmpathyDispatcher, empathy_dispatcher, G_TYPE_OBJECT);

enum
{
  OBSERVE,
  APPROVE,
  DISPATCH,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static EmpathyDispatcher *dispatcher = NULL;

typedef struct
{
  EmpathyDispatcher *dispatcher;
  EmpathyDispatchOperation *operation;
  TpConnection *connection;
  gchar *channel_type;
  guint handle_type;
  guint handle;
  EmpathyContact *contact;

  /* Properties to pass to the channel when requesting it */
  GHashTable *request;
  EmpathyDispatcherRequestCb *cb;
  gpointer user_data;
  gpointer *request_data;
} DispatcherRequestData;

typedef struct
{
  TpChannel *channel;
  /* Channel type specific wrapper object */
  GObject *channel_wrapper;
} DispatchData;

typedef struct
{
  McAccount *account;
  /* ObjectPath => DispatchData.. */
  GHashTable *dispatched_channels;
  /* ObjectPath -> EmpathyDispatchOperations */
  GHashTable *dispatching_channels;
  /* ObjectPath -> EmpathyDispatchOperations */
  GHashTable *outstanding_channels;
  /* List of DispatcherRequestData */
  GList *outstanding_requests;
  /* List of requestable channel classes */
  GPtrArray *requestable_channels;
} ConnectionData;

static DispatchData *
new_dispatch_data (TpChannel *channel,
                   GObject *channel_wrapper)
{
  DispatchData *d = g_slice_new0 (DispatchData);
  d->channel = g_object_ref (channel);
  if (channel_wrapper != NULL)
    d->channel_wrapper = g_object_ref (channel_wrapper);

  return d;
}

static void
free_dispatch_data (DispatchData *data)
{
  g_object_unref (data->channel);
  if (data->channel_wrapper != NULL)
    g_object_unref (data->channel_wrapper);

  g_slice_free (DispatchData, data);
}

static DispatcherRequestData *
new_dispatcher_request_data (EmpathyDispatcher *dispatcher,
                             TpConnection *connection,
                             const gchar *channel_type,
                             guint handle_type,
                             guint handle,
                             GHashTable *request,
                             EmpathyContact *contact,
                             EmpathyDispatcherRequestCb *cb,
                             gpointer user_data)
{
  DispatcherRequestData *result = g_slice_new0 (DispatcherRequestData);

  result->dispatcher = g_object_ref (dispatcher);
  result->connection = connection;

  result->channel_type = g_strdup (channel_type);
  result->handle_type = handle_type;
  result->handle = handle;
  result->request = request;

  if (contact != NULL)
    result->contact = g_object_ref (contact);

  result->cb = cb;
  result->user_data = user_data;

  return result;
}

static void
free_dispatcher_request_data (DispatcherRequestData *r)
{
  g_free (r->channel_type);

  if (r->dispatcher != NULL)
    g_object_unref (r->dispatcher);

  if (r->contact != NULL)
    g_object_unref (r->contact);

  if (r->request != NULL)
    g_hash_table_unref (r->request);

  g_slice_free (DispatcherRequestData, r);
}

static ConnectionData *
new_connection_data (McAccount *account)
{
  ConnectionData *cd = g_slice_new0 (ConnectionData);
  cd->account = g_object_ref (account);

  cd->dispatched_channels = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) free_dispatch_data);

  cd->dispatching_channels = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, g_object_unref);

  cd->outstanding_channels = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, NULL);

  return cd;
}

static void
free_connection_data (ConnectionData *cd)
{
  GList *l;
  g_object_unref (cd->account);
  g_hash_table_destroy (cd->dispatched_channels);
  g_hash_table_destroy (cd->dispatching_channels);
  int i;

  for (l = cd->outstanding_requests ; l != NULL; l = g_list_delete_link (l,l))
    {
      free_dispatcher_request_data (l->data);
    }

  if (cd->requestable_channels  != NULL)
    {
      for (i = 0 ; i < cd->requestable_channels->len ; i++)
          g_value_array_free (
            g_ptr_array_index (cd->requestable_channels, i));
      g_ptr_array_free (cd->requestable_channels, TRUE);
    }
}

static void
dispatcher_connection_invalidated_cb (TpConnection *connection,
                                      guint domain,
                                      gint code,
                                      gchar *message,
                                      EmpathyDispatcher *dispatcher)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  ConnectionData *cd;

  DEBUG ("Error: %s", message);
  cd = g_hash_table_lookup (priv->connections, connection);

  g_hash_table_remove (priv->accounts, cd->account);
  g_hash_table_remove (priv->connections, connection);
}

static gboolean
dispatcher_operation_can_start (EmpathyDispatcher *self,
                                EmpathyDispatchOperation *operation,
                                ConnectionData *cd)
{
  GList *l;
  const gchar *channel_type =
    empathy_dispatch_operation_get_channel_type (operation);

  for (l = cd->outstanding_requests; l != NULL; l = g_list_next (l))
    {
      DispatcherRequestData *d = (DispatcherRequestData *) l->data;

      if (d->operation == NULL && !tp_strdiff (d->channel_type, channel_type))
        {
          return FALSE;
        }
    }

  return TRUE;
}

static void
dispatch_operation_flush_requests (EmpathyDispatcher *dispatcher,
                                   EmpathyDispatchOperation *operation,
                                   GError *error,
                                   ConnectionData *cd)
{
  GList *l;

  l = cd->outstanding_requests;
  while (l != NULL)
    {
      DispatcherRequestData *d = (DispatcherRequestData *) l->data;
      GList *lt = l;

      l = g_list_next (l);

      if (d->operation == operation)
        {
          if (d->cb != NULL)
            {
              if (error != NULL)
                d->cb (NULL, error, d->user_data);
              else
                d->cb (operation, NULL, d->user_data);
            }

          cd->outstanding_requests = g_list_delete_link
            (cd->outstanding_requests, lt);

          free_dispatcher_request_data (d);
        }
    }
}

static void
dispatcher_channel_invalidated_cb (TpProxy *proxy,
                                   guint domain,
                                   gint code,
                                   gchar *message,
                                   EmpathyDispatcher *dispatcher)
{
  /* Channel went away... */
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  TpConnection *connection;
  EmpathyDispatchOperation *operation;
  ConnectionData *cd;
  const gchar *object_path;

  connection = tp_channel_borrow_connection (TP_CHANNEL (proxy));

  cd = g_hash_table_lookup (priv->connections, connection);
  /* Connection itself invalidated? */
  if (cd == NULL)
    return;

  object_path = tp_proxy_get_object_path (proxy);

  DEBUG ("Channel %s invalidated", object_path);

  g_hash_table_remove (cd->dispatched_channels, object_path);
  g_hash_table_remove (cd->dispatching_channels, object_path);

  priv->channels = g_list_remove (priv->channels, proxy);

  operation = g_hash_table_lookup (cd->outstanding_channels, object_path);
  if (operation != NULL)
    {
      GError error = { domain, code, message };
      dispatch_operation_flush_requests (dispatcher, operation, &error, cd);
      g_hash_table_remove (cd->outstanding_channels, object_path);
      g_object_unref (operation);
    }
}

static void
dispatch_operation_approved_cb (EmpathyDispatchOperation *operation,
                                EmpathyDispatcher *dispatcher)
{
  g_assert (empathy_dispatch_operation_is_incoming (operation));
  DEBUG ("Send of for dispatching: %s",
    empathy_dispatch_operation_get_object_path (operation));
  g_signal_emit (dispatcher, signals[DISPATCH], 0, operation);
}

static void
dispatch_operation_claimed_cb (EmpathyDispatchOperation *operation,
                               EmpathyDispatcher *dispatcher)
{
  /* Our job is done, remove the dispatch operation and mark the channel as
   * dispatched */
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  TpConnection *connection;
  ConnectionData *cd;
  const gchar *object_path;

  connection = empathy_dispatch_operation_get_tp_connection (operation);
  cd = g_hash_table_lookup (priv->connections, connection);
  g_assert (cd != NULL);
  g_object_unref (G_OBJECT (connection));

  object_path = empathy_dispatch_operation_get_object_path (operation);

  if (g_hash_table_lookup (cd->dispatched_channels, object_path) == NULL)
    {
      DispatchData *d;
      d = new_dispatch_data (
        empathy_dispatch_operation_get_channel (operation),
        empathy_dispatch_operation_get_channel_wrapper (operation));
      g_hash_table_insert (cd->dispatched_channels,
        g_strdup (object_path), d);
    }
  g_hash_table_remove (cd->dispatching_channels, object_path);

  DEBUG ("Channel claimed: %s", object_path);
}

static void
dispatch_operation_ready_cb (EmpathyDispatchOperation *operation,
                             EmpathyDispatcher *dispatcher)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  TpConnection *connection;
  ConnectionData *cd;
  EmpathyDispatchOperationState status;

  g_signal_connect (operation, "approved",
    G_CALLBACK (dispatch_operation_approved_cb), dispatcher);

  g_signal_connect (operation, "claimed",
    G_CALLBACK (dispatch_operation_claimed_cb), dispatcher);

  /* Signal the observers */
  DEBUG ("Send to observers: %s",
    empathy_dispatch_operation_get_object_path (operation));
  g_signal_emit (dispatcher, signals[OBSERVE], 0, operation);

  empathy_dispatch_operation_start (operation);

  /* Signal potential requestors */
  connection =  empathy_dispatch_operation_get_tp_connection (operation);
  cd = g_hash_table_lookup (priv->connections, connection);
  g_assert (cd != NULL);
  g_object_unref (G_OBJECT (connection));

  g_object_ref (operation);
  g_object_ref (dispatcher);

  dispatch_operation_flush_requests (dispatcher, operation, NULL, cd);
  status = empathy_dispatch_operation_get_status (operation);
  g_object_unref (operation);

  if (status == EMPATHY_DISPATCHER_OPERATION_STATE_CLAIMED)
    return;

  if (status == EMPATHY_DISPATCHER_OPERATION_STATE_APPROVING)
    {
      DEBUG ("Send to approvers: %s",
        empathy_dispatch_operation_get_object_path (operation));
      g_signal_emit (dispatcher, signals[APPROVE], 0, operation);
    }
  else
    {
      g_assert (status == EMPATHY_DISPATCHER_OPERATION_STATE_DISPATCHING);
      DEBUG ("Send of for dispatching: %s",
        empathy_dispatch_operation_get_object_path (operation));
      g_signal_emit (dispatcher, signals[DISPATCH], 0, operation);
    }

  g_object_unref (dispatcher);
}

static void
dispatcher_start_dispatching (EmpathyDispatcher *self,
                              EmpathyDispatchOperation *operation,
                              ConnectionData *cd)
{
  const gchar *object_path =
    empathy_dispatch_operation_get_object_path (operation);

  DEBUG ("Dispatching process started for %s", object_path);

  if (g_hash_table_lookup (cd->dispatching_channels, object_path) == NULL)
    {
      g_assert (g_hash_table_lookup (cd->outstanding_channels,
        object_path) == NULL);

      g_hash_table_insert (cd->dispatching_channels,
        g_strdup (object_path), operation);

      switch (empathy_dispatch_operation_get_status (operation))
        {
          case EMPATHY_DISPATCHER_OPERATION_STATE_PREPARING:
            g_signal_connect (operation, "ready",
              G_CALLBACK (dispatch_operation_ready_cb), dispatcher);
            break;
          case EMPATHY_DISPATCHER_OPERATION_STATE_PENDING:
            dispatch_operation_ready_cb (operation, dispatcher);
            break;
          default:
            g_assert_not_reached();
        }

    }
  else if (empathy_dispatch_operation_get_status (operation) >=
      EMPATHY_DISPATCHER_OPERATION_STATE_PENDING)
    {
      /* Already dispatching and the operation is pending, thus the observers
       * have seen it (if applicable), so we can flush the request right away.
       */
      dispatch_operation_flush_requests (self, operation, NULL, cd);
    }
}

static void
dispatcher_flush_outstanding_operations (EmpathyDispatcher *self,
                                         ConnectionData *cd)
{
  GHashTableIter iter;
  gpointer value;

  g_hash_table_iter_init (&iter, cd->outstanding_channels);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      EmpathyDispatchOperation *operation = EMPATHY_DISPATCH_OPERATION (value);

      if (dispatcher_operation_can_start (self, operation, cd))
        {
          dispatcher_start_dispatching (dispatcher, operation, cd);
          g_hash_table_iter_remove (&iter);
        }
    }
}

static void
dispatcher_connection_new_channel (EmpathyDispatcher *dispatcher,
                                   TpConnection *connection,
                                   const gchar *object_path,
                                   const gchar *channel_type,
                                   guint handle_type,
                                   guint handle,
                                   GHashTable *properties,
                                   gboolean incoming)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  TpChannel         *channel;
  ConnectionData *cd;
  EmpathyDispatchOperation *operation;
  EmpathyContact *contact = NULL;
  int i;
  /* Channel types we never want to dispatch because they're either deprecated
   * or can't sensibly be dispatch (e.g. channels that should always be
   * requested) */
  const char *blacklist[] = {
    TP_IFACE_CHANNEL_TYPE_CONTACT_LIST,
    TP_IFACE_CHANNEL_TYPE_TUBES,
    TP_IFACE_CHANNEL_TYPE_ROOM_LIST,
    NULL
  };

  cd = g_hash_table_lookup (priv->connections, connection);

  /* Don't bother with channels we have already dispatched or are dispatching
   * currently. This can happen when NewChannel(s) is fired after
   * RequestChannel/CreateChannel/EnsureChannel */
  if (g_hash_table_lookup (cd->dispatched_channels, object_path) != NULL)
    return;

  if (g_hash_table_lookup (cd->dispatching_channels, object_path) != NULL)
    return;

  /* Should never occur, but just in case a CM fires spurious NewChannel(s) 
   * signals */
  if (g_hash_table_lookup (cd->outstanding_channels, object_path) != NULL)
    return;

  /* Only pick up non-requested text and file channels. For all other it
   * doesn't make sense to handle it if we didn't request it. The same goes
   * for channels we discovered by the Channels property or ListChannels */
  if (!incoming && tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_TEXT)
        && tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER))
    {
      DEBUG ("Ignoring incoming channel of type %s on %s",
        channel_type, object_path);
      return;
    }

  for (i = 0 ; blacklist[i] != NULL; i++)
    {
      if (!tp_strdiff (channel_type, blacklist[i]))
        {
          DEBUG ("Ignoring blacklisted channel type %s on %s",
            channel_type, object_path);
          return;
        }
    }

  DEBUG ("New channel of type %s on %s", channel_type, object_path);

  if (properties == NULL)
    channel = tp_channel_new (connection, object_path, channel_type,
      handle_type, handle, NULL);
  else
    channel = tp_channel_new_from_properties (connection, object_path,
      properties, NULL);

  g_signal_connect (channel, "invalidated",
    G_CALLBACK (dispatcher_channel_invalidated_cb),
    dispatcher);

  priv->channels = g_list_prepend (priv->channels, channel);

  if (handle_type == TP_CONN_HANDLE_TYPE_CONTACT)
    {
      EmpathyContactFactory *factory = empathy_contact_factory_dup_singleton ();
      contact = empathy_contact_factory_get_from_handle (factory,
        cd->account, handle);
      g_object_unref (factory);
    }

  operation = empathy_dispatch_operation_new (connection, channel, contact,
    incoming);

  g_object_unref (channel);

  if (incoming)
    {
      /* Request could either be by us or by a remote party. If there are no
       * outstanding requests for this channel type we can assume it's remote.
       * Otherwise we wait untill they are all satisfied */
      if (dispatcher_operation_can_start (dispatcher, operation, cd))
        dispatcher_start_dispatching (dispatcher, operation, cd);
      else
        g_hash_table_insert (cd->outstanding_channels,
          g_strdup (object_path), operation);
    }
  else
    {
      dispatcher_start_dispatching (dispatcher, operation, cd);
    }
}

static void
dispatcher_connection_new_channel_cb (TpConnection *connection,
                                      const gchar *object_path,
                                      const gchar  *channel_type,
                                      guint handle_type,
                                      guint handle,
                                      gboolean suppress_handler,
                                      gpointer user_data,
                                      GObject *object)
{
  EmpathyDispatcher *dispatcher = EMPATHY_DISPATCHER (object);

  /* Empathy heavily abuses surpress handler (don't try this at home), if
   * surpress handler is true then it is an outgoing channel, which is
   * requested either by us or some other party (like the megaphone applet).
   * Otherwise it's an incoming channel */
  dispatcher_connection_new_channel (dispatcher, connection,
    object_path, channel_type, handle_type, handle, NULL, !suppress_handler);
}

static void
dispatcher_connection_new_channel_with_properties (EmpathyDispatcher *dispatcher,
                                                   TpConnection *connection,
                                                   const gchar *object_path,
                                                   GHashTable *properties)
{
  const gchar *channel_type;
  guint handle_type;
  guint handle;
  gboolean requested;
  gboolean valid;


  channel_type = tp_asv_get_string (properties,
    TP_IFACE_CHANNEL ".ChannelType");
  if (channel_type == NULL)
    {
      g_message ("%s had an invalid ChannelType property", object_path);
      return;
    }

  handle_type = tp_asv_get_uint32 (properties,
    TP_IFACE_CHANNEL ".TargetHandleType", &valid);
  if (!valid)
    {
      g_message ("%s had an invalid TargetHandleType property", object_path);
      return;
    }

  handle = tp_asv_get_uint32 (properties,
    TP_IFACE_CHANNEL ".TargetHandle", &valid);
  if (!valid)
    {
      g_message ("%s had an invalid TargetHandle property", object_path);
      return;
    }

  /* We assume there is no channel dispather, so we're the only one dispatching
   * it. Which means that a requested channel it is outgoing one */
  requested = tp_asv_get_boolean (properties,
    TP_IFACE_CHANNEL ".Requested", &valid);
  if (!valid)
    {
      g_message ("%s had an invalid Requested property", object_path);
      return;
    }

  dispatcher_connection_new_channel (dispatcher, connection,
    object_path, channel_type, handle_type, handle, properties, !requested);
}

static void
dispatcher_connection_new_channels_cb (TpConnection *connection,
                                       const GPtrArray *channels,
                                       gpointer user_data,
                                       GObject *object)
{
  EmpathyDispatcher *dispatcher = EMPATHY_DISPATCHER (object);
  int i;

  for (i = 0; i < channels->len ; i++)
    {
      GValueArray *arr = g_ptr_array_index (channels, i);
      const gchar *object_path;
      GHashTable *properties;

      object_path = g_value_get_boxed (g_value_array_get_nth (arr, 0));
      properties = g_value_get_boxed (g_value_array_get_nth (arr, 1));

      dispatcher_connection_new_channel_with_properties (dispatcher,
        connection, object_path, properties);
    }
}

static void
dispatcher_connection_got_all (TpProxy *proxy,
                               GHashTable *properties,
                               const GError *error,
                               gpointer user_data,
                               GObject *object)
{
  EmpathyDispatcher *dispatcher = EMPATHY_DISPATCHER (object);
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  GPtrArray *channels;
  GPtrArray *requestable_channels;

  if (error) {
    DEBUG ("Error: %s", error->message);
    return;
  }

  channels = tp_asv_get_boxed (properties, "Channels",
    TP_ARRAY_TYPE_CHANNEL_DETAILS_LIST);

  if (channels == NULL)
    DEBUG ("No Channels property !?! on connection");
  else
    dispatcher_connection_new_channels_cb (TP_CONNECTION (proxy),
      channels, NULL, object);

  requestable_channels = tp_asv_get_boxed (properties,
    "RequestableChannelClasses", TP_ARRAY_TYPE_REQUESTABLE_CHANNEL_CLASS_LIST);

  if (requestable_channels == NULL)
    DEBUG ("No RequestableChannelClasses property !?! on connection");
  else
    {
      ConnectionData *cd;

      cd = g_hash_table_lookup (priv->connections, proxy);
      g_assert (cd != NULL);

      cd->requestable_channels = g_boxed_copy (
        TP_ARRAY_TYPE_REQUESTABLE_CHANNEL_CLASS_LIST, requestable_channels);
    }
}

static void
dispatcher_connection_list_channels_cb (TpConnection    *connection,
                                        const GPtrArray *channels,
                                        const GError    *error,
                                        gpointer         user_data,
                                        GObject         *dispatcher)
{
  int i;

  if (error)
    {
      DEBUG ("Error: %s", error->message);
      return;
    }

  for (i = 0; i < channels->len; i++)
    {
      GValueArray *values;

      values = g_ptr_array_index (channels, i);
      /* We don't have any extra info, so assume already existing channels are
       * incoming... */
      dispatcher_connection_new_channel (EMPATHY_DISPATCHER (dispatcher),
        connection,
        g_value_get_boxed (g_value_array_get_nth (values, 0)),
        g_value_get_string (g_value_array_get_nth (values, 1)),
        g_value_get_uint (g_value_array_get_nth (values, 2)),
        g_value_get_uint (g_value_array_get_nth (values, 3)),
        NULL, TRUE);
    }
}

static void
dispatcher_connection_advertise_capabilities_cb (TpConnection    *connection,
                                                 const GPtrArray *capabilities,
                                                 const GError    *error,
                                                 gpointer         user_data,
                                                 GObject         *dispatcher)
{
  if (error)
    DEBUG ("Error: %s", error->message);
}

static void
dispatcher_connection_ready_cb (TpConnection *connection,
                                const GError *error,
                                gpointer dispatcher)
{
  GPtrArray   *capabilities;
  GType        cap_type;
  GValue       cap = {0, };
  const gchar *remove = NULL;

  if (error)
    {
      dispatcher_connection_invalidated_cb (connection, error->domain,
        error->code, error->message, dispatcher);
      return;
    }

  g_signal_connect (connection, "invalidated",
    G_CALLBACK (dispatcher_connection_invalidated_cb), dispatcher);

  if (tp_proxy_has_interface_by_id (TP_PROXY (connection),
      TP_IFACE_QUARK_CONNECTION_INTERFACE_REQUESTS))
    {
      tp_cli_connection_interface_requests_connect_to_new_channels (connection,
        dispatcher_connection_new_channels_cb,
        NULL, NULL, G_OBJECT (dispatcher), NULL);

      tp_cli_dbus_properties_call_get_all (connection, -1,
        TP_IFACE_CONNECTION_INTERFACE_REQUESTS,
        dispatcher_connection_got_all,
        NULL, NULL, dispatcher);
    }
  else
    {
      tp_cli_connection_connect_to_new_channel (connection,
        dispatcher_connection_new_channel_cb,
        NULL, NULL, G_OBJECT (dispatcher), NULL);

      tp_cli_connection_call_list_channels (connection, -1,
        dispatcher_connection_list_channels_cb, NULL, NULL,
        G_OBJECT (dispatcher));

    }

  /* Advertise VoIP capabilities */
  capabilities = g_ptr_array_sized_new (1);
  cap_type = dbus_g_type_get_struct ("GValueArray", G_TYPE_STRING,
    G_TYPE_UINT, G_TYPE_INVALID);
  g_value_init (&cap, cap_type);
  g_value_take_boxed (&cap, dbus_g_type_specialized_construct (cap_type));
  dbus_g_type_struct_set (&cap,
        0, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
        1, TP_CHANNEL_MEDIA_CAPABILITY_AUDIO |
           TP_CHANNEL_MEDIA_CAPABILITY_VIDEO |
           TP_CHANNEL_MEDIA_CAPABILITY_NAT_TRAVERSAL_STUN  |
           TP_CHANNEL_MEDIA_CAPABILITY_NAT_TRAVERSAL_GTALK_P2P, G_MAXUINT);
  g_ptr_array_add (capabilities, g_value_get_boxed (&cap));

  tp_cli_connection_interface_capabilities_call_advertise_capabilities (
    connection, -1, capabilities, &remove,
    dispatcher_connection_advertise_capabilities_cb,
    NULL, NULL, G_OBJECT (dispatcher));

  g_value_unset (&cap);
  g_ptr_array_free (capabilities, TRUE);
}

static void
dispatcher_update_account (EmpathyDispatcher *dispatcher,
                           McAccount *account)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  TpConnection *connection;

  connection = g_hash_table_lookup (priv->accounts, account);
  if (connection != NULL)
    return;

  connection = mission_control_get_tpconnection (priv->mc, account, NULL);
  if (connection == NULL)
    return;

  g_hash_table_insert (priv->connections, g_object_ref (connection),
    new_connection_data (account));

  g_hash_table_insert (priv->accounts, g_object_ref (account),
    g_object_ref (connection));

  tp_connection_call_when_ready (connection, dispatcher_connection_ready_cb,
    dispatcher);

  g_object_unref (connection);
}

static void
dispatcher_account_connection_cb (EmpathyAccountManager *manager,
                                  McAccount *account,
                                  TpConnectionStatusReason reason,
                                  TpConnectionStatus status,
                                  TpConnectionStatus previous,
                                  EmpathyDispatcher *dispatcher)
{
  dispatcher_update_account (dispatcher, account);
}

static GObject*
dispatcher_constructor (GType type,
                        guint n_construct_params,
                        GObjectConstructParam *construct_params)
{
  GObject *retval;

  if (dispatcher == NULL)
    {
      retval = G_OBJECT_CLASS (empathy_dispatcher_parent_class)->constructor
          (type, n_construct_params, construct_params);

      dispatcher = EMPATHY_DISPATCHER (retval);
      g_object_add_weak_pointer (retval, (gpointer) &dispatcher);
    }
  else
    {
      retval = g_object_ref (dispatcher);
    }

  return retval;
}

static void
dispatcher_finalize (GObject *object)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (object);
  GList *l;
  GHashTableIter iter;
  gpointer connection;

  g_signal_handlers_disconnect_by_func (priv->account_manager,
      dispatcher_account_connection_cb, object);

  for (l = priv->channels; l; l = l->next)
    {
      g_signal_handlers_disconnect_by_func (l->data,
          dispatcher_channel_invalidated_cb, object);
    }

  g_list_free (priv->channels);

  g_hash_table_iter_init (&iter, priv->connections);
  while (g_hash_table_iter_next (&iter, &connection, NULL))
    {
      g_signal_handlers_disconnect_by_func (connection,
          dispatcher_connection_invalidated_cb, object);
    }

  g_object_unref (priv->account_manager);
  g_object_unref (priv->mc);

  g_hash_table_destroy (priv->accounts);
  g_hash_table_destroy (priv->connections);
}

static void
empathy_dispatcher_class_init (EmpathyDispatcherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = dispatcher_finalize;
  object_class->constructor = dispatcher_constructor;

  signals[OBSERVE] =
    g_signal_new ("observe",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, EMPATHY_TYPE_DISPATCH_OPERATION);

  signals[APPROVE] =
    g_signal_new ("approve",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, EMPATHY_TYPE_DISPATCH_OPERATION);

  signals[DISPATCH] =
    g_signal_new ("dispatch",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, EMPATHY_TYPE_DISPATCH_OPERATION);

  g_type_class_add_private (object_class, sizeof (EmpathyDispatcherPriv));

}

static void
empathy_dispatcher_init (EmpathyDispatcher *dispatcher)
{
  GList *accounts, *l;
  EmpathyDispatcherPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (dispatcher,
    EMPATHY_TYPE_DISPATCHER, EmpathyDispatcherPriv);

  dispatcher->priv = priv;
  priv->mc = empathy_mission_control_dup_singleton ();
  priv->account_manager = empathy_account_manager_dup_singleton ();

  g_signal_connect (priv->account_manager,
    "account-connection-changed",
    G_CALLBACK (dispatcher_account_connection_cb),
    dispatcher);

  priv->accounts = g_hash_table_new_full (empathy_account_hash,
        empathy_account_equal, g_object_unref, g_object_unref);

  priv->connections = g_hash_table_new_full (g_direct_hash, g_direct_equal,
    g_object_unref, (GDestroyNotify) free_connection_data);

  priv->channels = NULL;

  accounts = mc_accounts_list_by_enabled (TRUE);

  for (l = accounts; l; l = l->next)
    {
      dispatcher_update_account (dispatcher, l->data);
      g_object_unref (l->data);
    }
  g_list_free (accounts);
}

EmpathyDispatcher *
empathy_dispatcher_dup_singleton (void)
{
  return EMPATHY_DISPATCHER (g_object_new (EMPATHY_TYPE_DISPATCHER, NULL));
}

static void
dispatcher_request_failed (EmpathyDispatcher *dispatcher,
                           DispatcherRequestData *request_data,
                           const GError *error)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  ConnectionData *conn_data;

  conn_data = g_hash_table_lookup (priv->connections, request_data->connection);
  if (request_data->cb != NULL)
    request_data->cb (NULL, error, request_data->user_data);

  conn_data->outstanding_requests =
      g_list_remove (conn_data->outstanding_requests, request_data);
  free_dispatcher_request_data (request_data);
}

static void
dispatcher_connection_new_requested_channel (EmpathyDispatcher *dispatcher,
                                             DispatcherRequestData *request_data,
                                             const gchar *object_path,
                                             GHashTable *properties,
                                             const GError *error)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  EmpathyDispatchOperation *operation = NULL;
  ConnectionData *conn_data;

  conn_data = g_hash_table_lookup (priv->connections,
    request_data->connection);

  if (error)
    {
      DEBUG ("Channel request failed: %s", error->message);

      dispatcher_request_failed (dispatcher, request_data, error);

      goto out;
    }

  operation = g_hash_table_lookup (conn_data->outstanding_channels,
    object_path);

  if (operation != NULL)
    g_hash_table_remove (conn_data->outstanding_channels, object_path);
  else
    operation = g_hash_table_lookup (conn_data->dispatching_channels,
        object_path);

  if (operation == NULL)
    {
      DispatchData *data = g_hash_table_lookup (conn_data->dispatched_channels,
        object_path);

      if (data != NULL)
        {
          operation = empathy_dispatch_operation_new_with_wrapper (
            request_data->connection,
            data->channel, request_data->contact, FALSE,
            data->channel_wrapper);
        }
      else
        {
          TpChannel *channel;

          if (properties != NULL)
            channel = tp_channel_new_from_properties (request_data->connection,
              object_path, properties, NULL);
          else
            channel = tp_channel_new (request_data->connection, object_path,
              request_data->channel_type, request_data->handle_type,
              request_data->handle, NULL);

          g_signal_connect (channel, "invalidated",
            G_CALLBACK (dispatcher_channel_invalidated_cb),
            request_data->dispatcher);

          priv->channels = g_list_prepend (priv->channels, channel);

          operation = empathy_dispatch_operation_new (request_data->connection,
             channel, request_data->contact, FALSE);
          g_object_unref (channel);
        }
    }
  else
    {
      /* Already existed set potential extra information */
      g_object_set (G_OBJECT (operation),
        "contact", request_data->contact,
        NULL);
    }

  request_data->operation = operation;

  /* (pre)-approve this right away as we requested it
   * This might cause the channel to be claimed, in which case the operation
   * will disappear. So ref it, and check the status before starting the
   * dispatching */

  g_object_ref (operation);
  empathy_dispatch_operation_approve (operation);

   if (empathy_dispatch_operation_get_status (operation) <
     EMPATHY_DISPATCHER_OPERATION_STATE_APPROVING)
      dispatcher_start_dispatching (request_data->dispatcher, operation,
          conn_data);

  g_object_unref (operation);

out:
  dispatcher_flush_outstanding_operations (request_data->dispatcher,
    conn_data);
}

static void
dispatcher_request_channel_cb (TpConnection *connection,
                               const gchar  *object_path,
                               const GError *error,
                               gpointer user_data,
                               GObject *weak_object)
{
  EmpathyDispatcher *dispatcher = EMPATHY_DISPATCHER (weak_object);
  DispatcherRequestData *request_data = (DispatcherRequestData*) user_data;

  dispatcher_connection_new_requested_channel (dispatcher,
    request_data, object_path, NULL, error);
}

static void
dispatcher_request_channel (DispatcherRequestData *request_data)
{
  tp_cli_connection_call_request_channel (request_data->connection, -1,
    request_data->channel_type,
    request_data->handle_type,
    request_data->handle,
    TRUE, dispatcher_request_channel_cb,
    request_data, NULL, G_OBJECT (request_data->dispatcher));
}

void
empathy_dispatcher_call_with_contact (EmpathyContact *contact,
                                      EmpathyDispatcherRequestCb *callback,
                                      gpointer user_data)
{
  EmpathyDispatcher *dispatcher = empathy_dispatcher_dup_singleton();
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  McAccount *account;
  TpConnection *connection;
  ConnectionData *cd;
  DispatcherRequestData *request_data;

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));

  account = empathy_contact_get_account (contact);
  connection = g_hash_table_lookup (priv->accounts, account);

  g_assert (connection != NULL);
  cd = g_hash_table_lookup (priv->connections, connection);
  request_data  = new_dispatcher_request_data (dispatcher, connection,
    TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA, TP_HANDLE_TYPE_NONE, 0, NULL,
    contact, callback, user_data);

  cd->outstanding_requests = g_list_prepend
    (cd->outstanding_requests, request_data);

  dispatcher_request_channel (request_data);

  g_object_unref (dispatcher);
}

static void
dispatcher_chat_with_contact_cb (EmpathyContact *contact,
                                 const GError *error,
                                 gpointer user_data,
                                 GObject *object)
{
  DispatcherRequestData *request_data = (DispatcherRequestData *) user_data;

  request_data->handle = empathy_contact_get_handle (contact);

  dispatcher_request_channel (request_data);
}

void
empathy_dispatcher_chat_with_contact (EmpathyContact *contact,
                                      EmpathyDispatcherRequestCb *callback,
                                      gpointer user_data)
{
  EmpathyDispatcher *dispatcher;
  EmpathyDispatcherPriv *priv;
  McAccount *account;
  TpConnection *connection;
  ConnectionData *connection_data;
  DispatcherRequestData *request_data;

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));

  dispatcher = empathy_dispatcher_dup_singleton();
  priv = GET_PRIV (dispatcher);

  account = empathy_contact_get_account (contact);
  connection = g_hash_table_lookup (priv->accounts, account);
  connection_data = g_hash_table_lookup (priv->connections, connection);

  /* The contact handle might not be known yet */
  request_data  = new_dispatcher_request_data (dispatcher, connection,
    TP_IFACE_CHANNEL_TYPE_TEXT, TP_HANDLE_TYPE_CONTACT, 0, NULL,
    contact, callback, user_data);

  connection_data->outstanding_requests = g_list_prepend
    (connection_data->outstanding_requests, request_data);

  empathy_contact_call_when_ready (contact,
    EMPATHY_CONTACT_READY_HANDLE, dispatcher_chat_with_contact_cb,
    request_data, NULL, G_OBJECT (dispatcher));

  g_object_unref (dispatcher);
}

void
empathy_dispatcher_chat_with_contact_id (McAccount *account,
                                         const gchar *contact_id,
                                         EmpathyDispatcherRequestCb *callback,
                                         gpointer user_data)
{
  EmpathyDispatcher *dispatcher = empathy_dispatcher_dup_singleton ();
  EmpathyContactFactory *factory;
  EmpathyContact        *contact;

  g_return_if_fail (MC_IS_ACCOUNT (account));
  g_return_if_fail (!EMP_STR_EMPTY (contact_id));

  factory = empathy_contact_factory_dup_singleton ();
  contact = empathy_contact_factory_get_from_id (factory, account, contact_id);

  empathy_dispatcher_chat_with_contact (contact, callback, user_data);

  g_object_unref (contact);
  g_object_unref (factory);
  g_object_unref (dispatcher);
}

static void
dispatcher_request_handles_cb (TpConnection *connection,
                               const GArray *handles,
                               const GError *error,
                               gpointer user_data,
                               GObject *object)
{
  DispatcherRequestData *request_data = (DispatcherRequestData *) user_data;

  if (error != NULL)
    {
      EmpathyDispatcher *dispatcher = EMPATHY_DISPATCHER (object);
      EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
      ConnectionData *cd;

      cd = g_hash_table_lookup (priv->connections, request_data->connection);

      if (request_data->cb)
        request_data->cb (NULL, error, request_data->user_data);

      cd->outstanding_requests = g_list_remove (cd->outstanding_requests,
        request_data);

      free_dispatcher_request_data (request_data);

      dispatcher_flush_outstanding_operations (dispatcher, cd);
      return;
    }

  request_data->handle = g_array_index (handles, guint, 0);
  dispatcher_request_channel (request_data);
}

void
empathy_dispatcher_join_muc (McAccount *account,
                             const gchar *roomname,
                             EmpathyDispatcherRequestCb *callback,
                             gpointer user_data)
{
  EmpathyDispatcher *dispatcher;
  EmpathyDispatcherPriv *priv;
  DispatcherRequestData *request_data;
  TpConnection *connection;
  ConnectionData *connection_data;
  const gchar *names[] = { roomname, NULL };

  g_return_if_fail (MC_IS_ACCOUNT (account));
  g_return_if_fail (!EMP_STR_EMPTY (roomname));

  dispatcher = empathy_dispatcher_dup_singleton();
  priv = GET_PRIV (dispatcher);

  connection = g_hash_table_lookup (priv->accounts, account);
  connection_data = g_hash_table_lookup (priv->connections, connection);


  /* Don't know the room handle yet */
  request_data  = new_dispatcher_request_data (dispatcher, connection,
    TP_IFACE_CHANNEL_TYPE_TEXT, TP_HANDLE_TYPE_ROOM, 0, NULL,
    NULL, callback, user_data);

  connection_data->outstanding_requests = g_list_prepend
    (connection_data->outstanding_requests, request_data);

  tp_cli_connection_call_request_handles (connection, -1,
    TP_HANDLE_TYPE_ROOM, names,
    dispatcher_request_handles_cb, request_data, NULL,
    G_OBJECT (dispatcher));

  g_object_unref (dispatcher);
}

static void
dispatcher_create_channel_cb (TpConnection *connect,
                              const gchar *object_path,
                              GHashTable *properties,
                              const GError *error,
                              gpointer user_data,
                              GObject *weak_object)
{
  EmpathyDispatcher *dispatcher = EMPATHY_DISPATCHER (weak_object);
  DispatcherRequestData *request_data = (DispatcherRequestData*) user_data;

  dispatcher_connection_new_requested_channel (dispatcher,
    request_data, object_path, properties, error);
}

void
empathy_dispatcher_create_channel (EmpathyDispatcher *dispatcher,
                                   McAccount *account,
                                   GHashTable *request,
                                   EmpathyDispatcherRequestCb *callback,
                                   gpointer user_data)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  ConnectionData *connection_data;
  DispatcherRequestData *request_data;
  const gchar *channel_type;
  guint handle_type;
  guint handle;
  gboolean valid;
  TpConnection *connection;

  g_return_if_fail (EMPATHY_IS_DISPATCHER (dispatcher));
  g_return_if_fail (MC_IS_ACCOUNT (account));
  g_return_if_fail (request != NULL);

  connection = g_hash_table_lookup (priv->accounts, account);
  g_assert (connection != NULL);

  connection_data = g_hash_table_lookup (priv->connections, connection);
  g_assert (connection_data != NULL);

  channel_type = tp_asv_get_string (request, TP_IFACE_CHANNEL ".ChannelType");

  handle_type = tp_asv_get_uint32 (request,
    TP_IFACE_CHANNEL ".TargetHandleType", &valid);
  if (!valid)
    handle_type = TP_UNKNOWN_HANDLE_TYPE;

  handle = tp_asv_get_uint32 (request, TP_IFACE_CHANNEL ".TargetHandle", NULL);

  request_data  = new_dispatcher_request_data (dispatcher, connection,
    channel_type, handle_type, handle, request,
    NULL, callback, user_data);

  connection_data->outstanding_requests = g_list_prepend
    (connection_data->outstanding_requests, request_data);

  tp_cli_connection_interface_requests_call_create_channel (
    request_data->connection, -1,
    request_data->request, dispatcher_create_channel_cb, request_data, NULL,
    G_OBJECT (request_data->dispatcher));
}

static void
dispatcher_create_channel_with_contact_cb (EmpathyContact *contact,
                                           const GError *error,
                                           gpointer user_data,
                                           GObject *object)
{
  DispatcherRequestData *request_data = (DispatcherRequestData *) user_data;
  GValue *target_handle;

  g_assert (request_data->request);

  if (error != NULL)
    {
      dispatcher_request_failed (request_data->dispatcher,
        request_data, error);
      return;
    }

  request_data->handle = empathy_contact_get_handle (contact);

  target_handle = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (target_handle, request_data->handle);
  g_hash_table_insert (request_data->request,
    TP_IFACE_CHANNEL ".TargetHandle", target_handle);

  tp_cli_connection_interface_requests_call_create_channel (
    request_data->connection, -1,
    request_data->request, dispatcher_create_channel_cb, request_data, NULL,
    G_OBJECT (request_data->dispatcher));
}

static void
dispatcher_send_file_connection_ready_cb (TpConnection *connection,
                                          const GError *error,
                                          gpointer user_data)
{
  DispatcherRequestData *request_data = (DispatcherRequestData *) user_data;

  if (error !=  NULL)
    {
      dispatcher_request_failed (request_data->dispatcher,
          request_data, error);
      return;
    }

  empathy_contact_call_when_ready (request_data->contact,
    EMPATHY_CONTACT_READY_HANDLE, dispatcher_create_channel_with_contact_cb,
    request_data, NULL, G_OBJECT (request_data->dispatcher));
}

void
empathy_dispatcher_send_file_to_contact (EmpathyContact *contact,
                                         const gchar *filename,
                                         guint64 size,
                                         guint64 date,
                                         const gchar *content_type,
                                         EmpathyDispatcherRequestCb *callback,
                                         gpointer user_data)
{
  EmpathyDispatcher *dispatcher = empathy_dispatcher_dup_singleton();
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  McAccount *account = empathy_contact_get_account (contact);
  TpConnection *connection = g_hash_table_lookup (priv->accounts, account);
  ConnectionData *connection_data =
    g_hash_table_lookup (priv->connections, connection);
  DispatcherRequestData *request_data;
  GValue *value;
  GHashTable *request = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));
  g_return_if_fail (!EMP_STR_EMPTY (filename));
  g_return_if_fail (!EMP_STR_EMPTY (content_type));

  /* org.freedesktop.Telepathy.Channel.ChannelType */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER);
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".ChannelType", value);

  /* org.freedesktop.Telepathy.Channel.TargetHandleType */
  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, TP_HANDLE_TYPE_CONTACT);
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".TargetHandleType", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.ContentType */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, content_type);
  g_hash_table_insert (request,
    TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".ContentType", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Filename */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, filename);
  g_hash_table_insert (request,
    TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Filename", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Size */
  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, size);
  g_hash_table_insert (request,
    TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Size", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Date */
  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, date);
  g_hash_table_insert (request,
    TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Date", value);


  /* The contact handle might not be known yet */
  request_data  = new_dispatcher_request_data (dispatcher, connection,
    TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER, TP_HANDLE_TYPE_CONTACT, 0, request,
    contact, callback, user_data);
  connection_data->outstanding_requests = g_list_prepend
    (connection_data->outstanding_requests, request_data);

  tp_connection_call_when_ready (connection,
      dispatcher_send_file_connection_ready_cb, (gpointer) request_data);

  g_object_unref (dispatcher);
}

GStrv
empathy_dispatcher_find_channel_class (EmpathyDispatcher *dispatcher,
                                       McAccount *account,
                                       const gchar *channel_type,
                                       guint handle_type)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  ConnectionData *cd;
  TpConnection *connection;
  int i;
  GPtrArray *classes;

  g_return_val_if_fail (channel_type != NULL, NULL);
  g_return_val_if_fail (handle_type != 0, NULL);

  connection = g_hash_table_lookup (priv->accounts, account);

  if (connection == NULL)
    return NULL;

  cd = g_hash_table_lookup (priv->connections, connection);

  if (cd == NULL)
    return NULL;


  classes = cd->requestable_channels;
  if (classes == NULL)
    return NULL;

  for (i = 0; i < classes->len; i++)
    {
      GValueArray *class;
      GValue *fixed;
      GValue *allowed;
      GHashTable *fprops;
      const gchar *c_type;
      guint32 h_type;
      gboolean valid;

      class = g_ptr_array_index (classes, i);
      fixed = g_value_array_get_nth (class, 0);

      fprops = g_value_get_boxed (fixed);
      c_type = tp_asv_get_string (fprops, TP_IFACE_CHANNEL ".ChannelType");

      if (tp_strdiff (channel_type, c_type))
        continue;

      h_type = tp_asv_get_uint32 (fprops,
        TP_IFACE_CHANNEL ".TargetHandleType", &valid);

      if (!valid || handle_type != h_type)
        continue;

      allowed = g_value_array_get_nth (class, 1);

      return g_value_get_boxed (allowed);
    }

  return NULL;
}

