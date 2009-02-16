/*
 * Copyright (C) 2008 Collabora Ltd.
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
 * Authors: Guillaume Desmottes <guillaume.desmottes@collabora.co.uk>
 *          Elliot Fairweather <elliot.fairweather@collabora.co.uk>
 */

#include <config.h>

#include <telepathy-glib/connection.h>
#include <telepathy-glib/util.h>
#include <extensions/extensions.h>

#include "empathy-contact-factory.h"
#include "empathy-enum-types.h"
#include "empathy-tp-tube.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_TP
#include "empathy-debug.h"

typedef struct {
  TpSocketAddressType type;
  EmpatyTpTubeAcceptStreamTubeCb *callback;
  gpointer user_data;
} EmpathyTpTubeAcceptData;

static EmpathyTpTubeAcceptData *
new_empathy_tp_tube_accept_data (TpSocketAddressType type,
  EmpatyTpTubeAcceptStreamTubeCb *callback, gpointer user_data)
{
  EmpathyTpTubeAcceptData *r;

  r = g_slice_new0 (EmpathyTpTubeAcceptData);
  r->type = type;
  r->callback = callback;
  r->user_data = user_data;

  return r;
}

static void
free_empathy_tp_tube_accept_data (gpointer data)
{
  g_slice_free (EmpathyTpTubeAcceptData, data);
}


#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyTpTube)
typedef struct
{
  TpChannel *channel;
  guint initiator;
  guint type;
  gchar *service;
  /* FIXME readd support for parameters.. */
  GHashTable *parameters;
  guint state;
  EmpathyContact *initiator_contact;
  EmpathyContactFactory *factory;
} EmpathyTpTubePriv;

enum
{
  PROP_0,
  PROP_CHANNEL,
  PROP_STATE,
};

enum
{
  DESTROY,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyTpTube, empathy_tp_tube, G_TYPE_OBJECT)

static void
tp_tube_state_changed_cb (TpProxy *proxy,
                          guint state,
                          gpointer user_data,
                          GObject *tube)
{
  EmpathyTpTubePriv *priv = GET_PRIV (tube);

  DEBUG ("Tube state changed");

  priv->state = state;
  g_object_notify (tube, "state");
}

static void
tp_tube_invalidated_cb (TpChannel     *channel,
                        GQuark         domain,
                        gint           code,
                        gchar         *message,
                        EmpathyTpTube *tube)
{
  DEBUG ("Channel invalidated: %s", message);
  g_signal_emit (tube, signals[DESTROY], 0);
}

static void
tp_tube_async_cb (TpChannel *channel,
                  const GError *error,
                  gpointer user_data,
                  GObject *tube)
{
  if (error)
      DEBUG ("Error %s: %s", (gchar*) user_data, error->message);
}

static void
tp_tube_set_property (GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
  EmpathyTpTubePriv *priv = GET_PRIV (object);

  switch (prop_id)
    {
      case PROP_CHANNEL:
        priv->channel = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
  }
}

static void
tp_tube_get_property (GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
  EmpathyTpTubePriv *priv = GET_PRIV (object);

  switch (prop_id)
    {
      case PROP_CHANNEL:
        g_value_set_object (value, priv->channel);
        break;
      case PROP_STATE:
        g_value_set_uint (value, priv->state);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
  }
}

static GObject *
tp_tube_constructor (GType type,
                     guint n_props,
                     GObjectConstructParam *props)
{
  GObject *self;
  EmpathyTpTubePriv *priv;

  self = G_OBJECT_CLASS (empathy_tp_tube_parent_class)->constructor (
      type, n_props, props);
  priv = GET_PRIV (self);

  g_signal_connect (priv->channel, "invalidated",
      G_CALLBACK (tp_tube_invalidated_cb), self);

  emp_cli_channel_interface_tube_connect_to_tube_channel_state_changed (
    TP_PROXY (priv->channel), tp_tube_state_changed_cb, NULL, NULL,
    self, NULL);

  return self;
}

static void
tp_tube_finalize (GObject *object)
{
  EmpathyTpTubePriv *priv = GET_PRIV (object);

  DEBUG ("Finalizing: %p", object);

  if (priv->channel)
    {
      g_signal_handlers_disconnect_by_func (priv->channel,
          tp_tube_invalidated_cb, object);
      tp_cli_channel_call_close (priv->channel, -1, tp_tube_async_cb,
        "closing tube", NULL, NULL);
      g_object_unref (priv->channel);
    }
  if (priv->initiator_contact)
      g_object_unref (priv->initiator_contact);
  if (priv->factory)
      g_object_unref (priv->factory);

  g_free (priv->service);

  if (priv->parameters != NULL)
  g_hash_table_destroy (priv->parameters);

  G_OBJECT_CLASS (empathy_tp_tube_parent_class)->finalize (object);
}

static void
empathy_tp_tube_class_init (EmpathyTpTubeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = tp_tube_constructor;
  object_class->finalize = tp_tube_finalize;
  object_class->set_property = tp_tube_set_property;
  object_class->get_property = tp_tube_get_property;

  g_object_class_install_property (object_class, PROP_CHANNEL,
      g_param_spec_object ("channel", "channel", "channel", TP_TYPE_CHANNEL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
        G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class, PROP_STATE,
      g_param_spec_uint ("state", "state", "state", 0, G_MAXUINT, 0,
        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
        G_PARAM_STATIC_BLURB));

  signals[DESTROY] = g_signal_new ("destroy",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (EmpathyTpTubePriv));
}

static void
empathy_tp_tube_init (EmpathyTpTube *tube)
{
  EmpathyTpTubePriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (tube,
		EMPATHY_TYPE_TP_TUBE, EmpathyTpTubePriv);

  tube->priv = priv;

  priv->factory = empathy_contact_factory_dup_singleton ();
}

EmpathyTpTube *
empathy_tp_tube_new (TpChannel *channel)
{
  g_return_val_if_fail (TP_IS_CHANNEL (channel), NULL);

  return g_object_new (EMPATHY_TYPE_TP_TUBE, "channel", channel,  NULL);
}

EmpathyTpTube *
empathy_tp_tube_new_stream_tube (EmpathyContact *contact,
                                 TpSocketAddressType type,
                                 const gchar *hostname,
                                 guint port,
                                 const gchar *service,
                                 GHashTable *parameters)
{
  MissionControl *mc;
  McAccount *account;
  TpConnection *connection;
  TpChannel *channel;
  gchar *object_path;
  GHashTable *params;
  GValue *address;
  GValue *control_param;
  EmpathyTpTube *tube = NULL;
  GError *error = NULL;
  GHashTable *request;
  GHashTable *channel_properties;
  GValue *value;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);
  g_return_val_if_fail (hostname != NULL, NULL);
  g_return_val_if_fail (service != NULL, NULL);

  mc = empathy_mission_control_dup_singleton ();
  account = empathy_contact_get_account (contact);
  connection = mission_control_get_tpconnection (mc, account, NULL);
  g_object_unref (mc);

  tp_connection_run_until_ready (connection, FALSE, NULL, NULL);

  request = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);

  /* org.freedesktop.Telepathy.Channel.ChannelType */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, EMP_IFACE_CHANNEL_TYPE_STREAM_TUBE);
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".ChannelType", value);

  /* org.freedesktop.Telepathy.Channel.TargetHandleType */
  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, TP_HANDLE_TYPE_CONTACT);
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".TargetHandleType", value);

  /* org.freedesktop.Telepathy.Channel.TargetHandleType */
  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, empathy_contact_get_handle (contact));
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".TargetHandle", value);

  /* org.freedesktop.Telepathy.Channel.Type.StreamTube.Service */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, service);
  g_hash_table_insert (request,
    EMP_IFACE_CHANNEL_TYPE_STREAM_TUBE  ".Service", value);

  if (!tp_cli_connection_interface_requests_run_create_channel (connection, -1,
    request, &object_path, &channel_properties, &error, NULL))
    {
      DEBUG ("Error requesting channel: %s", error->message);
      g_clear_error (&error);
      g_object_unref (connection);
      return NULL;
    }

  DEBUG ("Offering a new stream tube");

  channel = tp_channel_new_from_properties (connection, object_path,
      channel_properties, NULL);

  tp_channel_run_until_ready (channel, NULL, NULL);

  #define ADDRESS_TYPE dbus_g_type_get_struct ("GValueArray",\
      G_TYPE_STRING, G_TYPE_UINT, G_TYPE_INVALID)
  params = g_hash_table_new (g_str_hash, g_str_equal);
  address = tp_g_value_slice_new (ADDRESS_TYPE);
  g_value_take_boxed (address, dbus_g_type_specialized_construct (ADDRESS_TYPE));
  dbus_g_type_struct_set (address, 0, hostname, 1, port, G_MAXUINT);
  control_param = tp_g_value_slice_new (G_TYPE_STRING);

  if (!emp_cli_channel_type_stream_tube_run_offer_stream_tube (
        TP_PROXY(channel), -1, type, address,
        TP_SOCKET_ACCESS_CONTROL_LOCALHOST, control_param, parameters,
        &error, NULL))
    {
      DEBUG ("Couldn't offer tube: %s", error->message);
      g_clear_error (&error);
      goto OUT;
    }

  DEBUG ("Stream tube offered");

  tube = empathy_tp_tube_new (channel);

OUT:
  g_object_unref (channel);
  g_free (object_path);
  g_hash_table_destroy (request);
  g_hash_table_destroy (channel_properties);
  tp_g_value_slice_free (address);
  tp_g_value_slice_free (control_param);
  g_object_unref (connection);

  return tube;
}

static void
tp_tube_accept_stream_cb (TpProxy *proxy,
                          const GValue *address,
                          const GError *error,
                          gpointer user_data,
                          GObject *weak_object)
{
  EmpathyTpTube *tube = EMPATHY_TP_TUBE (weak_object);
  EmpathyTpTubeAcceptData *data = (EmpathyTpTubeAcceptData *)user_data;
  EmpathyTpTubeAddress eaddress;

  eaddress.type = data->type;

  if (error)
    {
      DEBUG ("Error accepting tube: %s", error->message);
      data->callback (tube, NULL, error, data->user_data);
      return;
    }

  switch (eaddress.type)
    {
      case TP_SOCKET_ADDRESS_TYPE_UNIX:
      case TP_SOCKET_ADDRESS_TYPE_ABSTRACT_UNIX:
        eaddress.a.socket.path = g_value_get_boxed (address);
        break;
     case TP_SOCKET_ADDRESS_TYPE_IPV4:
     case TP_SOCKET_ADDRESS_TYPE_IPV6:
        dbus_g_type_struct_get (address,
          0, &eaddress.a.inet.hostname,
          1, &eaddress.a.inet.port, G_MAXUINT);
        break;
    }

   data->callback (tube, &eaddress, NULL, data->user_data);
}

void
empathy_tp_tube_accept_stream_tube (EmpathyTpTube *tube,
  TpSocketAddressType type, EmpatyTpTubeAcceptStreamTubeCb *callback,
  gpointer user_data)
{
  EmpathyTpTubePriv *priv = GET_PRIV (tube);
  GValue *control_param;
  EmpathyTpTubeAcceptData *data;

  g_return_if_fail (EMPATHY_IS_TP_TUBE (tube));

  DEBUG ("Accepting stream tube");
  /* FIXME allow other acls */
  control_param = tp_g_value_slice_new (G_TYPE_STRING);

  data = new_empathy_tp_tube_accept_data (type, callback, user_data);

  emp_cli_channel_type_stream_tube_call_accept_stream_tube (
     TP_PROXY (priv->channel), -1, type, TP_SOCKET_ACCESS_CONTROL_LOCALHOST,
     control_param, tp_tube_accept_stream_cb, data,
     free_empathy_tp_tube_accept_data, G_OBJECT (tube));

  tp_g_value_slice_free (control_param);
}
