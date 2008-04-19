/*
 *  Copyright (C) 2008 Collabora Ltd.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Authors: Elliot Fairweather <elliot.fairweather@collabora.co.uk>
 *           Guillaume Desmottes <guillaume.desmottes@collabora.co.uk>
 */

#include <string.h>
#include <dbus/dbus-glib.h>

#include <telepathy-glib/channel.h>

#include "empathy-contact.h"
#include "empathy-debug.h"
#include "empathy-marshal.h"
#include "empathy-tp-contact-factory.h"
#include "empathy-tube.h"
#include "empathy-tubes.h"
#include "empathy-utils.h"

#define DEBUG_DOMAIN "EmpathyTubes"

#define GET_PRIV(object) (G_TYPE_INSTANCE_GET_PRIVATE \
    ((object), EMPATHY_TYPE_TUBES, EmpathyTubesPriv))

typedef struct _EmpathyTubesPriv EmpathyTubesPriv;

struct _EmpathyTubesPriv
{
  TpChannel *channel;
};

enum
{
  NEW_TUBE,
  TUBE_CLOSED,
  TUBE_STATE_CHANGED,
  STREAM_TUBE_NEW_CONNECTION,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_CHANNEL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyTubes, empathy_tubes, G_TYPE_OBJECT)


static void
empathy_tubes_new_tube_cb (TpChannel *channel,
                           guint id,
                           guint initiator,
                           guint type,
                           const gchar *service,
                           GHashTable *params,
                           guint state,
                           gpointer data,
                           GObject *tubes)
{
  EmpathyTube *tube;

  empathy_debug (DEBUG_DOMAIN,
      "New tube - id: %d, initiator: %d, type: %d, service: %s, state: %d",
      id, initiator, type, service, state);
  tube = g_object_new (EMPATHY_TYPE_TUBE, "tubes", tubes, "id", id,
      "initiator", initiator, "type", type, "service", service,
      "parameters", params, "state", state, NULL);
  g_signal_emit (tubes, signals[NEW_TUBE], 0, tube);
}


static void
empathy_tubes_tube_closed_cb (TpChannel *channel,
                              guint id,
                              gpointer data,
                              GObject *tubes)
{
  empathy_debug (DEBUG_DOMAIN, "Tube closed - id: %d", id);
  g_signal_emit (tubes, signals[TUBE_CLOSED], 0, id);
}


static void
empathy_tubes_stream_tube_new_connection_cb (TpChannel *channel,
                                             guint id,
                                             guint handle,
                                             gpointer data,
                                             GObject *tubes)
{
  empathy_debug (DEBUG_DOMAIN,
      "Stream tube new connection - id: %d, handle: %d", id, handle);
  g_signal_emit (tubes, signals[STREAM_TUBE_NEW_CONNECTION], 0,
      id, handle);
}


static void
empathy_tubes_tube_state_changed_cb (TpChannel *channel,
                                     guint id,
                                     guint state,
                                     gpointer data,
                                     GObject *tubes)
{
  empathy_debug (DEBUG_DOMAIN, "Tube state changed - id: %d, state: %d",
      id, state);
  g_signal_emit (tubes, signals[TUBE_STATE_CHANGED], 0, id, state);
}


static void
empathy_tubes_channel_invalidated_cb (TpChannel *channel,
                                      guint domain,
                                      gint code,
                                      gchar *message,
                                      gpointer data)
{
  EmpathyTubes *tubes = EMPATHY_TUBES (data);
  EmpathyTubesPriv *priv = GET_PRIV (tubes);

  empathy_debug (DEBUG_DOMAIN, "Channel invalidated");

  g_signal_handlers_disconnect_by_func (priv->channel,
      empathy_tubes_channel_invalidated_cb, tubes);

  // disconnect tubes interface signals?
}


static void
empathy_tubes_dispose (GObject *object)
{
  EmpathyTubes *tubes = EMPATHY_TUBES (object);
  EmpathyTubesPriv *priv = GET_PRIV (tubes);

  empathy_debug (DEBUG_DOMAIN, "Disposing: %p", object);

  if (priv->channel)
    {
      g_signal_handlers_disconnect_by_func (priv->channel,
          empathy_tubes_channel_invalidated_cb, object);
      g_object_unref (priv->channel);
      priv->channel = NULL;
    }

  (G_OBJECT_CLASS (empathy_tubes_parent_class)->dispose) (object);
}


static void
empathy_tubes_set_property (GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  EmpathyTubes *tubes = EMPATHY_TUBES (object);
  EmpathyTubesPriv *priv = GET_PRIV (tubes);

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
empathy_tubes_get_property (GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  EmpathyTubes *tubes = EMPATHY_TUBES (object);
  EmpathyTubesPriv *priv = GET_PRIV (tubes);

  switch (prop_id)
    {
      case PROP_CHANNEL:
        g_value_set_object (value, priv->channel);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}


static GObject *
empathy_tubes_constructor (GType type,
                           guint n_construct_params,
                           GObjectConstructParam *construct_params)
{
  GObject *object = G_OBJECT_CLASS (empathy_tubes_parent_class)->constructor
      (type, n_construct_params, construct_params);
  EmpathyTubes *tubes = EMPATHY_TUBES (object);
  EmpathyTubesPriv *priv = GET_PRIV (tubes);

  g_signal_connect (priv->channel, "invalidated",
      G_CALLBACK (empathy_tubes_channel_invalidated_cb), tubes);

  tp_cli_channel_type_tubes_connect_to_new_tube (priv->channel,
      empathy_tubes_new_tube_cb, NULL, NULL, G_OBJECT (tubes), NULL);
  tp_cli_channel_type_tubes_connect_to_tube_closed (priv->channel,
      empathy_tubes_tube_closed_cb, NULL, NULL, G_OBJECT (tubes), NULL);
  tp_cli_channel_type_tubes_connect_to_tube_state_changed (priv->channel,
      empathy_tubes_tube_state_changed_cb, NULL, NULL, G_OBJECT (tubes), NULL);
  tp_cli_channel_type_tubes_connect_to_stream_tube_new_connection
      (priv->channel, empathy_tubes_stream_tube_new_connection_cb,
       NULL, NULL, G_OBJECT (tubes), NULL);

  object = G_OBJECT (tubes);
  return object;
}


static void empathy_tubes_class_init (EmpathyTubesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = empathy_tubes_constructor;
  object_class->dispose = empathy_tubes_dispose;
  object_class->set_property = empathy_tubes_set_property;
  object_class->get_property = empathy_tubes_get_property;

  g_type_class_add_private (klass, sizeof (EmpathyTubesPriv));

  signals[NEW_TUBE] =
      g_signal_new ("new-tube", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, EMPATHY_TYPE_TUBE);
  signals[TUBE_CLOSED] =
      g_signal_new ("tube-closed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__UINT,
      G_TYPE_NONE, 1, G_TYPE_UINT);
  signals[TUBE_STATE_CHANGED] =
      g_signal_new ("tube-state-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, _empathy_marshal_VOID__UINT_UINT,
      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
  signals[STREAM_TUBE_NEW_CONNECTION] =
      g_signal_new ("stream-tube-new-connection", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, _empathy_marshal_VOID__UINT_UINT,
      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);

  g_object_class_install_property (object_class, PROP_CHANNEL,
      g_param_spec_object ("channel", "channel", "channel",
      TP_TYPE_CHANNEL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}


static void
empathy_tubes_init (EmpathyTubes *empathy_tubes)
{
}


EmpathyTubes *
empathy_tubes_new (TpChannel *channel)
{
  return g_object_new (empathy_tubes_get_type (), "channel", channel, NULL);
}


guint
empathy_tubes_offer_stream_tube_ipv4 (EmpathyTubes *tubes,
                                      gchar *host,
                                      guint port,
                                      gchar *service)
{
  EmpathyTubesPriv *priv = GET_PRIV (tubes);
  GError *error = NULL;
  GHashTable *params;
  GValue *address;
  GValue *control_param;
  guint id;

  empathy_debug (DEBUG_DOMAIN, "Offering a new stream tube");

  params = g_hash_table_new (g_str_hash, g_str_equal);

  address = g_new0 (GValue, 1);
  g_value_init (address,
      dbus_g_type_get_struct ("GValueArray", G_TYPE_STRING, G_TYPE_UINT,
      G_TYPE_INVALID));
  g_value_take_boxed (address,
      dbus_g_type_specialized_construct (dbus_g_type_get_struct ("GValueArray",
        G_TYPE_STRING, G_TYPE_UINT, G_TYPE_INVALID)));
  dbus_g_type_struct_set (address, 0, host, 1, port, G_MAXUINT);

  /* localhost access control, variant is ignored */
  control_param = g_new0 (GValue, 1);
  g_value_init (control_param, G_TYPE_STRING);

  if (!tp_cli_channel_type_tubes_run_offer_stream_tube (priv->channel, -1,
        service, params, TP_SOCKET_ADDRESS_TYPE_IPV4, address,
        TP_SOCKET_ACCESS_CONTROL_LOCALHOST, control_param, &id, &error, NULL))
    {
      empathy_debug (DEBUG_DOMAIN, "Couldn't offer tube: %s", error->message);
      g_clear_error (&error);
      return 0;
    }

  return id;
}


GSList *
empathy_tubes_list_tubes (EmpathyTubes *tubes)
{
  EmpathyTubesPriv *priv = GET_PRIV (tubes);
  GPtrArray *tube_infos;
  GError *error;
  GSList *list = NULL;
  guint i;

  if (!tp_cli_channel_type_tubes_run_list_tubes (priv->channel, -1,
      &tube_infos, &error, NULL))
    {
      empathy_debug (DEBUG_DOMAIN, "Couldn't list tubes: %s", error->message);
      g_clear_error (&error);
      return NULL;
    }

  for (i = 0; i < tube_infos->len; i++)
    {
      GValueArray *values;
      guint id;
      guint initiator;
      guint type;
      const gchar *service;
      GHashTable *params;
      guint state;

      values = g_ptr_array_index (tube_infos, i);
      id = g_value_get_uint (g_value_array_get_nth (values, 0));
      initiator = g_value_get_uint (g_value_array_get_nth (values, 1));
      type = g_value_get_uint (g_value_array_get_nth (values, 2));
      service = g_value_get_string (g_value_array_get_nth (values, 3));
      params = g_value_get_boxed (g_value_array_get_nth (values, 4));
      state = g_value_get_uint (g_value_array_get_nth (values, 5));

      empathy_debug (DEBUG_DOMAIN,
          "Listing tubes - %d, id: %d, initiator: %d, service: %s, "
          "state: %d, type: %d",
          i + 1, id, initiator, service, state, type);

      list = g_slist_prepend (list,
          g_object_new (EMPATHY_TYPE_TUBE, "tubes", tubes, "id", id,
          "initiator", initiator, "type", type, "service", service,
          "parameters", params, "state", state, NULL));

      g_value_array_free (values);
    }

  g_ptr_array_free (tube_infos, TRUE);
  return list;
}


EmpathyTube *
empathy_tubes_get_tube (EmpathyTubes *tubes,
                        guint tube_id)
{
  // maybe this function should call list tubes directly?
  EmpathyTube *tube = NULL;
  GSList *tube_list, *list;
  tube_list = empathy_tubes_list_tubes (tubes);

  for (list = tube_list; list != NULL; list = g_slist_next (list))
    {
      EmpathyTube *t = EMPATHY_TUBE (list->data);
      guint id;
      g_object_get (G_OBJECT (t), "id", &id, NULL);

      if (id == tube_id)
        tube = g_object_ref (t);

      g_object_unref (t);
    }

  g_slist_free (tube_list);
  return tube;
}


void
empathy_tubes_close (EmpathyTubes *tubes)
{
  EmpathyTubesPriv *priv = GET_PRIV (tubes);
  GError *error = NULL;

  empathy_debug (DEBUG_DOMAIN, "Closing channel");

  if (!tp_cli_channel_run_close (priv->channel, -1, &error, NULL))
    {
      empathy_debug (DEBUG_DOMAIN, "Error closing channel: %s",
          error ? error->message : "No error given");
      g_clear_error (&error);
    }
}
