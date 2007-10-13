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
#include <libtelepathy/tp-props-iface.h>

#include "empathy-tp-chat.h"
#include "empathy-contact-factory.h"
#include "empathy-marshal.h"
#include "empathy-debug.h"
#include "empathy-time.h"
#include "empathy-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_TP_CHAT, EmpathyTpChatPriv))

#define DEBUG_DOMAIN "TpChat"

struct _EmpathyTpChatPriv {
	EmpathyContactFactory *factory;
	EmpathyContact        *user;
	McAccount             *account;
	gchar                 *id;
	MissionControl        *mc;
	gboolean               acknowledge;

	TpChan                *tp_chan;
	DBusGProxy	      *props_iface;
	DBusGProxy            *text_iface;
	DBusGProxy	      *chat_state_iface;
};

static void             empathy_tp_chat_class_init    (EmpathyTpChatClass        *klass);
static void             empathy_tp_chat_init          (EmpathyTpChat             *chat);
static void             tp_chat_finalize              (GObject                   *object);
static GObject *        tp_chat_constructor           (GType                      type,
						       guint                      n_props,
						       GObjectConstructParam     *props);
static void             tp_chat_get_property          (GObject                   *object,
						       guint                      param_id,
						       GValue                    *value,
						       GParamSpec                *pspec);
static void             tp_chat_set_property          (GObject                   *object,
						       guint                      param_id,
						       const GValue              *value,
						       GParamSpec                *pspec);
static void             tp_chat_destroy_cb            (TpChan                    *text_chan,
						       EmpathyTpChat             *chat);
static void             tp_chat_closed_cb             (TpChan                    *text_chan,
						       EmpathyTpChat             *chat);
static void             tp_chat_received_cb           (DBusGProxy                *text_iface,
						       guint                      message_id,
						       guint                      timestamp,
						       guint                      from_handle,
						       guint                      message_type,
						       guint                      message_flags,
						       gchar                     *message_body,
						       EmpathyTpChat             *chat);
static void             tp_chat_sent_cb               (DBusGProxy                *text_iface,
						       guint                      timestamp,
						       guint                      message_type,
						       gchar                     *message_body,
						       EmpathyTpChat             *chat);
static void             tp_chat_send_error_cb         (DBusGProxy                *text_iface,
						       guint                      error_code,
						       guint                      timestamp,
						       guint                      message_type,
						       gchar                     *message_body,
						       EmpathyTpChat             *chat);
static void             tp_chat_state_changed_cb      (DBusGProxy                *chat_state_iface,
						       guint                      handle,
						       TelepathyChannelChatState  state,
						       EmpathyTpChat             *chat);
static EmpathyMessage * tp_chat_build_message         (EmpathyTpChat             *chat,
						       guint                      type,
						       guint                      timestamp,
						       guint                      from_handle,
						       const gchar               *message_body);
static void             tp_chat_properties_ready_cb   (TpPropsIface              *props_iface,
						       EmpathyTpChat             *chat);
static void             tp_chat_properties_changed_cb (TpPropsIface              *props_iface,
						       guint                      prop_id,
						       TpPropsChanged             flag,
						       EmpathyTpChat             *chat);
enum {
	PROP_0,
	PROP_ACCOUNT,
	PROP_TP_CHAN,
	PROP_ACKNOWLEDGE,

	PROP_ANONYMOUS,
	PROP_INVITE_ONLY,
	PROP_LIMIT,
	PROP_LIMITED,
	PROP_MODERATED,
	PROP_NAME,
	PROP_DESCRIPTION,
	PROP_PASSWORD,
	PROP_PASSWORD_REQUIRED,
	PROP_PERSISTENT,
	PROP_PRIVATE,
	PROP_SUBJECT,
	PROP_SUBJECT_CONTACT,
	PROP_SUBJECT_TIMESTAMP
};

enum {
	MESSAGE_RECEIVED,
	SEND_ERROR,
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
							      "The text channel for the chat",
							      TELEPATHY_CHAN_TYPE,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	/* Normal properties */
	g_object_class_install_property (object_class,
					 PROP_ACKNOWLEDGE,
					 g_param_spec_boolean ("acknowledge",
							       "acknowledge",
							       "acknowledge",
							       FALSE,
							       G_PARAM_READWRITE));

	/* Properties of Text Channel */
	g_object_class_install_property (object_class,
					 PROP_ANONYMOUS,
					 g_param_spec_boolean ("anonymous",
							       "anonymous",
							       "anonymous",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_INVITE_ONLY,
					 g_param_spec_boolean ("invite-only",
							       "invite-only",
							       "invite-only",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_LIMIT,
					 g_param_spec_uint ("limit",
							    "limit",
							    "limit",
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_LIMITED,
					 g_param_spec_boolean ("limited",
							       "limited",
							       "limited",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_MODERATED,
					 g_param_spec_boolean ("moderated",
							       "moderated",
							       "moderated",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "name",
							      "name",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_DESCRIPTION,
					 g_param_spec_string ("description",
							      "description",
							      "description",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_PASSWORD,
					 g_param_spec_string ("password",
							      "password",
							      "password",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_PASSWORD_REQUIRED,
					 g_param_spec_boolean ("password-required",
							       "password-required",
							       "password-required",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_PERSISTENT,
					 g_param_spec_boolean ("persistent",
							       "persistent",
							       "persistent",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_PRIVATE,
					 g_param_spec_boolean ("private",
							       "private",
							       "private"
							       "private",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SUBJECT,
					 g_param_spec_string ("subject",
							      "subject",
							      "subject",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SUBJECT_CONTACT,
					 g_param_spec_uint ("subject-contact",
							    "subject-contact",
							    "subject-contact",
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SUBJECT_TIMESTAMP,
					 g_param_spec_uint ("subject-timestamp",
							    "subject-timestamp",
							    "subject-timestamp",
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READWRITE));

	/* Signals */
	signals[MESSAGE_RECEIVED] =
		g_signal_new ("message-received",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, EMPATHY_TYPE_MESSAGE);

	signals[SEND_ERROR] =
		g_signal_new ("send-error",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__OBJECT_UINT,
			      G_TYPE_NONE,
			      2, EMPATHY_TYPE_MESSAGE, G_TYPE_UINT);

	signals[CHAT_STATE_CHANGED] =
		g_signal_new ("chat-state-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__OBJECT_UINT,
			      G_TYPE_NONE,
			      2, EMPATHY_TYPE_CONTACT, G_TYPE_UINT);

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

	if (priv->text_iface) {
		dbus_g_proxy_disconnect_signal (priv->text_iface, "Received",
						G_CALLBACK (tp_chat_received_cb),
						chat);
		dbus_g_proxy_disconnect_signal (priv->text_iface, "Sent",
						G_CALLBACK (tp_chat_sent_cb),
						chat);
		dbus_g_proxy_disconnect_signal (priv->text_iface, "SendError",
						G_CALLBACK (tp_chat_send_error_cb),
						chat);
	}

	if (priv->chat_state_iface) {
		dbus_g_proxy_disconnect_signal (priv->chat_state_iface, "ChatStateChanged",
						G_CALLBACK (tp_chat_state_changed_cb),
						chat);
	}

	if (priv->tp_chan) {
		g_signal_handlers_disconnect_by_func (priv->tp_chan,
						      tp_chat_destroy_cb,
						      object);
		dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (priv->tp_chan), "Closed",
						G_CALLBACK (tp_chat_closed_cb),
						chat);
		if (priv->acknowledge) {
			empathy_debug (DEBUG_DOMAIN, "Closing channel...");
			if (!tp_chan_close (DBUS_G_PROXY (priv->tp_chan), &error)) {
				empathy_debug (DEBUG_DOMAIN, 
					      "Error closing text channel: %s",
					      error ? error->message : "No error given");
				g_clear_error (&error);
			}
		}
		g_object_unref (priv->tp_chan);
	}

	if (priv->factory) {
		g_object_unref (priv->factory);
	}
	if (priv->user) {
		g_object_unref (priv->user);
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

	priv->factory = empathy_contact_factory_new ();
	priv->user = empathy_contact_factory_get_user (priv->factory, priv->account);
	priv->mc = empathy_mission_control_new ();

	priv->text_iface = tp_chan_get_interface (priv->tp_chan,
						  TELEPATHY_CHAN_IFACE_TEXT_QUARK);
	priv->chat_state_iface = tp_chan_get_interface (priv->tp_chan,
							TELEPATHY_CHAN_IFACE_CHAT_STATE_QUARK);
	priv->props_iface = tp_chan_get_interface (priv->tp_chan,
						   TELEPATHY_PROPS_IFACE_QUARK);

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
	dbus_g_proxy_connect_signal (priv->text_iface, "SendError",
				     G_CALLBACK (tp_chat_send_error_cb),
				     chat, NULL);

	if (priv->chat_state_iface != NULL) {
		dbus_g_proxy_connect_signal (priv->chat_state_iface,
					     "ChatStateChanged",
					     G_CALLBACK (tp_chat_state_changed_cb),
					     chat, NULL);
	}
	if (priv->props_iface != NULL) {
		tp_props_iface_set_mapping (TELEPATHY_PROPS_IFACE (priv->props_iface),
					    "anonymous", PROP_ANONYMOUS,
					    "invite-only", PROP_INVITE_ONLY,
					    "limit", PROP_LIMIT,
					    "limited", PROP_LIMITED,
					    "moderated", PROP_MODERATED,
					    "name", PROP_NAME,
					    "description", PROP_DESCRIPTION,
					    "password", PROP_PASSWORD,
					    "password-required", PROP_PASSWORD_REQUIRED,
					    "persistent", PROP_PERSISTENT,
					    "private", PROP_PRIVATE,
					    "subject", PROP_SUBJECT,
					    "subject-contact", PROP_SUBJECT_CONTACT,
					    "subject-timestamp", PROP_SUBJECT_TIMESTAMP,
					    NULL);
		g_signal_connect (priv->props_iface, "properties-ready",
				  G_CALLBACK (tp_chat_properties_ready_cb),
				  chat);
		g_signal_connect (priv->props_iface, "properties-changed",
				  G_CALLBACK (tp_chat_properties_changed_cb),
				  chat);
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
	EmpathyTpChat     *chat;

	priv = GET_PRIV (object);
	chat = EMPATHY_TP_CHAT (object);

	if (param_id >= PROP_ANONYMOUS &&
	    param_id <= PROP_SUBJECT_TIMESTAMP) {
		if (priv->props_iface) {
			tp_props_iface_get_value (TELEPATHY_PROPS_IFACE (priv->props_iface),
						  param_id,
						  value);
		}

		return;
	}

	switch (param_id) {
	case PROP_ACCOUNT:
		g_value_set_object (value, priv->account);
		break;
	case PROP_TP_CHAN:
		g_value_set_object (value, priv->tp_chan);
		break;
	case PROP_ACKNOWLEDGE:
		g_value_set_boolean (value, priv->acknowledge);
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
	EmpathyTpChat     *chat;

	priv = GET_PRIV (object);
	chat = EMPATHY_TP_CHAT (object);

	if (param_id >= PROP_ANONYMOUS &&
	    param_id <= PROP_SUBJECT_TIMESTAMP) {
		if (priv->props_iface) {
			tp_props_iface_set_value (TELEPATHY_PROPS_IFACE (priv->props_iface),
						  param_id,
						  value);
		}

		return;
	}

	switch (param_id) {
	case PROP_ACCOUNT:
		priv->account = g_object_ref (g_value_get_object (value));
		break;
	case PROP_TP_CHAN:
		priv->tp_chan = g_object_ref (g_value_get_object (value));
		break;
	case PROP_ACKNOWLEDGE:
		empathy_tp_chat_set_acknowledge (EMPATHY_TP_CHAT (object),
						 g_value_get_boolean (value));
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
empathy_tp_chat_new_with_contact (EmpathyContact *contact)
{
	EmpathyTpChat  *chat;
	MissionControl *mc;
	McAccount      *account;
	TpConn         *tp_conn;
	TpChan         *text_chan;
	const gchar    *bus_name;
	guint           handle;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	mc = empathy_mission_control_new ();
	account = empathy_contact_get_account (contact);

	if (mission_control_get_connection_status (mc, account, NULL) != 0) {
		/* The account is not connected, nothing to do. */
		return NULL;
	}

	tp_conn = mission_control_get_connection (mc, account, NULL);
	g_return_val_if_fail (tp_conn != NULL, NULL);
	bus_name = dbus_g_proxy_get_bus_name (DBUS_G_PROXY (tp_conn));
	handle = empathy_contact_get_handle (contact);

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

gboolean
empathy_tp_chat_get_acknowledge (EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_CHAT (chat), FALSE);

	priv = GET_PRIV (chat);

	return priv->acknowledge;
}

void
empathy_tp_chat_set_acknowledge (EmpathyTpChat *chat,
				 gboolean       acknowledge)
{
	EmpathyTpChatPriv *priv;

	g_return_if_fail (EMPATHY_IS_TP_CHAT (chat));

	priv = GET_PRIV (chat);

	priv->acknowledge = acknowledge;
	g_object_notify (G_OBJECT (chat), "acknowledge");
}

TpChan *
empathy_tp_chat_get_channel (EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	return priv->tp_chan;
}

GList *
empathy_tp_chat_get_pendings (EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv;
	GPtrArray         *messages_list;
	guint              i;
	GList             *messages = NULL;
	GError            *error = NULL;

	g_return_val_if_fail (EMPATHY_IS_TP_CHAT (chat), NULL);

	priv = GET_PRIV (chat);

	/* If we do this call async, don't forget to ignore Received signal
	 * until we get the answer */
	if (!tp_chan_type_text_list_pending_messages (priv->text_iface,
						      priv->acknowledge,
						      &messages_list,
						      &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "Error retrieving pending messages: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		return NULL;
	}

	for (i = 0; i < messages_list->len; i++) {
		EmpathyMessage *message;
		GValueArray    *message_struct;
		const gchar    *message_body;
		guint           message_id;
		guint           timestamp;
		guint           from_handle;
		guint           message_type;
		guint           message_flags;

		message_struct = g_ptr_array_index (messages_list, i);

		message_id = g_value_get_uint (g_value_array_get_nth (message_struct, 0));
		timestamp = g_value_get_uint (g_value_array_get_nth (message_struct, 1));
		from_handle = g_value_get_uint (g_value_array_get_nth (message_struct, 2));
		message_type = g_value_get_uint (g_value_array_get_nth (message_struct, 3));
		message_flags = g_value_get_uint (g_value_array_get_nth (message_struct, 4));
		message_body = g_value_get_string (g_value_array_get_nth (message_struct, 5));

		empathy_debug (DEBUG_DOMAIN, "Message pending: %s", message_body);

		message = tp_chat_build_message (chat,
						 message_type,
						 timestamp,
						 from_handle,
						 message_body);

		messages = g_list_prepend (messages, message);

		g_value_array_free (message_struct);
	}
	messages = g_list_reverse (messages);

	g_ptr_array_free (messages_list, TRUE);

	return messages;
}

void
empathy_tp_chat_send (EmpathyTpChat *chat,
		      EmpathyMessage *message)
{
	EmpathyTpChatPriv *priv;
	const gchar       *message_body;
	EmpathyMessageType  message_type;
	GError            *error = NULL;

	g_return_if_fail (EMPATHY_IS_TP_CHAT (chat));
	g_return_if_fail (EMPATHY_IS_MESSAGE (message));

	priv = GET_PRIV (chat);

	message_body = empathy_message_get_body (message);
	message_type = empathy_message_get_type (message);

	empathy_debug (DEBUG_DOMAIN, "Sending message: %s", message_body);
	if (!tp_chan_type_text_send (priv->text_iface,
				     message_type,
				     message_body,
				     &error)) {
		empathy_debug (DEBUG_DOMAIN, 
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
		empathy_debug (DEBUG_DOMAIN, "Set state: %d", state);
		if (!tp_chan_iface_chat_state_set_chat_state (priv->chat_state_iface,
							      state,
							      &error)) {
			empathy_debug (DEBUG_DOMAIN,
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

	if (!priv->id) {
		priv->id = empathy_inspect_channel (priv->account, priv->tp_chan);
	}

	return priv->id;
}

static void
tp_chat_destroy_cb (TpChan        *text_chan,
		    EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv;

	priv = GET_PRIV (chat);

	empathy_debug (DEBUG_DOMAIN, "Channel Closed or CM crashed");

	g_object_unref  (priv->tp_chan);
	priv->tp_chan = NULL;
	priv->text_iface = NULL;
	priv->chat_state_iface = NULL;
	priv->props_iface = NULL;

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
	EmpathyMessage    *message;

	priv = GET_PRIV (chat);

	empathy_debug (DEBUG_DOMAIN, "Message received: %s", message_body);

	message = tp_chat_build_message (chat,
					 message_type,
					 timestamp,
					 from_handle,
					 message_body);

	g_signal_emit (chat, signals[MESSAGE_RECEIVED], 0, message);
	g_object_unref (message);

	if (priv->acknowledge) {
		GArray *message_ids;

		message_ids = g_array_new (FALSE, FALSE, sizeof (guint));
		g_array_append_val (message_ids, message_id);
		tp_chan_type_text_acknowledge_pending_messages (priv->text_iface,
								message_ids, NULL);
		g_array_free (message_ids, TRUE);
	}
}

static void
tp_chat_sent_cb (DBusGProxy    *text_iface,
		 guint          timestamp,
		 guint          message_type,
		 gchar         *message_body,
		 EmpathyTpChat *chat)
{
	EmpathyMessage *message;

	empathy_debug (DEBUG_DOMAIN, "Message sent: %s", message_body);

	message = tp_chat_build_message (chat,
					 message_type,
					 timestamp,
					 0,
					 message_body);

	g_signal_emit (chat, signals[MESSAGE_RECEIVED], 0, message);
	g_object_unref (message);
}

static void
tp_chat_send_error_cb (DBusGProxy    *text_iface,
		       guint          error_code,
		       guint          timestamp,
		       guint          message_type,
		       gchar         *message_body,
		       EmpathyTpChat *chat)
{
	EmpathyMessage *message;

	empathy_debug (DEBUG_DOMAIN, "Message sent error: %s (%d)",
		       message_body, error_code);

	message = tp_chat_build_message (chat,
					 message_type,
					 timestamp,
					 0,
					 message_body);

	g_signal_emit (chat, signals[SEND_ERROR], 0, message, error_code);
	g_object_unref (message);
}

static void
tp_chat_state_changed_cb (DBusGProxy                *chat_state_iface,
			  guint                      handle,
			  TelepathyChannelChatState  state,
			  EmpathyTpChat             *chat)
{
	EmpathyTpChatPriv *priv;
	EmpathyContact     *contact;

	priv = GET_PRIV (chat);

	contact = empathy_contact_factory_get_from_handle (priv->factory,
							   priv->account,
							   handle);

	empathy_debug (DEBUG_DOMAIN, "Chat state changed for %s (%d): %d",
		      empathy_contact_get_name (contact),
		      handle,
		      state);

	g_signal_emit (chat, signals[CHAT_STATE_CHANGED], 0, contact, state);
	g_object_unref (contact);
}

static EmpathyMessage *
tp_chat_build_message (EmpathyTpChat *chat,
		       guint          type,
		       guint          timestamp,
		       guint          from_handle,
		       const gchar   *message_body)
{
	EmpathyTpChatPriv *priv;
	EmpathyMessage    *message;
	EmpathyContact    *sender;

	priv = GET_PRIV (chat);

	if (from_handle == 0) {
		sender = g_object_ref (priv->user);
	} else {
		sender = empathy_contact_factory_get_from_handle (priv->factory,
								  priv->account,
								  from_handle);
	}

	message = empathy_message_new (message_body);
	empathy_message_set_type (message, type);
	empathy_message_set_sender (message, sender);
	empathy_message_set_receiver (message, priv->user);
	empathy_message_set_timestamp (message, timestamp);

	g_object_unref (sender);

	return message;
}

static void
tp_chat_properties_ready_cb (TpPropsIface  *props_iface,
			     EmpathyTpChat *chat)
{
	g_object_notify (G_OBJECT (chat), "anonymous");
	g_object_notify (G_OBJECT (chat), "invite-only");
	g_object_notify (G_OBJECT (chat), "limit");
	g_object_notify (G_OBJECT (chat), "limited");
	g_object_notify (G_OBJECT (chat), "moderated");
	g_object_notify (G_OBJECT (chat), "name");
	g_object_notify (G_OBJECT (chat), "description");
	g_object_notify (G_OBJECT (chat), "password");
	g_object_notify (G_OBJECT (chat), "password-required");
	g_object_notify (G_OBJECT (chat), "persistent");
	g_object_notify (G_OBJECT (chat), "private");
	g_object_notify (G_OBJECT (chat), "subject");
	g_object_notify (G_OBJECT (chat), "subject-contact");
	g_object_notify (G_OBJECT (chat), "subject-timestamp");
}

static void
tp_chat_properties_changed_cb (TpPropsIface   *props_iface,
			       guint           prop_id,
			       TpPropsChanged  flag,
			       EmpathyTpChat  *chat)
{
	switch (prop_id) {
	case PROP_ANONYMOUS:
		g_object_notify (G_OBJECT (chat), "anonymous");
		break;
	case PROP_INVITE_ONLY:
		g_object_notify (G_OBJECT (chat), "invite-only");
		break;
	case PROP_LIMIT:
		g_object_notify (G_OBJECT (chat), "limit");
		break;
	case PROP_LIMITED:
		g_object_notify (G_OBJECT (chat), "limited");
		break;
	case PROP_MODERATED:
		g_object_notify (G_OBJECT (chat), "moderated");
		break;
	case PROP_NAME:
		g_object_notify (G_OBJECT (chat), "name");
		break;
	case PROP_DESCRIPTION:
		g_object_notify (G_OBJECT (chat), "description");
		break;
	case PROP_PASSWORD:
		g_object_notify (G_OBJECT (chat), "password");
		break;
	case PROP_PASSWORD_REQUIRED:
		g_object_notify (G_OBJECT (chat), "password-required");
		break;
	case PROP_PERSISTENT:
		g_object_notify (G_OBJECT (chat), "persistent");
		break;
	case PROP_PRIVATE:
		g_object_notify (G_OBJECT (chat), "private");
		break;
	case PROP_SUBJECT:
		g_object_notify (G_OBJECT (chat), "subject");
		break;
	case PROP_SUBJECT_CONTACT:
		g_object_notify (G_OBJECT (chat), "subject-contact");
		break;
	case PROP_SUBJECT_TIMESTAMP:
		g_object_notify (G_OBJECT (chat), "subject-timestamp");
		break;
	}
}

