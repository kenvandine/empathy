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


#include <telepathy-glib/connection.h>
#include <telepathy-glib/channel.h>

#include "empathy-contact.h"
#include "empathy-contact-factory.h"
#include "empathy-debug.h"
#include "empathy-enum-types.h"
#include "empathy-tubes.h"
#include "empathy-tube.h"
#include "empathy-utils.h"

#define DEBUG_DOMAIN "EmpathyTube"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EMPATHY_TYPE_TUBE, \
    EmpathyTubePriv))

typedef struct _EmpathyTubePriv EmpathyTubePriv;

struct _EmpathyTubePriv
{
  EmpathyTubes *tubes;
  guint id;
  guint initiator;
  guint type;
  const gchar *service;
  GHashTable *parameters;
  guint state;
  EmpathyContact *initiator_contact;
  gboolean closed;
};

enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_TUBES,
  PROP_ID,
  PROP_INITIATOR,
  PROP_TYPE,
  PROP_SERVICE,
  PROP_PARAMETERS,
  PROP_STATE,
  PROP_INITIATOR_CONTACT,
  PROP_CLOSED
};

G_DEFINE_TYPE (EmpathyTube, empathy_tube, G_TYPE_OBJECT)


static void
empathy_tube_closed_cb (EmpathyTubes *tubes,
                        guint id,
                        gpointer data)
{
  EmpathyTube *tube = EMPATHY_TUBE (data);
  EmpathyTubePriv *priv = GET_PRIV (tube);

  if (id != priv->id)
    return;

  empathy_debug (DEBUG_DOMAIN, "Tube closed");

  priv->closed = TRUE;
}

static void
empathy_tube_state_changed_cb (EmpathyTubes *tubes,
                               guint id,
                               guint state,
                               gpointer data)
{
  EmpathyTube *tube = EMPATHY_TUBE (data);
  EmpathyTubePriv *priv = GET_PRIV (tube);

  if (id != priv->id)
    return;

  empathy_debug (DEBUG_DOMAIN, "Tube state changed");
  priv->state = state;
}


static void
empathy_tube_stream_tube_new_connection_cb (EmpathyTubes *tubes,
                                            guint id,
                                            guint handle,
                                            gpointer data)
{
  EmpathyTube *tube = EMPATHY_TUBE (data);
  EmpathyTubePriv *priv = GET_PRIV (tube);

  if (id != priv->id)
    return;

  empathy_debug (DEBUG_DOMAIN, "Stream tube new connection");
}


static void
empathy_tube_set_property (GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
  EmpathyTubePriv *priv = GET_PRIV (object);

  switch (prop_id)
    {
      case PROP_TUBES:
        priv->tubes = g_value_dup_object (value);
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
      case PROP_CLOSED:
        priv->closed = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
  }
}


static void
empathy_tube_get_property (GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
  EmpathyTubePriv *priv = GET_PRIV (object);

  switch (prop_id)
    {
      case PROP_TUBES:
        g_value_set_object (value, priv->tubes);
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
      case PROP_CLOSED:
        g_value_set_boolean (value, priv->closed);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
  }
}


static GObject *
empathy_tube_constructor (GType type,
                          guint n_props,
                          GObjectConstructParam *props)
{
  GObject *self = G_OBJECT_CLASS (empathy_tube_parent_class)->constructor
      (type, n_props, props);
  EmpathyTubePriv *priv = GET_PRIV (self);
  TpChannel *channel;
  TpConnection *connection;
  MissionControl *mc = empathy_mission_control_new ();
  EmpathyContactFactory *factory = empathy_contact_factory_new ();
  McAccount *account;

  g_object_get (G_OBJECT (priv->tubes), "channel", &channel, NULL);
  g_object_get (G_OBJECT (channel), "connection", &connection, NULL);

  account = mission_control_get_account_for_tpconnection (mc, connection, NULL);

  g_assert (connection != NULL);
  g_assert (priv->initiator != 0);
  priv->initiator_contact =
      g_object_ref (empathy_contact_factory_get_from_handle (factory,
      account, priv->initiator));
  g_assert (priv->initiator_contact != NULL);

  g_signal_connect (priv->tubes, "tube-closed",
      G_CALLBACK (empathy_tube_closed_cb), (gpointer) self);
  g_signal_connect (priv->tubes, "tube-state-changed",
      G_CALLBACK (empathy_tube_state_changed_cb), (gpointer) self);
  g_signal_connect (priv->tubes, "stream-tube-new-connection",
      G_CALLBACK (empathy_tube_stream_tube_new_connection_cb),
      (gpointer) self);

  priv->closed = FALSE;

  g_object_unref (connection);
  g_object_unref (channel);
  g_object_unref (mc);

  return self;
}


static void
empathy_tube_dispose (GObject *object)
{
  EmpathyTube *tube = EMPATHY_TUBE (object);
  EmpathyTubePriv *priv = GET_PRIV (tube);

  empathy_debug (DEBUG_DOMAIN, "Disposing: %p", object);

  if (priv->tubes)
    {
      g_signal_handlers_disconnect_by_func (priv->tubes,
          empathy_tube_closed_cb, object);
      g_signal_handlers_disconnect_by_func (priv->tubes,
          empathy_tube_state_changed_cb, object);
      g_signal_handlers_disconnect_by_func (priv->tubes,
          empathy_tube_stream_tube_new_connection_cb, object);
      g_object_unref (priv->tubes);
      priv->tubes = NULL;
    }

  g_object_unref (priv->initiator_contact);

  (G_OBJECT_CLASS (empathy_tube_parent_class)->dispose) (object);
}


static void
empathy_tube_finalize (GObject *object)
{
  empathy_debug (DEBUG_DOMAIN, "Finalizing: %p", object);

  G_OBJECT_CLASS (empathy_tube_parent_class)->finalize (object);
}


static void
empathy_tube_class_init (EmpathyTubeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = empathy_tube_constructor;
  object_class->dispose = empathy_tube_dispose;
  object_class->finalize = empathy_tube_finalize;
  object_class->set_property = empathy_tube_set_property;
  object_class->get_property = empathy_tube_get_property;

  g_object_class_install_property (object_class, PROP_TUBES,
      g_param_spec_object ("tubes", "tubes", "tubes", EMPATHY_TYPE_TUBES,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

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

  g_object_class_install_property (object_class, PROP_CLOSED,
      g_param_spec_boolean ("closed", "closed", "closed", FALSE,
      G_PARAM_READABLE | G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (klass, sizeof (EmpathyTubePriv));
}


static void
empathy_tube_init (EmpathyTube *tubes)
{
}


void
empathy_tube_close (EmpathyTube *tube)
{
  EmpathyTubePriv *priv = GET_PRIV (tube);
  TpChannel *channel;
  GError *error = NULL;

  empathy_debug (DEBUG_DOMAIN, "Closing tube - id: %d", priv->id);

  g_object_get (G_OBJECT (priv->tubes), "channel", &channel, NULL);

  if (!tp_cli_channel_type_tubes_run_close_tube (channel, -1, priv->id,
      &error, NULL))
    {
      empathy_debug (DEBUG_DOMAIN, "Couldn't close tube: %s", error->message);
      g_clear_error (&error);
    }

  g_object_unref (channel);
}


static void
empathy_tube_accept_stream_tube (EmpathyTube *tube,
                                 TpSocketAddressType address_type,
                                 TpSocketAccessControl access_type,
                                 GValue *control_param)
{
  EmpathyTubePriv *priv = GET_PRIV (tube);
  GValue *socket = g_new0 (GValue, 1);
  TpChannel *channel;
  GError *error = NULL;

  empathy_debug (DEBUG_DOMAIN, "Accepting stream tube - id: %d", priv->id);

  g_object_get (G_OBJECT (priv->tubes), "channel", &channel, NULL);

  if (!tp_cli_channel_type_tubes_run_accept_stream_tube (channel, -1, priv->id,
      address_type, access_type, control_param, &socket, &error, NULL))
    {
      empathy_debug (DEBUG_DOMAIN, "Error accepting tube: %s", error->message);
      g_clear_error (&error);
    }

  g_object_unref (channel);
  g_free (socket);
}


void
empathy_tube_accept_stream_tube_unix (EmpathyTube *tube)
{
  GValue *control_param = g_new0 (GValue, 1);

  empathy_debug (DEBUG_DOMAIN, "Accepting stream tube - UNIX, localhost");

  g_value_init (control_param, G_TYPE_STRING);
  empathy_tube_accept_stream_tube (tube, TP_SOCKET_ADDRESS_TYPE_UNIX,
      TP_SOCKET_ACCESS_CONTROL_LOCALHOST, control_param);

  g_free (control_param);
}


void
empathy_tube_accept_stream_tube_ipv4 (EmpathyTube *tube)
{
  GValue *control_param = g_new0 (GValue, 1);

  empathy_debug (DEBUG_DOMAIN, "Accepting stream tube - IPv4, localhost");

  g_value_init (control_param, G_TYPE_STRING);
  empathy_tube_accept_stream_tube (tube, TP_SOCKET_ADDRESS_TYPE_IPV4,
      TP_SOCKET_ACCESS_CONTROL_LOCALHOST, control_param);

  g_free (control_param);
}


gchar *
empathy_tube_get_stream_tube_socket_unix (EmpathyTube *tube)
{
  EmpathyTubePriv *priv = GET_PRIV (tube);
  GValue *address = g_new0 (GValue, 1);
  guint address_type;
  gchar *address_name = NULL;
  TpChannel *channel;
  GError *error = NULL;

  empathy_debug (DEBUG_DOMAIN, "Getting stream tube socket address");

  g_object_get (G_OBJECT (priv->tubes), "channel", &channel, NULL);

  if (!tp_cli_channel_type_tubes_run_get_stream_tube_socket_address (channel,
      -1, priv->id, &address_type, &address, &error, NULL))
    {
      empathy_debug (DEBUG_DOMAIN, "Couldn't get socket address: %s",
          error->message);
      g_clear_error (&error);
      return NULL;
    }

  dbus_g_type_struct_get (address, 0, &address_name, G_MAXUINT);

  empathy_debug (DEBUG_DOMAIN, "UNIX Socket - %s", address_name);

  g_object_unref (channel);
  g_free (address);
  return address_name;
}


void
empathy_tube_get_stream_tube_socket_ipv4 (EmpathyTube *tube,
                                          gchar **hostname,
                                          guint *port)
{
  EmpathyTubePriv *priv = GET_PRIV (tube);
  GValue *address = g_new0 (GValue, 1);
  guint address_type;
  TpChannel *channel;
  GError *error = NULL;

  empathy_debug (DEBUG_DOMAIN, "Getting stream tube socket address");

  g_object_get (G_OBJECT (priv->tubes), "channel", &channel, NULL);

  if (!tp_cli_channel_type_tubes_run_get_stream_tube_socket_address (channel,
      -1, priv->id, &address_type, &address, &error, NULL))
    {
      empathy_debug (DEBUG_DOMAIN, "Couldn't get socket address: %s",
          error->message);
      g_clear_error (&error);
      return;
    }

  dbus_g_type_struct_get (address, 0, hostname, 1, port, G_MAXUINT);

  g_object_unref (channel);
  g_free (address);
}
