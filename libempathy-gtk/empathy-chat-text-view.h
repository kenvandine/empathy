/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
 * Copyright (C) 2008 Collabora Ltd.
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_CHAT_TEXT_VIEW_H__
#define __EMPATHY_CHAT_TEXT_VIEW_H__

#include <gtk/gtk.h>

#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-message.h>

#include "empathy-chat-view.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_CHAT_TEXT_VIEW         (empathy_chat_text_view_get_type ())
#define EMPATHY_CHAT_TEXT_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CHAT_TEXT_VIEW, EmpathyChatTextView))
#define EMPATHY_CHAT_TEXT_VIEW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_CHAT_TEXT_VIEW, EmpathyChatTextViewClass))
#define EMPATHY_IS_CHAT_TEXT_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CHAT_TEXT_VIEW))
#define EMPATHY_IS_CHAT_TEXT_VIEW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CHAT_TEXT_VIEW))
#define EMPATHY_CHAT_TEXT_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CHAT_TEXT_VIEW, EmpathyChatTextViewClass))

typedef struct _EmpathyChatTextView      EmpathyChatTextView;
typedef struct _EmpathyChatTextViewClass EmpathyChatTextViewClass;

struct _EmpathyChatTextView {
	GtkTextView parent;
	gpointer priv;
};

struct _EmpathyChatTextViewClass {
	GtkTextViewClass parent_class;

	/* <vtable> */
	void (*append_message) (EmpathyChatTextView *view,
				EmpathyMessage      *message);
};

#define EMPATHY_CHAT_TEXT_VIEW_TAG_CUT "cut"
#define EMPATHY_CHAT_TEXT_VIEW_TAG_HIGHLIGHT "highlight"
#define EMPATHY_CHAT_TEXT_VIEW_TAG_SPACING "spacing"
#define EMPATHY_CHAT_TEXT_VIEW_TAG_TIME "time"
#define EMPATHY_CHAT_TEXT_VIEW_TAG_ACTION "action"
#define EMPATHY_CHAT_TEXT_VIEW_TAG_BODY "body"
#define EMPATHY_CHAT_TEXT_VIEW_TAG_EVENT "event"
#define EMPATHY_CHAT_TEXT_VIEW_TAG_LINK "link"

GType                empathy_chat_text_view_get_type         (void) G_GNUC_CONST;
EmpathyContact *     empathy_chat_text_view_get_last_contact (EmpathyChatTextView *view);
void                 empathy_chat_text_view_set_only_if_date (EmpathyChatTextView *view,
							      gboolean             only_if_date);
void                 empathy_chat_text_view_append_body      (EmpathyChatTextView *view,
							      const gchar         *body,
							      const gchar         *tag);
void                 empathy_chat_text_view_append_spacing   (EmpathyChatTextView *view);
GtkTextTag *         empathy_chat_text_view_tag_set          (EmpathyChatTextView *view,
							      const gchar         *tag_name,
							      const gchar         *first_property_name,
							      ...);

G_END_DECLS

#endif /* __EMPATHY_CHAT_TEXT_VIEW_H__ */
