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

#include <libmissioncontrol/mission-control.h>

#include "empathy-tp-chatroom.h"
#include "empathy-tp-contact-list.h"
#include "empathy-contact-list.h"
#include "empathy-contact-manager.h"
#include "gossip-telepathy-group.h"
#include "gossip-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_TP_CHATROOM, EmpathyTpChatroomPriv))

#define DEBUG_DOMAIN "TpChatroom"

struct _EmpathyTpChatroomPriv {
	EmpathyContactManager *manager;
	EmpathyTpContactList  *list;
	GossipTelepathyGroup  *group;
};

static void            empathy_tp_chatroom_class_init (EmpathyTpChatroomClass  *klass);
static void            tp_chatroom_iface_init         (EmpathyContactListIface *iface);
static void            empathy_tp_chatroom_init       (EmpathyTpChatroom       *chatroom);
static void            tp_chatroom_finalize           (GObject                 *object);
static void            tp_chatroom_setup              (EmpathyContactList      *list);
static GossipContact * tp_chatroom_find               (EmpathyContactList      *list,
						       const gchar             *id);
static void            tp_chatroom_add                (EmpathyContactList      *list,
						       GossipContact           *contact,
						       const gchar             *message);
static void            tp_chatroom_remove             (EmpathyContactList      *list,
						       GossipContact           *contact,
						       const gchar             *message);
static GList *         tp_chatroom_get_contacts       (EmpathyContactList      *list);

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
	iface->setup = tp_chatroom_setup;
	iface->find = tp_chatroom_find;
	iface->add = tp_chatroom_add;
	iface->remove = tp_chatroom_remove;
	iface->get_contacts = tp_chatroom_get_contacts;
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
	g_object_unref (priv->manager);
	g_object_unref (priv->list);

	G_OBJECT_CLASS (empathy_tp_chatroom_parent_class)->finalize (object);
}

EmpathyTpChatroom *
empathy_tp_chatroom_new (McAccount *account,
			 TpChan    *tp_chan)
{
	EmpathyTpChatroomPriv *priv;
	EmpathyTpChatroom     *chatroom;
	TpConn                *tp_conn;
	MissionControl        *mc;

	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (TELEPATHY_IS_CHAN (tp_chan), NULL);

	chatroom = g_object_new (EMPATHY_TYPE_TP_CHATROOM,
				 "account", account,
				 "tp-chan", tp_chan,
				 NULL);

	priv = GET_PRIV (chatroom);

	mc = gossip_mission_control_new ();
	tp_conn = mission_control_get_connection (mc, account, NULL);
	priv->manager = empathy_contact_manager_new ();
	priv->group = gossip_telepathy_group_new (tp_chan, tp_conn);
	priv->list = empathy_contact_manager_get_list (priv->manager, account);

	g_object_unref (mc);
	g_object_unref (tp_conn);

	return chatroom;
}

static void
tp_chatroom_setup (EmpathyContactList *list)
{
}

static GossipContact *
tp_chatroom_find (EmpathyContactList *list,
		  const gchar        *id)
{
	return NULL;
}

static void
tp_chatroom_add (EmpathyContactList *list,
		 GossipContact      *contact,
		 const gchar        *message)
{
}

static void
tp_chatroom_remove (EmpathyContactList *list,
		    GossipContact      *contact,
		    const gchar        *message)
{
}

static GList *
tp_chatroom_get_contacts (EmpathyContactList *list)
{
	return NULL;
}

