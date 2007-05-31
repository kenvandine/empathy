/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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
 *          Martyn Russell <martyn@imendio.com>
 */

#ifndef __GOSSIP_CHATROOM_MANAGER_H__
#define __GOSSIP_CHATROOM_MANAGER_H__

#include <glib-object.h>

#include <libmissioncontrol/mc-account.h>

#include "gossip-chatroom.h"

G_BEGIN_DECLS

#define GOSSIP_TYPE_CHATROOM_MANAGER         (gossip_chatroom_manager_get_type ())
#define GOSSIP_CHATROOM_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CHATROOM_MANAGER, GossipChatroomManager))
#define GOSSIP_CHATROOM_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CHATROOM_MANAGER, GossipChatroomManagerClass))
#define GOSSIP_IS_CHATROOM_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CHATROOM_MANAGER))
#define GOSSIP_IS_CHATROOM_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CHATROOM_MANAGER))
#define GOSSIP_CHATROOM_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CHATROOM_MANAGER, GossipChatroomManagerClass))

typedef struct _GossipChatroomManager      GossipChatroomManager;
typedef struct _GossipChatroomManagerClass GossipChatroomManagerClass;
typedef struct _GossipChatroomManagerPriv  GossipChatroomManagerPriv;

struct _GossipChatroomManager {
	GObject parent;
};

struct _GossipChatroomManagerClass {
	GObjectClass parent_class;
};

GType                  gossip_chatroom_manager_get_type      (void) G_GNUC_CONST;
GossipChatroomManager *gossip_chatroom_manager_new           (void);
gboolean               gossip_chatroom_manager_add           (GossipChatroomManager *manager,
							      GossipChatroom        *chatroom);
void                   gossip_chatroom_manager_remove        (GossipChatroomManager *manager,
							      GossipChatroom        *chatroom);
GossipChatroom *       gossip_chatroom_manager_find          (GossipChatroomManager *manager,
							      McAccount             *account,
							      const gchar           *room);
GList *                gossip_chatroom_manager_get_chatrooms (GossipChatroomManager *manager,
							      McAccount             *account);
guint                  gossip_chatroom_manager_get_count     (GossipChatroomManager *manager,
							      McAccount             *account);
void                   gossip_chatroom_manager_store         (GossipChatroomManager *manager);

G_END_DECLS

#endif /* __GOSSIP_CHATROOM_MANAGER_H__ */
