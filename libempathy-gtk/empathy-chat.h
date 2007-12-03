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

#ifndef __EMPATHY_CHAT_H__
#define __EMPATHY_CHAT_H__

#include <glib-object.h>

#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-message.h>
#include <libempathy/empathy-tp-chat.h>

#include "empathy-chat-view.h"
#include "empathy-spell.h" 

G_BEGIN_DECLS

#define EMPATHY_TYPE_CHAT         (empathy_chat_get_type ())
#define EMPATHY_CHAT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CHAT, EmpathyChat))
#define EMPATHY_CHAT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_CHAT, EmpathyChatClass))
#define EMPATHY_IS_CHAT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CHAT))
#define EMPATHY_IS_CHAT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CHAT))
#define EMPATHY_CHAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CHAT, EmpathyChatClass))

typedef struct _EmpathyChat       EmpathyChat;
typedef struct _EmpathyChatClass  EmpathyChatClass;
typedef struct _EmpathyChatPriv   EmpathyChatPriv;

#include "empathy-chat-window.h"

struct _EmpathyChat {
	GObject          parent;

	/* Protected */
	EmpathyChatView *view;
	GtkWidget       *input_text_view;
};

struct _EmpathyChatClass {
	GObjectClass parent;

	/* VTable */
	const gchar *    (*get_name)            (EmpathyChat   *chat);
	gchar *          (*get_tooltip)         (EmpathyChat   *chat);
	const gchar *    (*get_status_icon_name)(EmpathyChat   *chat);
	GtkWidget *      (*get_widget)          (EmpathyChat   *chat);
	gboolean         (*is_group_chat)       (EmpathyChat   *chat);
	void             (*set_tp_chat)         (EmpathyChat   *chat,
						 EmpathyTpChat *tp_chat);
	gboolean         (*key_press_event)     (EmpathyChat   *chat,
						 GdkEventKey   *event);
};

GType              empathy_chat_get_type              (void);

EmpathyChatView *  empathy_chat_get_view              (EmpathyChat       *chat);
EmpathyChatWindow *empathy_chat_get_window            (EmpathyChat       *chat);
void               empathy_chat_set_window            (EmpathyChat       *chat,
						       EmpathyChatWindow *window);
void               empathy_chat_present               (EmpathyChat       *chat);
void               empathy_chat_clear                 (EmpathyChat       *chat);
void               empathy_chat_scroll_down           (EmpathyChat       *chat);
void               empathy_chat_cut                   (EmpathyChat       *chat);
void               empathy_chat_copy                  (EmpathyChat       *chat);
void               empathy_chat_paste                 (EmpathyChat       *chat);
const gchar *      empathy_chat_get_name              (EmpathyChat       *chat);
gchar *            empathy_chat_get_tooltip           (EmpathyChat       *chat);
const gchar *      empathy_chat_get_status_icon_name  (EmpathyChat       *chat);
GtkWidget *        empathy_chat_get_widget            (EmpathyChat       *chat);
gboolean           empathy_chat_is_group_chat         (EmpathyChat       *chat);
gboolean           empathy_chat_is_connected          (EmpathyChat       *chat);
void               empathy_chat_save_geometry         (EmpathyChat       *chat,
						       gint               x,
						       gint               y,
						       gint               w,
						       gint               h);
void               empathy_chat_load_geometry         (EmpathyChat       *chat,
						       gint              *x,
						       gint              *y,
						       gint              *w,
						       gint              *h);
void               empathy_chat_set_tp_chat           (EmpathyChat       *chat,
						       EmpathyTpChat     *tp_chat);
const gchar *      empathy_chat_get_id                (EmpathyChat       *chat);
McAccount *        empathy_chat_get_account           (EmpathyChat       *chat);

/* For spell checker dialog to correct the misspelled word. */
gboolean           empathy_chat_get_is_command        (const gchar      *str);
void               empathy_chat_correct_word          (EmpathyChat       *chat,
						       GtkTextIter       start,
						       GtkTextIter       end,
						       const gchar      *new_word);
gboolean           empathy_chat_should_play_sound     (EmpathyChat       *chat);
gboolean           empathy_chat_should_highlight_nick (EmpathyMessage    *message);

G_END_DECLS

#endif /* __EMPATHY_CHAT_H__ */
