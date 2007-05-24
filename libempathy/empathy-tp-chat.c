/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include <string.h>

#include <libtelepathy/tp-chan-type-text-gen.h>
#include <libtelepathy/tp-chan-iface-chat-state-gen.h>
#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-helpers.h>

#include "empathy-tp-chat.h"
#include "empathy-contact-manager.h"
#include "empathy-tp-contact-list.h"
#include "empathy-marshal.h"
#include "gossip-debug.h"
#include "gossip-time.h"
#include "gossip-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_TP_CHAT, EmpathyTpChatPriv))

#define DEBUG_DOMAIN "TpChat"

struct _EmpathyTpChatPriv {
	EmpathyTpContactList  *list;
	EmpathyContactManager *manager;
	McAccount             *account;
	gchar                 *id;
	MissionControl        *mc;

	TpChan                *tp_chan;
	DBusGProxy            *text_iface;
	DBusGProxy	      *chat_state_iface;
};

static void      empathy_tp_chat_class_init (EmpathyTpChatClass        *klass);
static void      empathy_tp_chat_init       (EmpathyTpChat             *chat);
static void      tp_chat_finalize           (GObject                   *object);
static GObject * tp_chat_constructor        (GType                      type,
					     guint                      n_props,
					     GObjectConstructParam     *props);
static void      tp_chat_get_property       (GObject                   *object,
					     guint                      param_id,
					     GValue                    *value,
					     GParamSpec                *pspec);
static void      tp_chat_set_property       (GObject                   *object,
					     guint                      param_id,
					     const GValue              *value,
					     GParamSpec                *pspec);
static void      tp_chat_destroy_cb         (TpChan                    *text_chan,
					     EmpathyTpChat             *chat);
static void      tp_chat_closed_cb          (TpChan                    *text_chan,
					     EmpathyTpChat             *chat);
static void      tp_chat_received_cb        (DBusGProxy                *text_iface,
					     guint                      message_id,
					     guint                      timestamp,
					     guint                      from_handle,
					     guint                      message_type,
					     guint                      message_flags,
					     gchar                     *message_body,
					     EmpathyTpChat             *chat);
static void      tp_chat_sent_cb            (DBusGProxy                *text_iface,
					     guint                      timestamp,
					     guint                      message_type,
					     gchar                     *message_body,
					     EmpathyTpChat             *chat);
static void      tp_chat_state_changed_cb   (DBusGProxy                *chat_state_iface,
					     guint                      handle,
					     TelepathyChannelChatState  state,
					     EmpathyTpChat             *chat);
static void      tp_chat_emit_message       (EmpathyTpChat             *chat,
					     guint                      type,
					     guint                      timestamp,
					     guint                      from_handle,
					     const gchar               *message_body);
enum {
	PROP_0,
	PROP_ACCOUNT,
	PROP_TP_CHAN
};

enum {
	MESSAGE_RECEIVED,
	CHAT_STATE_CHANGED,
	DESTROY,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyTpChat, empathy_tp_chat, G_TYPE_OBJECT);

static void
empathy_tp_chat_class_init (EmpathyTpChatClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tp_chat_finalize;
	object_class->constructor = tp_chat_constructor;
	object_class->get_property = tp_chat_get_property;
	object_class->set_property = tp_chat_set_property;

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
							      "The text channel for the chat",
							      TELEPATHY_CHAN_TYPE,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	signals[MESSAGE_RECEIVED] =
		g_signal_new ("message-received",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_MESSAGE);

	signals[CHAT_STATE_CHANGED] =
		g_signal_new ("chat-state-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__OBJECT_UINT,
			      G_TYPE_NONE,
			      2, GOSSIP_TYPE_CONTACT, G_TYPE_UINT);

	signals[DESTROY] =
		g_signal_new ("destroy",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_type_class_add_private (object_class, sizeof (EmpathyTpChatPriv));
}

static void
empathy_tp_chat_init (EmpathyTpChat *chat)
{
}


static void
tp_chat_finalize (GObject *object)
{
	EmpathyTpChatPriv *priv;
	EmpathyTpChat     *chat;
	GError            *error = NULL;

	chat = EMPATHY_TP_CHAT (object);
	priv = GET_PRIV (chat);

	if (priv->tp_chan) {
		gossip_debug (DEBUG_DOMAIN, "Closing channel...");

		g_signal_handlers_disconnect_by_func (priv->tp_chan,
						      tp_chat_destroy_cb,
						      object);

		if (!tp_chan_close (DBUS_G_PROXY (priv->tp_chan), &error)) {
			gossip_debug (DEBUG_DOMAIN, 
				      "Error closing text channel: %s",
				      error ? error->message : "No error given");
			g_clear_error (&error);
		}
		g_object_unref (priv->tp_chan);
	}

	if (priv->manager) {
		g_object_unref (priv->manager);
	}
	if (priv->list) {
		g_object_unref (priv->list);
	}
	if (priv->account) {
		g_object_unref (priv->account);
	}
	if (priv->mc) {
		g_object_unref (priv->mc);
	}
	g_free (priv->id);

	G_OBJECT_CLASS (empathy_tp_chat_parent_class)->finalize (object);
}

static GObject *
tp_chat_constructor (GType                  type,
		     guint                  n_props,
		     GObjectConstructParam *props)
{
	GObject           *chat;
	EmpathyTpChatPriv *priv;

	chat = G_OBJECT_CLASS (empathy_tp_chat_parent_class)->constructor (type, n_props, props);

	priv = GET_PRIV (chat);

	priv->manager = empathy_contact_manager_new ();
	priv->list = empathy_contact_manager_get_list (priv->manager, priv->account);
	priv->mc = gossip_mission_control_new ();
	g_object_ref (priv->list);

	priv->text_iface = tp_chan_get_interface (priv->tp_chan,
						  TELEPATHY_CHAN_IFACE_TEXT_QUARK);
	priv->chat_state_iface = tp_chan_get_interface (priv->tp_chan,
							TELEPATHY_CHAN_IFACE_CHAT_STATE_QUARK);

	g_signal_connect (priv->tp_chan, "destroy",
			  G_CALLBACK (tp_chat_destroy_cb),
			  chat);
	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->tp_chan), "Closed",
				     G_CALLBACK (tp_chat_closed_cb),
				     chat, NULL);
	dbus_g_proxy_connect_signal (priv->text_iface, "Received",
				     G_CALLBACK (tp_chat_received_cb),
				     chat, NULL);
	dbus_g_proxy_connect_signal (priv->text_iface, "Sent",
				     G_CALLBACK (tp_chat_sent_cb),
				     chat, NULL);

	if (priv->chat_state_iface != NULL) {
		dbus_g_proxy_connect_signal (priv->chat_state_iface,
					     "ChatStateChanged",
					     G_CALLBACK (tp_chat_state_changed_cb),
					     chat, NULL);
	}

	return chat;
}

static void
tp_chat_get_property (GObject    *object,
		      guint       param_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	EmpathyTpChatPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ACCOUNT:
		g_value_set_object (value, priv->account);
		break;
	case PROP_TP_CHAN:
		g_value_set_object (value, priv->tp_chan);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
tp_chat_set_property (GObject      *object,
		      guint         param_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	EmpathyTpChatPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ACCOUNT:
		priv->account = g_object_ref (g_value_get_object (value));
		break;
	case PROP_TP_CHAN:
		priv->tp_chan = g_object_ref (g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

EmpathyTpChat *
empathy_tp_chat_new (McAccount *account,
		     TpChan    *tp_chan)
{
	return g_object_new (EMPATHY_TYPE_TP_CHAT, 
			     "account", account,
			     "tp-chan", tp_chan,
			     NULL);
}

EmpathyTpChat *
empathy_tp_chat_new_with_contact (GossipContact *contact)
{
	EmpathyTpChat  *chat;
	MissionControl *mc;
	McAccount      *account;
	TpConn         *tp_conn;
	TpChan         *text_chan;
	const gchar    *bus_name;
	guint           handle;

	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	mc = gossip_mission_control_new ();
	account = gossip_contact_get_account (contact);

	if (mission_control_get_connection_status (mc, account, NULL) != 0) {
		/* The account is not connected, nothing to do. */
		return NULL;
	}

	tp_conn = mission_control_get_connection (mc, account, NULL);
	g_return_val_if_fail (tp_conn != NULL, NULL);
	bus_name = dbus_g_proxy_get_bus_name (DBUS_G_PROXY (tp_conn));
	handle = gossip_contact_get_handle (contact);

	text_chan = tp_conn_new_channel (tp_get_bus (),
					 tp_conn,
					 bus_name,
					 TP_IFACE_CHANNEL_TYPE_TEXT,
					 TP_HANDLE_TYPE_CONTACT,
					 handle,
					 TRUE);

	chat = empathy_tp_chat_new (account, text_chan);

	g_object_unref (tp_conn);
	g_object_unref (text_chan);
	g_object_unref (mc);

	return chat;
}

void
empathy_tp_chat_request_pending (EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv;
	GPtrArray         *messages_list;
	guint              i;
	GError            *error = NULL;

	g_return_if_fail (EMPATHY_IS_TP_CHAT (chat));

	priv = GET_PRIV (chat);

	/* If we do this call async, don't forget to ignore Received signal
	 * until we get the answer */
	if (!tp_chan_type_text_list_pending_messages (priv->text_iface,
						      TRUE,
						      &messages_list,
						      &error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Error retrieving pending messages: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		return;
	}

	for (i = 0; i < messages_list->len; i++) {
		GValueArray *message_struct;
		const gchar *message_body;
		guint        message_id;
		guint        timestamp;
		guint        from_handle;
		guint        message_type;
		guint        message_flags;

		message_struct = g_ptr_array_index (messages_list, i);

		message_id = g_value_get_uint (g_value_array_get_nth (message_struct, 0));
		timestamp = g_value_get_uint (g_value_array_get_nth (message_struct, 1));
		from_handle = g_value_get_uint (g_value_array_get_nth (message_struct, 2));
		message_type = g_value_get_uint (g_value_array_get_nth (message_struct, 3));
		message_flags = g_value_get_uint (g_value_array_get_nth (message_struct, 4));
		message_body = g_value_get_string (g_value_array_get_nth (message_struct, 5));

		gossip_debug (DEBUG_DOMAIN, "Message pending: %s", message_body);

		tp_chat_emit_message (chat,
				      message_type,
				      timestamp,
				      from_handle,
				      message_body);

		g_value_array_free (message_struct);
	}

	g_ptr_array_free (messages_list, TRUE);
}

void
empathy_tp_chat_send (EmpathyTpChat *chat,
		      GossipMessage *message)
{
	EmpathyTpChatPriv *priv;
	const gchar       *message_body;
	GossipMessageType  message_type;
	GError            *error = NULL;

	g_return_if_fail (EMPATHY_IS_TP_CHAT (chat));
	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	priv = GET_PRIV (chat);

	message_body = gossip_message_get_body (message);
	message_type = gossip_message_get_type (message);

	gossip_debug (DEBUG_DOMAIN, "Sending message: %s", message_body);
	if (!tp_chan_type_text_send (priv->text_iface,
				     message_type,
				     message_body,
				     &error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Send Error: %s", 
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}
}

void
empathy_tp_chat_set_state (EmpathyTpChat             *chat,
			   TelepathyChannelChatState  state)
{
	EmpathyTpChatPriv *priv;
	GError            *error = NULL;

	g_return_if_fail (EMPATHY_IS_TP_CHAT (chat));

	priv = GET_PRIV (chat);

	if (priv->chat_state_iface) {
		gossip_debug (DEBUG_DOMAIN, "Set state: %d", state);
		if (!tp_chan_iface_chat_state_set_chat_state (priv->chat_state_iface,
							      state,
							      &error)) {
			gossip_debug (DEBUG_DOMAIN,
				      "Set Chat State Error: %s",
				      error ? error->message : "No error given");
			g_clear_error (&error);
		}
	}
}

const gchar *
empathy_tp_chat_get_id (EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	if (priv->id) {
		return priv->id;
	}

	priv->id = empathy_tp_chat_build_id_for_chan (priv->account, priv->tp_chan);

	return priv->id;
}

gchar *
empathy_tp_chat_build_id (McAccount   *account,
			  const gchar *contact_id)
{
	/* A handle name is unique only for a specific account */
	return g_strdup_printf ("%s/%s",
				mc_account_get_unique_name (account),
				contact_id);
}

gchar *
empathy_tp_chat_build_id_for_chan (McAccount *account,
				   TpChan    *tp_chan)
{
	MissionControl *mc;
	TpConn         *tp_conn;
	GArray         *handles;
	gchar         **names;
	gchar          *id;
	GError         *error = NULL;
	
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (TELEPATHY_IS_CHAN (tp_chan), NULL);

	mc = gossip_mission_control_new ();
	tp_conn = mission_control_get_connection (mc, account, NULL);
	g_object_unref (mc);

	/* Get the handle's name */
	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, tp_chan->handle);
	if (!tp_conn_inspect_handles (DBUS_G_PROXY (tp_conn),
				      tp_chan->handle_type,
				      handles,
				      &names,
				      &error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Couldn't get id: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		g_array_free (handles, TRUE);
		g_object_unref (tp_conn);
		
		return NULL;
	}

	id = empathy_tp_chat_build_id (account, *names);

	g_strfreev (names);
	g_object_unref (tp_conn);

	return id;
}

static void
tp_chat_destroy_cb (TpChan        *text_chan,
		    EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv;

	priv = GET_PRIV (chat);

	gossip_debug (DEBUG_DOMAIN, "Channel Closed or CM crashed");

	g_object_unref  (priv->tp_chan);
	priv->tp_chan = NULL;
	priv->text_iface = NULL;
	priv->chat_state_iface = NULL;

	g_signal_emit (chat, signals[DESTROY], 0);
}

static void
tp_chat_closed_cb (TpChan        *text_chan,
		   EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv;

	priv = GET_PRIV (chat);

	/* The channel is closed, do just like if the proxy was destroyed */
	g_signal_handlers_disconnect_by_func (priv->tp_chan,
					      tp_chat_destroy_cb,
					      chat);
	tp_chat_destroy_cb (text_chan, chat);

}

static void
tp_chat_received_cb (DBusGProxy    *text_iface,
		     guint          message_id,
		     guint          timestamp,
		     guint          from_handle,
		     guint          message_type,
		     guint          message_flags,
		     gchar         *message_body,
		     EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv;
	GArray            *message_ids;

	priv = GET_PRIV (chat);

	gossip_debug (DEBUG_DOMAIN, "Message received: %s", message_body);

	tp_chat_emit_message (chat,
			      message_type,
			      timestamp,
			      from_handle,
			      message_body);

	message_ids = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (message_ids, message_id);
	tp_chan_type_text_acknowledge_pending_messages (priv->text_iface,
							message_ids, NULL);
	g_array_free (message_ids, TRUE);
}

static void
tp_chat_sent_cb (DBusGProxy    *text_iface,
		 guint          timestamp,
		 guint          message_type,
		 gchar         *message_body,
		 EmpathyTpChat *chat)
{
	gossip_debug (DEBUG_DOMAIN, "Message sent: %s", message_body);

	tp_chat_emit_message (chat,
			      message_type,
			      timestamp,
			      0,
			      message_body);
}

static void
tp_chat_state_changed_cb (DBusGProxy                *chat_state_iface,
			  guint                      handle,
			  TelepathyChannelChatState  state,
			  EmpathyTpChat             *chat)
{
	EmpathyTpChatPriv *priv;
	GossipContact     *contact;

	priv = GET_PRIV (chat);

	contact = empathy_tp_contact_list_get_from_handle (priv->list, handle);

	gossip_debug (DEBUG_DOMAIN, "Chat state changed for %s (%d): %d",
		      gossip_contact_get_name (contact),
		      handle,
		      state);

	g_signal_emit (chat, signals[CHAT_STATE_CHANGED], 0, contact, state);

	g_object_unref (contact);
}

static void
tp_chat_emit_message (EmpathyTpChat *chat,
		      guint          type,
		      guint          timestamp,
		      guint          from_handle,
		      const gchar   *message_body)
{
	EmpathyTpChatPriv *priv;
	GossipMessage     *message;
	GossipContact     *sender;

	priv = GET_PRIV (chat);

	if (from_handle == 0) {
		sender = empathy_tp_contact_list_get_user (priv->list);
		g_object_ref (sender);
	} else {
		sender = empathy_tp_contact_list_get_from_handle (priv->list,
								  from_handle);
	}

	message = gossip_message_new (message_body);
	gossip_message_set_type (message, type);
	gossip_message_set_sender (message, sender);
	gossip_message_set_timestamp (message, (GossipTime) timestamp);

	g_signal_emit (chat, signals[MESSAGE_RECEIVED], 0, message);

	g_object_unref (message);
	g_object_unref (sender);
}

