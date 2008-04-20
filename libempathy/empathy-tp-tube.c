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

#include "empathy-contact-factory.h"
#include "empathy-debug.h"
#include "empathy-enum-types.h"
#include "empathy-tp-tube.h"
#include "empathy-utils.h"

#define DEBUG_DOMAIN "TpTube"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EMPATHY_TYPE_TP_TUBE, \
    EmpathyTpTubePriv))

typedef struct _EmpathyTpTubePriv EmpathyTpTubePriv;

struct _EmpathyTpTubePriv
{
  TpChannel *channel;
  guint id;
  guint initiator;
  guint type;
  gchar *service;
  GHashTable *parameters;
  guint state;
  EmpathyContact *initiator_contact;
  EmpathyContactFactory *factory;
};

enum
{
  PROP_0,
  PROP_CHANNEL,
  PROP_TP_TUBES,
  PROP_ID,
  PROP_INITIATOR,
  PROP_TYPE,
  PROP_SERVICE,
  PROP_PARAMETERS,
  PROP_STATE,
  PROP_INITIATOR_CONTACT
};

enum
{
  DESTROY,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyTpTube, empathy_tp_tube, G_TYPE_OBJECT)

static void
tp_tube_state_changed_cb (TpChannel *channel,
                          guint id,
                          guint state,
                          gpointer user_data,
                          GObject *tube)
{
  EmpathyTpTubePriv *priv = GET_PRIV (tube);

  if (id != priv->id)
      return;

  empathy_debug (DEBUG_DOMAIN, "Tube state changed");

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
  empathy_debug (DEBUG_DOMAIN, "Channel invalidated: %s", message);
  g_signal_emit (tube, signals[DESTROY], 0);
}

static void
tp_tube_closed_cb (TpChannel *channel,
                   guint id,
                   gpointer user_data,
                   GObject *tube)
{
  EmpathyTpTubePriv *priv = GET_PRIV (tube);

  if (id != priv->id)
      return;

  empathy_debug (DEBUG_DOMAIN, "Tube closed");
  g_signal_emit (tube, signals[DESTROY], 0);
}

static void
tp_tube_async_cb (TpChannel *channel,
                  const GError *error,
                  gpointer user_data,
                  GObject *tube)
{
  if (error)
      empathy_debug (DEBUG_DOMAIN, "Error %s: %s", user_data, error->message);
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
      case PROP_ID:
        priv->id = g_value_get_uint (value);
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
      case PROP_ID:
        g_value_set_uint (value, priv->id);
        break;
      case PROP_INITIATOR:
        g_value_set_uint (value, priv->initiator);
        break;
      case PROP_TYPE:
        g_value_set_uint (value, priv->type);
        break;
      case PROP_SERVICE:
        g_value_set_string (value, priv->service);
        break;
      case PROP_PARAMETERS:
        g_value_set_boxed (value, priv->parameters);
        break;
      case PROP_STATE:
        g_value_set_uint (value, priv->state);
        break;
      case PROP_INITIATOR_CONTACT:
        g_value_set_object (value, priv->initiator_contact);
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
  GPtrArray *tubes;
  guint i;
  GError *error = NULL;

  self = G_OBJECT_CLASS (empathy_tp_tube_parent_class)->constructor (
      type, n_props, props);
  priv = GET_PRIV (self);

  g_signal_connect (priv->channel, "invalidated",
      G_CALLBACK (tp_tube_invalidated_cb), self);
  tp_cli_channel_type_tubes_connect_to_tube_closed (priv->channel,
      tp_tube_closed_cb, NULL, NULL, self, NULL);
  tp_cli_channel_type_tubes_connect_to_tube_state_changed (priv->channel,
      tp_tube_state_changed_cb, NULL, NULL, self, NULL);

  /* FIXME: It is absolutely not opimized to list all tubes to get information
   * about our tube, but we don't really have the choice to avoid races. */
  if (!tp_cli_channel_type_tubes_run_list_tubes (priv->channel, -1, &tubes,
      &error, NULL))
    {
      empathy_debug (DEBUG_DOMAIN, "Couldn't list tubes: %s",
          error->message);
      g_clear_error (&error);
      return self;
    }

  for (i = 0; i < tubes->len; i++)
    {
      GValueArray *values;
      guint id;

      values = g_ptr_array_index (tubes, i);
      id = g_value_get_uint (g_value_array_get_nth (values, 0));

      if (id == priv->id)
        {
          TpConnection *connection;
          MissionControl *mc;
          McAccount *account;

          g_object_get (priv->channel, "connection", &connection, NULL);
          mc = empathy_mission_control_new ();
          account = mission_control_get_account_for_tpconnection (mc,
              connection, NULL);

          priv->initiator = g_value_get_uint (g_value_array_get_nth (values, 1));
          priv->type = g_value_get_uint (g_value_array_get_nth (values, 2));
          priv->service = g_value_dup_string (g_value_array_get_nth (values, 3));
          priv->parameters = g_value_dup_boxed (g_value_array_get_nth (values, 4));
          priv->state = g_value_get_uint (g_value_array_get_nth (values, 5));
          priv->initiator_contact = empathy_contact_factory_get_from_handle (
              priv->factory, account, priv->initiator);

          g_object_unref (connection);
          g_object_unref (mc);
          g_object_unref (account);
        }

      g_value_array_free (values);
    }
  g_ptr_array_free (tubes, TRUE);

  return self;
}

static void
tp_tube_finalize (GObject *object)
{
  EmpathyTpTubePriv *priv = GET_PRIV (object);

  empathy_debug (DEBUG_DOMAIN, "Finalizing: %p", object);

  if (priv->channel)
    {
      g_signal_handlers_disconnect_by_func (priv->channel,
          tp_tube_invalidated_cb, object);
      tp_cli_channel_type_tubes_call_close_tube (priv->channel, -1, priv->id,
          tp_tube_async_cb, "closing tube", NULL, NULL);
      g_object_unref (priv->channel);
    }
  if (priv->initiator_contact)
      g_object_unref (priv->initiator_contact);
  if (priv->factory)
      g_object_unref (priv->factory);

  g_free (priv->service);
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

  g_object_class_install_property (object_class, PROP_ID,
      g_param_spec_uint ("id", "id", "id", 0, G_MAXUINT, 0,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
        G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class, PROP_INITIATOR,
      g_param_spec_uint ("initiator", "initiator", "initiator",
        0, G_MAXUINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_NAME |
        G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class, PROP_TYPE,
      g_param_spec_uint ("type", "type", "type", 0, G_MAXUINT, 0,
        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class, PROP_SERVICE,
      g_param_spec_string ("service", "service", "service", NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class, PROP_PARAMETERS,
      g_param_spec_boxed ("parameters", "parameters", "parameters",
      G_TYPE_HASH_TABLE, G_PARAM_READABLE | G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class, PROP_STATE,
      g_param_spec_uint ("state", "state", "state", 0, G_MAXUINT, 0,
        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
        G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class, PROP_INITIATOR_CONTACT,
     g_param_spec_object ("initiator-contact", "initiator contact",
     "initiator contact", EMPATHY_TYPE_CONTACT, G_PARAM_READABLE |
     G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

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
  EmpathyTpTubePriv *priv = GET_PRIV (tube);

  priv->factory = empathy_contact_factory_new ();
}

EmpathyTpTube *
empathy_tp_tube_new (TpChannel *channel, guint tube_id)
{
  g_return_val_if_fail (TP_IS_CHANNEL (channel), NULL);

  return g_object_new (EMPATHY_TYPE_TP_TUBE,
      "channel", channel, "id", tube_id, NULL);
}

EmpathyTpTube *
empathy_tp_tube_new_stream_tube (EmpathyContact *contact,
                                 TpSocketAddressType type,
                                 const gchar *hostname,
                                 guint port,
                                 const gchar *service)
{
  MissionControl *mc;
  McAccount *account;
  TpConnection *connection;
  TpChannel *channel;
  gchar *object_path;
  guint id;
  GHashTable *params;
  GValue *address;
  GValue *control_param;
  EmpathyTpTube *tube = NULL;
  GError *error = NULL;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);
  g_return_val_if_fail (hostname != NULL, NULL);
  g_return_val_if_fail (service != NULL, NULL);

  mc = empathy_mission_control_new ();
  account = empathy_contact_get_account (contact);
  connection = mission_control_get_tpconnection (mc, account, NULL);
  g_object_unref (mc);

  if (!tp_cli_connection_run_request_channel (connection, -1,
      TP_IFACE_CHANNEL_TYPE_TUBES, TP_HANDLE_TYPE_CONTACT,
      empathy_contact_get_handle (contact), FALSE, &object_path, &error, NULL))
    {
      empathy_debug (DEBUG_DOMAIN, "Error requesting channel: %s", error->message);
      g_clear_error (&error);
      g_object_unref (connection);
      return NULL;
    }

  empathy_debug (DEBUG_DOMAIN, "Offering a new stream tube");

  channel = tp_channel_new (connection, object_path,
      TP_IFACE_CHANNEL_TYPE_TUBES, TP_HANDLE_TYPE_CONTACT,
      empathy_contact_get_handle (contact), NULL);

  #define ADDRESS_TYPE dbus_g_type_get_struct ("GValueArray",\
      G_TYPE_STRING, G_TYPE_UINT, G_TYPE_INVALID)
  params = g_hash_table_new (g_str_hash, g_str_equal);
  address = tp_g_value_slice_new (ADDRESS_TYPE);
  g_value_take_boxed (address, dbus_g_type_specialized_construct (ADDRESS_TYPE));
  dbus_g_type_struct_set (address, 0, hostname, 1, port, G_MAXUINT);
  control_param = tp_g_value_slice_new (G_TYPE_STRING);

  if (!tp_cli_channel_type_tubes_run_offer_stream_tube (channel, -1,
        service, params, type, address,
        TP_SOCKET_ACCESS_CONTROL_LOCALHOST, control_param, &id, &error, NULL))
    {
      empathy_debug (DEBUG_DOMAIN, "Couldn't offer tube: %s", error->message);
      g_clear_error (&error);
      goto OUT;
    }

  empathy_debug (DEBUG_DOMAIN, "Stream tube id=%d offered", id);

  tube = empathy_tp_tube_new (channel, id);

OUT:
  g_object_unref (channel);
  g_free (object_path);
  g_hash_table_destroy (params);
  tp_g_value_slice_free (address);
  tp_g_value_slice_free (control_param);
  g_object_unref (connection);

  return tube;
}

static void
tp_tube_accept_stream_cb (TpChannel *proxy,
                          const GValue *address,
                          const GError *error,
                          gpointer user_data,
                          GObject *weak_object)
{
  if (error)
      empathy_debug (DEBUG_DOMAIN, "Error accepting tube: %s", error->message);
}

void
empathy_tp_tube_accept_stream_tube (EmpathyTpTube *tube,
                                    TpSocketAddressType type)
{
  EmpathyTpTubePriv *priv = GET_PRIV (tube);
  GValue *control_param;

  g_return_if_fail (EMPATHY_IS_TP_TUBE (tube));

  empathy_debug (DEBUG_DOMAIN, "Accepting stream tube - id: %d", priv->id);

  control_param = tp_g_value_slice_new (G_TYPE_STRING);
  tp_cli_channel_type_tubes_call_accept_stream_tube (priv->channel, -1, priv->id,
      type, TP_SOCKET_ACCESS_CONTROL_LOCALHOST, control_param,
      tp_tube_accept_stream_cb, NULL, NULL, G_OBJECT (tube));

  tp_g_value_slice_free (control_param);
}

void
empathy_tp_tube_get_socket (EmpathyTpTube *tube,
                            gchar **hostname,
                            guint *port)
{
  EmpathyTpTubePriv *priv = GET_PRIV (tube);
  GValue *address;
  guint address_type;
  GError *error = NULL;

  g_return_if_fail (EMPATHY_IS_TP_TUBE (tube));

  empathy_debug (DEBUG_DOMAIN, "Getting stream tube socket address");

  address = g_slice_new0 (GValue);
  if (!tp_cli_channel_type_tubes_run_get_stream_tube_socket_address (priv->channel,
      -1, priv->id, &address_type, &address, &error, NULL))
    {
      empathy_debug (DEBUG_DOMAIN, "Couldn't get socket address: %s",
          error->message);
      g_clear_error (&error);
      return;
    }

  switch (address_type)
    {
    case TP_SOCKET_ADDRESS_TYPE_UNIX:
    case TP_SOCKET_ADDRESS_TYPE_ABSTRACT_UNIX:
        dbus_g_type_struct_get (address, 0, hostname, G_MAXUINT);
        break;
    case TP_SOCKET_ADDRESS_TYPE_IPV4:
    case TP_SOCKET_ADDRESS_TYPE_IPV6:
        dbus_g_type_struct_get (address, 0, hostname, 1, port, G_MAXUINT);    
        break;
    }

  tp_g_value_slice_free (address);
}

