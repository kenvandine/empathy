/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Geert-Jan Van den Bogaerde <geertjan@gnome.org>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __GOSSIP_CHAT_H__
#define __GOSSIP_CHAT_H__

#include <glib-object.h>

#include <libempathy/gossip-contact.h>
#include <libempathy/gossip-message.h>
#include <libempathy/empathy-tp-chat.h>

#include "gossip-chat-view.h"
#include "gossip-spell.h" 

G_BEGIN_DECLS

#define GOSSIP_TYPE_CHAT         (gossip_chat_get_type ())
#define GOSSIP_CHAT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CHAT, GossipChat))
#define GOSSIP_CHAT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_CHAT, GossipChatClass))
#define GOSSIP_IS_CHAT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CHAT))
#define GOSSIP_IS_CHAT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CHAT))
#define GOSSIP_CHAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CHAT, GossipChatClass))

typedef struct _GossipChat       GossipChat;
typedef struct _GossipChatClass  GossipChatClass;
typedef struct _GossipChatPriv   GossipChatPriv;

#include "gossip-chat-window.h"

struct _GossipChat {
	GObject          parent;

	/* Protected */
	GossipChatView  *view;
	GtkWidget       *input_text_view;
	gboolean         is_first_char;
	McAccount       *account;
};

struct _GossipChatClass {
	GObjectClass parent;

	/* VTable */
	const gchar *    (*get_name)            (GossipChat  *chat);
	gchar *          (*get_tooltip)         (GossipChat  *chat);
	const gchar *    (*get_status_icon_name)(GossipChat  *chat);
	GossipContact *  (*get_contact)         (GossipChat  *chat);
	GtkWidget *      (*get_widget)          (GossipChat  *chat);

	gboolean         (*get_show_contacts)   (GossipChat  *chat);
	void             (*set_show_contacts)   (GossipChat  *chat,
						 gboolean     show);
	gboolean         (*is_group_chat)       (GossipChat  *chat);
	void             (*save_geometry)       (GossipChat  *chat,
						 gint         x,
						 gint         y,
						 gint         w,
						 gint         h);
	void             (*load_geometry)       (GossipChat  *chat,
						 gint        *x,
						 gint        *y,
						 gint        *w,
						 gint        *h);
};

GType             gossip_chat_get_type              (void);

GossipChatView *  gossip_chat_get_view              (GossipChat       *chat);
GossipChatWindow *gossip_chat_get_window            (GossipChat       *chat);
void              gossip_chat_set_window            (GossipChat       *chat,
						     GossipChatWindow *window);
void              gossip_chat_present               (GossipChat       *chat);
void              gossip_chat_clear                 (GossipChat       *chat);
void              gossip_chat_scroll_down           (GossipChat       *chat);
void              gossip_chat_cut                   (GossipChat       *chat);
void              gossip_chat_copy                  (GossipChat       *chat);
void              gossip_chat_paste                 (GossipChat       *chat);
const gchar *     gossip_chat_get_name              (GossipChat       *chat);
gchar *           gossip_chat_get_tooltip           (GossipChat       *chat);
const gchar *     gossip_chat_get_status_icon_name  (GossipChat       *chat);
GossipContact *   gossip_chat_get_contact           (GossipChat       *chat);
GossipContact *   gossip_chat_get_own_contact       (GossipChat       *chat);
GtkWidget *       gossip_chat_get_widget            (GossipChat       *chat);
gboolean          gossip_chat_get_show_contacts     (GossipChat       *chat);
void              gossip_chat_set_show_contacts     (GossipChat       *chat,
						     gboolean          show);
gboolean          gossip_chat_is_group_chat         (GossipChat       *chat);
gboolean          gossip_chat_is_connected          (GossipChat       *chat);

void              gossip_chat_save_geometry         (GossipChat       *chat,
						     gint              x,
						     gint              y,
						     gint              w,
						     gint              h);
void              gossip_chat_load_geometry         (GossipChat       *chat,
						     gint             *x,
						     gint             *y,
						     gint             *w,
						     gint             *h);
void              gossip_chat_set_tp_chat           (GossipChat       *chat,
						     EmpathyTpChat    *tp_chat);
const gchar *     gossip_chat_get_id                (GossipChat       *chat);

/* For spell checker dialog to correct the misspelled word. */
gboolean          gossip_chat_get_is_command        (const gchar      *str);
void              gossip_chat_correct_word          (GossipChat       *chat,
						     GtkTextIter       start,
						     GtkTextIter       end,
						     const gchar      *new_word);
gboolean          gossip_chat_should_play_sound     (GossipChat       *chat);
gboolean          gossip_chat_should_highlight_nick (GossipMessage    *message);

G_END_DECLS

#endif /* __GOSSIP_CHAT_H__ */
