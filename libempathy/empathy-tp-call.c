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

#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/dbus.h>

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
  TpChannel *channel;
  TpProxy *stream_engine;
  TpDBusDaemon *dbus_daemon;
  EmpathyTpGroup *group;
  EmpathyContact *contact;
  gboolean is_incoming;
  guint status;
  gboolean stream_engine_running;

  EmpathyTpCallStream *audio;
  EmpathyTpCallStream *video;
};

enum
{
  PROP_0,
  PROP_CHANNEL,
  PROP_CONTACT,
  PROP_IS_INCOMING,
  PROP_STATUS,
  PROP_AUDIO_STREAM,
  PROP_VIDEO_STREAM
};

G_DEFINE_TYPE (EmpathyTpCall, empathy_tp_call, G_TYPE_OBJECT)

static void
tp_call_add_stream (EmpathyTpCall *call,
                    guint stream_id,
                    guint contact_handle,
                    guint stream_type,
                    guint stream_state,
                    guint stream_direction)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

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
        g_object_notify (G_OBJECT (call), "audio-stream");
        break;
      case TP_MEDIA_STREAM_TYPE_VIDEO:
        empathy_debug (DEBUG_DOMAIN,
            "Video stream - id: %d, state: %d, direction: %d",
            stream_id, stream_state, stream_direction);
        priv->video->exists = TRUE;
        priv->video->id = stream_id;
        priv->video->state = stream_state;
        priv->video->direction = stream_direction;
        g_object_notify (G_OBJECT (call), "video-stream");
        break;
      default:
        empathy_debug (DEBUG_DOMAIN, "Unknown stream type: %d",
            stream_type);
    }
}

static void
tp_call_stream_added_cb (TpChannel *channel,
                         guint stream_id,
                         guint contact_handle,
                         guint stream_type,
                         gpointer user_data,
                         GObject *call)
{
  empathy_debug (DEBUG_DOMAIN,
      "Stream added - stream id: %d, contact handle: %d, stream type: %d",
      stream_id, contact_handle, stream_type);

  tp_call_add_stream (EMPATHY_TP_CALL (call), stream_id, contact_handle,
      stream_type, TP_MEDIA_STREAM_STATE_DISCONNECTED,
      TP_MEDIA_STREAM_DIRECTION_NONE);
}

static void
tp_call_stream_removed_cb (TpChannel *channel,
                           guint stream_id,
                           gpointer user_data,
                           GObject *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  empathy_debug (DEBUG_DOMAIN, "Stream removed - stream id: %d", stream_id);

  if (stream_id == priv->audio->id)
    {
      priv->audio->exists = FALSE;
      g_object_notify (call, "audio-stream");
    }
  else if (stream_id == priv->video->id)
    {
      priv->video->exists = FALSE;
      g_object_notify (call, "video-stream");
    }
}

static void
tp_call_stream_state_changed_cb (TpChannel *proxy,
                                 guint stream_id,
                                 guint stream_state,
                                 gpointer user_data,
                                 GObject *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  empathy_debug (DEBUG_DOMAIN,
      "Stream state changed - stream id: %d, state state: %d",
      stream_id, stream_state);

  if (stream_id == priv->audio->id)
    {
      priv->audio->state = stream_state;
      g_object_notify (call, "audio-stream");
    }
  else if (stream_id == priv->video->id)
    {
      priv->video->state = stream_state;
      g_object_notify (call, "video-stream");
    }
}

static void
tp_call_stream_direction_changed_cb (TpChannel *channel,
                                     guint stream_id,
                                     guint stream_direction,
                                     guint pending_flags,
                                     gpointer user_data,
                                     GObject *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  empathy_debug (DEBUG_DOMAIN,
      "Stream direction changed - stream: %d, direction: %d",
      stream_id, stream_direction);

  if (stream_id == priv->audio->id)
    {
      priv->audio->direction = stream_direction;
      g_object_notify (call, "audio-stream");
    }
  else if (stream_id == priv->video->id)
    {
      priv->video->direction = stream_direction;
      g_object_notify (call, "video-stream");
    }
}

static void
tp_call_request_streams_cb (TpChannel *channel,
                            const GPtrArray *streams,
                            const GError *error,
                            gpointer user_data,
                            GObject *call)
{
  guint i;

  if (error)
    {
      empathy_debug (DEBUG_DOMAIN, "Error requesting streams: %s", error->message);
      return;
    }

  for (i = 0; i < streams->len; i++)
    {
      GValueArray *values;
      guint stream_id;
      guint contact_handle;
      guint stream_type;
      guint stream_state;
      guint stream_direction;

      values = g_ptr_array_index (streams, i);
      stream_id = g_value_get_uint (g_value_array_get_nth (values, 0));
      contact_handle = g_value_get_uint (g_value_array_get_nth (values, 1));
      stream_type = g_value_get_uint (g_value_array_get_nth (values, 2));
      stream_state = g_value_get_uint (g_value_array_get_nth (values, 3));
      stream_direction = g_value_get_uint (g_value_array_get_nth (values, 4));

      tp_call_add_stream (EMPATHY_TP_CALL (call), stream_id, contact_handle,
          stream_type, stream_state, stream_direction);
  }
}

static void
tp_call_request_streams_for_capabilities (EmpathyTpCall *call,
                                          EmpathyCapabilities capabilities)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);
  GArray *stream_types;
  guint handle;
  guint stream_type;

  empathy_debug (DEBUG_DOMAIN, "Requesting new stream for capabilities %d",
      capabilities);

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

  tp_cli_channel_type_streamed_media_call_request_streams (priv->channel, -1,
      handle, stream_types, tp_call_request_streams_cb, NULL, NULL,
      G_OBJECT (call));

  g_array_free (stream_types, TRUE);
}

static void
tp_call_request_streams (EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);
  EmpathyCapabilities capabilities;
  TpConnection *connection;

  empathy_debug (DEBUG_DOMAIN,
      "Requesting appropriate audio/video streams from contact");

  /* FIXME: SIP don't have capabilities interface but we know it supports
   *        only audio and not video. */
  g_object_get (priv->channel, "connection", &connection, NULL);
  if (!tp_proxy_has_interface_by_id (connection,
           TP_IFACE_QUARK_CONNECTION_INTERFACE_CAPABILITIES))
      capabilities = EMPATHY_CAPABILITIES_AUDIO;
  else
      capabilities = empathy_contact_get_capabilities (priv->contact);

  tp_call_request_streams_for_capabilities (call, capabilities);
  g_object_unref (connection);
}

static void
tp_call_group_ready_cb (EmpathyTpCall *call)
{
  EmpathyTpCallPriv  *priv = GET_PRIV (call);
  EmpathyPendingInfo *invitation;

  invitation = empathy_tp_group_get_invitation (priv->group, &priv->contact);
  priv->is_incoming = (invitation != NULL);
  priv->status = EMPATHY_TP_CALL_STATUS_PENDING;

  if (!priv->is_incoming)
      tp_call_request_streams (call);

  g_object_notify (G_OBJECT (call), "is-incoming");
  g_object_notify (G_OBJECT (call), "contact");
  g_object_notify (G_OBJECT (call), "status");
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

  if (priv->status == EMPATHY_TP_CALL_STATUS_PENDING &&
      ((priv->is_incoming && contact != priv->contact) ||
       (!priv->is_incoming && contact == priv->contact)))
    {
      priv->status = EMPATHY_TP_CALL_STATUS_ACCEPTED;
      g_object_notify (G_OBJECT (call), "status");
    }
}

static void
tp_call_channel_invalidated_cb (TpChannel     *channel,
                                GQuark         domain,
                                gint           code,
                                gchar         *message,
                                EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  empathy_debug (DEBUG_DOMAIN, "Channel invalidated: %s", message);
  priv->status = EMPATHY_TP_CALL_STATUS_CLOSED;
  g_object_notify (G_OBJECT (call), "status");
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
tp_call_close_channel (EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  if (priv->status == EMPATHY_TP_CALL_STATUS_CLOSED)
      return;

  empathy_debug (DEBUG_DOMAIN, "Closing channel");

  tp_cli_channel_call_close (priv->channel, -1,
      (tp_cli_channel_callback_for_close) tp_call_async_cb,
      "closing channel", NULL, G_OBJECT (call));

  priv->status = EMPATHY_TP_CALL_STATUS_CLOSED;
  g_object_notify (G_OBJECT (call), "status");
}

static void
tp_call_stream_engine_invalidated_cb (TpProxy       *stream_engine,
				      GQuark         domain,
				      gint           code,
				      gchar         *message,
				      EmpathyTpCall *call)
{
  empathy_debug (DEBUG_DOMAIN, "Stream engine proxy invalidated: %s",
      message);
  tp_call_close_channel (call);
}

static void
tp_call_stream_engine_watch_name_owner_cb (TpDBusDaemon *daemon,
					   const gchar *name,
					   const gchar *new_owner,
					   gpointer call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  /* G_STR_EMPTY(new_owner) means either stream-engine has not started yet or
   * has crashed. We want to close the channel if stream-engine has crashed.
   * */
  empathy_debug (DEBUG_DOMAIN,
                 "Watch SE: name='%s' SE running='%s' new_owner='%s'",
                 name, priv->stream_engine_running ? "yes" : "no",
                 new_owner ? new_owner : "none");
  if (priv->stream_engine_running && G_STR_EMPTY (new_owner))
    {
      empathy_debug (DEBUG_DOMAIN, "Stream engine falled off the bus");
      tp_call_close_channel (call);
      return;
    }

  priv->stream_engine_running = !G_STR_EMPTY (new_owner);
}

static void
tp_call_stream_engine_handle_channel (EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);
  gchar *channel_type;
  gchar *object_path;
  guint handle_type;
  guint handle;
  TpProxy *connection;

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
      G_CALLBACK (tp_call_stream_engine_invalidated_cb),
      call);
  
  /* FIXME: dbus daemon should be unique */
  priv->dbus_daemon = tp_dbus_daemon_new (tp_get_bus ());
  tp_dbus_daemon_watch_name_owner (priv->dbus_daemon, STREAM_ENGINE_BUS_NAME,
      tp_call_stream_engine_watch_name_owner_cb,
      call, NULL);

  g_object_get (priv->channel,
      "connection", &connection,
      "channel-type", &channel_type,
      "object-path", &object_path,
      "handle_type", &handle_type,
      "handle", &handle,
      NULL);

  emp_cli_channel_handler_call_handle_channel (priv->stream_engine, -1,
        connection->bus_name,
        connection->object_path,
        channel_type, object_path, handle_type, handle,
        tp_call_async_cb, "calling handle channel", NULL,
        G_OBJECT (call));

  g_object_unref (connection);
  g_free (channel_type);
  g_free (object_path);
}

static GObject *
tp_call_constructor (GType type,
                     guint n_construct_params,
                     GObjectConstructParam *construct_params)
{
  GObject *object;
  EmpathyTpCall *call;
  EmpathyTpCallPriv *priv;

  object = G_OBJECT_CLASS (empathy_tp_call_parent_class)->constructor (type,
      n_construct_params, construct_params);

  call = EMPATHY_TP_CALL (object);
  priv = GET_PRIV (call);

  /* Setup streamed media channel */
  g_signal_connect (priv->channel, "invalidated",
      G_CALLBACK (tp_call_channel_invalidated_cb), call);
  tp_cli_channel_type_streamed_media_connect_to_stream_added (priv->channel,
      tp_call_stream_added_cb, NULL, NULL, G_OBJECT (call), NULL);
  tp_cli_channel_type_streamed_media_connect_to_stream_removed (priv->channel,
      tp_call_stream_removed_cb, NULL, NULL, G_OBJECT (call), NULL);
  tp_cli_channel_type_streamed_media_connect_to_stream_state_changed (priv->channel,
      tp_call_stream_state_changed_cb, NULL, NULL, G_OBJECT (call), NULL);
  tp_cli_channel_type_streamed_media_connect_to_stream_direction_changed (priv->channel,
      tp_call_stream_direction_changed_cb, NULL, NULL, G_OBJECT (call), NULL);
  tp_cli_channel_type_streamed_media_call_list_streams (priv->channel, -1,
      tp_call_request_streams_cb, NULL, NULL, G_OBJECT (call));

  /* Setup group interface */
  priv->group = empathy_tp_group_new (priv->channel);

  g_signal_connect (G_OBJECT (priv->group), "member-added",
      G_CALLBACK (tp_call_member_added_cb), call);

  if (empathy_tp_group_is_ready (priv->group))
      tp_call_group_ready_cb (call);
  else
      g_signal_connect_swapped (priv->group, "ready",
          G_CALLBACK (tp_call_group_ready_cb), call);

  /* Start stream engine */
  tp_call_stream_engine_handle_channel (call);

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

  if (priv->channel != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->channel,
          tp_call_channel_invalidated_cb, object);
      g_object_unref (priv->channel);
    }

  if (priv->stream_engine != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->stream_engine,
          tp_call_stream_engine_invalidated_cb, object);
      g_object_unref (priv->stream_engine);
    }

  if (priv->contact != NULL)
      g_object_unref (priv->contact);

  if (priv->dbus_daemon != NULL)
    {
      tp_dbus_daemon_cancel_name_owner_watch (priv->dbus_daemon,
          STREAM_ENGINE_BUS_NAME,
          tp_call_stream_engine_watch_name_owner_cb,
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
    case PROP_CHANNEL:
      priv->channel = g_value_dup_object (value);
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
  priv->stream_engine_running = FALSE;
  priv->audio = g_slice_new0 (EmpathyTpCallStream);
  priv->video = g_slice_new0 (EmpathyTpCallStream);
  priv->audio->exists = FALSE;
  priv->video->exists = FALSE;
}

EmpathyTpCall *
empathy_tp_call_new (TpChannel *channel)
{
  g_return_val_if_fail (TP_IS_CHANNEL (channel), NULL);

  return g_object_new (EMPATHY_TYPE_TP_CALL,
      "channel", channel,
      NULL);
}

void
empathy_tp_call_accept_incoming_call (EmpathyTpCall *call)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);
  EmpathyContact *self_contact;

  g_return_if_fail (EMPATHY_IS_TP_CALL (call));
  g_return_if_fail (priv->status == EMPATHY_TP_CALL_STATUS_PENDING);

  empathy_debug (DEBUG_DOMAIN, "Accepting incoming call");

  self_contact = empathy_tp_group_get_self_contact (priv->group);
  empathy_tp_group_add_member (priv->group, self_contact, NULL);
  g_object_unref (self_contact);
}

void
empathy_tp_call_request_video_stream_direction (EmpathyTpCall *call,
                                                gboolean is_sending)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);
  guint new_direction;

  g_return_if_fail (EMPATHY_IS_TP_CALL (call));
  g_return_if_fail (priv->status == EMPATHY_TP_CALL_STATUS_ACCEPTED);

  empathy_debug (DEBUG_DOMAIN,
      "Requesting video stream direction - is_sending: %d", is_sending);

  if (!priv->video->exists)
    {
      tp_call_request_streams_for_capabilities (call, EMPATHY_CAPABILITIES_VIDEO);
      return;
    }

  if (is_sending)
      new_direction = priv->video->direction | TP_MEDIA_STREAM_DIRECTION_SEND;
  else
      new_direction = priv->video->direction & ~TP_MEDIA_STREAM_DIRECTION_SEND;

  tp_cli_channel_type_streamed_media_call_request_stream_direction (priv->channel,
      -1, priv->video->id, new_direction,
      (tp_cli_channel_type_streamed_media_callback_for_request_stream_direction)
      tp_call_async_cb, NULL, NULL, G_OBJECT (call));
}

void
empathy_tp_call_add_preview_video (EmpathyTpCall *call,
                                   guint preview_video_socket_id)
{
  EmpathyTpCallPriv *priv = GET_PRIV (call);

  g_return_if_fail (EMPATHY_IS_TP_CALL (call));
  g_return_if_fail (priv->status != EMPATHY_TP_CALL_STATUS_CLOSED);

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

  g_return_if_fail (EMPATHY_IS_TP_CALL (call));
  g_return_if_fail (priv->status != EMPATHY_TP_CALL_STATUS_CLOSED);

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

  g_return_if_fail (EMPATHY_IS_TP_CALL (call));
  g_return_if_fail (priv->status != EMPATHY_TP_CALL_STATUS_CLOSED);

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

  g_return_if_fail (EMPATHY_IS_TP_CALL (call));
  g_return_if_fail (priv->status != EMPATHY_TP_CALL_STATUS_CLOSED);

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

  g_return_if_fail (EMPATHY_IS_TP_CALL (call));
  g_return_if_fail (priv->status != EMPATHY_TP_CALL_STATUS_CLOSED);

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

  g_return_if_fail (EMPATHY_IS_TP_CALL (call));
  g_return_if_fail (priv->status != EMPATHY_TP_CALL_STATUS_CLOSED);

  empathy_debug (DEBUG_DOMAIN, "Setting input mute: %d", is_muted);

  emp_cli_stream_engine_call_mute_input (priv->stream_engine, -1,
      dbus_g_proxy_get_path (DBUS_G_PROXY (priv->channel)),
      priv->audio->id, is_muted,
      tp_call_async_cb,
      "muting input", NULL,
      G_OBJECT (call));
}

