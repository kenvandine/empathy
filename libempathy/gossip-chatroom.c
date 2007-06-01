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

#include "config.h"

#include <string.h>

#include <glib.h>

#include "gossip-chatroom.h"
#include "gossip-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CHATROOM, GossipChatroomPriv))

struct _GossipChatroomPriv {
	McAccount *account;
	gchar     *room;
	gchar     *name;
	gboolean   auto_connect;
};

static void gossip_chatroom_class_init (GossipChatroomClass *klass);
static void gossip_chatroom_init       (GossipChatroom      *chatroom);
static void chatroom_finalize          (GObject             *object);
static void chatroom_get_property      (GObject             *object,
					guint                param_id,
					GValue              *value,
					GParamSpec          *pspec);
static void chatroom_set_property      (GObject             *object,
					guint                param_id,
					const GValue        *value,
					GParamSpec          *pspec);

enum {
	PROP_0,
	PROP_ACCOUNT,
	PROP_ROOM,
	PROP_NAME,
	PROP_AUTO_CONNECT,
};

G_DEFINE_TYPE (GossipChatroom, gossip_chatroom, G_TYPE_OBJECT);

static void
gossip_chatroom_class_init (GossipChatroomClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize     = chatroom_finalize;
	object_class->get_property = chatroom_get_property;
	object_class->set_property = chatroom_set_property;

	g_object_class_install_property (object_class,
					 PROP_ACCOUNT,
					 g_param_spec_object ("account",
							      "Chatroom Account",
							      "The account associated with an chatroom",
							      MC_TYPE_ACCOUNT,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_ROOM,
					 g_param_spec_string ("room",
							      "Chatroom Room",
							      "Chatroom represented as 'room@server'",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Chatroom Name",
							      "Chatroom name",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_AUTO_CONNECT,
					 g_param_spec_boolean ("auto_connect",
							       "Chatroom Auto Connect",
							       "Connect on startup",
							       FALSE,
							       G_PARAM_READWRITE));


	g_type_class_add_private (object_class, sizeof (GossipChatroomPriv));
}

static void
gossip_chatroom_init (GossipChatroom *chatroom)
{
}

static void
chatroom_finalize (GObject *object)
{
	GossipChatroomPriv *priv;

	priv = GET_PRIV (object);

	g_object_unref (priv->account);
	g_free (priv->room);
	g_free (priv->name);

	(G_OBJECT_CLASS (gossip_chatroom_parent_class)->finalize) (object);
}

static void
chatroom_get_property (GObject    *object,
		       guint       param_id,
		       GValue     *value,
		       GParamSpec *pspec)
{
	GossipChatroomPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ACCOUNT:
		g_value_set_object (value, priv->account);
		break;
	case PROP_ROOM:
		g_value_set_string (value, priv->room);
		break;
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_AUTO_CONNECT:
		g_value_set_boolean (value, priv->auto_connect);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
chatroom_set_property (GObject      *object,
		       guint         param_id,
		       const GValue *value,
		       GParamSpec   *pspec)
{
	GossipChatroomPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ACCOUNT:
		gossip_chatroom_set_account (GOSSIP_CHATROOM (object),
					     g_value_get_object (value));
		break;
	case PROP_ROOM:
		gossip_chatroom_set_room (GOSSIP_CHATROOM (object),
					  g_value_get_string (value));
		break;
	case PROP_NAME:
		gossip_chatroom_set_name (GOSSIP_CHATROOM (object),
					  g_value_get_string (value));
		break;
	case PROP_AUTO_CONNECT:
		gossip_chatroom_set_auto_connect (GOSSIP_CHATROOM (object),
						  g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

GossipChatroom *
gossip_chatroom_new (McAccount   *account,
		     const gchar *room)
{
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (room != NULL, NULL);

	return g_object_new (GOSSIP_TYPE_CHATROOM,
			     "account", account,
			     "room", room,
			     NULL);
}

GossipChatroom *
gossip_chatroom_new_full (McAccount   *account,
			  const gchar *room,
			  const gchar *name,
			  gboolean     auto_connect)
{
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (room != NULL, NULL);

	return g_object_new (GOSSIP_TYPE_CHATROOM,
			     "account", account,
			     "room", room,
			     "name", name,
			     "auto_connect", auto_connect,
			     NULL);
}

McAccount *
gossip_chatroom_get_account (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);
	return priv->account;
}

void
gossip_chatroom_set_account (GossipChatroom *chatroom,
			     McAccount      *account)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (MC_IS_ACCOUNT (account));

	priv = GET_PRIV (chatroom);

	if (account == priv->account) {
		return;
	}
	if (priv->account) {
		g_object_unref (priv->account);
	}
	priv->account = g_object_ref (account);

	g_object_notify (G_OBJECT (chatroom), "account");
}

const gchar *
gossip_chatroom_get_room (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);
	return priv->room;
}

void
gossip_chatroom_set_room (GossipChatroom *chatroom,
			  const gchar    *room)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));
	g_return_if_fail (room != NULL);

	priv = GET_PRIV (chatroom);

	g_free (priv->room);
	priv->room = g_strdup (room);

	g_object_notify (G_OBJECT (chatroom), "room");
}

const gchar *
gossip_chatroom_get_name (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);
	
	if (G_STR_EMPTY (priv->name)) {
		return priv->room;
	}
	
	return priv->name;
}

void
gossip_chatroom_set_name (GossipChatroom *chatroom,
			  const gchar    *name)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GET_PRIV (chatroom);

	g_free (priv->name);
	priv->name = NULL;
	if (name) {
		priv->name = g_strdup (name);
	}

	g_object_notify (G_OBJECT (chatroom), "name");
}

gboolean
gossip_chatroom_get_auto_connect (GossipChatroom *chatroom)
{
	GossipChatroomPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (chatroom), FALSE);

	priv = GET_PRIV (chatroom);
	return priv->auto_connect;
}

void
gossip_chatroom_set_auto_connect (GossipChatroom *chatroom,
				  gboolean        auto_connect)
{
	GossipChatroomPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHATROOM (chatroom));

	priv = GET_PRIV (chatroom);

	priv->auto_connect = auto_connect;

	g_object_notify (G_OBJECT (chatroom), "auto-connect");
}

gboolean
gossip_chatroom_equal (gconstpointer v1,
		       gconstpointer v2)
{
	McAccount   *account_a;
	McAccount   *account_b;
	const gchar *room_a;
	const gchar *room_b;

	g_return_val_if_fail (GOSSIP_IS_CHATROOM (v1), FALSE);
	g_return_val_if_fail (GOSSIP_IS_CHATROOM (v2), FALSE);

	account_a = gossip_chatroom_get_account (GOSSIP_CHATROOM (v1));
	account_b = gossip_chatroom_get_account (GOSSIP_CHATROOM (v2));

	room_a = gossip_chatroom_get_room (GOSSIP_CHATROOM (v1));
	room_b = gossip_chatroom_get_room (GOSSIP_CHATROOM (v2));

	return gossip_account_equal (account_a, account_b) && g_str_equal (room_a, room_b);
}


