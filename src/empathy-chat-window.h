/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
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

#ifndef __EMPATHY_CHAT_WINDOW_H__
#define __EMPATHY_CHAT_WINDOW_H__

#include <glib-object.h>
#include <gtk/gtkwidget.h>

#include <libmissioncontrol/mc-account.h>
#include <libempathy-gtk/empathy-chat.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_CHAT_WINDOW         (empathy_chat_window_get_type ())
#define EMPATHY_CHAT_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CHAT_WINDOW, EmpathyChatWindow))
#define EMPATHY_CHAT_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_CHAT_WINDOW, EmpathyChatWindowClass))
#define EMPATHY_IS_CHAT_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CHAT_WINDOW))
#define EMPATHY_IS_CHAT_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CHAT_WINDOW))
#define EMPATHY_CHAT_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CHAT_WINDOW, EmpathyChatWindowClass))

typedef struct _EmpathyChatWindow      EmpathyChatWindow;
typedef struct _EmpathyChatWindowClass EmpathyChatWindowClass;

struct _EmpathyChatWindow {
	GObject parent;
	gpointer priv;
};

struct _EmpathyChatWindowClass {
	GObjectClass parent_class;
};

GType              empathy_chat_window_get_type       (void);
EmpathyChatWindow *empathy_chat_window_get_default    (void);
EmpathyChatWindow *empathy_chat_window_new            (void);
GtkWidget *        empathy_chat_window_get_dialog     (EmpathyChatWindow *window);
void               empathy_chat_window_add_chat       (EmpathyChatWindow *window,
						       EmpathyChat       *chat);
void               empathy_chat_window_remove_chat    (EmpathyChatWindow *window,
						       EmpathyChat       *chat);
void               empathy_chat_window_move_chat      (EmpathyChatWindow *old_window,
						       EmpathyChatWindow *new_window,
						       EmpathyChat       *chat);
void               empathy_chat_window_switch_to_chat (EmpathyChatWindow *window,
						       EmpathyChat       *chat);
gboolean           empathy_chat_window_has_focus      (EmpathyChatWindow *window);
EmpathyChat *      empathy_chat_window_find_chat      (McAccount        *account,
						       const gchar      *id);
void               empathy_chat_window_present_chat   (EmpathyChat      *chat);


G_END_DECLS

#endif /* __EMPATHY_CHAT_WINDOW_H__ */
