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

#ifndef __GOSSIP_CHATROOM_H__
#define __GOSSIP_CHATROOM_H__

#include <glib-object.h>

#include <libmissioncontrol/mc-account.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_CHATROOM             (gossip_chatroom_get_type ())
#define GOSSIP_CHATROOM(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CHATROOM, GossipChatroom))
#define GOSSIP_CHATROOM_CLASS(k)         (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CHATROOM, GossipChatroomClass))
#define GOSSIP_IS_CHATROOM(o)            (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CHATROOM))
#define GOSSIP_IS_CHATROOM_CLASS(k)      (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CHATROOM))
#define GOSSIP_CHATROOM_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CHATROOM, GossipChatroomClass))

#define GOSSIP_TYPE_CHATROOM_INVITE       (gossip_chatroom_invite_get_gtype ())

typedef struct _GossipChatroom      GossipChatroom;
typedef struct _GossipChatroomClass GossipChatroomClass;
typedef struct _GossipChatroomPriv  GossipChatroomPriv;

struct _GossipChatroom {
	GObject parent;
};

struct _GossipChatroomClass {
	GObjectClass parent_class;
};

GType           gossip_chatroom_get_type         (void) G_GNUC_CONST;
GossipChatroom *gossip_chatroom_new              (McAccount      *account,
						  const gchar    *room);
GossipChatroom *gossip_chatroom_new_full         (McAccount      *account,
						  const gchar    *room,
						  const gchar    *name,
						  gboolean        auto_connect);
McAccount *     gossip_chatroom_get_account      (GossipChatroom *chatroom);
void            gossip_chatroom_set_account      (GossipChatroom *chatroom,
						  McAccount      *account);
const gchar *   gossip_chatroom_get_room         (GossipChatroom *chatroom);
void            gossip_chatroom_set_room         (GossipChatroom *chatroom,
						  const gchar    *room);
const gchar *   gossip_chatroom_get_name         (GossipChatroom *chatroom);
void            gossip_chatroom_set_name         (GossipChatroom *chatroom,
						  const gchar    *name);
gboolean        gossip_chatroom_get_auto_connect (GossipChatroom *chatroom);
void            gossip_chatroom_set_auto_connect (GossipChatroom *chatroom,
						  gboolean        auto_connect);
gboolean        gossip_chatroom_equal            (gconstpointer   v1,
						  gconstpointer   v2);


G_BEGIN_DECLS

#endif /* __GOSSIP_CHATROOM_H__ */
