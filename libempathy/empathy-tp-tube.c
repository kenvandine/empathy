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

#include "empathy-contact.h"
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
      case PROP_INITIATOR:
        priv->initiator = g_value_get_uint (value);
        break;
      case PROP_TYPE:
        priv->type = g_value_get_uint (value);
        break;
      case PROP_SERVICE:
        priv->service = g_value_dup_string (value);
        break;
      case PROP_PARAMETERS:
        priv->parameters = g_value_dup_boxed (value);
        break;
      case PROP_STATE:
        priv->state = g_value_get_uint (value);
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
  TpConnection *connection;
  MissionControl *mc;
  McAccount *account;

  self = G_OBJECT_CLASS (empathy_tp_tube_parent_class)->constructor (
      type, n_props, props);
  priv = GET_PRIV (self);

  g_object_get (priv->channel, "connection", &connection, NULL);
  mc = empathy_mission_control_new ();
  account = mission_control_get_account_for_tpconnection (mc, connection, NULL);

  priv->factory = empathy_contact_factory_new ();
  priv->initiator_contact = empathy_contact_factory_get_from_handle (priv->factory,
      account, priv->initiator);
  g_object_ref (priv->initiator_contact);

  g_signal_connect (priv->channel, "invalidated",
      G_CALLBACK (tp_tube_invalidated_cb), self);

  tp_cli_channel_type_tubes_connect_to_tube_closed (priv->channel,
      tp_tube_closed_cb, NULL, NULL, self, NULL);
  tp_cli_channel_type_tubes_connect_to_tube_state_changed (priv->channel,
      tp_tube_state_changed_cb, NULL, NULL, self, NULL);

  g_object_unref (connection);
  g_object_unref (mc);
  g_object_unref (account);

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
        0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class, PROP_TYPE,
      g_param_spec_uint ("type", "type", "type", 0, G_MAXUINT, 0,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
        G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class, PROP_SERVICE,
      g_param_spec_string ("service", "service", "service", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class, PROP_PARAMETERS,
      g_param_spec_boxed ("parameters", "parameters", "parameters",
      G_TYPE_HASH_TABLE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class, PROP_STATE,
      g_param_spec_uint ("state", "state", "state", 0, G_MAXUINT, 0,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
        G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class, PROP_INITIATOR_CONTACT,
     g_param_spec_object ("initiator-contact", "initiator contact",
     "initiator contact", EMPATHY_TYPE_CONTACT, G_PARAM_READABLE |
     G_PARAM_STATIC_NAME |  G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  signals[DESTROY] = g_signal_new ("destroy",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (EmpathyTpTubePriv));
}

static void
empathy_tp_tube_init (EmpathyTpTube *tp_tubes)
{
}

EmpathyTpTube *
empathy_tp_tube_new (TpChannel *channel, guint tube_id)
{
  EmpathyTpTube *tube = NULL;
  GPtrArray *tubes;
  guint i;
  GError *error = NULL;

  g_return_val_if_fail (TP_IS_CHANNEL (channel), NULL);

  if (!tp_cli_channel_type_tubes_run_list_tubes (channel, -1, &tubes,
      &error, NULL))
    {
      empathy_debug (DEBUG_DOMAIN, "Couldn't list tubes: %s",
          error->message);
      g_clear_error (&error);
      return NULL;
    }

  for (i = 0; i < tubes->len; i++)
    {
      GValueArray *values;
      guint id;

      values = g_ptr_array_index (tubes, i);
      id = g_value_get_uint (g_value_array_get_nth (values, 0));

      if (id != tube_id)
        {
          g_value_array_free (values);
          continue;
        }

      tube = g_object_new (EMPATHY_TYPE_TP_TUBE,
          "channel", channel,
          "id", id,
          "initiator", g_value_get_uint (g_value_array_get_nth (values, 1)),
          "type", g_value_get_uint (g_value_array_get_nth (values, 2)),
          "service", g_value_get_string (g_value_array_get_nth (values, 3)),
          "parameters", g_value_get_boxed (g_value_array_get_nth (values, 4)),
          "state", g_value_get_uint (g_value_array_get_nth (values, 5)),
          NULL);

      g_value_array_free (values);
    }
  g_ptr_array_free (tubes, TRUE);

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

static void
tp_tube_accept_stream_tube (EmpathyTpTube *tube,
                            TpSocketAddressType address_type,
                            TpSocketAccessControl access_type,
                            GValue *control_param)
{
  EmpathyTpTubePriv *priv = GET_PRIV (tube);

  empathy_debug (DEBUG_DOMAIN, "Accepting stream tube - id: %d", priv->id);

  tp_cli_channel_type_tubes_call_accept_stream_tube (priv->channel, -1, priv->id,
      address_type, access_type, control_param,
      tp_tube_accept_stream_cb, NULL, NULL, G_OBJECT (tube));
}

void
empathy_tp_tube_accept_unix_stream_tube (EmpathyTpTube *tube)
{
  GValue control_param = {0, };

  g_return_if_fail (EMPATHY_IS_TP_TUBE (tube));

  g_value_init (&control_param, G_TYPE_STRING);
  tp_tube_accept_stream_tube (tube, TP_SOCKET_ADDRESS_TYPE_UNIX,
      TP_SOCKET_ACCESS_CONTROL_LOCALHOST, &control_param);

  g_value_reset (&control_param);
}

void
empathy_tp_tube_accept_ipv4_stream_tube (EmpathyTpTube *tube)
{
  GValue control_param = {0, };

  g_return_if_fail (EMPATHY_IS_TP_TUBE (tube));

  g_value_init (&control_param, G_TYPE_STRING);
  tp_tube_accept_stream_tube (tube, TP_SOCKET_ADDRESS_TYPE_IPV4,
      TP_SOCKET_ACCESS_CONTROL_LOCALHOST, &control_param);

  g_value_reset (&control_param);
}

gchar *
empathy_tp_tube_get_unix_socket (EmpathyTpTube *tube)
{
  EmpathyTpTubePriv *priv = GET_PRIV (tube);
  GValue *address = g_new0 (GValue, 1);;
  guint address_type;
  gchar *address_name = NULL;
  GError *error = NULL;

  g_return_val_if_fail (EMPATHY_IS_TP_TUBE (tube), NULL);

  empathy_debug (DEBUG_DOMAIN, "Getting stream tube socket address");

  /* FIXME: We shouldn't use _run_ here because the user may not expect to
   * reenter the mainloop.
   * FIXME: Do we have to give an initialised GValue for address? Are we
   * freeing it correctly? */
  if (!tp_cli_channel_type_tubes_run_get_stream_tube_socket_address (priv->channel,
      -1, priv->id, &address_type, &address, &error, NULL))
    {
      empathy_debug (DEBUG_DOMAIN, "Couldn't get socket address: %s",
          error->message);
      g_clear_error (&error);
      return NULL;
    }

  dbus_g_type_struct_get (address, 0, &address_name, G_MAXUINT);
  g_free (address);

  empathy_debug (DEBUG_DOMAIN, "UNIX Socket - %s", address_name);

  return address_name;
}

void
empathy_tp_tube_get_ipv4_socket (EmpathyTpTube *tube,
                                 gchar **hostname,
                                 guint *port)
{
  EmpathyTpTubePriv *priv = GET_PRIV (tube);
  GValue *address = g_new0 (GValue, 1);
  guint address_type;
  GError *error = NULL;

  g_return_if_fail (EMPATHY_IS_TP_TUBE (tube));

  empathy_debug (DEBUG_DOMAIN, "Getting stream tube socket address");

  if (!tp_cli_channel_type_tubes_run_get_stream_tube_socket_address (priv->channel,
      -1, priv->id, &address_type, &address, &error, NULL))
    {
      empathy_debug (DEBUG_DOMAIN, "Couldn't get socket address: %s",
          error->message);
      g_clear_error (&error);
      return;
    }

  dbus_g_type_struct_get (address, 0, hostname, 1, port, G_MAXUINT);

  g_free (address);
}

