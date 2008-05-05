/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
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
 */

#ifndef __EMPATHY_CHAT_VIEW_H__
#define __EMPATHY_CHAT_VIEW_H__

#include <gtk/gtktextview.h>

#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-message.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_CHAT_VIEW         (empathy_chat_view_get_type ())
#define EMPATHY_CHAT_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CHAT_VIEW, EmpathyChatView))
#define EMPATHY_CHAT_VIEW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_CHAT_VIEW, EmpathyChatViewClass))
#define EMPATHY_IS_CHAT_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CHAT_VIEW))
#define EMPATHY_IS_CHAT_VIEW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CHAT_VIEW))
#define EMPATHY_CHAT_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CHAT_VIEW, EmpathyChatViewClass))

typedef struct _EmpathyChatView      EmpathyChatView;
typedef struct _EmpathyChatViewClass EmpathyChatViewClass;

#include "empathy-theme.h"

struct _EmpathyChatView {
	GtkTextView parent;
	gpointer priv;
};

struct _EmpathyChatViewClass {
	GtkTextViewClass parent_class;
};

GType            empathy_chat_view_get_type             (void) G_GNUC_CONST;
EmpathyChatView *empathy_chat_view_new                  (void);
void             empathy_chat_view_append_message       (EmpathyChatView *view,
							 EmpathyMessage  *msg);
void             empathy_chat_view_append_event         (EmpathyChatView *view,
							 const gchar     *str);
void             empathy_chat_view_append_button        (EmpathyChatView *view,
							 const gchar     *message,
							 GtkWidget       *button1,
							 GtkWidget       *button2);
void             empathy_chat_view_scroll               (EmpathyChatView *view,
							 gboolean         allow_scrolling);
void             empathy_chat_view_scroll_down          (EmpathyChatView *view);
gboolean         empathy_chat_view_get_selection_bounds (EmpathyChatView *view,
							 GtkTextIter     *start,
							 GtkTextIter     *end);
void             empathy_chat_view_clear                (EmpathyChatView *view);
gboolean         empathy_chat_view_find_previous        (EmpathyChatView *view,
							 const gchar     *search_criteria,
							 gboolean         new_search);
gboolean         empathy_chat_view_find_next            (EmpathyChatView *view,
							 const gchar     *search_criteria,
							 gboolean         new_search);
void             empathy_chat_view_find_abilities       (EmpathyChatView *view,
							 const gchar     *search_criteria,
							 gboolean        *can_do_previous,
							 gboolean        *can_do_next);
void             empathy_chat_view_highlight            (EmpathyChatView *view,
							 const gchar     *text);
void             empathy_chat_view_copy_clipboard       (EmpathyChatView *view);
EmpathyTheme *   empathy_chat_view_get_theme            (EmpathyChatView *view);
void             empathy_chat_view_set_theme            (EmpathyChatView *view,
							 EmpathyTheme    *theme);
void             empathy_chat_view_set_margin           (EmpathyChatView *view,
							 gint             margin);
GtkWidget *      empathy_chat_view_get_smiley_menu      (GCallback        callback,
							 gpointer         user_data);
time_t           empathy_chat_view_get_last_timestamp   (EmpathyChatView *view);
void             empathy_chat_view_set_last_timestamp   (EmpathyChatView *view,
							 time_t           timestamp);
EmpathyContact * empathy_chat_view_get_last_contact     (EmpathyChatView *view);
GdkPixbuf *      empathy_chat_view_get_avatar_pixbuf_with_cache (EmpathyContact *contact);

G_END_DECLS

#endif /* __EMPATHY_CHAT_VIEW_H__ */
