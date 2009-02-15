/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007-2008 Collabora Ltd.
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include <string.h>

#include <telepathy-glib/channel.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/util.h>

#include "empathy-tp-chat.h"
#include "empathy-tp-contact-factory.h"
#include "empathy-contact-monitor.h"
#include "empathy-contact-list.h"
#include "empathy-marshal.h"
#include "empathy-time.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_TP | EMPATHY_DEBUG_CHAT
#include "empathy-debug.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyTpChat)
typedef struct {
	gboolean               dispose_has_run;
	EmpathyTpContactFactory *factory;
	EmpathyContactMonitor *contact_monitor;
	EmpathyContact        *user;
	EmpathyContact        *remote_contact;
	GList                 *members;
	TpChannel             *channel;
	gboolean               listing_pending_messages;
	/* Queue of messages not signalled yet */
	GQueue                *messages_queue;
	/* Queue of messages signalled but not acked yet */
	GQueue                *pending_messages_queue;
	gboolean               had_properties_list;
	GPtrArray             *properties;
	gboolean               ready;
} EmpathyTpChatPriv;

typedef struct {
	gchar          *name;
	guint           id;
	TpPropertyFlags flags;
	GValue         *value;
} TpChatProperty;

static void tp_chat_iface_init         (EmpathyContactListIface *iface);

enum {
	PROP_0,
	PROP_CHANNEL,
	PROP_REMOTE_CONTACT,
	PROP_READY,
};

enum {
	MESSAGE_RECEIVED,
	SEND_ERROR,
	CHAT_STATE_CHANGED,
	PROPERTY_CHANGED,
	DESTROY,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (EmpathyTpChat, empathy_tp_chat, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (EMPATHY_TYPE_CONTACT_LIST,
						tp_chat_iface_init));

static void acknowledge_messages (EmpathyTpChat *chat, GArray *ids);

static void
tp_chat_invalidated_cb (TpProxy       *proxy,
			guint          domain,
			gint           code,
			gchar         *message,
			EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	DEBUG ("Channel invalidated: %s", message);
	g_signal_emit (chat, signals[DESTROY], 0);

	g_object_unref (priv->channel);
	priv->channel = NULL;
}

static void
tp_chat_async_cb (TpChannel *proxy,
		  const GError *error,
		  gpointer user_data,
		  GObject *weak_object)
{
	if (error) {
		DEBUG ("Error %s: %s", (gchar*) user_data, error->message);
	}
}

static void
tp_chat_add (EmpathyContactList *list,
	     EmpathyContact     *contact,
	     const gchar        *message)
{
	EmpathyTpChatPriv *priv = GET_PRIV (list);
	TpHandle           handle;
	GArray             handles = {(gchar *) &handle, 1};

	g_return_if_fail (EMPATHY_IS_TP_CHAT (list));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	handle = empathy_contact_get_handle (contact);
	tp_cli_channel_interface_group_call_add_members (priv->channel, -1,
							 &handles, NULL,
							 NULL, NULL, NULL,
							 NULL);
}

static void
tp_chat_remove (EmpathyContactList *list,
		EmpathyContact     *contact,
		const gchar        *message)
{
	EmpathyTpChatPriv *priv = GET_PRIV (list);
	TpHandle           handle;
	GArray             handles = {(gchar *) &handle, 1};

	g_return_if_fail (EMPATHY_IS_TP_CHAT (list));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	handle = empathy_contact_get_handle (contact);
	tp_cli_channel_interface_group_call_remove_members (priv->channel, -1,
							    &handles, NULL,
							    NULL, NULL, NULL,
							    NULL);
}

static GList *
tp_chat_get_members (EmpathyContactList *list)
{
	EmpathyTpChatPriv *priv = GET_PRIV (list);
	GList             *members = NULL;

	g_return_val_if_fail (EMPATHY_IS_TP_CHAT (list), NULL);

	if (priv->members) {
		members = g_list_copy (priv->members);
		g_list_foreach (members, (GFunc) g_object_ref, NULL);
	} else {
		members = g_list_prepend (members, g_object_ref (priv->user));
		members = g_list_prepend (members, g_object_ref (priv->remote_contact));
	}

	return members;
}

static EmpathyContactMonitor *
tp_chat_get_monitor (EmpathyContactList *list)
{
	EmpathyTpChatPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_CHAT (list), NULL);

	priv = GET_PRIV (list);

	if (priv->contact_monitor == NULL) {
		priv->contact_monitor = empathy_contact_monitor_new_for_iface (list);
	}

	return priv->contact_monitor;
}

static void
tp_chat_emit_queued_messages (EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);
	EmpathyMessage    *message;

	/* Check if we can now emit some queued messages */
	while ((message = g_queue_peek_head (priv->messages_queue)) != NULL) {
		if (empathy_message_get_sender (message) == NULL) {
			break;
		}

		DEBUG ("Queued message ready");
		g_queue_pop_head (priv->messages_queue);
		g_queue_push_tail (priv->pending_messages_queue, message);
		g_signal_emit (chat, signals[MESSAGE_RECEIVED], 0, message);
	}
}

static void
tp_chat_got_sender_cb (EmpathyTpContactFactory *factory,
		       GList                   *contacts,
		       gpointer                 message,
		       GObject                 *chat)
{
	empathy_message_set_sender (message, contacts->data);
	tp_chat_emit_queued_messages (EMPATHY_TP_CHAT (chat));
}

static void
tp_chat_build_message (EmpathyTpChat *chat,
		       guint          id,
		       guint          type,
		       guint          timestamp,
		       guint          from_handle,
		       const gchar   *message_body)
{
	EmpathyTpChatPriv *priv;
	EmpathyMessage    *message;

	priv = GET_PRIV (chat);

	message = empathy_message_new (message_body);
	empathy_message_set_tptype (message, type);
	empathy_message_set_receiver (message, priv->user);
	empathy_message_set_timestamp (message, timestamp);
	empathy_message_set_id (message, id);
	g_queue_push_tail (priv->messages_queue, message);

	if (from_handle == 0) {
		empathy_message_set_sender (message, priv->user);
		tp_chat_emit_queued_messages (chat);
	} else {
		empathy_tp_contact_factory_get_from_handles (priv->factory,
			1, &from_handle,
			tp_chat_got_sender_cb,
			message, NULL, G_OBJECT (chat));
	}
}

static void
tp_chat_received_cb (TpChannel   *channel,
		     guint        message_id,
		     guint        timestamp,
		     guint        from_handle,
		     guint        message_type,
		     guint        message_flags,
		     const gchar *message_body,
		     gpointer     user_data,
		     GObject     *chat_)
{
	EmpathyTpChat *chat = EMPATHY_TP_CHAT (chat_);
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	if (priv->channel == NULL)
		return;

	if (priv->listing_pending_messages) {
		return;
	}
 
 	DEBUG ("Message received: %s", message_body);

	if (message_flags & TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT &&
	    !tp_strdiff (message_body, "")) {
		GArray *ids;

		DEBUG ("Empty message with NonTextContent, ignoring and acking.");

		ids = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
		g_array_append_val (ids, message_id);
		acknowledge_messages (chat, ids);
		g_array_free (ids, TRUE);

		return;
	}

	tp_chat_build_message (chat,
			       message_id,
			       message_type,
			       timestamp,
			       from_handle,
			       message_body);
}

static void
tp_chat_sent_cb (TpChannel   *channel,
		 guint        timestamp,
		 guint        message_type,
		 const gchar *message_body,
		 gpointer     user_data,
		 GObject     *chat_)
{
	EmpathyTpChat *chat = EMPATHY_TP_CHAT (chat_);
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	if (priv->channel == NULL)
		return;

	DEBUG ("Message sent: %s", message_body);

	tp_chat_build_message (chat,
			       0,
			       message_type,
			       timestamp,
			       0,
			       message_body);
}

static void
tp_chat_send_error_cb (TpChannel   *channel,
		       guint        error_code,
		       guint        timestamp,
		       guint        message_type,
		       const gchar *message_body,
		       gpointer     user_data,
		       GObject     *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	if (priv->channel == NULL)
		return;

	DEBUG ("Message sent error: %s (%d)", message_body, error_code);

	tp_chat_build_message (EMPATHY_TP_CHAT (chat),
			       0,
			       message_type,
			       timestamp,
			       0,
			       message_body);
}

static void
tp_chat_send_cb (TpChannel    *proxy,
		 const GError *error,
		 gpointer      user_data,
		 GObject      *chat)
{
	EmpathyMessage *message = EMPATHY_MESSAGE (user_data);

	if (error) {
		DEBUG ("Error: %s", error->message);
		g_signal_emit (chat, signals[SEND_ERROR], 0, message,
			       TP_CHANNEL_TEXT_SEND_ERROR_UNKNOWN);
	}
}

typedef struct {
	EmpathyTpChat *chat;
	TpChannelChatState state;
} StateChangedData;

static void
tp_chat_state_changed_got_contact_cb (EmpathyTpContactFactory *factory,
				      GList                   *contacts,
				      gpointer                 user_data,
				      GObject                 *chat)
{
	EmpathyContact *contact = contacts->data;
	TpChannelChatState state;

	state = GPOINTER_TO_UINT (user_data);
	DEBUG ("Chat state changed for %s (%d): %d",
		empathy_contact_get_name (contact),
		empathy_contact_get_handle (contact), state);

	g_signal_emit (chat, signals[CHAT_STATE_CHANGED], 0, contact, state);
}

static void
tp_chat_state_changed_cb (TpChannel *channel,
			  TpHandle   handle,
			  TpChannelChatState state,
			  gpointer   user_data,
			  GObject   *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	empathy_tp_contact_factory_get_from_handles (priv->factory, 1, &handle,
		tp_chat_state_changed_got_contact_cb, GUINT_TO_POINTER (state),
		NULL, chat);
}

static void
tp_chat_list_pending_messages_cb (TpChannel       *channel,
				  const GPtrArray *messages_list,
				  const GError    *error,
				  gpointer         user_data,
				  GObject         *chat_)
{
	EmpathyTpChat     *chat = EMPATHY_TP_CHAT (chat_);
	EmpathyTpChatPriv *priv = GET_PRIV (chat);
	guint              i;
	GArray            *empty_non_text_content_ids = NULL;

	priv->listing_pending_messages = FALSE;

	if (priv->channel == NULL)
		return;

	if (error) {
		DEBUG ("Error listing pending messages: %s", error->message);
		return;
	}

	for (i = 0; i < messages_list->len; i++) {
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

		DEBUG ("Message pending: %s", message_body);

		if (message_flags & TP_CHANNEL_TEXT_MESSAGE_FLAG_NON_TEXT_CONTENT &&
		    !tp_strdiff (message_body, "")) {
			DEBUG ("Empty message with NonTextContent, ignoring and acking.");

			if (empty_non_text_content_ids == NULL) {
				empty_non_text_content_ids = g_array_new (FALSE, FALSE, sizeof (guint));
			}

			g_array_append_val (empty_non_text_content_ids, message_id);
			continue;
		}

		tp_chat_build_message (chat,
				       message_id,
				       message_type,
				       timestamp,
				       from_handle,
				       message_body);
	}

	if (empty_non_text_content_ids != NULL) {
		acknowledge_messages (chat, empty_non_text_content_ids);
		g_array_free (empty_non_text_content_ids, TRUE);
	}
}

static void
tp_chat_property_flags_changed_cb (TpProxy         *proxy,
				   const GPtrArray *properties,
				   gpointer         user_data,
				   GObject         *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);
	guint              i, j;

	if (priv->channel == NULL)
		return;

	if (!priv->had_properties_list || !properties) {
		return;
	}

	for (i = 0; i < properties->len; i++) {
		GValueArray    *prop_struct;
		TpChatProperty *property;
		guint           id;
		guint           flags;

		prop_struct = g_ptr_array_index (properties, i);
		id = g_value_get_uint (g_value_array_get_nth (prop_struct, 0));
		flags = g_value_get_uint (g_value_array_get_nth (prop_struct, 1));

		for (j = 0; j < priv->properties->len; j++) {
			property = g_ptr_array_index (priv->properties, j);
			if (property->id == id) {
				property->flags = flags;
				DEBUG ("property %s flags changed: %d",
					property->name, property->flags);
				break;
			}
		}
	}
}

static void
tp_chat_properties_changed_cb (TpProxy         *proxy,
			       const GPtrArray *properties,
			       gpointer         user_data,
			       GObject         *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);
	guint              i, j;

	if (priv->channel == NULL)
		return;

	if (!priv->had_properties_list || !properties) {
		return;
	}

	for (i = 0; i < properties->len; i++) {
		GValueArray    *prop_struct;
		TpChatProperty *property;
		guint           id;
		GValue         *src_value;

		prop_struct = g_ptr_array_index (properties, i);
		id = g_value_get_uint (g_value_array_get_nth (prop_struct, 0));
		src_value = g_value_get_boxed (g_value_array_get_nth (prop_struct, 1));

		for (j = 0; j < priv->properties->len; j++) {
			property = g_ptr_array_index (priv->properties, j);
			if (property->id == id) {
				if (property->value) {
					g_value_copy (src_value, property->value);
				} else {
					property->value = tp_g_value_slice_dup (src_value);
				}

				DEBUG ("property %s changed", property->name);
				g_signal_emit (chat, signals[PROPERTY_CHANGED], 0,
					       property->name, property->value);
				break;
			}
		}
	}
}

static void
tp_chat_get_properties_cb (TpProxy         *proxy,
			   const GPtrArray *properties,
			   const GError    *error,
			   gpointer         user_data,
			   GObject         *chat)
{
	if (error) {
		DEBUG ("Error getting properties: %s", error->message);
		return;
	}

	tp_chat_properties_changed_cb (proxy, properties, user_data, chat);
}

static void
tp_chat_list_properties_cb (TpProxy         *proxy,
			    const GPtrArray *properties,
			    const GError    *error,
			    gpointer         user_data,
			    GObject         *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);
	GArray            *ids;
	guint              i;

	if (priv->channel == NULL)
		return;

	priv->had_properties_list = TRUE;

	if (error) {
		DEBUG ("Error listing properties: %s", error->message);
		return;
	}

	ids = g_array_sized_new (FALSE, FALSE, sizeof (guint), properties->len);
	priv->properties = g_ptr_array_sized_new (properties->len);
	for (i = 0; i < properties->len; i++) {
		GValueArray    *prop_struct;
		TpChatProperty *property;

		prop_struct = g_ptr_array_index (properties, i);
		property = g_slice_new0 (TpChatProperty);
		property->id = g_value_get_uint (g_value_array_get_nth (prop_struct, 0));
		property->name = g_value_dup_string (g_value_array_get_nth (prop_struct, 1));
		property->flags = g_value_get_uint (g_value_array_get_nth (prop_struct, 3));

		DEBUG ("Adding property name=%s id=%d flags=%d",
			property->name, property->id, property->flags);
		g_ptr_array_add (priv->properties, property);
		if (property->flags & TP_PROPERTY_FLAG_READ) {
			g_array_append_val (ids, property->id);
		}
	}

	tp_cli_properties_interface_call_get_properties (proxy, -1,
							 ids,
							 tp_chat_get_properties_cb,
							 NULL, NULL,
							 chat);

	g_array_free (ids, TRUE);
}

void
empathy_tp_chat_set_property (EmpathyTpChat *chat,
			      const gchar   *name,
			      const GValue  *value)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);
	TpChatProperty    *property;
	guint              i;

	for (i = 0; i < priv->properties->len; i++) {
		property = g_ptr_array_index (priv->properties, i);
		if (!tp_strdiff (property->name, name)) {
			GPtrArray   *properties;
			GValueArray *prop;
			GValue       id = {0, };
			GValue       dest_value = {0, };

			if (!(property->flags & TP_PROPERTY_FLAG_WRITE)) {
				break;
			}

			g_value_init (&id, G_TYPE_UINT);
			g_value_init (&dest_value, G_TYPE_VALUE);
			g_value_set_uint (&id, property->id);
			g_value_set_boxed (&dest_value, value);

			prop = g_value_array_new (2);
			g_value_array_append (prop, &id);
			g_value_array_append (prop, &dest_value);

			properties = g_ptr_array_sized_new (1);
			g_ptr_array_add (properties, prop);

			DEBUG ("Set property %s", name);
			tp_cli_properties_interface_call_set_properties (priv->channel, -1,
									 properties,
									 (tp_cli_properties_interface_callback_for_set_properties)
									 tp_chat_async_cb,
									 "Seting property", NULL,
									 G_OBJECT (chat));

			g_ptr_array_free (properties, TRUE);
			g_value_array_free (prop);

			break;
		}
	}
}

static void
tp_chat_dispose (GObject *object)
{
	EmpathyTpChat *self = EMPATHY_TP_CHAT (object);
	EmpathyTpChatPriv *priv = GET_PRIV (self);

	if (priv->dispose_has_run)
		return;

	priv->dispose_has_run = TRUE;

	if (priv->channel != NULL) {
		g_signal_handlers_disconnect_by_func (priv->channel,
			tp_chat_invalidated_cb, self);
		g_object_unref (priv->channel);
	}
	priv->channel = NULL;

	if (priv->remote_contact != NULL)
		g_object_unref (priv->remote_contact);
	priv->remote_contact = NULL;

	if (priv->factory != NULL)
		g_object_unref (priv->factory);
	priv->factory = NULL;

	if (priv->user != NULL);
		g_object_unref (priv->user);
	priv->user = NULL;

	if (priv->contact_monitor)
		g_object_unref (priv->contact_monitor);
	priv->contact_monitor = NULL;

	g_queue_foreach (priv->messages_queue, (GFunc) g_object_unref, NULL);
	g_queue_clear (priv->messages_queue);

	g_queue_foreach (priv->pending_messages_queue,
		(GFunc) g_object_unref, NULL);
	g_queue_clear (priv->pending_messages_queue);

	if (G_OBJECT_CLASS (empathy_tp_chat_parent_class)->dispose)
		G_OBJECT_CLASS (empathy_tp_chat_parent_class)->dispose (object);
}

static void
tp_chat_finalize (GObject *object)
{
	EmpathyTpChatPriv *priv = GET_PRIV (object);
	guint              i;

	DEBUG ("Finalize: %p", object);

	if (priv->properties) {
		for (i = 0; i < priv->properties->len; i++) {
			TpChatProperty *property;

			property = g_ptr_array_index (priv->properties, i);
			g_free (property->name);
			if (property->value) {
				tp_g_value_slice_free (property->value);
			}
			g_slice_free (TpChatProperty, property);
		}
		g_ptr_array_free (priv->properties, TRUE);
	}

	g_queue_free (priv->messages_queue);
	g_queue_free (priv->pending_messages_queue);

	G_OBJECT_CLASS (empathy_tp_chat_parent_class)->finalize (object);
}

static void
tp_chat_check_if_ready (EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	if (priv->ready || priv->user == NULL ||
	    (priv->members == NULL && priv->remote_contact == NULL)) {
		return;
	}

	DEBUG ("Ready!");

	priv->listing_pending_messages = TRUE;
	tp_cli_channel_type_text_call_list_pending_messages (priv->channel, -1,
							     FALSE,
							     tp_chat_list_pending_messages_cb,
							     NULL, NULL,
							     G_OBJECT (chat));

	tp_cli_channel_type_text_connect_to_received (priv->channel,
						      tp_chat_received_cb,
						      NULL, NULL,
						      G_OBJECT (chat), NULL);
	tp_cli_channel_type_text_connect_to_sent (priv->channel,
						  tp_chat_sent_cb,
						  NULL, NULL,
						  G_OBJECT (chat), NULL);
	tp_cli_channel_type_text_connect_to_send_error (priv->channel,
							tp_chat_send_error_cb,
							NULL, NULL,
							G_OBJECT (chat), NULL);
	tp_cli_channel_interface_chat_state_connect_to_chat_state_changed (priv->channel,
									   tp_chat_state_changed_cb,
									   NULL, NULL,
									   G_OBJECT (chat), NULL);
	tp_cli_channel_interface_chat_state_connect_to_chat_state_changed (priv->channel,
									   tp_chat_state_changed_cb,
									   NULL, NULL,
									   G_OBJECT (chat), NULL);
	priv->ready = TRUE;
	g_object_notify (G_OBJECT (chat), "ready");
}

static void
tp_chat_update_remote_contact (EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);
	EmpathyContact *contact = NULL;
	TpHandle self_handle;
	TpHandleType handle_type;
	GList *l;

	/* If this is a named chatroom, never pretend it is a private chat */
	tp_channel_get_handle (priv->channel, &handle_type);
	if (handle_type == TP_HANDLE_TYPE_ROOM) {
		return;
	}

	/* This is an MSN-like chat where anyone can join the chat at anytime.
	 * If there is only one non-self contact member, we are in a private
	 * chat and we set the "remote-contact" property to that contact. If
	 * there are more, set the "remote-contact" property to NULL and the
	 * UI will display a contact list. */
	self_handle = tp_channel_group_get_self_handle (priv->channel);
	for (l = priv->members; l; l = l->next) {
		/* Skip self contact if member */
		if (empathy_contact_get_handle (l->data) == self_handle) {
			continue;
		}

		/* We have more than one remote contact, break */
		if (contact != NULL) {
			contact = NULL;
			break;
		}

		/* If we didn't find yet a remote contact, keep this one */
		contact = l->data;
	}

	if (priv->remote_contact == contact) {
		return;
	}

	DEBUG ("Changing remote contact from %p to %p",
		priv->remote_contact, contact);

	if (priv->remote_contact) {
		g_object_unref (priv->remote_contact);
	}

	priv->remote_contact = contact ? g_object_ref (contact) : NULL;
	g_object_notify (G_OBJECT (chat), "remote-contact");
}

static void
tp_chat_got_added_contacts_cb (EmpathyTpContactFactory *factory,
			       GList                   *contacts,
			       gpointer                 user_data,
			       GObject                 *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);
	GList *l;
	const TpIntSet *members;
	TpHandle handle;
	EmpathyContact *contact;

	members = tp_channel_group_get_members (priv->channel);
	for (l = contacts; l; l = l->next) {
		contact = l->data;
		handle = empathy_contact_get_handle (contact);

		/* Make sure the contact is still member */
		if (tp_intset_is_member (members, handle)) {
			priv->members = g_list_prepend (priv->members,
				g_object_ref (contact));
			g_signal_emit_by_name (chat, "members-changed",
					       contact, NULL, 0, NULL, FALSE);
		}
	}

	tp_chat_update_remote_contact (EMPATHY_TP_CHAT (chat));
	tp_chat_check_if_ready (EMPATHY_TP_CHAT (chat));
}

static void
tp_chat_group_members_changed_cb (TpChannel     *self,
				  gchar         *message,
				  GArray        *added,
				  GArray        *removed,
				  GArray        *local_pending,
				  GArray        *remote_pending,
				  guint          actor,
				  guint          reason,
				  EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);
	EmpathyContact *contact;
	TpHandle handle;
	guint i;
	GList *l;

	/* Remove contacts that are not members anymore */
	for (i = 0; i < removed->len; i++) {
		for (l = priv->members; l; l = l->next) {
			contact = l->data;
			handle = empathy_contact_get_handle (contact);
			if (handle == g_array_index (removed, TpHandle, i)) {
				priv->members = g_list_delete_link (priv->members, l);
				g_signal_emit_by_name (chat, "members-changed",
						       contact, actor, reason,
						       message, FALSE);
				g_object_unref (contact);
				break;
			}
		}
	}

	/* Request added contacts */
	if (added->len > 0) {
		empathy_tp_contact_factory_get_from_handles (priv->factory,
			added->len, (TpHandle*) added->data,
			tp_chat_got_added_contacts_cb, NULL, NULL,
			G_OBJECT (chat));
	}

	tp_chat_update_remote_contact (chat);
}

static void
tp_chat_got_remote_contact_cb (EmpathyTpContactFactory *factory,
			       GList *contacts,
			       gpointer user_data,
			       GObject *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	priv->remote_contact = g_object_ref (contacts->data);
	g_object_notify (chat, "remote-contact");

	tp_chat_check_if_ready (EMPATHY_TP_CHAT (chat));
}

static void
tp_chat_got_self_contact_cb (EmpathyTpContactFactory *factory,
			     GList                   *contacts,
			     gpointer                 user_data,
			     GObject                 *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	priv->user = g_object_ref (contacts->data);
	empathy_contact_set_is_user (priv->user, TRUE);
	tp_chat_check_if_ready (EMPATHY_TP_CHAT (chat));
}

static void
tp_chat_get_self_handle_cb (TpConnection *connection,
			    TpHandle self_handle,
			    const GError *error,
			    gpointer user_data,
			    GObject *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	if (error) {
		DEBUG ("Error: %s", error->message);
		tp_cli_channel_call_close (priv->channel, -1,
					   NULL, NULL, NULL, NULL);
		return;
	}

	empathy_tp_contact_factory_get_from_handles (priv->factory,
		1, &self_handle,
		tp_chat_got_self_contact_cb,
		NULL, NULL, chat);
}

static GObject *
tp_chat_constructor (GType                  type,
		     guint                  n_props,
		     GObjectConstructParam *props)
{
	GObject           *chat;
	EmpathyTpChatPriv *priv;
	TpConnection      *connection;
	TpHandle           handle;

	chat = G_OBJECT_CLASS (empathy_tp_chat_parent_class)->constructor (type, n_props, props);

	priv = GET_PRIV (chat);

	connection = tp_channel_borrow_connection (priv->channel);
	priv->factory = empathy_tp_contact_factory_dup_singleton (connection);
	g_signal_connect (priv->channel, "invalidated",
			  G_CALLBACK (tp_chat_invalidated_cb),
			  chat);

	if (tp_proxy_has_interface_by_id (priv->channel,
					  TP_IFACE_QUARK_CHANNEL_INTERFACE_GROUP)) {
		const TpIntSet *members;
		GArray *handles;

		/* Get self contact from the group's self handle */
		handle = tp_channel_group_get_self_handle (priv->channel);
		empathy_tp_contact_factory_get_from_handles (priv->factory,
			1, &handle,
			tp_chat_got_self_contact_cb,
			NULL, NULL, chat);

		/* Get initial member contacts */
		members = tp_channel_group_get_members (priv->channel);
		handles = tp_intset_to_array (members);		
		empathy_tp_contact_factory_get_from_handles (priv->factory,
			handles->len, (TpHandle*) handles->data,
			tp_chat_got_added_contacts_cb, NULL, NULL, chat);

		g_signal_connect_swapped (priv->channel, "group-members-changed",
			G_CALLBACK (tp_chat_group_members_changed_cb), chat);
	} else {
		/* Get the self contact from the connection's self handle */
		tp_cli_connection_call_get_self_handle (connection, -1,
			tp_chat_get_self_handle_cb, NULL, NULL, chat);

		/* Get the remote contact */
		handle = tp_channel_get_handle (priv->channel, NULL);
		empathy_tp_contact_factory_get_from_handles (priv->factory,
			1, &handle, tp_chat_got_remote_contact_cb,
			NULL, NULL, chat);
	}

	if (tp_proxy_has_interface_by_id (priv->channel,
					  TP_IFACE_QUARK_PROPERTIES_INTERFACE)) {
		tp_cli_properties_interface_call_list_properties (priv->channel, -1,
								  tp_chat_list_properties_cb,
								  NULL, NULL,
								  G_OBJECT (chat));
		tp_cli_properties_interface_connect_to_properties_changed (priv->channel,
									   tp_chat_properties_changed_cb,
									   NULL, NULL,
									   G_OBJECT (chat), NULL);
		tp_cli_properties_interface_connect_to_property_flags_changed (priv->channel,
									       tp_chat_property_flags_changed_cb,
									       NULL, NULL,
									       G_OBJECT (chat), NULL);
	}

	return chat;
}

static void
tp_chat_get_property (GObject    *object,
		      guint       param_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	EmpathyTpChatPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_CHANNEL:
		g_value_set_object (value, priv->channel);
		break;
	case PROP_REMOTE_CONTACT:
		g_value_set_object (value, priv->remote_contact);
		break;
	case PROP_READY:
		g_value_set_boolean (value, priv->ready);
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
	EmpathyTpChatPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_CHANNEL:
		priv->channel = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
empathy_tp_chat_class_init (EmpathyTpChatClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = tp_chat_dispose;
	object_class->finalize = tp_chat_finalize;
	object_class->constructor = tp_chat_constructor;
	object_class->get_property = tp_chat_get_property;
	object_class->set_property = tp_chat_set_property;

	g_object_class_install_property (object_class,
					 PROP_CHANNEL,
					 g_param_spec_object ("channel",
							      "telepathy channel",
							      "The text channel for the chat",
							      TP_TYPE_CHANNEL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
					 PROP_REMOTE_CONTACT,
					 g_param_spec_object ("remote-contact",
							      "The remote contact",
							      "The remote contact if there is no group iface on the channel",
							      EMPATHY_TYPE_CONTACT,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_READY,
					 g_param_spec_boolean ("ready",
							       "Is the object ready",
							       "This object can't be used until this becomes true",
							       FALSE,
							       G_PARAM_READABLE));

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
			      _empathy_marshal_VOID__OBJECT_UINT,
			      G_TYPE_NONE,
			      2, EMPATHY_TYPE_MESSAGE, G_TYPE_UINT);

	signals[CHAT_STATE_CHANGED] =
		g_signal_new ("chat-state-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      _empathy_marshal_VOID__OBJECT_UINT,
			      G_TYPE_NONE,
			      2, EMPATHY_TYPE_CONTACT, G_TYPE_UINT);

	signals[PROPERTY_CHANGED] =
		g_signal_new ("property-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      _empathy_marshal_VOID__STRING_BOXED,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_VALUE);

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
	EmpathyTpChatPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (chat,
		EMPATHY_TYPE_TP_CHAT, EmpathyTpChatPriv);

	chat->priv = priv;
	priv->contact_monitor = NULL;
	priv->messages_queue = g_queue_new ();
	priv->pending_messages_queue = g_queue_new ();
}

static void
tp_chat_iface_init (EmpathyContactListIface *iface)
{
	iface->add         = tp_chat_add;
	iface->remove      = tp_chat_remove;
	iface->get_members = tp_chat_get_members;
	iface->get_monitor = tp_chat_get_monitor;
}

EmpathyTpChat *
empathy_tp_chat_new (TpChannel *channel)
{
	return g_object_new (EMPATHY_TYPE_TP_CHAT,
			     "channel", channel,
			     NULL);
}

void
empathy_tp_chat_close (EmpathyTpChat *chat) {
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	/* If there are still messages left, it'll come back..
	   We loose the ordering of sent messages though */
	g_signal_handlers_disconnect_by_func (priv->channel,
		tp_chat_invalidated_cb, chat);

	tp_cli_channel_call_close (priv->channel, -1, tp_chat_async_cb,
		"closing channel", NULL, NULL);

	g_object_unref (priv->channel);
	priv->channel = NULL;

	g_signal_emit (chat, signals[DESTROY], 0);
}

const gchar *
empathy_tp_chat_get_id (EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	g_return_val_if_fail (EMPATHY_IS_TP_CHAT (chat), NULL);

	return tp_channel_get_identifier (priv->channel);
}

EmpathyContact *
empathy_tp_chat_get_remote_contact (EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	g_return_val_if_fail (EMPATHY_IS_TP_CHAT (chat), NULL);
	g_return_val_if_fail (priv->ready, NULL);

	return priv->remote_contact;
}

TpChannel *
empathy_tp_chat_get_channel (EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	g_return_val_if_fail (EMPATHY_IS_TP_CHAT (chat), NULL);

	return priv->channel;
}

gboolean
empathy_tp_chat_is_ready (EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	g_return_val_if_fail (EMPATHY_IS_TP_CHAT (chat), FALSE);

	return priv->ready;
}

void
empathy_tp_chat_send (EmpathyTpChat *chat,
		      EmpathyMessage *message)
{
	EmpathyTpChatPriv        *priv = GET_PRIV (chat);
	const gchar              *message_body;
	TpChannelTextMessageType  message_type;

	g_return_if_fail (EMPATHY_IS_TP_CHAT (chat));
	g_return_if_fail (EMPATHY_IS_MESSAGE (message));
	g_return_if_fail (priv->ready);

	message_body = empathy_message_get_body (message);
	message_type = empathy_message_get_tptype (message);

	DEBUG ("Sending message: %s", message_body);
	tp_cli_channel_type_text_call_send (priv->channel, -1,
					    message_type,
					    message_body,
					    tp_chat_send_cb,
					    g_object_ref (message),
					    (GDestroyNotify) g_object_unref,
					    G_OBJECT (chat));
}

void
empathy_tp_chat_set_state (EmpathyTpChat      *chat,
			   TpChannelChatState  state)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	g_return_if_fail (EMPATHY_IS_TP_CHAT (chat));
	g_return_if_fail (priv->ready);

	DEBUG ("Set state: %d", state);
	tp_cli_channel_interface_chat_state_call_set_chat_state (priv->channel, -1,
								 state,
								 tp_chat_async_cb,
								 "setting chat state",
								 NULL,
								 G_OBJECT (chat));
}


const GList *
empathy_tp_chat_get_pending_messages (EmpathyTpChat *chat)
{
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	g_return_val_if_fail (EMPATHY_IS_TP_CHAT (chat), NULL);
	g_return_val_if_fail (priv->ready, NULL);

	return priv->pending_messages_queue->head;
}

static void
acknowledge_messages (EmpathyTpChat *chat, GArray *ids) {
	EmpathyTpChatPriv *priv = GET_PRIV (chat);

	tp_cli_channel_type_text_call_acknowledge_pending_messages (
		priv->channel, -1, ids, tp_chat_async_cb,
		"acknowledging received message", NULL, G_OBJECT (chat));
}

void
empathy_tp_chat_acknowledge_message (EmpathyTpChat *chat,
				     EmpathyMessage *message) {
	EmpathyTpChatPriv *priv = GET_PRIV (chat);
	GArray *message_ids;
	GList *m;
	guint id;

	g_return_if_fail (EMPATHY_IS_TP_CHAT (chat));
	g_return_if_fail (priv->ready);

	if (empathy_message_get_sender (message) == priv->user)
		goto out;

	message_ids = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);

	id = empathy_message_get_id (message);
	g_array_append_val (message_ids, id);
	acknowledge_messages (chat, message_ids);
	g_array_free (message_ids, TRUE);

out:
	m = g_queue_find (priv->pending_messages_queue, message);
	g_assert (m != NULL);
	g_queue_delete_link (priv->pending_messages_queue, m);
	g_object_unref (message);
}

void
empathy_tp_chat_acknowledge_messages (EmpathyTpChat *chat,
				      const GList *messages) {
	EmpathyTpChatPriv *priv = GET_PRIV (chat);
	/* Copy messages as the messges list (probably is) our own */
	GList *msgs = g_list_copy ((GList *) messages);
	GList *l;
	guint length;
	GArray *message_ids;

	g_return_if_fail (EMPATHY_IS_TP_CHAT (chat));
	g_return_if_fail (priv->ready);

	length = g_list_length ((GList *)messages);

	if (length == 0)
		return;

	message_ids = g_array_sized_new (FALSE, FALSE, sizeof (guint), length);

	for (l = msgs; l != NULL; l = g_list_next (l)) {
		GList *m;

		EmpathyMessage *message = EMPATHY_MESSAGE (l->data);

		m = g_queue_find (priv->pending_messages_queue, message);
		g_assert (m != NULL);
		g_queue_delete_link (priv->pending_messages_queue, m);

		if (empathy_message_get_sender (message) != priv->user) {
			guint id = empathy_message_get_id (message);
			g_array_append_val (message_ids, id);
		}
		g_object_unref (message);
	}

	if (message_ids->len > 0)
		acknowledge_messages (chat, message_ids);

	g_array_free (message_ids, TRUE);
	g_list_free (msgs);
}

