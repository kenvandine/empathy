/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include "empathy-tp-chatroom.h"
#include "empathy-contact-list.h"
#include "empathy-contact-factory.h"
#include "empathy-tp-group.h"
#include "empathy-utils.h"
#include "empathy-debug.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_TP_CHATROOM, EmpathyTpChatroomPriv))

#define DEBUG_DOMAIN "TpChatroom"

struct _EmpathyTpChatroomPriv {
	EmpathyContactFactory *factory;
	EmpathyTpGroup        *group;

	gboolean               is_invited;
	EmpathyContact        *invitor;
	gchar                 *invit_message;
};

static void            empathy_tp_chatroom_class_init (EmpathyTpChatroomClass  *klass);
static void            tp_chatroom_iface_init         (EmpathyContactListIface *iface);
static void            empathy_tp_chatroom_init       (EmpathyTpChatroom       *chatroom);
static void            tp_chatroom_finalize           (GObject                 *object);
static void            tp_chatroom_member_added_cb    (EmpathyTpGroup          *group,
						       EmpathyContact          *contact,
						       EmpathyContact          *actor,
						       guint                    reason,
						       const gchar             *message,
						       EmpathyTpChatroom       *chatroom);
static void            tp_chatroom_member_removed_cb  (EmpathyTpGroup          *group,
						       EmpathyContact          *contact,
						       EmpathyContact          *actor,
						       guint                    reason,
						       const gchar             *message,
						       EmpathyTpChatroom       *chatroom);
static void            tp_chatroom_add                (EmpathyContactList      *list,
						       EmpathyContact           *contact,
						       const gchar             *message);
static void            tp_chatroom_remove             (EmpathyContactList      *list,
						       EmpathyContact           *contact,
						       const gchar             *message);
static GList *         tp_chatroom_get_members        (EmpathyContactList      *list);

G_DEFINE_TYPE_WITH_CODE (EmpathyTpChatroom, empathy_tp_chatroom, EMPATHY_TYPE_TP_CHAT,
			 G_IMPLEMENT_INTERFACE (EMPATHY_TYPE_CONTACT_LIST,
						tp_chatroom_iface_init));

static void
empathy_tp_chatroom_class_init (EmpathyTpChatroomClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tp_chatroom_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyTpChatroomPriv));
}

static void
tp_chatroom_iface_init (EmpathyContactListIface *iface)
{
	iface->add         = tp_chatroom_add;
	iface->remove      = tp_chatroom_remove;
	iface->get_members = tp_chatroom_get_members;
}

static void
empathy_tp_chatroom_init (EmpathyTpChatroom *chatroom)
{
}

static void
tp_chatroom_finalize (GObject *object)
{
	EmpathyTpChatroomPriv *priv;
	EmpathyTpChatroom     *chatroom;

	chatroom = EMPATHY_TP_CHATROOM (object);
	priv = GET_PRIV (chatroom);

	g_object_unref (priv->group);
	g_object_unref (priv->factory);

	if (priv->invitor) {
		g_object_unref (priv->invitor);
	}

	g_free (priv->invit_message);

	G_OBJECT_CLASS (empathy_tp_chatroom_parent_class)->finalize (object);
}

EmpathyTpChatroom *
empathy_tp_chatroom_new (McAccount *account,
			 TpChan    *tp_chan)
{
	EmpathyTpChatroomPriv *priv;
	EmpathyTpChatroom     *chatroom;
	GList                 *members, *l;
	EmpathyContact        *user;

	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (TELEPATHY_IS_CHAN (tp_chan), NULL);

	chatroom = g_object_new (EMPATHY_TYPE_TP_CHATROOM,
				 "account", account,
				 "tp-chan", tp_chan,
				 NULL);

	priv = GET_PRIV (chatroom);

	priv->factory = empathy_contact_factory_new ();
	priv->group = empathy_tp_group_new (account, tp_chan);

	g_signal_connect (priv->group, "member-added",
			  G_CALLBACK (tp_chatroom_member_added_cb),
			  chatroom);
	g_signal_connect (priv->group, "member-removed",
			  G_CALLBACK (tp_chatroom_member_removed_cb),
			  chatroom);

	/* Check if we are invited to join the chat */
	user = empathy_tp_group_get_self_contact (priv->group);
	members = empathy_tp_group_get_local_pendings (priv->group);
	for (l = members; l; l = l->next) {
		EmpathyPendingInfo *info;

		info = l->data;

		if (!empathy_contact_equal (user, info->member)) {
			continue;
		}

		priv->invitor = g_object_ref (info->actor);
		priv->invit_message = g_strdup (info->message);
		priv->is_invited = TRUE;

		empathy_debug (DEBUG_DOMAIN, "We are invited to join by %s (%d): %s",
			       empathy_contact_get_id (priv->invitor),
			       empathy_contact_get_handle (priv->invitor),
			       priv->invit_message);
	}

	g_list_foreach (members, (GFunc) empathy_pending_info_free, NULL);
	g_list_free (members);
	g_object_unref (user);

	return chatroom;
}

gboolean
empathy_tp_chatroom_get_invitation (EmpathyTpChatroom  *chatroom,
				    EmpathyContact     **contact,
				    const gchar       **message)
{
	EmpathyTpChatroomPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_CHATROOM (chatroom), FALSE);

	priv = GET_PRIV (chatroom);

	if (*contact) {
		*contact = priv->invitor;
	}
	if (*message) {
		*message = priv->invit_message;
	}

	return priv->is_invited;
}

void
empathy_tp_chatroom_accept_invitation (EmpathyTpChatroom *chatroom)
{
	EmpathyTpChatroomPriv *priv;
	EmpathyContact        *user;

	g_return_if_fail (EMPATHY_IS_TP_CHATROOM (chatroom));

	priv = GET_PRIV (chatroom);

	/* Clear invitation data */
	priv->is_invited = FALSE;
	if (priv->invitor) {
		g_object_unref (priv->invitor);
		priv->invitor = NULL;
	}
	g_free (priv->invit_message);
	priv->invit_message = NULL;

	/* Add ourself in the members of the room */
	user = empathy_tp_group_get_self_contact (priv->group);
	empathy_tp_group_add_member (priv->group, user, "");
	g_object_unref (user);
}

void
empathy_tp_chatroom_set_topic (EmpathyTpChatroom *chatroom,
			       const gchar       *topic)
{
	/* FIXME: not implemented */
}

static void
tp_chatroom_member_added_cb (EmpathyTpGroup    *group,
			     EmpathyContact    *contact,
			     EmpathyContact    *actor,
			     guint              reason,
			     const gchar       *message,
			     EmpathyTpChatroom *chatroom)
{
	g_signal_emit_by_name (chatroom, "members-changed",
			       contact, actor, reason, message,
			       TRUE);
}

static void
tp_chatroom_member_removed_cb (EmpathyTpGroup    *group,
			       EmpathyContact    *contact,
			       EmpathyContact    *actor,
			       guint              reason,
			       const gchar       *message,
			       EmpathyTpChatroom *chatroom)
{
	g_signal_emit_by_name (chatroom, "members-changed",
			       contact, actor, reason, message,
			       FALSE);
}

static void
tp_chatroom_add (EmpathyContactList *list,
		 EmpathyContact     *contact,
		 const gchar        *message)
{
	EmpathyTpChatroomPriv *priv;

	g_return_if_fail (EMPATHY_IS_TP_CHATROOM (list));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	priv = GET_PRIV (list);

	empathy_tp_group_add_member (priv->group, contact, message);
}

static void
tp_chatroom_remove (EmpathyContactList *list,
		    EmpathyContact     *contact,
		    const gchar        *message)
{
	EmpathyTpChatroomPriv *priv;

	g_return_if_fail (EMPATHY_IS_TP_CHATROOM (list));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	priv = GET_PRIV (list);

	empathy_tp_group_remove_member (priv->group, contact, message);
}

static GList *
tp_chatroom_get_members (EmpathyContactList *list)
{
	EmpathyTpChatroomPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_CHATROOM (list), NULL);

	priv = GET_PRIV (list);

	return empathy_tp_group_get_members (priv->group);
}

