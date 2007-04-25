/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Geert-Jan Van den Bogaerde <geertjan@gnome.org>
 */

#ifndef __GOSSIP_CHAT_WINDOW_H__
#define __GOSSIP_CHAT_WINDOW_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_CHAT_WINDOW         (gossip_chat_window_get_type ())
#define GOSSIP_CHAT_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CHAT_WINDOW, GossipChatWindow))
#define GOSSIP_CHAT_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CHAT_WINDOW, GossipChatWindowClass))
#define GOSSIP_IS_CHAT_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CHAT_WINDOW))
#define GOSSIP_IS_CHAT_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CHAT_WINDOW))
#define GOSSIP_CHAT_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CHAT_WINDOW, GossipChatWindowClass))

typedef struct _GossipChatWindow      GossipChatWindow;
typedef struct _GossipChatWindowClass GossipChatWindowClass;
typedef struct _GossipChatWindowPriv  GossipChatWindowPriv;

#include "gossip-chat.h"

struct _GossipChatWindow {
	GObject parent;
};

struct _GossipChatWindowClass {
	GObjectClass parent_class;
};

GType             gossip_chat_window_get_type        (void);
GossipChatWindow *gossip_chat_window_get_default     (void);

GossipChatWindow *gossip_chat_window_new             (void);

GtkWidget *       gossip_chat_window_get_dialog      (GossipChatWindow *window);

void              gossip_chat_window_add_chat        (GossipChatWindow *window,
						      GossipChat       *chat);
void              gossip_chat_window_remove_chat     (GossipChatWindow *window,
						      GossipChat       *chat);
void              gossip_chat_window_move_chat       (GossipChatWindow *old_window,
						      GossipChatWindow *new_window,
						      GossipChat       *chat);
void              gossip_chat_window_switch_to_chat  (GossipChatWindow *window,
						      GossipChat       *chat);
gboolean          gossip_chat_window_has_focus       (GossipChatWindow *window);

G_END_DECLS

#endif /* __GOSSIP_CHAT_WINDOW_H__ */
