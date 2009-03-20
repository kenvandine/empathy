/*
 * empathy-call-handler.c - Source for EmpathyCallHandler
 * Copyright (C) 2008 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
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
 */


#include <stdio.h>
#include <stdlib.h>

#include <telepathy-glib/util.h>

#include <telepathy-farsight/channel.h>
#include <telepathy-farsight/stream.h>

#include "empathy-call-handler.h"
#include "empathy-dispatcher.h"
#include "empathy-marshal.h"
#include "empathy-utils.h"

G_DEFINE_TYPE(EmpathyCallHandler, empathy_call_handler, G_TYPE_OBJECT)

/* signal enum */
enum {
  CONFERENCE_ADDED,
  SRC_PAD_ADDED,
  SINK_PAD_ADDED,
  REQUEST_RESOURCE,
  CLOSED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

enum {
  PROP_TP_CALL = 1,
  PROP_GST_BUS,
  PROP_CONTACT
};

/* private structure */

typedef struct {
  gboolean dispose_has_run;
  EmpathyTpCall *call;
  EmpathyContact *contact;
  TfChannel *tfchannel;
} EmpathyCallHandlerPriv;

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyCallHandler)

static void
empathy_call_handler_dispose (GObject *object)
{
  EmpathyCallHandlerPriv *priv = GET_PRIV (object);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->contact != NULL)
    g_object_unref (priv->contact);

  priv->contact = NULL;

  if (priv->tfchannel != NULL)
    g_object_unref (priv->tfchannel);

  priv->tfchannel = NULL;

  if (priv->call != NULL)
    {
      empathy_tp_call_close (priv->call);
      g_object_unref (priv->call);
    }

  priv->call = NULL;

  /* release any references held by the object here */
  if (G_OBJECT_CLASS (empathy_call_handler_parent_class)->dispose)
    G_OBJECT_CLASS (empathy_call_handler_parent_class)->dispose (object);
}

static void
empathy_call_handler_finalize (GObject *object)
{
  /* free any data held directly by the object here */
  if (G_OBJECT_CLASS (empathy_call_handler_parent_class)->finalize)
    G_OBJECT_CLASS (empathy_call_handler_parent_class)->finalize (object);
}

static void
empathy_call_handler_init (EmpathyCallHandler *obj)
{
  EmpathyCallHandlerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (obj,
    EMPATHY_TYPE_CALL_HANDLER, EmpathyCallHandlerPriv);

  obj->priv = priv;
}

static void
empathy_call_handler_set_property (GObject *object,
  guint property_id, const GValue *value, GParamSpec *pspec)
{
  EmpathyCallHandlerPriv *priv = GET_PRIV (object);

  switch (property_id)
    {
      case PROP_CONTACT:
        priv->contact = g_value_dup_object (value);
        break;
      case PROP_TP_CALL:
        priv->call = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
empathy_call_handler_get_property (GObject *object,
  guint property_id, GValue *value, GParamSpec *pspec)
{
  EmpathyCallHandlerPriv *priv = GET_PRIV (object);

  switch (property_id)
    {
      case PROP_CONTACT:
        g_value_set_object (value, priv->contact);
        break;
      case PROP_TP_CALL:
        g_value_set_object (value, priv->call);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}


static void
empathy_call_handler_class_init (EmpathyCallHandlerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (EmpathyCallHandlerPriv));

  object_class->set_property = empathy_call_handler_set_property;
  object_class->get_property = empathy_call_handler_get_property;
  object_class->dispose = empathy_call_handler_dispose;
  object_class->finalize = empathy_call_handler_finalize;

  param_spec = g_param_spec_object ("contact",
    "contact", "The remote contact",
    EMPATHY_TYPE_CONTACT,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONTACT, param_spec);

  param_spec = g_param_spec_object ("tp-call",
    "tp-call", "The calls channel wrapper",
    EMPATHY_TYPE_TP_CALL,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TP_CALL, param_spec);

  signals[CONFERENCE_ADDED] =
    g_signal_new ("conference-added", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE,
      1, FS_TYPE_CONFERENCE);

  signals[SRC_PAD_ADDED] =
    g_signal_new ("src-pad-added", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _empathy_marshal_VOID__OBJECT_UINT,
      G_TYPE_NONE,
      2, GST_TYPE_PAD, G_TYPE_UINT);

  signals[SINK_PAD_ADDED] =
    g_signal_new ("sink-pad-added", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _empathy_marshal_VOID__OBJECT_UINT,
      G_TYPE_NONE,
      2, GST_TYPE_PAD, G_TYPE_UINT);

  signals[REQUEST_RESOURCE] =
    g_signal_new ("request-resource", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0,
      g_signal_accumulator_true_handled, NULL,
      _empathy_marshal_BOOLEAN__UINT_UINT,
      G_TYPE_BOOLEAN, 2, G_TYPE_UINT, G_TYPE_UINT);

  signals[CLOSED] =
    g_signal_new ("closed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE,
      0);
}

EmpathyCallHandler *
empathy_call_handler_new_for_contact (EmpathyContact *contact)
{
  return EMPATHY_CALL_HANDLER (g_object_new (EMPATHY_TYPE_CALL_HANDLER,
    "contact", contact, NULL));
}

EmpathyCallHandler *
empathy_call_handler_new_for_channel (EmpathyTpCall *call)
{
  return EMPATHY_CALL_HANDLER (g_object_new (EMPATHY_TYPE_CALL_HANDLER,
    "tp-call", call, NULL));
}

void
empathy_call_handler_bus_message (EmpathyCallHandler *handler,
  GstBus *bus, GstMessage *message)
{
  EmpathyCallHandlerPriv *priv = GET_PRIV (handler);

  if (priv->tfchannel == NULL)
    return;

  tf_channel_bus_message (priv->tfchannel, message);
}

static void
empathy_call_handler_tf_channel_session_created_cb (TfChannel *tfchannel,
  FsConference *conference, FsParticipant *participant,
  EmpathyCallHandler *self)
{
  g_signal_emit (G_OBJECT (self), signals[CONFERENCE_ADDED], 0,
    GST_ELEMENT (conference));
}

static void
empathy_call_handler_tf_stream_src_pad_added_cb (TfStream *stream,
  GstPad *pad, FsCodec *codec, EmpathyCallHandler  *handler)
{
  guint media_type;

  g_object_get (stream, "media-type", &media_type, NULL);

  g_signal_emit (G_OBJECT (handler), signals[SRC_PAD_ADDED], 0,
    pad, media_type);
}


static gboolean
empathy_call_handler_tf_stream_request_resource_cb (TfStream *stream,
  guint direction, EmpathyTpCall *call)
{
  gboolean ret;
  guint media_type;

  g_object_get (G_OBJECT (stream), "media-type", &media_type, NULL);

  g_signal_emit (G_OBJECT (call),
    signals[REQUEST_RESOURCE], 0, media_type, direction, &ret);

  return ret;
}

static void
empathy_call_handler_tf_channel_stream_created_cb (TfChannel *tfchannel,
  TfStream *stream, EmpathyCallHandler *handler)
{
  guint media_type;
  GstPad *spad;

  g_signal_connect (stream, "src-pad-added",
      G_CALLBACK (empathy_call_handler_tf_stream_src_pad_added_cb), handler);
  g_signal_connect (stream, "request-resource",
      G_CALLBACK (empathy_call_handler_tf_stream_request_resource_cb),
        handler);

  g_object_get (stream, "media-type", &media_type,
    "sink-pad", &spad, NULL);

  g_signal_emit (G_OBJECT (handler), signals[SINK_PAD_ADDED], 0,
    spad, media_type);

  gst_object_unref (spad);
}

static void
empathy_call_handler_tf_channel_closed_cb (TfChannel *tfchannel,
  EmpathyCallHandler *handler)
{
  g_signal_emit (G_OBJECT (handler), signals[CLOSED], 0);
}

static GList *
empathy_call_handler_tf_channel_codec_config_get_defaults (FsCodec *codecs)
{
  GList *l = NULL;
  int i;

  for (i = 0; codecs[i].encoding_name != NULL; i++)
      l = g_list_append (l, fs_codec_copy (codecs + i));

  return l;
}

static GList *
empathy_call_handler_tf_channel_codec_config_cb (TfChannel *channel,
  guint stream_id, FsMediaType media_type, guint direction, gpointer user_data)
{
  FsCodec audio_codecs[] = {
    { FS_CODEC_ID_ANY, "SPEEX", FS_MEDIA_TYPE_AUDIO, 16000, },
    { FS_CODEC_ID_ANY, "SPEEX", FS_MEDIA_TYPE_AUDIO, 8000, },

    { FS_CODEC_ID_DISABLE, "DV",     FS_MEDIA_TYPE_AUDIO, },
    { FS_CODEC_ID_DISABLE, "MPA",    FS_MEDIA_TYPE_AUDIO, },
    { FS_CODEC_ID_DISABLE, "VORBIS", FS_MEDIA_TYPE_AUDIO, },
    { FS_CODEC_ID_DISABLE, "MP3",    FS_MEDIA_TYPE_AUDIO, },
    { 0, NULL, 0,}
  };
  FsCodec video_codecs[] = {
    { FS_CODEC_ID_ANY, "H264",   FS_MEDIA_TYPE_VIDEO, },
    { FS_CODEC_ID_ANY, "THEORA", FS_MEDIA_TYPE_VIDEO, },
    { FS_CODEC_ID_ANY, "H263",   FS_MEDIA_TYPE_VIDEO, },

    { FS_CODEC_ID_DISABLE, "DV",   FS_MEDIA_TYPE_VIDEO, },
    { FS_CODEC_ID_DISABLE, "JPEG", FS_MEDIA_TYPE_VIDEO, },
    { FS_CODEC_ID_DISABLE, "MPV",  FS_MEDIA_TYPE_VIDEO, },
    { 0, NULL, 0}
  };

  switch (media_type)
    {
      case FS_MEDIA_TYPE_AUDIO:
        return empathy_call_handler_tf_channel_codec_config_get_defaults
          (audio_codecs);
      case FS_MEDIA_TYPE_VIDEO:
        return empathy_call_handler_tf_channel_codec_config_get_defaults
          (video_codecs);
    }

  return NULL;
}

static void
empathy_call_handler_start_tpfs (EmpathyCallHandler *self)
{
  EmpathyCallHandlerPriv *priv = GET_PRIV (self);
  TpChannel *channel;

  g_object_get (priv->call, "channel", &channel, NULL);

  g_assert (channel != NULL);

  priv->tfchannel = tf_channel_new (channel);

  /* Set up the telepathy farsight channel */
  g_signal_connect (priv->tfchannel, "session-created",
      G_CALLBACK (empathy_call_handler_tf_channel_session_created_cb), self);
  g_signal_connect (priv->tfchannel, "stream-created",
      G_CALLBACK (empathy_call_handler_tf_channel_stream_created_cb), self);
  g_signal_connect (priv->tfchannel, "closed",
      G_CALLBACK (empathy_call_handler_tf_channel_closed_cb), self);
  g_signal_connect (priv->tfchannel, "stream-get-codec-config",
      G_CALLBACK (empathy_call_handler_tf_channel_codec_config_cb), self);

  g_object_unref (channel);
}

static void
empathy_call_handler_request_cb (EmpathyDispatchOperation *operation,
  const GError *error, gpointer user_data)
{
  EmpathyCallHandler *self = EMPATHY_CALL_HANDLER (user_data);
  EmpathyCallHandlerPriv *priv = GET_PRIV (self);

  if (error != NULL)
    return;

  priv->call = EMPATHY_TP_CALL (
    empathy_dispatch_operation_get_channel_wrapper (operation));

  g_object_ref (priv->call);

  empathy_call_handler_start_tpfs (self);

  empathy_tp_call_to (priv->call, priv->contact);

  empathy_dispatch_operation_claim (operation);
}

static void
empathy_call_handler_contact_ready_cb (EmpathyContact *contact,
  const GError *error, gpointer user_data, GObject *object)
{
  EmpathyCallHandler *self = EMPATHY_CALL_HANDLER (object);
  EmpathyCallHandlerPriv *priv = GET_PRIV (self);
  EmpathyDispatcher *dispatcher;
  McAccount *account;
  GStrv allowed;
  GValue *value;
  GHashTable *request = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);

  g_assert (priv->contact != NULL);

  dispatcher = empathy_dispatcher_dup_singleton ();
  account = empathy_contact_get_account (priv->contact);
  allowed = empathy_dispatcher_find_channel_class (dispatcher, account,
    TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA, TP_HANDLE_TYPE_CONTACT);

  if (!tp_strv_contains ((const gchar * const *) allowed,
      TP_IFACE_CHANNEL ".TargetHandle"))
    return;

  /* org.freedesktop.Telepathy.Channel.ChannelType */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA);
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".ChannelType", value);

  /* org.freedesktop.Telepathy.Channel.TargetHandleType */
  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, TP_HANDLE_TYPE_CONTACT);
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".TargetHandleType", value);

  /* org.freedesktop.Telepathy.Channel.TargetHandle*/
  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, empathy_contact_get_handle (priv->contact));
  g_hash_table_insert (request, TP_IFACE_CHANNEL ".TargetHandle", value);

  empathy_dispatcher_create_channel (dispatcher, account,
    request, empathy_call_handler_request_cb, self);

  g_object_unref (dispatcher);
}

void
empathy_call_handler_start_call (EmpathyCallHandler *handler)
{

  EmpathyCallHandlerPriv *priv = GET_PRIV (handler);

  if (priv->call == NULL)
    {
      empathy_contact_call_when_ready (priv->contact,
        EMPATHY_CONTACT_READY_HANDLE,
        empathy_call_handler_contact_ready_cb, NULL, NULL, G_OBJECT (handler));
    }
  else
    {
      empathy_call_handler_start_tpfs (handler);
      empathy_tp_call_accept_incoming_call (priv->call);
    }
}
