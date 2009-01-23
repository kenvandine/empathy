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

#include "config.h"

#include <string.h>

#include <glib.h>
#include <telepathy-glib/util.h>

#include "empathy-chatroom.h"
#include "empathy-utils.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyChatroom)
typedef struct {
	McAccount *account;
	gchar     *room;
	gchar     *name;
	gboolean   auto_connect;
	gboolean favorite;
	EmpathyTpChat *tp_chat;
} EmpathyChatroomPriv;


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
	PROP_FAVORITE,
	PROP_TP_CHAT,
};

G_DEFINE_TYPE (EmpathyChatroom, empathy_chatroom, G_TYPE_OBJECT);

static void
empathy_chatroom_class_init (EmpathyChatroomClass *klass)
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

  g_object_class_install_property (object_class,
      PROP_FAVORITE,
      g_param_spec_boolean ("favorite",
        "Favorite",
        "TRUE if the chatroom is in user's favorite list",
        FALSE,
        G_PARAM_READWRITE |
        G_PARAM_CONSTRUCT |
        G_PARAM_STATIC_NAME |
        G_PARAM_STATIC_NICK |
        G_PARAM_STATIC_BLURB));

	g_object_class_install_property (object_class,
					 PROP_TP_CHAT,
					 g_param_spec_object ("tp-chat",
							      "Chatroom channel wrapper",
							      "The wrapper for the chatroom channel if there is one",
							      EMPATHY_TYPE_TP_CHAT,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EmpathyChatroomPriv));
}

static void
empathy_chatroom_init (EmpathyChatroom *chatroom)
{
	EmpathyChatroomPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (chatroom,
		EMPATHY_TYPE_CHATROOM, EmpathyChatroomPriv);

	chatroom->priv = priv;
}

static void
chatroom_finalize (GObject *object)
{
	EmpathyChatroomPriv *priv;

	priv = GET_PRIV (object);

	if (priv->tp_chat != NULL)
		g_object_unref (priv->tp_chat);

	g_object_unref (priv->account);
	g_free (priv->room);
	g_free (priv->name);

	(G_OBJECT_CLASS (empathy_chatroom_parent_class)->finalize) (object);
}

static void
chatroom_get_property (GObject    *object,
		       guint       param_id,
		       GValue     *value,
		       GParamSpec *pspec)
{
	EmpathyChatroomPriv *priv;

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
  case PROP_FAVORITE:
    g_value_set_boolean (value, priv->favorite);
    break;
	case PROP_TP_CHAT:
		g_value_set_object (value, priv->tp_chat);
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
	EmpathyChatroomPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ACCOUNT:
		empathy_chatroom_set_account (EMPATHY_CHATROOM (object),
					     g_value_get_object (value));
		break;
	case PROP_ROOM:
		empathy_chatroom_set_room (EMPATHY_CHATROOM (object),
					  g_value_get_string (value));
		break;
	case PROP_NAME:
		empathy_chatroom_set_name (EMPATHY_CHATROOM (object),
					  g_value_get_string (value));
		break;
	case PROP_AUTO_CONNECT:
		empathy_chatroom_set_auto_connect (EMPATHY_CHATROOM (object),
						  g_value_get_boolean (value));
		break;
  case PROP_FAVORITE:
    priv->favorite = g_value_get_boolean (value);
    if (!priv->favorite)
      {
        empathy_chatroom_set_auto_connect (EMPATHY_CHATROOM (object),
            FALSE);
      }
    break;
	case PROP_TP_CHAT: {
		GObject *chat = g_value_dup_object (value);

		if (chat == (GObject *) priv->tp_chat)
			break;

		g_assert (chat == NULL || priv->tp_chat == NULL);

		if (priv->tp_chat != NULL) {
			g_object_unref (priv->tp_chat);
			priv->tp_chat = NULL;
		} else {
			priv->tp_chat = EMPATHY_TP_CHAT (chat);
		}
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

EmpathyChatroom *
empathy_chatroom_new (McAccount *account)
{
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

	return g_object_new (EMPATHY_TYPE_CHATROOM,
			     "account", account,
			     NULL);
}

EmpathyChatroom *
empathy_chatroom_new_full (McAccount   *account,
			  const gchar *room,
			  const gchar *name,
			  gboolean     auto_connect)
{
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (room != NULL, NULL);

	return g_object_new (EMPATHY_TYPE_CHATROOM,
			     "account", account,
			     "room", room,
			     "name", name,
			     "auto_connect", auto_connect,
			     NULL);
}

McAccount *
empathy_chatroom_get_account (EmpathyChatroom *chatroom)
{
	EmpathyChatroomPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);
	return priv->account;
}

void
empathy_chatroom_set_account (EmpathyChatroom *chatroom,
			     McAccount      *account)
{
	EmpathyChatroomPriv *priv;

	g_return_if_fail (EMPATHY_IS_CHATROOM (chatroom));
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
empathy_chatroom_get_room (EmpathyChatroom *chatroom)
{
	EmpathyChatroomPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);
	return priv->room;
}

void
empathy_chatroom_set_room (EmpathyChatroom *chatroom,
			  const gchar    *room)
{
	EmpathyChatroomPriv *priv;

	g_return_if_fail (EMPATHY_IS_CHATROOM (chatroom));
	g_return_if_fail (room != NULL);

	priv = GET_PRIV (chatroom);

	g_free (priv->room);
	priv->room = g_strdup (room);

	g_object_notify (G_OBJECT (chatroom), "room");
}

const gchar *
empathy_chatroom_get_name (EmpathyChatroom *chatroom)
{
	EmpathyChatroomPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);
	
	if (EMP_STR_EMPTY (priv->name)) {
		return priv->room;
	}
	
	return priv->name;
}

void
empathy_chatroom_set_name (EmpathyChatroom *chatroom,
			  const gchar    *name)
{
	EmpathyChatroomPriv *priv;

	g_return_if_fail (EMPATHY_IS_CHATROOM (chatroom));

	priv = GET_PRIV (chatroom);

	g_free (priv->name);
	priv->name = NULL;
	if (name) {
		priv->name = g_strdup (name);
	}

	g_object_notify (G_OBJECT (chatroom), "name");
}

gboolean
empathy_chatroom_get_auto_connect (EmpathyChatroom *chatroom)
{
	EmpathyChatroomPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CHATROOM (chatroom), FALSE);

	priv = GET_PRIV (chatroom);
	return priv->auto_connect;
}

void
empathy_chatroom_set_auto_connect (EmpathyChatroom *chatroom,
				  gboolean        auto_connect)
{
	EmpathyChatroomPriv *priv;

	g_return_if_fail (EMPATHY_IS_CHATROOM (chatroom));

	priv = GET_PRIV (chatroom);

	priv->auto_connect = auto_connect;

  if (priv->auto_connect)
    {
      /* auto_connect implies favorite */
      priv->favorite = TRUE;
      g_object_notify (G_OBJECT (chatroom), "favorite");
    }

	g_object_notify (G_OBJECT (chatroom), "auto-connect");
}

gboolean
empathy_chatroom_equal (gconstpointer v1,
		       gconstpointer v2)
{
	McAccount   *account_a;
	McAccount   *account_b;
	const gchar *room_a;
	const gchar *room_b;

	g_return_val_if_fail (EMPATHY_IS_CHATROOM (v1), FALSE);
	g_return_val_if_fail (EMPATHY_IS_CHATROOM (v2), FALSE);

	account_a = empathy_chatroom_get_account (EMPATHY_CHATROOM (v1));
	account_b = empathy_chatroom_get_account (EMPATHY_CHATROOM (v2));

	room_a = empathy_chatroom_get_room (EMPATHY_CHATROOM (v1));
	room_b = empathy_chatroom_get_room (EMPATHY_CHATROOM (v2));

	return empathy_account_equal (account_a, account_b) && !tp_strdiff (room_a,
      room_b);
}

EmpathyTpChat *
empathy_chatroom_get_tp_chat (EmpathyChatroom *chatroom) {
	EmpathyChatroomPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CHATROOM (chatroom), NULL);

	priv = GET_PRIV (chatroom);

	return priv->tp_chat;
}
