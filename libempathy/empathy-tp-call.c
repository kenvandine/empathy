/*
 *  Copyright (C) 2007 Elliot Fairweather
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
 */

#include <string.h>
#include <dbus/dbus-glib.h>

#include <libtelepathy/tp-chan-type-streamed-media-gen.h>
#include <libtelepathy/tp-connmgr.h>
#include <libtelepathy/tp-helpers.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/dbus.h>

#include <libmissioncontrol/mc-account.h>

#include <extensions/extensions.h>
#include <libempathy/empathy-contact-factory.h>
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-tp-group.h>
#include <libempathy/empathy-utils.h>

#include "empathy-tp-call.h"

#define DEBUG_DOMAIN "TpCall"

#define GET_PRIV(object) (G_TYPE_INSTANCE_GET_PRIVATE \
    ((object), EMPATHY_TYPE_TP_CALL, EmpathyTpCallPriv))

#define STREAM_ENGINE_BUS_NAME "org.freedesktop.Telepathy.StreamEngine"
#define STREAM_ENGINE_OBJECT_PATH "/org/freedesktop/Telepathy/StreamEngine"

typedef struct _EmpathyTpCallPriv EmpathyTpCallPriv;

struct _EmpathyTpCallPriv
{
  TpConn *connection;
  TpChan *channel;
  TpProxy *stream_engine;
  TpDBusDaemon *dbus_daemon;
  EmpathyTpGroup *group;
  EmpathyContact *contact;
  gboolean is_incoming;
  guint status;

  EmpathyTpCallStream *audio;
  EmpathyTpCallStream *video;
};

enum
{
  STATUS_CHANGED_SIGNAL,
  RECEIVING_VIDEO_SIGNAL,
  SENDING_VIDEO_SIGNAL,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_CONNECTION,
  PROP_CHANNEL,
  PROP_CONTACT,
  PROP_IS_INCOMING,
  PROP_STATUS,
  PROP_AUDIO_STREAM,
  PROP_VIDEO_STREAM
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyTpCall, empathy_tp_call, G_TYPE_OBJECT)

static void
tp_call_stream_state_changed_cb (DBusGProxy *channel,
                                 guint stream_id,
                                 guint stream_state,
                                 EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  empathy_debug (DEBUG_DOMAIN,
      "Stream state changed - stream id: %d, state state: %d",
      stream_id, stream_state);

  if (stream_id == priv->audio->id)
    {
      priv->audio->state = stream_state;
    }
  else if (stream_id == priv->video->id)
    {
      priv->video->state = stream_state;
      if (stream_state == TP_MEDIA_STREAM_STATE_CONNECTED)
      {
        if (priv->video->direction & TP_MEDIA_STREAM_DIRECTION_RECEIVE)
          {
            empathy_debug (DEBUG_DOMAIN, "RECEIVING");
            g_signal_emit_by_name (call, "receiving-video", TRUE);
          }
        if (priv->video->direction & TP_MEDIA_STREAM_DIRECTION_SEND)
          {
            empathy_debug (DEBUG_DOMAIN, "SENDING");
            g_signal_emit_by_name (call, "sending-video", TRUE);
          }
      }
    }

  g_signal_emit_by_name (call, "status-changed");
}

static void
tp_call_identify_streams (EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);
  GPtrArray *stream_infos;
  DBusGProxy *streamed_iface;
  GError *error = NULL;
  guint i;

  empathy_debug (DEBUG_DOMAIN, "Identifying audio/video streams");

  streamed_iface = tp_chan_get_interface (priv->channel,
      TELEPATHY_CHAN_IFACE_STREAMED_QUARK);

  if (!tp_chan_type_streamed_media_list_streams (streamed_iface, &stream_infos,
        &error))
    {
      empathy_debug (DEBUG_DOMAIN, "Couldn't list audio/video streams: %s",
          error->message);
      g_clear_error (&error);
      return;
    }

  for (i = 0; i < stream_infos->len; i++)
    {
      GValueArray *values;
      guint stream_id;
      guint stream_handle;
      guint stream_type;
      guint stream_state;
      guint stream_direction;

      values = g_ptr_array_index (stream_infos, i);
      stream_id = g_value_get_uint (g_value_array_get_nth (values, 0));
      stream_handle = g_value_get_uint (g_value_array_get_nth (values, 1));
      stream_type = g_value_get_uint (g_value_array_get_nth (values, 2));
      stream_state = g_value_get_uint (g_value_array_get_nth (values, 3));
      stream_direction = g_value_get_uint (g_value_array_get_nth (values, 4));

      switch (stream_type)
        {
        case TP_MEDIA_STREAM_TYPE_AUDIO:
          empathy_debug (DEBUG_DOMAIN,
              "Audio stream - id: %d, state: %d, direction: %d",
              stream_id, stream_state, stream_direction);
          priv->audio->exists = TRUE;
          priv->audio->id = stream_id;
          priv->audio->state = stream_state;
          priv->audio->direction = stream_direction;
          break;
        case TP_MEDIA_STREAM_TYPE_VIDEO:
          empathy_debug (DEBUG_DOMAIN,
              "Video stream - id: %d, state: %d, direction: %d",
              stream_id, stream_state, stream_direction);
          priv->video->exists = TRUE;
          priv->video->id = stream_id;
          priv->video->state = stream_state;
          priv->video->direction = stream_direction;
          break;
        default:
          empathy_debug (DEBUG_DOMAIN, "Unknown stream type: %d",
              stream_type);
        }

      g_value_array_free (values);
    }
}

static void
tp_call_stream_added_cb (DBusGProxy *channel,
                         guint stream_id,
                         guint contact_handle,
                         guint stream_type,
                         EmpathyTpCall *call)
{
  empathy_debug (DEBUG_DOMAIN,
      "Stream added - stream id: %d, contact handle: %d, stream type: %d",
      stream_id, contact_handle, stream_type);

  tp_call_identify_streams (call);
}


static void
tp_call_stream_removed_cb (DBusGProxy *channel,
                           guint stream_id,
                           EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  empathy_debug (DEBUG_DOMAIN, "Stream removed - stream id: %d", stream_id);

  if (stream_id == priv->audio->id)
    {
      priv->audio->exists = FALSE;
    }
  else if (stream_id == priv->video->id)
    {
      priv->video->exists = FALSE;
    }
}

static void
tp_call_channel_closed_cb (TpChan *channel,
                           EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);
  DBusGProxy *streamed_iface;
  DBusGProxy *group_iface;

  empathy_debug (DEBUG_DOMAIN, "Channel closed");

  priv->status = EMPATHY_TP_CALL_STATUS_CLOSED;
  g_signal_emit_by_name (call, "status-changed");

  streamed_iface = tp_chan_get_interface (priv->channel,
      TELEPATHY_CHAN_IFACE_STREAMED_QUARK);
  group_iface = tp_chan_get_interface (priv->channel,
      TELEPATHY_CHAN_IFACE_GROUP_QUARK);

  dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (priv->channel), "Closed",
      G_CALLBACK (tp_call_channel_closed_cb), (gpointer) call);
  dbus_g_proxy_disconnect_signal (streamed_iface, "StreamStateChanged",
      G_CALLBACK (tp_call_stream_state_changed_cb), (gpointer) call);
  dbus_g_proxy_disconnect_signal (streamed_iface, "StreamAdded",
      G_CALLBACK (tp_call_stream_added_cb), (gpointer) call);
  dbus_g_proxy_disconnect_signal (streamed_iface, "StreamRemoved",
      G_CALLBACK (tp_call_stream_removed_cb), (gpointer) call);
}

static void
tp_call_stream_direction_changed_cb (DBusGProxy *channel,
                                     guint stream_id,
                                     guint stream_direction,
                                     guint flags,
                                     EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  empathy_debug (DEBUG_DOMAIN,
      "Stream direction changed - stream: %d, direction: %d",
      stream_id, stream_direction);

  if (stream_id == priv->audio->id)
    {
      priv->audio->direction = stream_direction;
    }
  else if (stream_id == priv->video->id)
    {
      priv->video->direction = stream_direction;

      if (stream_direction & TP_MEDIA_STREAM_DIRECTION_RECEIVE)
        {
          empathy_debug (DEBUG_DOMAIN, "RECEIVING");
          g_signal_emit_by_name (call, "receiving-video", TRUE);
        }
      else
        {
          empathy_debug (DEBUG_DOMAIN, "NOT RECEIVING");
          g_signal_emit_by_name (call, "receiving-video", FALSE);
        }

      if (stream_direction & TP_MEDIA_STREAM_DIRECTION_SEND)
        {
          empathy_debug (DEBUG_DOMAIN, "SENDING");
          g_signal_emit_by_name (call, "sending-video", TRUE);
        }
      else
        {
          empathy_debug (DEBUG_DOMAIN, "NOT SENDING");
          g_signal_emit_by_name (call, "sending-video", FALSE);
        }
    }
}

static void
tp_call_request_streams_for_capabilities (EmpathyTpCall *call,
                                          EmpathyCapabilities capabilities)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);
  DBusGProxy *streamed_iface;
  GArray *stream_types;
  guint handle;
  guint stream_type;
  GError *error = NULL;

  empathy_debug (DEBUG_DOMAIN, "Requesting new stream for capabilities %d",
      capabilities);

  streamed_iface = tp_chan_get_interface (priv->channel,
      TELEPATHY_CHAN_IFACE_STREAMED_QUARK);
  stream_types = g_array_new (FALSE, FALSE, sizeof (guint));
  handle = empathy_contact_get_handle (priv->contact);

  if (capabilities & EMPATHY_CAPABILITIES_AUDIO)
    {
      stream_type = TP_MEDIA_STREAM_TYPE_AUDIO;
      g_array_append_val (stream_types, stream_type);
    }
  if (capabilities & EMPATHY_CAPABILITIES_VIDEO)
    {
      stream_type = TP_MEDIA_STREAM_TYPE_VIDEO;
      g_array_append_val (stream_types, stream_type);
    }

  if (!tp_chan_type_streamed_media_request_streams (streamed_iface, handle,
        stream_types, NULL, &error))
    {
      empathy_debug (DEBUG_DOMAIN, "Couldn't request new stream: %s",
          error->message);
      g_clear_error (&error);
    }

  g_array_free (stream_types, TRUE);
}

static void
tp_call_request_streams_capabilities_cb (EmpathyContact *contact,
                                         GParamSpec *property,
                                         gpointer user_data)
{
  EmpathyTpCall *call = EMPATHY_TP_CALL (user_data);

  g_signal_handlers_disconnect_by_func (contact,
      tp_call_request_streams_capabilities_cb,
      user_data);

  tp_call_request_streams_for_capabilities (call,
     empathy_contact_get_capabilities (contact));
}

static void
tp_call_request_streams (EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);
  EmpathyCapabilities capabilities;
  DBusGProxy *capabilities_iface;

  empathy_debug (DEBUG_DOMAIN,
      "Requesting appropriate audio/video streams from contact");


  /* FIXME: SIP don't have capabilities interface but we know it supports
   *        only audio and not video. */
  capabilities_iface = tp_conn_get_interface (priv->connection,
      TP_IFACE_QUARK_CONNECTION_INTERFACE_CAPABILITIES);
  if (!capabilities_iface)
    {
      capabilities = EMPATHY_CAPABILITIES_AUDIO;
    }
  else
    {
      capabilities = empathy_contact_get_capabilities (priv->contact);
      if (capabilities == EMPATHY_CAPABILITIES_UNKNOWN)
        {
          g_signal_connect (priv->contact, "notify::capabilities",
              G_CALLBACK (tp_call_request_streams_capabilities_cb), call);
          return;
        }
    }

  tp_call_request_streams_for_capabilities (call, capabilities);
}

static void
tp_call_is_ready (EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);
  EmpathyContact *self_contact;
  GList *members;
  GList *local_pendings;
  GList *remote_pendings;

  if (priv->status > EMPATHY_TP_CALL_STATUS_READYING)
    return;

  members = empathy_tp_group_get_members (priv->group);
  if (!members)
    return;

  self_contact = empathy_tp_group_get_self_contact (priv->group);
  local_pendings = empathy_tp_group_get_local_pendings (priv->group);
  remote_pendings = empathy_tp_group_get_remote_pendings (priv->group);

  if (local_pendings &&
      empathy_contact_equal (EMPATHY_CONTACT (((EmpathyPendingInfo *)
            local_pendings->data)->member), self_contact))
    {
      empathy_debug (DEBUG_DOMAIN,
          "Incoming call is ready - %p",
          ((EmpathyPendingInfo *) local_pendings->data)->member);
      priv->is_incoming = TRUE;
      priv->contact = g_object_ref (members->data);
    }
  else if (remote_pendings &&
      empathy_contact_equal (EMPATHY_CONTACT (members->data), self_contact))
    {
      empathy_debug (DEBUG_DOMAIN,
          "Outgoing call is ready - %p", remote_pendings->data);
      priv->is_incoming = FALSE;
      priv->contact = g_object_ref (remote_pendings->data);
      tp_call_request_streams (call);
    }

  g_object_unref (self_contact);
  g_list_foreach (members, (GFunc) g_object_unref, NULL);
  g_list_free (members);
  g_list_foreach (local_pendings, (GFunc) empathy_pending_info_free, NULL);
  g_list_free (local_pendings);
  g_list_foreach (remote_pendings, (GFunc) g_object_unref, NULL);
  g_list_free (remote_pendings);

  if (priv->contact)
    {
      priv->status = EMPATHY_TP_CALL_STATUS_PENDING;
      g_signal_emit (call, signals[STATUS_CHANGED_SIGNAL], 0);
    }
}

static void
tp_call_member_added_cb (EmpathyTpGroup *group,
                         EmpathyContact *contact,
                         EmpathyContact *actor,
                         guint reason,
                         const gchar *message,
                         EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  empathy_debug (DEBUG_DOMAIN, "New member added callback %p", contact);
  tp_call_is_ready (call);

  if (priv->status == EMPATHY_TP_CALL_STATUS_PENDING)
    {
      if ((priv->is_incoming &&
            !empathy_contact_equal (contact, priv->contact))
          || (!priv->is_incoming &&
            empathy_contact_equal (contact, priv->contact)))
        {
          priv->status = EMPATHY_TP_CALL_STATUS_ACCEPTED;
          g_signal_emit (call, signals[STATUS_CHANGED_SIGNAL], 0);
        }
    }
}

static void
tp_call_local_pending_cb (EmpathyTpGroup *group,
                          EmpathyContact *contact,
                          EmpathyContact *actor,
                          guint reason,
                          const gchar *message,
                          EmpathyTpCall *call)
{
  empathy_debug (DEBUG_DOMAIN, "New local pending added callback %p", contact);
  tp_call_is_ready (call);
}

static void
tp_call_remote_pending_cb (EmpathyTpGroup *group,
                           EmpathyContact *contact,
                           EmpathyContact *actor,
                           guint reason,
                           const gchar *message,
                           EmpathyTpCall *call)
{
  empathy_debug (DEBUG_DOMAIN, "New remote pending added callback %p", contact);
  tp_call_is_ready (call);
}

static void
tp_call_async_cb (TpProxy *proxy,
                  const GError *error,
                  gpointer user_data,
                  GObject *call)
{
  if (error)
    {
      empathy_debug (DEBUG_DOMAIN, "Error %s: %s",
          user_data, error->message);
    }
}

static void
tp_call_invalidated_cb (TpProxy       *stream_engine,
                        GQuark         domain,
                        gint           code,
                        gchar         *message,
                        EmpathyTpCall *call)
{
  empathy_debug (DEBUG_DOMAIN, "Stream engine proxy invalidated: %s",
      message);
  empathy_tp_call_close_channel (call);
}

static void
tp_call_watch_name_owner_cb (TpDBusDaemon *daemon,
                             const gchar *name,
                             const gchar *new_owner,
                             gpointer call)
{
  if (G_STR_EMPTY (new_owner))
    {
      empathy_debug (DEBUG_DOMAIN, "Stream engine falled off the bus");
      empathy_tp_call_close_channel (call);
    }
}

static void
tp_call_start_stream_engine (EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  empathy_debug (DEBUG_DOMAIN, "Revving up the stream engine");

  priv->stream_engine = g_object_new (TP_TYPE_PROXY,
      "bus-name", STREAM_ENGINE_BUS_NAME,
      "dbus-connection", tp_get_bus (),
      "object-path", STREAM_ENGINE_OBJECT_PATH,
       NULL);
  tp_proxy_add_interface_by_id (priv->stream_engine,
      EMP_IFACE_QUARK_STREAM_ENGINE);
  tp_proxy_add_interface_by_id (priv->stream_engine,
      EMP_IFACE_QUARK_CHANNEL_HANDLER);

  g_signal_connect (priv->stream_engine, "invalidated",
      G_CALLBACK (tp_call_invalidated_cb),
      call);
  
  /* FIXME: dbus daemon should be unique */
  priv->dbus_daemon = tp_dbus_daemon_new (tp_get_bus ());
  tp_dbus_daemon_watch_name_owner (priv->dbus_daemon, STREAM_ENGINE_BUS_NAME,
      tp_call_watch_name_owner_cb,
      call, NULL);

  emp_cli_channel_handler_call_handle_channel (priv->stream_engine, -1,
        dbus_g_proxy_get_bus_name (DBUS_G_PROXY (priv->connection)),
        dbus_g_proxy_get_path (DBUS_G_PROXY (priv->connection)),
        priv->channel->type,
        dbus_g_proxy_get_path (DBUS_G_PROXY (priv->channel)),
        priv->channel->handle_type, priv->channel->handle,
        tp_call_async_cb,
        "calling handle channel", NULL,
        G_OBJECT (call));
}

static GObject *
tp_call_constructor (GType type,
                     guint n_construct_params,
                     GObjectConstructParam *construct_params)
{
  GObject *object;
  EmpathyTpCall *call;
  EmpathyTpCallPriv *priv;
  DBusGProxy *streamed_iface;
  MissionControl *mc;
  McAccount *account;

  object = G_OBJECT_CLASS (empathy_tp_call_parent_class)->constructor (type,
      n_construct_params, construct_params);

  call = EMPATHY_TP_CALL (object);
  priv = GET_PRIV (call);

  dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->channel), "Closed",
     G_CALLBACK (tp_call_channel_closed_cb), (gpointer) call, NULL);

  streamed_iface = tp_chan_get_interface (priv->channel,
      TELEPATHY_CHAN_IFACE_STREAMED_QUARK);
  dbus_g_proxy_connect_signal (streamed_iface, "StreamStateChanged",
      G_CALLBACK (tp_call_stream_state_changed_cb),
      (gpointer) call, NULL);
  dbus_g_proxy_connect_signal (streamed_iface, "StreamDirectionChanged",
      G_CALLBACK (tp_call_stream_direction_changed_cb),
      (gpointer) call, NULL);
  dbus_g_proxy_connect_signal (streamed_iface, "StreamAdded",
      G_CALLBACK (tp_call_stream_added_cb), (gpointer) call, NULL);
  dbus_g_proxy_connect_signal (streamed_iface, "StreamRemoved",
      G_CALLBACK (tp_call_stream_removed_cb), (gpointer) call, NULL);

  mc = empathy_mission_control_new ();
  account = mission_control_get_account_for_connection (mc, priv->connection,
      NULL);
  priv->group = empathy_tp_group_new (account, priv->channel);
  g_object_unref (mc);

  g_signal_connect (G_OBJECT (priv->group), "member-added",
      G_CALLBACK (tp_call_member_added_cb), (gpointer) call);
  g_signal_connect (G_OBJECT (priv->group), "local-pending",
      G_CALLBACK (tp_call_local_pending_cb), (gpointer) call);
  g_signal_connect (G_OBJECT (priv->group), "remote-pending",
      G_CALLBACK (tp_call_remote_pending_cb), (gpointer) call);

  tp_call_start_stream_engine (call);
  /* FIXME: unnecessary for outgoing? */
  tp_call_identify_streams (call);

  return object;
}

static void 
tp_call_finalize (GObject *object)
{
  EmpathyTpCallPriv *priv = GET_PRIV (object);

  empathy_debug (DEBUG_DOMAIN, "Finalizing: %p", object);

  g_slice_free (EmpathyTpCallStream, priv->audio);
  g_slice_free (EmpathyTpCallStream, priv->video);
  g_object_unref (priv->group);

  if (priv->connection != NULL)
    g_object_unref (priv->connection);

  if (priv->channel != NULL)
    g_object_unref (priv->channel);

  if (priv->stream_engine != NULL)
    g_object_unref (priv->stream_engine);

  if (priv->contact != NULL)
      g_object_unref (priv->contact);

  if (priv->dbus_daemon != NULL)
    {
      tp_dbus_daemon_cancel_name_owner_watch (priv->dbus_daemon,
          STREAM_ENGINE_BUS_NAME,
          tp_call_watch_name_owner_cb,
          object);
      g_object_unref (priv->dbus_daemon);
    }

  (G_OBJECT_CLASS (empathy_tp_call_parent_class)->finalize) (object);
}

static void 
tp_call_set_property (GObject *object,
                      guint prop_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
  EmpathyTpCallPriv *priv = GET_PRIV (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      priv->connection = g_value_dup_object (value);
      break;
    case PROP_CHANNEL:
      priv->channel = g_value_dup_object (value);
      break;
    case PROP_CONTACT:
      /* FIXME should this one be writable in the first place ? */
      g_assert (priv->contact == NULL);
      priv->contact = g_value_dup_object (value);
      break;
    case PROP_IS_INCOMING:
      priv->is_incoming = g_value_get_boolean (value);
      break;
    case PROP_STATUS:
      priv->status = g_value_get_uint (value);
      break;
    case PROP_AUDIO_STREAM:
      priv->audio = g_value_get_pointer (value);
      break;
    case PROP_VIDEO_STREAM:
      priv->video = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
tp_call_get_property (GObject *object,
                      guint prop_id,
                      GValue *value,
                      GParamSpec *pspec)
{
  EmpathyTpCallPriv *priv = GET_PRIV (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, priv->connection);
      break;
    case PROP_CHANNEL:
      g_value_set_object (value, priv->channel);
      break;
    case PROP_CONTACT:
      g_value_set_object (value, priv->contact);
      break;
    case PROP_IS_INCOMING:
      g_value_set_boolean (value, priv->is_incoming);
      break;
    case PROP_STATUS:
      g_value_set_uint (value, priv->status);
      break;
    case PROP_AUDIO_STREAM:
      g_value_set_pointer (value, priv->audio);
      break;
    case PROP_VIDEO_STREAM:
      g_value_set_pointer (value, priv->video);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
empathy_tp_call_class_init (EmpathyTpCallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  emp_cli_init ();

  object_class->constructor = tp_call_constructor;
  object_class->finalize = tp_call_finalize;
  object_class->set_property = tp_call_set_property;
  object_class->get_property = tp_call_get_property;

  g_type_class_add_private (klass, sizeof (EmpathyTpCallPriv));

  signals[STATUS_CHANGED_SIGNAL] =
      g_signal_new ("status-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);
  signals[RECEIVING_VIDEO_SIGNAL] =
      g_signal_new ("receiving-video", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN,
      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
  signals[SENDING_VIDEO_SIGNAL] =
      g_signal_new ("sending-video", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN,
      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  g_object_class_install_property (object_class, PROP_CONNECTION,
      g_param_spec_object ("connection", "connection", "connection",
      TELEPATHY_CONN_TYPE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class, PROP_CHANNEL,
      g_param_spec_object ("channel", "channel", "channel",
      TELEPATHY_CHAN_TYPE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class, PROP_CONTACT,
      g_param_spec_object ("contact", "Call contact", "Call contact",
      EMPATHY_TYPE_CONTACT,
      G_PARAM_READABLE | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class, PROP_IS_INCOMING,
      g_param_spec_boolean ("is-incoming", "Is media stream incoming",
      "Is media stream incoming", FALSE, G_PARAM_READABLE |
      G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class, PROP_STATUS,
      g_param_spec_uint ("status", "Call status",
      "Call status", 0, 255, 0, G_PARAM_READABLE | G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class, PROP_AUDIO_STREAM,
      g_param_spec_pointer ("audio-stream", "Audio stream data",
      "Audio stream data",
      G_PARAM_READABLE | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class, PROP_VIDEO_STREAM,
      g_param_spec_pointer ("video-stream", "Video stream data",
      "Video stream data",
      G_PARAM_READABLE | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

static void
empathy_tp_call_init (EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  priv->status = EMPATHY_TP_CALL_STATUS_READYING;
  priv->contact = NULL;
  priv->audio = g_slice_new0 (EmpathyTpCallStream);
  priv->video = g_slice_new0 (EmpathyTpCallStream);
  priv->audio->exists = FALSE;
  priv->video->exists = FALSE;
}

EmpathyTpCall *
empathy_tp_call_new (TpConn *connection, TpChan *channel)
{
  return g_object_new (EMPATHY_TYPE_TP_CALL,
      "connection", connection,
      "channel", channel,
      NULL);
}

void
empathy_tp_call_accept_incoming_call (EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);
  GList *local_pendings;

  empathy_debug (DEBUG_DOMAIN, "Accepting incoming call");

  local_pendings = empathy_tp_group_get_local_pendings (priv->group);

  empathy_tp_group_add_member (priv->group, EMPATHY_CONTACT
      (((EmpathyPendingInfo *) local_pendings->data)->member), NULL);

  g_list_foreach (local_pendings, (GFunc) empathy_pending_info_free, NULL);
  g_list_free (local_pendings);
}

void
empathy_tp_call_request_video_stream_direction (EmpathyTpCall *call,
                                                gboolean is_sending)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);
  DBusGProxy *streamed_iface;
  guint new_direction;
  GError *error = NULL;

  empathy_debug (DEBUG_DOMAIN,
      "Requesting video stream direction - is_sending: %d", is_sending);

  if (!priv->video->exists)
    {
      tp_call_request_streams_for_capabilities (call, EMPATHY_CAPABILITIES_VIDEO);
      return;
    }

  streamed_iface = tp_chan_get_interface (priv->channel,
      TELEPATHY_CHAN_IFACE_STREAMED_QUARK);

  if (is_sending)
    {
      new_direction = priv->video->direction | TP_MEDIA_STREAM_DIRECTION_SEND;
    }
  else
    {
      new_direction = priv->video->direction & ~TP_MEDIA_STREAM_DIRECTION_SEND;
    }

  if (!tp_chan_type_streamed_media_request_stream_direction (streamed_iface,
        priv->video->id, new_direction, &error))
    {
      empathy_debug (DEBUG_DOMAIN,
          "Couldn't request video stream direction: %s", error->message);
      g_clear_error (&error);
    }
}

void
empathy_tp_call_close_channel (EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);
  GError *error = NULL;

  if (priv->status == EMPATHY_TP_CALL_STATUS_CLOSED)
      return;

  empathy_debug (DEBUG_DOMAIN, "Closing channel");

  if (!tp_chan_close (DBUS_G_PROXY (priv->channel), &error))
    {
      empathy_debug (DEBUG_DOMAIN, "Error closing channel: %s",
          error ? error->message : "No error given");
      g_clear_error (&error);
    }
  else
        priv->status = EMPATHY_TP_CALL_STATUS_CLOSED;
}

void
empathy_tp_call_add_preview_video (EmpathyTpCall *call,
                                   guint preview_video_socket_id)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  empathy_debug (DEBUG_DOMAIN, "Adding preview video");

  emp_cli_stream_engine_call_add_preview_window (priv->stream_engine, -1,
      preview_video_socket_id,
      tp_call_async_cb,
      "adding preview window", NULL,
      G_OBJECT (call));
}

void
empathy_tp_call_remove_preview_video (EmpathyTpCall *call,
                                      guint preview_video_socket_id)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  empathy_debug (DEBUG_DOMAIN, "Removing preview video");

  emp_cli_stream_engine_call_remove_preview_window (priv->stream_engine, -1,
      preview_video_socket_id,
      tp_call_async_cb,
      "removing preview window", NULL,
      G_OBJECT (call));
}

void
empathy_tp_call_add_output_video (EmpathyTpCall *call,
                                  guint output_video_socket_id)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  empathy_debug (DEBUG_DOMAIN, "Adding output video - socket: %d",
      output_video_socket_id);

  emp_cli_stream_engine_call_set_output_window (priv->stream_engine, -1,
      dbus_g_proxy_get_path (DBUS_G_PROXY (priv->channel)),
      priv->video->id, output_video_socket_id,
      tp_call_async_cb,
      "setting output window", NULL,
      G_OBJECT (call));
}

void
empathy_tp_call_set_output_volume (EmpathyTpCall *call,
                                   guint volume)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  if (priv->status == EMPATHY_TP_CALL_STATUS_CLOSED)
    return;

  empathy_debug (DEBUG_DOMAIN, "Setting output volume: %d", volume);

  emp_cli_stream_engine_call_set_output_volume (priv->stream_engine, -1,
      dbus_g_proxy_get_path (DBUS_G_PROXY (priv->channel)),
      priv->audio->id, volume,
      tp_call_async_cb,
      "setting output volume", NULL,
      G_OBJECT (call));
}

void
empathy_tp_call_mute_output (EmpathyTpCall *call,
                             gboolean is_muted)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  if (priv->status == EMPATHY_TP_CALL_STATUS_CLOSED)
    return;

  empathy_debug (DEBUG_DOMAIN, "Setting output mute: %d", is_muted);

  emp_cli_stream_engine_call_mute_output (priv->stream_engine, -1,
      dbus_g_proxy_get_path (DBUS_G_PROXY (priv->channel)),
      priv->audio->id, is_muted,
      tp_call_async_cb,
      "muting output", NULL,
      G_OBJECT (call));
}

void
empathy_tp_call_mute_input (EmpathyTpCall *call,
                            gboolean is_muted)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  if (priv->status == EMPATHY_TP_CALL_STATUS_CLOSED)
    return;

  empathy_debug (DEBUG_DOMAIN, "Setting input mute: %d", is_muted);

  emp_cli_stream_engine_call_mute_input (priv->stream_engine, -1,
      dbus_g_proxy_get_path (DBUS_G_PROXY (priv->channel)),
      priv->audio->id, is_muted,
      tp_call_async_cb,
      "muting input", NULL,
      G_OBJECT (call));
}

