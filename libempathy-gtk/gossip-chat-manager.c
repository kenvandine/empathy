/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>

#include <libgossip/gossip-account.h>
#include <libgossip/gossip-event.h>
#include <libgossip/gossip-session.h>
#include <libgossip/gossip-message.h>
#include <libgossip/gossip-debug.h>
#include <libgossip/gossip-event-manager.h>

#include "gossip-app.h"
#include "gossip-chat.h"
#include "gossip-chat-manager.h"

#define DEBUG_DOMAIN "ChatManager"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CHAT_MANAGER, GossipChatManagerPriv))

typedef struct _GossipChatManagerPriv GossipChatManagerPriv;

struct _GossipChatManagerPriv {
	GHashTable *chats;
	GHashTable *events;
};

static void chat_manager_finalize           (GObject             *object);
static void chat_manager_new_message_cb     (GossipSession       *session,
					     GossipMessage       *msg,
					     GossipChatManager   *manager);
static void chat_manager_event_activated_cb (GossipEventManager  *event_manager,
					     GossipEvent         *event,
					     GObject             *object);
static void chat_manager_get_chats_foreach  (GossipContact       *contact,
					     GossipPrivateChat   *chat,
					     GList              **chats);
static void chat_manager_chat_removed_cb    (GossipChatManager   *manager,
					     GossipChat          *chat,
					     gboolean             is_last_ref);

G_DEFINE_TYPE (GossipChatManager, gossip_chat_manager, G_TYPE_OBJECT);

static void
gossip_chat_manager_class_init (GossipChatManagerClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = chat_manager_finalize;

	g_type_class_add_private (object_class, sizeof (GossipChatManagerPriv));
}

static void
gossip_chat_manager_init (GossipChatManager *manager)
{
	GossipChatManagerPriv *priv;

	priv = GET_PRIV (manager);

	priv->chats = g_hash_table_new_full (gossip_contact_hash,
					     gossip_contact_equal,
					     (GDestroyNotify) g_object_unref,
					     (GDestroyNotify) g_object_unref);

	priv->events = g_hash_table_new_full (gossip_contact_hash,
					      gossip_contact_equal,
					      (GDestroyNotify) g_object_unref,
					      (GDestroyNotify) g_object_unref);

	/* Connect to signals on GossipSession to listen for new messages */
	g_signal_connect (gossip_app_get_session (),
			  "new-message",
			  G_CALLBACK (chat_manager_new_message_cb),
			  manager);
}

static void
chat_manager_finalize (GObject *object)
{
	GossipChatManagerPriv *priv;

	priv = GET_PRIV (object);

	g_hash_table_destroy (priv->chats);
	g_hash_table_destroy (priv->events);

	G_OBJECT_CLASS (gossip_chat_manager_parent_class)->finalize (object);
}

static void
chat_manager_new_message_cb (GossipSession     *session,
			     GossipMessage     *message,
			     GossipChatManager *manager)
{
	GossipChatManagerPriv *priv;
	GossipPrivateChat     *chat;
	GossipContact         *sender;
	GossipEvent           *event = NULL;
	GossipEvent           *old_event;

	priv = GET_PRIV (manager);

	sender = gossip_message_get_sender (message);
	chat = g_hash_table_lookup (priv->chats, sender);

	old_event = g_hash_table_lookup (priv->events, sender);

	/* Add event to event manager if one doesn't exist already. */
	if (!chat) {
		gossip_debug (DEBUG_DOMAIN, "New chat for: %s",
			      gossip_contact_get_id (sender));
		chat = gossip_chat_manager_get_chat (manager, sender);

		if (!old_event) {
			event = gossip_event_new (GOSSIP_EVENT_NEW_MESSAGE);
		}
	} else {
		GossipChatWindow *window;

		window = gossip_chat_get_window (GOSSIP_CHAT (chat));

		if (!window && !old_event) {
			event = gossip_event_new (GOSSIP_EVENT_NEW_MESSAGE);
		}
	}

	gossip_private_chat_append_message (chat, message);

	if (event) {
		gchar *str;

		str = g_strdup_printf (_("New message from %s"),
				       gossip_contact_get_name (sender));
		g_object_set (event,
			      "message", str,
			      "data", message,
			      NULL);
		g_free (str);

		gossip_event_manager_add (gossip_app_get_event_manager (),
					  event,
					  chat_manager_event_activated_cb,
					  G_OBJECT (manager));

		g_hash_table_insert (priv->events,
				     g_object_ref (sender),
				     g_object_ref (event));
	}
}

static void
chat_manager_event_activated_cb (GossipEventManager *event_manager,
				 GossipEvent        *event,
				 GObject            *object)
{
	GossipMessage *message;
	GossipContact *contact;

	message = GOSSIP_MESSAGE (gossip_event_get_data (event));
	contact = gossip_message_get_sender (message);

	gossip_chat_manager_show_chat (GOSSIP_CHAT_MANAGER (object), contact);
}

static void
chat_manager_get_chats_foreach (GossipContact      *contact,
				GossipPrivateChat  *chat,
				GList             **chats)
{
	const gchar *contact_id;

	contact_id = gossip_contact_get_id (contact);
	*chats = g_list_prepend (*chats, g_strdup (contact_id));
}

GossipChatManager *
gossip_chat_manager_new (void)
{
	return g_object_new (GOSSIP_TYPE_CHAT_MANAGER, NULL);
}

static void
chat_manager_chat_removed_cb (GossipChatManager *manager,
			      GossipChat        *chat,
			      gboolean           is_last_ref) 
{
	GossipChatManagerPriv *priv;
	GossipContact         *contact;

	if (!is_last_ref) {
		return;
	}
	
	priv = GET_PRIV (manager);
	
	contact = gossip_chat_get_contact (chat);

	gossip_debug (DEBUG_DOMAIN, 
		      "Removing an old chat:'%s'",
		      gossip_contact_get_id (contact));

	g_hash_table_remove (priv->chats, contact);
}			      

GossipPrivateChat *
gossip_chat_manager_get_chat (GossipChatManager *manager,
			      GossipContact     *contact)
{
	GossipChatManagerPriv *priv;
	GossipPrivateChat     *chat;

	g_return_val_if_fail (GOSSIP_IS_CHAT_MANAGER (manager), NULL);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	priv = GET_PRIV (manager);

	chat = g_hash_table_lookup (priv->chats, contact);

	if (!chat) {
		GossipSession *session;
		GossipAccount *account;
		GossipContact *own_contact;

		session = gossip_app_get_session ();
		account = gossip_contact_get_account (contact);
		own_contact = gossip_session_get_own_contact (session, account);

		chat = gossip_private_chat_new (own_contact, contact);
		g_hash_table_insert (priv->chats,
				     g_object_ref (contact),
				     chat);
		g_object_add_toggle_ref (G_OBJECT (chat),
					 (GToggleNotify) chat_manager_chat_removed_cb, 
					 manager);

		gossip_debug (DEBUG_DOMAIN, 
			      "Creating a new chat:'%s'",
			      gossip_contact_get_id (contact));
	}

	return chat;
}

GList *
gossip_chat_manager_get_chats (GossipChatManager *manager)
{
	GossipChatManagerPriv *priv;
	GList                 *chats = NULL;

	g_return_val_if_fail (GOSSIP_IS_CHAT_MANAGER (manager), NULL);

	priv = GET_PRIV (manager);

	g_hash_table_foreach (priv->chats,
			      (GHFunc) chat_manager_get_chats_foreach,
			      &chats);

	chats = g_list_sort (chats, (GCompareFunc) strcmp);

	return chats;
}

void
gossip_chat_manager_remove_events (GossipChatManager *manager,
				   GossipContact     *contact)
{
	GossipChatManagerPriv *priv;
	GossipEvent           *event;

	g_return_if_fail (GOSSIP_IS_CHAT_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	gossip_debug (DEBUG_DOMAIN, 
		      "Removing events for contact:'%s'",
		      gossip_contact_get_id (contact));

	priv = GET_PRIV (manager);

	event = g_hash_table_lookup (priv->events, contact);
	if (event) {
		gossip_event_manager_remove (gossip_app_get_event_manager (),
					     event, G_OBJECT (manager));
		g_hash_table_remove (priv->events, contact);
	}
}

void
gossip_chat_manager_show_chat (GossipChatManager *manager,
			       GossipContact     *contact)
{
	GossipPrivateChat     *chat;

	g_return_if_fail (GOSSIP_IS_CHAT_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	chat = gossip_chat_manager_get_chat (manager, contact);

	gossip_chat_present (GOSSIP_CHAT (chat));

	gossip_chat_manager_remove_events(manager, contact);
}
