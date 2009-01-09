/*
 * Copyright (C) 2007-2008 Collabora Ltd.
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

#include <libmissioncontrol/mission-control.h>
#include <libmissioncontrol/mc-account.h>

#include <extensions/extensions.h>

#include "empathy-dispatcher.h"
#include "empathy-utils.h"
#include "empathy-tube-handler.h"
#include "empathy-account-manager.h"
#include "empathy-contact-factory.h"
#include "empathy-tp-group.h"
#include "empathy-tp-file.h"
#include "empathy-chatroom-manager.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_DISPATCHER
#include <libempathy/empathy-debug.h>

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyDispatcher)
typedef struct {
  EmpathyAccountManager *account_manager;
  MissionControl *mc;
  /* connection to connection data mapping */
  GHashTable     *connections;
  /* accounts to connection mapping */
  GHashTable     *accounts;
  gpointer       token;
  GSList         *tubes;
  EmpathyChatroomManager *chatroom_mgr;
} EmpathyDispatcherPriv;

G_DEFINE_TYPE (EmpathyDispatcher, empathy_dispatcher, G_TYPE_OBJECT);

enum {
  OBSERVE,
  APPROVE,
  DISPATCH,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static EmpathyDispatcher *dispatcher = NULL;

typedef struct {
  EmpathyDispatcherTube  public;
  EmpathyContactFactory *factory;
  gchar                 *bus_name;
  gchar                 *object_path;
  guint                  ref_count;
  gboolean               handled;
} DispatcherTube;

typedef struct {
  EmpathyDispatcher *dispatcher;
  EmpathyDispatchOperation *operation;
  TpConnection *connection;
  gchar *channel_type;
  guint handle_type;
  guint handle;
  EmpathyContact *contact;
  /* Properties to pass to the channel when requesting it */
  GHashTable *properties;
  EmpathyDispatcherRequestCb *cb;
  gpointer user_data;
  gpointer *request_data;
} DispatcherRequestData;

typedef struct {
  TpChannel *channel;
  /* Channel type specific wrapper object */
  GObject *channel_wrapper;
} DispatchData;

typedef struct {
  McAccount *account;
  /* ObjectPath => DispatchData.. */
  GHashTable *dispatched_channels;
  /* ObjectPath -> EmpathyDispatchOperations */
  GHashTable *dispatching_channels;
  /* ObjectPath -> EmpathyDispatchOperations */
  GHashTable *outstanding_channels;
  /* List of DispatcherRequestData */
  GList *outstanding_requests;
} ConnectionData;

static DispatchData *
new_dispatch_data (TpChannel *channel, GObject *channel_wrapper)
{
  DispatchData *d = g_slice_new0 (DispatchData);
  d->channel = channel;
  d->channel_wrapper = channel_wrapper;

  return d;
}

static void
free_dispatch_data (DispatchData *data)
{
  g_object_unref (data->channel);
  g_object_unref (data->channel_wrapper);

  g_slice_free (DispatchData, data);
}


static DispatcherRequestData *
new_dispatcher_request_data (EmpathyDispatcher *dispatcher,
  TpConnection *connection, const gchar *channel_type, guint handle_type,
  guint handle, GHashTable *properties,
  EmpathyContact *contact, EmpathyDispatcherRequestCb *cb, gpointer user_data)
{
  DispatcherRequestData *result = g_slice_new0 (DispatcherRequestData);

  result->dispatcher = dispatcher;
  result->connection = connection;

  result->channel_type = g_strdup (channel_type);
  result->handle_type = handle_type;
  result->handle = handle;

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

  if (r->contact != NULL)
    g_object_unref (r->contact);

  if (r->properties != NULL)
    g_hash_table_unref (r->properties);

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

  for (l = cd->outstanding_requests ; l != NULL; l = g_list_delete_link (l,l))
    {
      free_dispatcher_request_data (l->data);
    }
}

#if 0
GType
empathy_dispatcher_tube_get_type (void)
{
	static GType type_id = 0;

	if (!type_id) {
		type_id = g_boxed_type_register_static ("EmpathyDispatcherTube",
							(GBoxedCopyFunc) empathy_dispatcher_tube_ref,
							(GBoxedFreeFunc) empathy_dispatcher_tube_unref);
	}

	return type_id;
}

EmpathyDispatcherTube *
empathy_dispatcher_tube_ref (EmpathyDispatcherTube *data)
{
	DispatcherTube *tube = (DispatcherTube*) data;

	g_return_val_if_fail (tube != NULL, NULL);

	tube->ref_count++;

	return data;
}

void
empathy_dispatcher_tube_unref (EmpathyDispatcherTube *data)
{
	DispatcherTube *tube = (DispatcherTube*) data;

	g_return_if_fail (tube != NULL);

	if (--tube->ref_count == 0) {
		if (!tube->handled) {
			DEBUG ("Tube can't be handled, closing");
			tp_cli_channel_type_tubes_call_close_tube (tube->public.channel, -1,
								   tube->public.id,
								   NULL, NULL, NULL,
								   NULL);
		}

		g_free (tube->bus_name);
		g_free (tube->object_path);
		g_object_unref (tube->factory);
		g_object_unref (tube->public.channel);
		g_object_unref (tube->public.initiator);
		g_slice_free (DispatcherTube, tube);
	}
}

static void
dispatcher_tubes_handle_tube_cb (TpProxy      *channel,
				 const GError *error,
				 gpointer      user_data,
				 GObject      *dispatcher)
{
	DispatcherTube *tube = user_data;

	if (error) {
		DEBUG ("Error: %s", error->message);
	} else {
		tube->handled = TRUE;
	}
}


void
empathy_dispatcher_tube_process (EmpathyDispatcher     *dispatcher,
				 EmpathyDispatcherTube *etube)
{
	DispatcherTube *tube = (DispatcherTube*) etube;

	if (tube->public.activatable) {
		TpProxy *connection;
		TpProxy *thandler;
		gchar   *object_path;
		guint    handle_type;
		guint    handle;

		/* Create the proxy for the tube handler */
		thandler = g_object_new (TP_TYPE_PROXY,
					 "dbus-connection", tp_get_bus (),
					 "bus-name", tube->bus_name,
					 "object-path", tube->object_path,
					 NULL);
		tp_proxy_add_interface_by_id (thandler, EMP_IFACE_QUARK_TUBE_HANDLER);

		/* Give the tube to the handler */
		g_object_get (tube->public.channel,
			      "connection", &connection,
			      "object-path", &object_path,
			      "handle_type", &handle_type,
			      "handle", &handle,
			      NULL);

		DEBUG ("Dispatching tube");
		emp_cli_tube_handler_call_handle_tube (thandler, -1,
						       connection->bus_name,
						       connection->object_path,
						       object_path, handle_type,
						       handle, tube->public.id,
						       dispatcher_tubes_handle_tube_cb,
						       empathy_dispatcher_tube_ref (etube),
						       (GDestroyNotify) empathy_dispatcher_tube_unref,
						       G_OBJECT (dispatcher));

		g_object_unref (thandler);
		g_object_unref (connection);
		g_free (object_path);
	}
}

static void
dispatcher_tubes_new_tube_cb (TpChannel   *channel,
			      guint        id,
			      guint        initiator,
			      guint        type,
			      const gchar *service,
			      GHashTable  *parameters,
			      guint        state,
			      gpointer     user_data,
			      GObject     *dispatcher)
{
	static TpDBusDaemon   *daemon = NULL;
	DispatcherTube        *tube;
	McAccount             *account;
	guint                  number;
	gchar                **names;
	gboolean               running = FALSE;
	GError                *error = NULL;

	/* Increase tube count */
	number = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (channel), "tube-count"));
	g_object_set_data (G_OBJECT (channel), "tube-count", GUINT_TO_POINTER (++number));
	DEBUG ("Increased tube count for channel %p: %d", channel, number);

	/* We dispatch only local pending tubes */
	if (state != TP_TUBE_STATE_LOCAL_PENDING) {
		return;
	}

	if (!daemon) {
		daemon = tp_dbus_daemon_new (tp_get_bus ());
	}

	account = empathy_channel_get_account (channel);
	tube = g_slice_new (DispatcherTube);
	tube->ref_count = 1;
	tube->handled = FALSE;
	tube->factory = empathy_contact_factory_dup_singleton ();
	tube->bus_name = empathy_tube_handler_build_bus_name (type, service);
	tube->object_path = empathy_tube_handler_build_object_path (type, service);
	tube->public.activatable = FALSE;
	tube->public.id = id;
	tube->public.channel = g_object_ref (channel);
	tube->public.initiator = empathy_contact_factory_get_from_handle (tube->factory,
									  account,
									  initiator);
	g_object_unref (account);

	DEBUG ("Looking for tube handler: %s", tube->bus_name);
	/* Check if that bus-name has an owner, if it has one that means the
	 * app is already running and we can directly give the channel. */
	tp_cli_dbus_daemon_run_name_has_owner (daemon, -1, tube->bus_name,
					       &running, NULL, NULL);
	if (running) {
		DEBUG ("Tube handler running");
		tube->public.activatable = TRUE;
		empathy_dispatcher_tube_process (EMPATHY_DISPATCHER (dispatcher),
						 (EmpathyDispatcherTube*) tube);
		empathy_dispatcher_tube_unref ((EmpathyDispatcherTube*) tube);
		return;
	}

	DEBUG ("Tube handler is not running. Try to activate it");
	/* Check if that bus-name is activatable, if not that means the
	 * application needed to handle this tube isn't installed. */
	if (!tp_cli_dbus_daemon_run_list_activatable_names (daemon, -1,
							    &names, &error,
							    NULL)) {
		DEBUG ("Error listing activatable names: %s", error->message);
		g_clear_error (&error);
	} else {
		gchar **name;

		for (name = names; *name; name++) {
			if (!tp_strdiff (*name, tube->bus_name)) {
				DEBUG ("Found tube handler");
				tube->public.activatable = TRUE;
				break;
			}
		}
		g_strfreev (names);
	}

	if (!tube->public.activatable)
		DEBUG ("Didn't find tube handler");

	g_signal_emit (dispatcher, signals[FILTER_TUBE], 0, tube);
	empathy_dispatcher_tube_unref ((EmpathyDispatcherTube*) tube);
}

static void
dispatcher_tubes_list_tubes_cb (TpChannel       *channel,
				const GPtrArray *tubes,
				const GError    *error,
				gpointer         user_data,
				GObject         *dispatcher)
{
	guint i;

	if (error) {
		DEBUG ("Error: %s", error->message);
		return;
	}

	for (i = 0; i < tubes->len; i++) {
		GValueArray *values;

		values = g_ptr_array_index (tubes, i);
		dispatcher_tubes_new_tube_cb (channel,
					      g_value_get_uint (g_value_array_get_nth (values, 0)),
					      g_value_get_uint (g_value_array_get_nth (values, 1)),
					      g_value_get_uint (g_value_array_get_nth (values, 2)),
					      g_value_get_string (g_value_array_get_nth (values, 3)),
					      g_value_get_boxed (g_value_array_get_nth (values, 4)),
					      g_value_get_uint (g_value_array_get_nth (values, 5)),
					      user_data, dispatcher);
	}
}

static void
dispatcher_tubes_channel_invalidated_cb (TpProxy           *proxy,
					 guint              domain,
					 gint               code,
					 gchar             *message,
					 EmpathyDispatcher *dispatcher)
{
	EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);

	DEBUG ("%s", message);

	priv->tubes = g_slist_remove (priv->tubes, proxy);
	g_object_unref (proxy);
}

static void
dispatcher_tubes_tube_closed_cb (TpChannel *channel,
				 guint      id,
				 gpointer   user_data,
				 GObject   *dispatcher)
{
	guint number;

	number = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (channel), "tube-count"));
	if (number == 1) {
		DEBUG ("No more tube, closing channel");
		tp_cli_channel_call_close (channel, -1, NULL, NULL, NULL, NULL);
	}
	else if (number > 1) {
		DEBUG ("Decrease tube count: %d", number);
		g_object_set_data (G_OBJECT (channel), "tube-count", GUINT_TO_POINTER (--number));
	}
}


static void
dispatcher_tubes_handle_channel (EmpathyDispatcher *dispatcher,
				 TpChannel         *channel)
{
	EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);

	DEBUG ("Called");

	priv->tubes = g_slist_prepend (priv->tubes, g_object_ref (channel));
	g_signal_connect (channel, "invalidated",
			  G_CALLBACK (dispatcher_tubes_channel_invalidated_cb),
			  dispatcher);

	tp_cli_channel_type_tubes_connect_to_tube_closed (channel,
							  dispatcher_tubes_tube_closed_cb,
							  NULL, NULL,
							  G_OBJECT (dispatcher), NULL);
	tp_cli_channel_type_tubes_connect_to_new_tube (channel,
						       dispatcher_tubes_new_tube_cb,
						       NULL, NULL,
						       G_OBJECT (dispatcher), NULL);
	tp_cli_channel_type_tubes_call_list_tubes (channel, -1,
						   dispatcher_tubes_list_tubes_cb,
						   NULL, NULL,
						   G_OBJECT (dispatcher));
}

#endif

static void
dispatcher_connection_invalidated_cb (TpConnection  *connection,
  guint          domain, gint           code, gchar         *message,
  EmpathyDispatcher *dispatcher)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  ConnectionData *cd;

  DEBUG ("Error: %s", message);
  cd = g_hash_table_lookup (priv->connections, connection);

  g_hash_table_remove (priv->accounts, cd->account);
  g_hash_table_remove (priv->connections, connection);
}

#if 0

typedef struct
{
  EmpathyDispatcher *self;
  EmpathyChatroom *chatroom;
} dispatcher_connection_invalidated_cb_ctx;

static dispatcher_connection_invalidated_cb_ctx *
dispatcher_connection_invalidated_cb_ctx_new (EmpathyDispatcher *dispatcher,
                                              EmpathyChatroom *chatroom)
{
  dispatcher_connection_invalidated_cb_ctx *ctx;

  ctx = g_slice_new (dispatcher_connection_invalidated_cb_ctx);

  ctx->self = g_object_ref (dispatcher);
  ctx->chatroom = g_object_ref (chatroom);

  return ctx;
}

static void
dispatcher_connection_invalidated_cb_ctx_free (
    dispatcher_connection_invalidated_cb_ctx *ctx)
{
  g_object_unref (ctx->self);
  g_object_unref (ctx->chatroom);

  g_slice_free (dispatcher_connection_invalidated_cb_ctx, ctx);
}

static void dispatcher_chatroom_invalidated_cb (
    TpProxy *channel,
    guint domain,
    gint code,
    gchar *message,
    dispatcher_connection_invalidated_cb_ctx *ctx)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (ctx->self);
  gboolean favorite;

  g_object_get (ctx->chatroom, "favorite", &favorite, NULL);

  if (favorite)
    {
      /* Chatroom is in favorites so don't remove it from the manager */
      g_object_set (ctx->chatroom, "tp-channel", NULL, NULL);
    }
  else
    {
      empathy_chatroom_manager_remove (priv->chatroom_mgr, ctx->chatroom);
    }
}

#endif


/********************* Sanity from here at some point *********/
static gboolean
dispatcher_operation_can_start (EmpathyDispatcher *self,
  EmpathyDispatchOperation *operation, ConnectionData *cd)
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
  EmpathyDispatchOperation *operation, GError *error, ConnectionData *cd)
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
dispatcher_channel_invalidated_cb (TpProxy *proxy, guint domain, gint code,
  gchar *message, EmpathyDispatcher *dispatcher)
{
  /* Channel went away... */
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  TpConnection *connection;
  EmpathyDispatchOperation *operation;
  ConnectionData *cd;
  const gchar *object_path;

  g_object_get (G_OBJECT (proxy), "connection", &connection, NULL);

  cd = g_hash_table_lookup (priv->connections, connection);

  object_path = tp_proxy_get_object_path (proxy);

  DEBUG ("Channel %s invalidated", object_path);

  g_hash_table_remove (cd->dispatched_channels, object_path);
  g_hash_table_remove (cd->dispatching_channels, object_path);

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

}

static void
dispatcher_start_dispatching (EmpathyDispatcher *self,
  EmpathyDispatchOperation *operation, ConnectionData *cd)
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
  const gchar  *object_path, const gchar  *channel_type,
  guint handle_type, guint handle, GHashTable *properties,
  gboolean incoming)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  TpChannel         *channel;
  ConnectionData *cd;
  EmpathyDispatchOperation *operation;
  EmpathyContact *contact = NULL;

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

  DEBUG ("New channel of type %s on %s",
    channel_type, object_path);

  if (properties == NULL)
    channel = tp_channel_new (connection, object_path, channel_type,
      handle_type, handle, NULL);
  else
    channel = tp_channel_new_from_properties (connection, object_path,
      properties, NULL);

  g_signal_connect (channel, "invalidated",
    G_CALLBACK (dispatcher_channel_invalidated_cb),
    dispatcher);

  if (handle_type == TP_CONN_HANDLE_TYPE_CONTACT)
    {
      EmpathyContactFactory *factory = empathy_contact_factory_new ();
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
  const gchar  *object_path, const gchar  *channel_type,
  guint         handle_type, guint         handle,
  gboolean      suppress_handler, gpointer      user_data,
  GObject      *object)
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
dispatcher_connection_new_channel_with_properties (
  EmpathyDispatcher *dispatcher, TpConnection *connection,
  const gchar *object_path, GHashTable *properties)
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
dispatcher_connection_new_channels_cb (
  TpConnection *connection, const GPtrArray *channels, gpointer user_data,
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

#if 0  /* old dispatching  */
  channel = tp_channel_new (connection, object_path, channel_type,
    handle_type, handle, NULL);
  tp_channel_run_until_ready (channel, NULL, NULL);

  if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_TUBES)) {
    dispatcher_tubes_handle_channel (dispatcher, channel);
  }

  if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_TEXT) &&
      handle_type == TP_HANDLE_TYPE_ROOM)
    {
      /* Add the chatroom to the chatroom manager */
      EmpathyChatroom *chatroom;
      GArray *handles;
      gchar **room_ids;
      MissionControl *mc;
      McAccount *account;
      dispatcher_connection_invalidated_cb_ctx *ctx;

      handles = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), 1);
      g_array_append_val (handles, handle);

      tp_cli_connection_run_inspect_handles (connection, -1,
          TP_HANDLE_TYPE_ROOM, handles, &room_ids, NULL, NULL);

      mc = empathy_mission_control_new ();
      account = mission_control_get_account_for_tpconnection (mc, connection,
          NULL);

      chatroom = empathy_chatroom_manager_find (priv->chatroom_mgr, account,
          room_ids[0]);
      if (chatroom == NULL)
        {
          chatroom = empathy_chatroom_new (account);
          empathy_chatroom_set_name (chatroom, room_ids[0]);
          empathy_chatroom_set_room (chatroom, room_ids[0]);
          empathy_chatroom_manager_add (priv->chatroom_mgr, chatroom);
        }
      else
        {
          g_object_ref (chatroom);
        }

      g_object_set (chatroom, "tp-channel", channel, NULL);

      ctx = dispatcher_connection_invalidated_cb_ctx_new (dispatcher, chatroom);

      g_signal_connect_data (channel, "invalidated",
          G_CALLBACK (dispatcher_chatroom_invalidated_cb), ctx,
          (GClosureNotify) dispatcher_connection_invalidated_cb_ctx_free, 0);

      g_free (room_ids[0]);
      g_free (room_ids);
      g_array_free (handles, TRUE);
      g_object_unref (mc);
      g_object_unref (account);
      g_object_unref (chatroom);
    }

	if (suppress_handler) {
		g_signal_emit (dispatcher, signals[DISPATCH_CHANNEL], 0, channel);
	} else {
		g_signal_emit (dispatcher, signals[FILTER_CHANNEL], 0, channel);
	}

	g_object_unref (channel);

}
#endif

static void
dispatcher_connection_got_channels_property (TpProxy *proxy,
  const GValue *channels_prop, const GError *error, gpointer user_data,
  GObject *object)
{
  GPtrArray *channels;

  if (error) {
    DEBUG ("Error: %s", error->message);
    return;
  }

  channels = g_value_get_boxed (channels_prop);
  dispatcher_connection_new_channels_cb (TP_CONNECTION (proxy),
    channels, NULL, object);
}

static void
dispatcher_connection_list_channels_cb (TpConnection    *connection,
					const GPtrArray *channels,
					const GError    *error,
					gpointer         user_data,
					GObject         *dispatcher)
{
	int i;

	if (error) {
		DEBUG ("Error: %s", error->message);
		return;
	}

	for (i = 0; i < channels->len; i++) {
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
	if (error) {
		DEBUG ("Error: %s", error->message);
	}
}

static void
dispatcher_connection_ready_cb (TpConnection  *connection,
  const GError *error, gpointer       dispatcher)
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

      tp_cli_dbus_properties_call_get (connection, -1,
        TP_IFACE_CONNECTION_INTERFACE_REQUESTS, "Channels",
        dispatcher_connection_got_channels_property,
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
}

static void
dispatcher_update_account (EmpathyDispatcher *dispatcher, McAccount *account)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  TpConnection *connection;

  connection = g_hash_table_lookup (priv->accounts, account);
  if  (connection != NULL)
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
}

static void
dispatcher_account_connection_cb (EmpathyAccountManager *manager,
  McAccount *account, TpConnectionStatusReason reason,
  TpConnectionStatus status, TpConnectionStatus previous,
  EmpathyDispatcher *dispatcher)
{
  dispatcher_update_account (dispatcher, account);
}

static void
dispatcher_finalize (GObject *object)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (object);

  g_signal_handlers_disconnect_by_func (priv->account_manager,
      dispatcher_account_connection_cb, object);

  g_object_unref (priv->account_manager);
  g_object_unref (priv->mc);

  g_hash_table_destroy (priv->connections);

  g_object_unref (priv->chatroom_mgr);
}

static void
empathy_dispatcher_class_init (EmpathyDispatcherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = dispatcher_finalize;

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
  priv->mc = empathy_mission_control_new ();
  priv->account_manager = empathy_account_manager_dup_singleton ();

  g_signal_connect (priv->account_manager,
    "account-connection-changed",
    G_CALLBACK (dispatcher_account_connection_cb),
    dispatcher);

  priv->accounts = g_hash_table_new_full (empathy_account_hash,
        empathy_account_equal, g_object_unref, g_object_unref);

  priv->connections = g_hash_table_new_full (g_direct_hash, g_direct_equal,
    g_object_unref, (GDestroyNotify) free_connection_data);

  accounts = mc_accounts_list_by_enabled (TRUE);

  for (l = accounts; l; l = l->next) {
    dispatcher_update_account (dispatcher, l->data);
    g_object_unref (l->data);
  }
  g_list_free (accounts);

  priv->chatroom_mgr = empathy_chatroom_manager_new (NULL);
}

EmpathyDispatcher *
empathy_get_dispatcher (void)
{
  if (!dispatcher) {
    dispatcher = g_object_new (EMPATHY_TYPE_DISPATCHER, NULL);
    g_object_add_weak_pointer (G_OBJECT (dispatcher), (gpointer) &dispatcher);
  } else {
    g_object_ref (dispatcher);
  }

  return dispatcher;
}

static void
dispatcher_request_channel_cb (TpConnection *connection,
  const gchar  *object_path, const GError *error,
  gpointer      user_data, GObject      *weak_object)
{
  DispatcherRequestData *request_data = (DispatcherRequestData*) user_data;
  EmpathyDispatcherPriv *priv = GET_PRIV (request_data->dispatcher);
  EmpathyDispatchOperation *operation = NULL;
  ConnectionData *conn_data;

  conn_data = g_hash_table_lookup (priv->connections,
    request_data->connection);

  if (error)
    {
      DEBUG ("Channel request failed: %s", error->message);

      if (request_data->cb != NULL)
          request_data->cb (NULL, error, request_data->user_data);

      conn_data->outstanding_requests =
        g_list_remove (conn_data->outstanding_requests, request_data);
      free_dispatcher_request_data (request_data);

      goto out;
  }

  operation = g_hash_table_lookup (conn_data->outstanding_channels,
    object_path);

  if (operation != NULL)
    g_hash_table_remove (conn_data->outstanding_channels, object_path);
  else
    operation = g_hash_table_lookup (conn_data->dispatching_channels,
        object_path);

  /* FIXME check if we got an existing channel back */
  if (operation == NULL)
    {
      DispatchData *data = g_hash_table_lookup (conn_data->dispatched_channels,
        object_path);

      if (data != NULL)
        {
          operation = empathy_dispatch_operation_new_with_wrapper (connection,
            data->channel, request_data->contact, FALSE,
            data->channel_wrapper);
        }
      else
        {
          TpChannel *channel = tp_channel_new (connection, object_path,
            request_data->channel_type, request_data->handle_type,
            request_data->handle, NULL);

          g_signal_connect (channel, "invalidated",
            G_CALLBACK (dispatcher_channel_invalidated_cb),
            request_data->dispatcher);

          operation = empathy_dispatch_operation_new (connection, channel,
            request_data->contact, FALSE);
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

  /* (pre)-approve this right away as we requested it */
  empathy_dispatch_operation_approve (operation);

  dispatcher_start_dispatching (request_data->dispatcher, operation,
        conn_data);
out:
  dispatcher_flush_outstanding_operations (request_data->dispatcher,
    conn_data);
}

void
empathy_dispatcher_call_with_contact ( EmpathyContact *contact,
  EmpathyDispatcherRequestCb *callback, gpointer user_data)
{
  g_assert_not_reached ();
}

void
empathy_dispatcher_call_with_contact_id (McAccount *account,
  const gchar  *contact_id, EmpathyDispatcherRequestCb *callback,
  gpointer user_data)
{
  g_assert_not_reached ();
}

static void
dispatcher_chat_with_contact_cb (EmpathyContact *contact, gpointer user_data)
{
  DispatcherRequestData *request_data = (DispatcherRequestData *) user_data;

  request_data->handle = empathy_contact_get_handle (contact);

  /* Note this does rape the surpress handler semantics */
  tp_cli_connection_call_request_channel (request_data->connection, -1,
    request_data->channel_type,
    request_data->handle_type,
    request_data->handle,
    TRUE, dispatcher_request_channel_cb,
    request_data, NULL, NULL);
}

void
empathy_dispatcher_chat_with_contact (EmpathyContact *contact,
  EmpathyDispatcherRequestCb *callback, gpointer user_data)
{
  EmpathyDispatcher *dispatcher = empathy_get_dispatcher();
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
  McAccount *account = empathy_contact_get_account (contact);
  TpConnection *connection = g_hash_table_lookup (priv->accounts, account);
  ConnectionData *connection_data =
    g_hash_table_lookup (priv->connections, connection);
  DispatcherRequestData *request_data;

  /* The contact handle might not be known yet */
  request_data  = new_dispatcher_request_data (dispatcher, connection,
    TP_IFACE_CHANNEL_TYPE_TEXT, TP_HANDLE_TYPE_CONTACT, 0, NULL,
    contact, callback, user_data);

  connection_data->outstanding_requests = g_list_prepend
    (connection_data->outstanding_requests, request_data);

  empathy_contact_call_when_ready (contact,
    EMPATHY_CONTACT_READY_HANDLE, dispatcher_chat_with_contact_cb,
    request_data);

  g_object_unref (dispatcher);
}

void
empathy_dispatcher_chat_with_contact_id (McAccount *account, const gchar
  *contact_id, EmpathyDispatcherRequestCb *callback, gpointer user_data)
{
  EmpathyDispatcher *dispatcher = empathy_get_dispatcher ();
  EmpathyContactFactory *factory;
  EmpathyContact        *contact;

  factory = empathy_contact_factory_dup_singleton ();
  contact = empathy_contact_factory_get_from_id (factory, account, contact_id);

  empathy_dispatcher_chat_with_contact (contact, callback, user_data);

  g_object_unref (contact);
  g_object_unref (factory);
  g_object_unref (dispatcher);
}

#if 0
typedef struct {
	GFile *gfile;
	TpHandle handle;
} FileChannelRequest;

static void
tp_file_state_notify_cb (EmpathyTpFile *tp_file)
{
	EmpFileTransferState state;

	state = empathy_tp_file_get_state (tp_file, NULL);
	if (state == EMP_FILE_TRANSFER_STATE_COMPLETED ||
	    state == EMP_FILE_TRANSFER_STATE_CANCELLED) {
		DEBUG ("Transfer is done, unref the object");
		g_object_unref (tp_file);
	}
}

static void
file_channel_create_cb (TpConnection *connection,
			const gchar  *object_path,
			GHashTable   *properties,
			const GError *error,
			gpointer      user_data,
			GObject      *weak_object)
{
	TpChannel *channel;
	EmpathyTpFile *tp_file;
	FileChannelRequest *request = (FileChannelRequest *) user_data;

	if (error) {
		DEBUG ("Couldn't request channel: %s", error->message);
		return;
	}

	channel = tp_channel_new (connection,
				 object_path,
				 EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER,
				 TP_HANDLE_TYPE_CONTACT,
				 request->handle,
				 NULL);

	/* We give the ref to the callback, it is responsible to unref the
	 * object once the transfer is done. */
	tp_file = empathy_tp_file_new (channel);
	empathy_tp_file_offer (tp_file, request->gfile, NULL);
	g_signal_connect (tp_file, "notify::state",
			  G_CALLBACK (tp_file_state_notify_cb),
			  NULL);

	g_object_unref (request->gfile);
	g_slice_free (FileChannelRequest, request);
	g_object_unref (channel);
}

#endif

void
empathy_dispatcher_send_file (EmpathyContact *contact,
			      GFile          *gfile)
{
  g_assert_not_reached();
  return;
}

#if 0
	MissionControl     *mc;
	McAccount          *account;
	TpConnection       *connection;
	guint               handle;
	FileChannelRequest *request;
	GHashTable         *args;
	GValue             *value;
	GFileInfo          *info;
	gchar              *filename;
	GTimeVal            last_modif;
	GError             *error = NULL;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));
	g_return_if_fail (G_IS_FILE (gfile));

	info = g_file_query_info (gfile,
				  G_FILE_ATTRIBUTE_STANDARD_SIZE ","
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				  G_FILE_ATTRIBUTE_TIME_MODIFIED,
				  0, NULL, &error);

	if (error) {
		DEBUG ("Can't get info about the file: %s", error->message);
		g_clear_error (&error);
		return;
	}

	mc = empathy_mission_control_new ();
	account = empathy_contact_get_account (contact);
	connection = mission_control_get_tpconnection (mc, account, NULL);
	handle = empathy_contact_get_handle (contact);

	request = g_slice_new0 (FileChannelRequest);
	request->gfile = g_object_ref (gfile);
	request->handle = handle;

	filename = g_file_get_basename (request->gfile);
	tp_connection_run_until_ready (connection, FALSE, NULL, NULL);

	DEBUG ("Sending %s from a stream to %s (size %"G_GINT64_FORMAT", content-type %s)",
	       filename, empathy_contact_get_name (contact),
	       g_file_info_get_size (info),
	       g_file_info_get_content_type (info));

	args = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
				      (GDestroyNotify) tp_g_value_slice_free);

	/* org.freedesktop.Telepathy.Channel.ChannelType */
	value = tp_g_value_slice_new (G_TYPE_STRING);
	g_value_set_string (value, EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER);
	g_hash_table_insert (args, TP_IFACE_CHANNEL ".ChannelType", value);

	/* org.freedesktop.Telepathy.Channel.TargetHandleType */
	value = tp_g_value_slice_new (G_TYPE_UINT);
	g_value_set_uint (value, TP_HANDLE_TYPE_CONTACT);
	g_hash_table_insert (args, TP_IFACE_CHANNEL ".TargetHandleType", value);

	/* org.freedesktop.Telepathy.Channel.TargetHandle */
	value = tp_g_value_slice_new (G_TYPE_UINT);
	g_value_set_uint (value, handle);
	g_hash_table_insert (args, TP_IFACE_CHANNEL ".TargetHandle", value);

	/* org.freedesktop.Telepathy.Channel.Type.FileTransfer.ContentType */
	value = tp_g_value_slice_new (G_TYPE_STRING);
	g_value_set_string (value, g_file_info_get_content_type (info));
	g_hash_table_insert (args,
		EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".ContentType", value);

	/* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Filename */
	value = tp_g_value_slice_new (G_TYPE_STRING);
	g_value_set_string (value, g_filename_display_basename (filename));
	g_hash_table_insert (args,
		EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Filename", value);

	/* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Size */
	value = tp_g_value_slice_new (G_TYPE_UINT64);
	g_value_set_uint64 (value, g_file_info_get_size (info));
	g_hash_table_insert (args,
		EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Size", value);

	/* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Date */
	g_file_info_get_modification_time (info, &last_modif);
	value = tp_g_value_slice_new (G_TYPE_UINT64);
	g_value_set_uint64 (value, last_modif.tv_sec);
	g_hash_table_insert (args,
		EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Date", value);

	/* FIXME: Description ? */
	/* FIXME: ContentHashType and ContentHash ? */

	tp_cli_connection_interface_requests_call_create_channel (connection, -1,
		args, file_channel_create_cb, request, NULL, NULL);

	g_hash_table_destroy (args);
	g_free (filename);
	g_object_unref (mc);
	g_object_unref (connection);
}

#endif
