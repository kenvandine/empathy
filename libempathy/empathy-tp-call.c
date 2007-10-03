/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Elliot Fairweather
 * Copyright (C) 2007 Collabora Ltd.
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
 * Authors: Elliot Fairweather <elliot.fairweather@collabora.co.uk>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <libtelepathy/tp-chan-type-streamed-media-gen.h>
#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-conn.h>

#include <libmissioncontrol/mission-control.h>

#include "empathy-tp-call.h"
#include "empathy-tp-group.h"
#include "empathy-utils.h"
#include "empathy-debug.h"
#include "empathy-enum-types.h"
#include "tp-stream-engine-gen.h"

#define DEBUG_DOMAIN "TpCall"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EMPATHY_TYPE_TP_CALL, EmpathyTpCallPriv))

#define STREAM_ENGINE_BUS_NAME "org.freedesktop.Telepathy.StreamEngine"
#define STREAM_ENGINE_OBJECT_PATH "/org/freedesktop/Telepathy/StreamEngine"
#define STREAM_ENGINE_INTERFACE "org.freedesktop.Telepathy.StreamEngine"
#define CHANNEL_HANDLER_INTERFACE "org.freedesktop.Telepathy.ChannelHandler"

typedef struct _EmpathyTpCallPriv EmpathyTpCallPriv;

struct _EmpathyTpCallPriv {
	TpChan              *tp_chan;
	DBusGProxy          *streamed_iface;
	DBusGProxy          *se_ch_proxy;
	DBusGProxy          *se_proxy;
	McAccount           *account;
	EmpathyTpGroup      *group;
  	EmpathyContact      *contact;
	EmpathyTpCallStatus  status;
	gboolean             is_incoming;
	guint                audio_stream;
	guint                video_stream;
};

static void empathy_tp_call_class_init (EmpathyTpCallClass *klass);
static void empathy_tp_call_init       (EmpathyTpCall      *call);

enum {
	PROP_0,
	PROP_ACCOUNT,
	PROP_TP_CHAN,
	PROP_STATUS
};

enum {
	DESTROY,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyTpCall, empathy_tp_call, G_TYPE_OBJECT)

static void
tp_call_set_status (EmpathyTpCall       *call,
		    EmpathyTpCallStatus  status)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	priv->status = status;
	g_object_notify (G_OBJECT (call), "status");
}

static void
tp_call_set_property (GObject      *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	EmpathyTpCallPriv *priv = GET_PRIV (object);

	switch (prop_id) {
	case PROP_ACCOUNT:
        	priv->account = g_object_ref (g_value_get_object (value));
		break;
	case PROP_TP_CHAN:
        	priv->tp_chan = g_object_ref (g_value_get_object (value));
		break;
	case PROP_STATUS:
		tp_call_set_status (EMPATHY_TP_CALL (object),
				    g_value_get_enum (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tp_call_get_property (GObject    *object,
		      guint       prop_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
  EmpathyTpCallPriv *priv = GET_PRIV (object);

	switch (prop_id) {
	case PROP_ACCOUNT:
        	g_value_set_object (value, priv->account);
		break;
	case PROP_TP_CHAN:
        	g_value_set_object (value, priv->tp_chan);
		break;
	case PROP_STATUS:
		g_value_set_enum (value, priv->status);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tp_call_destroy_cb (TpChan        *call_chan,
		    EmpathyTpCall *call)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	empathy_debug (DEBUG_DOMAIN, "Channel Closed or CM crashed");

	g_object_unref  (priv->tp_chan);
	priv->tp_chan = NULL;
	priv->streamed_iface = NULL;

	g_signal_emit (call, signals[DESTROY], 0);
}

static void
tp_call_closed_cb (TpChan        *call_chan,
		   EmpathyTpCall *call)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	/* The channel is closed, do just like if the proxy was destroyed */
	g_signal_handlers_disconnect_by_func (priv->tp_chan,
					      tp_call_destroy_cb,
					      call);
	tp_call_destroy_cb (call_chan, call);
}

static void
tp_call_stream_added_cb (DBusGProxy    *streamed_iface,
			 guint          stream_id,
			 guint          contact_handle,
			 guint          stream_type,
			 EmpathyTpCall *call)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	empathy_debug (DEBUG_DOMAIN, "Stream added: id=%d, stream_type=%d",
		       stream_id, stream_type);

	switch (stream_type) {
	case TP_MEDIA_STREAM_TYPE_AUDIO:
		priv->audio_stream = stream_id;
		break;
	case TP_MEDIA_STREAM_TYPE_VIDEO:
		priv->video_stream = stream_id;
		break;
	default:
		empathy_debug (DEBUG_DOMAIN, "Unknown stream type: %d", stream_type);
	}
}


static void
tp_call_stream_removed_cb (DBusGProxy    *streamed_iface,
			   guint          stream_id,
			   EmpathyTpCall *call)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	empathy_debug (DEBUG_DOMAIN, "Stream removed: %d", stream_id);

	if (stream_id == priv->audio_stream) {
		priv->audio_stream = 0;
	}
	else if (stream_id == priv->video_stream) {
		priv->video_stream = 0;
	}
}

static void
tp_call_list_streams_cb (DBusGProxy *proxy,
			 GPtrArray  *streams,
			 GError     *error,
			 gpointer    user_data)
{
	guint i;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Failed to list streams: %s",
			       error->message);
		return;
	}

	for (i = 0; i < streams->len; i++) {
		GValueArray *values;
		guint        stream_id;
		guint        contact_handle;
		guint        stream_type;

		values = g_ptr_array_index (streams, i);
		stream_id = g_value_get_uint (g_value_array_get_nth (values, 0));
		contact_handle = g_value_get_uint (g_value_array_get_nth (values, 1));
		stream_type = g_value_get_uint (g_value_array_get_nth (values, 2));

		tp_call_stream_added_cb (proxy,
					 stream_id,
					 contact_handle,
					 stream_type,
					 user_data);
	}
}

static void
tp_call_member_added_cb (EmpathyTpGroup *group,
			 EmpathyContact *contact,
			 EmpathyContact *actor,
			 guint           reason,
			 const gchar    *message,
			 EmpathyTpCall  *call)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	empathy_debug (DEBUG_DOMAIN, "Members added %s (%d)",
		       empathy_contact_get_id (contact),
		       empathy_contact_get_handle (contact));

	if (!priv->contact) {
		if (!empathy_contact_is_user (contact)) {
			priv->is_incoming = TRUE;
			priv->contact = g_object_ref (contact);
			tp_call_set_status (call, EMPATHY_TP_CALL_STATUS_RINGING);
		}
		return;
	}

	/* We already have the other contact, that means we now have 2 members,
	 * so we can start the call */
	tp_call_set_status (call, EMPATHY_TP_CALL_STATUS_RUNNING);
}

static void
tp_call_remote_pending_cb (EmpathyTpGroup *group,
			   EmpathyContact *contact,
			   EmpathyContact *actor,
			   guint           reason,
			   const gchar    *message,
			   EmpathyTpCall  *call)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	empathy_debug (DEBUG_DOMAIN, "Remote pending: %s (%d)",
		       empathy_contact_get_id (contact),
		       empathy_contact_get_handle (contact));

	if (!priv->contact) {
		priv->is_incoming = FALSE;
		priv->contact = g_object_ref (contact);
		tp_call_set_status (call, EMPATHY_TP_CALL_STATUS_RINGING);
	}
}

static void
tp_call_async_cb (DBusGProxy *proxy,
		  GError     *error,
		  gpointer    user_data)
{
	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Failed to %s: %s",
			       user_data,
			       error->message);
	}
}

static GObject *
tp_call_constructor (GType                  type,
		     guint                  n_props,
		     GObjectConstructParam *props)
{
	GObject           *call;
	EmpathyTpCallPriv *priv;
	TpConn            *tp_conn;
	MissionControl    *mc;

	call = G_OBJECT_CLASS (empathy_tp_call_parent_class)->constructor (type, n_props, props);
	priv = GET_PRIV (call);

	priv->group = empathy_tp_group_new (priv->account, priv->tp_chan);
	priv->streamed_iface = tp_chan_get_interface (priv->tp_chan,
						      TELEPATHY_CHAN_IFACE_STREAMED_QUARK);

	/* Connect signals */
	dbus_g_proxy_connect_signal (priv->streamed_iface, "StreamAdded",
				     G_CALLBACK (tp_call_stream_added_cb),
				     call, NULL);
	dbus_g_proxy_connect_signal (priv->streamed_iface, "StreamRemoved",
				     G_CALLBACK (tp_call_stream_removed_cb), 
				     call, NULL);
	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->tp_chan), "Closed",
				     G_CALLBACK (tp_call_closed_cb),
				     call, NULL);
	g_signal_connect (priv->tp_chan, "destroy",
			  G_CALLBACK (tp_call_destroy_cb),
			  call);
	g_signal_connect (priv->group, "member-added",
			  G_CALLBACK (tp_call_member_added_cb),
			  call);
	g_signal_connect (priv->group, "remote-pending",
			  G_CALLBACK (tp_call_remote_pending_cb),
			  call);

	/* Start stream engine */
	mc = empathy_mission_control_new ();
	tp_conn = mission_control_get_connection (mc, priv->account, NULL);
	priv->se_ch_proxy = dbus_g_proxy_new_for_name (tp_get_bus (),
						       STREAM_ENGINE_BUS_NAME,
						       STREAM_ENGINE_OBJECT_PATH,
						       CHANNEL_HANDLER_INTERFACE);
	priv->se_proxy = dbus_g_proxy_new_for_name (tp_get_bus (),
						    STREAM_ENGINE_BUS_NAME,
						    STREAM_ENGINE_OBJECT_PATH,
						    STREAM_ENGINE_INTERFACE);
	org_freedesktop_Telepathy_ChannelHandler_handle_channel_async (priv->se_ch_proxy,
		dbus_g_proxy_get_bus_name (DBUS_G_PROXY (tp_conn)),
		dbus_g_proxy_get_path (DBUS_G_PROXY (tp_conn)),
		priv->tp_chan->type,
		dbus_g_proxy_get_path (DBUS_G_PROXY (priv->tp_chan)),
		priv->tp_chan->handle_type,
		priv->tp_chan->handle,
		tp_call_async_cb,
		"handle channel");
	g_object_unref (tp_conn);
	g_object_unref (mc);

	/* Get streams */
	tp_chan_type_streamed_media_list_streams_async (priv->streamed_iface,
							tp_call_list_streams_cb,
							call);

	return call;
}

static void
tp_call_finalize (GObject *object)
{
	EmpathyTpCallPriv *priv = GET_PRIV (object);

	empathy_debug (DEBUG_DOMAIN, "Finalizing: %p", object);

	if (priv->tp_chan) {
		GError *error = NULL;

		g_signal_handlers_disconnect_by_func (priv->tp_chan,
						      tp_call_destroy_cb,
						      object);
		empathy_debug (DEBUG_DOMAIN, "Closing channel...");
		if (!tp_chan_close (DBUS_G_PROXY (priv->tp_chan), &error)) {
			empathy_debug (DEBUG_DOMAIN, 
				      "Error closing text channel: %s",
				      error ? error->message : "No error given");
			g_clear_error (&error);
		}
		g_object_unref (priv->tp_chan);
	}

	g_object_unref (priv->group);
	g_object_unref (priv->contact);
	g_object_unref (priv->account);
	g_object_unref (priv->se_ch_proxy);
	g_object_unref (priv->se_proxy);

	G_OBJECT_CLASS (empathy_tp_call_parent_class)->finalize (object);
}

static void
empathy_tp_call_class_init (EmpathyTpCallClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->constructor = tp_call_constructor;
	object_class->finalize = tp_call_finalize;
	object_class->set_property = tp_call_set_property;
	object_class->get_property = tp_call_get_property;

	/* Construct-only properties */
	g_object_class_install_property (object_class,
					 PROP_ACCOUNT,
					 g_param_spec_object ("account",
							      "channel Account",
							      "The account associated with the channel",
							      MC_TYPE_ACCOUNT,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_TP_CHAN,
					 g_param_spec_object ("tp-chan",
							      "telepathy channel",
							      "The media channel for the call",
							      TELEPATHY_CHAN_TYPE,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	/* Normal properties */
	g_object_class_install_property (object_class,
					 PROP_STATUS,
					 g_param_spec_enum ("status",
							    "call status",
							    "The status of the call",
							    EMPATHY_TYPE_TP_CALL_STATUS,
							    EMPATHY_TP_CALL_STATUS_PREPARING,
							    G_PARAM_READABLE));

	/* Signals */
	signals[DESTROY] =
		g_signal_new ("destroy",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);


	g_type_class_add_private (klass, sizeof (EmpathyTpCallPriv));
}

static void
empathy_tp_call_init (EmpathyTpCall *call)
{
}

EmpathyTpCall *
empathy_tp_call_new (McAccount *account, TpChan *channel)
{
	return g_object_new (EMPATHY_TYPE_TP_CALL,
			     "account", account,
			     "tp_chan", channel,
			     NULL);
}

gboolean
empathy_tp_call_is_incoming (EmpathyTpCall *call)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	return priv->is_incoming;
}

EmpathyTpCallStatus
empathy_tp_call_get_status (EmpathyTpCall *call)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	return priv->status;
}

EmpathyContact *
empathy_tp_call_get_contact (EmpathyTpCall *call)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	return priv->contact;
}

void
empathy_tp_call_accept (EmpathyTpCall *call)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);
	EmpathyContact    *contact;

	contact = empathy_tp_group_get_self_contact (priv->group);
	empathy_tp_group_add_member (priv->group, contact, "");
}

void
empathy_tp_call_invite (EmpathyTpCall  *call,
			EmpathyContact *contact)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	empathy_tp_group_add_member (priv->group, contact, "you're welcome");
}

void
empathy_tp_call_request_streams (EmpathyTpCall *call,
				 gboolean       audio,
				 gboolean       video)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);
	GArray            *stream_types;
	guint              handle;
	guint              type;

	empathy_debug (DEBUG_DOMAIN, "Requesting streams for audio=%s video=%s",
		       audio ? "Yes" : "No",
		       video ? "Yes" : "No");

	stream_types = g_array_new (FALSE, FALSE, sizeof (guint));
	if (audio) {
		type = TP_MEDIA_STREAM_TYPE_AUDIO;
		g_array_append_val (stream_types, type);
	}
	if (video) {
		type = TP_MEDIA_STREAM_TYPE_VIDEO;
		g_array_append_val (stream_types, type);
	}

	handle = empathy_contact_get_handle (priv->contact);
	tp_chan_type_streamed_media_request_streams_async (priv->streamed_iface,
							   handle,
							   stream_types,
							   tp_call_list_streams_cb,
							   call);

	g_array_free (stream_types, TRUE);
}

void
empathy_tp_call_send_video (EmpathyTpCall *call,
			    gboolean       send)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);
	guint              new_direction;

	if (!priv->video_stream) {
		return;
	}

	if (send) {
		new_direction = TP_MEDIA_STREAM_DIRECTION_BIDIRECTIONAL;
	} else {
		new_direction = TP_MEDIA_STREAM_DIRECTION_RECEIVE;
	}

	tp_chan_type_streamed_media_request_stream_direction_async (priv->streamed_iface,
								    priv->video_stream,
								    new_direction,
								    tp_call_async_cb,
								    "request stream direction");
}

void
empathy_tp_call_add_preview_window (EmpathyTpCall *call,
				    guint          socket_id)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	org_freedesktop_Telepathy_StreamEngine_add_preview_window_async (priv->se_proxy,
									 socket_id,
									 tp_call_async_cb,
									 "add preview window");
}

void
empathy_tp_call_remove_preview_window (EmpathyTpCall *call,
				       guint          socket_id)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	org_freedesktop_Telepathy_StreamEngine_remove_preview_window_async (priv->se_proxy,
									    socket_id,
									    tp_call_async_cb,
									    "remove preview window");
}

void
empathy_tp_call_set_output_window (EmpathyTpCall *call,
				   guint          socket_id)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	org_freedesktop_Telepathy_StreamEngine_set_output_window_async (priv->se_proxy,
									dbus_g_proxy_get_path (DBUS_G_PROXY (priv->tp_chan)),
									priv->video_stream,
									socket_id,
									tp_call_async_cb,
									"set output window");
}

void
empathy_tp_call_set_output_volume (EmpathyTpCall *call,
				   guint          volume)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	org_freedesktop_Telepathy_StreamEngine_set_output_volume_async (priv->se_proxy,
									dbus_g_proxy_get_path (DBUS_G_PROXY (priv->tp_chan)),
									priv->audio_stream,
									volume,
									tp_call_async_cb,
									"set output volume");
}


void
empathy_tp_call_mute_output (EmpathyTpCall *call,
                             gboolean       is_muted)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	org_freedesktop_Telepathy_StreamEngine_mute_output_async (priv->se_proxy,
								  dbus_g_proxy_get_path (DBUS_G_PROXY (priv->tp_chan)),
								  priv->audio_stream,
								  is_muted,
								  tp_call_async_cb,
								  "mute output");
}


void
empathy_tp_call_mute_input (EmpathyTpCall *call,
                            gboolean       is_muted)
{
	EmpathyTpCallPriv *priv = GET_PRIV (call);

	org_freedesktop_Telepathy_StreamEngine_mute_input_async (priv->se_proxy,
								 dbus_g_proxy_get_path (DBUS_G_PROXY (priv->tp_chan)),
								 priv->audio_stream,
								 is_muted,
								 tp_call_async_cb,
								 "mute output");
}

