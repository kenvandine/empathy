/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include <libempathy/empathy-utils.h>
#include "empathy-theme-irc.h"
#include "empathy-ui-utils.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyThemeIrc)
typedef struct {
	gpointer dummy;
} EmpathyThemeIrcPriv;

G_DEFINE_TYPE (EmpathyThemeIrc, empathy_theme_irc, EMPATHY_TYPE_CHAT_TEXT_VIEW);

static void
theme_irc_create_tags (EmpathyThemeIrc *theme)
{
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (theme));

	gtk_text_buffer_create_tag (buffer, EMPATHY_THEME_IRC_TAG_NICK_SELF, NULL);
	gtk_text_buffer_create_tag (buffer, EMPATHY_THEME_IRC_TAG_NICK_OTHER, NULL);
	gtk_text_buffer_create_tag (buffer, EMPATHY_THEME_IRC_TAG_NICK_HIGHLIGHT, NULL);
}

static void
theme_irc_append_message (EmpathyChatTextView *view,
			  EmpathyMessage      *message)
{
	GtkTextBuffer *buffer;
	const gchar   *name;
	const gchar   *nick_tag;
	GtkTextIter    iter;
	gchar         *tmp;
	EmpathyContact *contact;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	contact = empathy_message_get_sender (message);
	name = empathy_contact_get_name (contact);

	if (empathy_message_get_tptype (message) == TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION) {
		tmp = g_strdup_printf (" * %s %s",
				       empathy_contact_get_name (contact),
				       empathy_message_get_body (message));
		empathy_chat_text_view_append_body (view, tmp,
						    EMPATHY_CHAT_TEXT_VIEW_TAG_ACTION);
		g_free (tmp);
		return;
	}

	if (empathy_contact_is_user (contact)) {
		nick_tag = EMPATHY_THEME_IRC_TAG_NICK_SELF;
	} else {
		if (empathy_message_should_highlight (message)) {
			nick_tag = EMPATHY_THEME_IRC_TAG_NICK_HIGHLIGHT;
		} else {
			nick_tag = EMPATHY_THEME_IRC_TAG_NICK_OTHER;
		}
	}

	gtk_text_buffer_get_end_iter (buffer, &iter);

	/* The nickname. */
	tmp = g_strdup_printf ("%s: ", name);
	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  tmp,
						  -1,
						  "cut",
						  nick_tag,
						  NULL);
	g_free (tmp);

	/* The text body. */
	empathy_chat_text_view_append_body (view,
					    empathy_message_get_body (message),
					    EMPATHY_CHAT_TEXT_VIEW_TAG_BODY);
}

static void
empathy_theme_irc_class_init (EmpathyThemeIrcClass *class)
{
	GObjectClass             *object_class;
	EmpathyChatTextViewClass *chat_text_view_class;

	object_class = G_OBJECT_CLASS (class);
	chat_text_view_class = EMPATHY_CHAT_TEXT_VIEW_CLASS (class);

	chat_text_view_class->append_message = theme_irc_append_message;

	g_type_class_add_private (object_class, sizeof (EmpathyThemeIrcPriv));
}

static void
empathy_theme_irc_init (EmpathyThemeIrc *theme)
{
	EmpathyThemeIrcPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (theme,
		EMPATHY_TYPE_THEME_IRC, EmpathyThemeIrcPriv);

	theme->priv = priv;

	theme_irc_create_tags (theme);

	/* Define margin */
	g_object_set (theme,
		      "left-margin", 3,
		      "right-margin", 3,
		      NULL);
}

EmpathyThemeIrc *
empathy_theme_irc_new (void)
{
	return g_object_new (EMPATHY_TYPE_THEME_IRC, NULL);
}

