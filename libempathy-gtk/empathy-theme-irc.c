/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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
 */

#include "config.h"

#include <glib/gi18n.h>
#include <libempathy/empathy-debug.h>

#include "empathy-chat.h"
#include "empathy-theme-utils.h"
#include "empathy-theme-irc.h"

#define DEBUG_DOMAIN "Theme"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EMPATHY_TYPE_THEME_IRC, EmpathyThemeIrcPriv))

typedef struct _EmpathyThemeIrcPriv EmpathyThemeIrcPriv;

struct _EmpathyThemeIrcPriv {
	gint my_prop;
};

static void         theme_irc_finalize      (GObject             *object);
static void         theme_irc_get_property  (GObject             *object,
					     guint                param_id,
					     GValue              *value,
					     GParamSpec          *pspec);
static void         theme_irc_set_property  (GObject             *object,
					     guint                param_id,
					     const GValue        *value,
					     GParamSpec          *pspec);
static EmpathyThemeContext *
theme_irc_setup_with_view                      (EmpathyTheme         *theme,
						EmpathyChatView      *view);
static void         theme_irc_detach_from_view (EmpathyTheme        *theme,
						EmpathyThemeContext *context,
						EmpathyChatView     *view);
static void         theme_irc_append_message   (EmpathyTheme        *theme,
						EmpathyThemeContext *context,
						EmpathyChatView     *view,
						EmpathyMessage      *message);
static void         theme_irc_append_event     (EmpathyTheme        *theme,
						EmpathyThemeContext *context,
						EmpathyChatView     *view,
						const gchar        *str);
static void         theme_irc_append_timestamp (EmpathyTheme        *theme,
						EmpathyThemeContext *context,
						EmpathyChatView     *view,
						EmpathyMessage      *message,
						gboolean            show_date,
						gboolean            show_time);
static void         theme_irc_append_spacing   (EmpathyTheme        *theme,
						EmpathyThemeContext *context,
						EmpathyChatView     *view);


enum {
	PROP_0,
	PROP_MY_PROP
};

G_DEFINE_TYPE (EmpathyThemeIrc, empathy_theme_irc, EMPATHY_TYPE_THEME);

static void
empathy_theme_irc_class_init (EmpathyThemeIrcClass *class)
{
	GObjectClass *object_class;
	EmpathyThemeClass *theme_class;

	object_class = G_OBJECT_CLASS (class);
	theme_class  = EMPATHY_THEME_CLASS (class);

	object_class->finalize     = theme_irc_finalize;
	object_class->get_property = theme_irc_get_property;
	object_class->set_property = theme_irc_set_property;

	theme_class->setup_with_view  = theme_irc_setup_with_view;
	theme_class->detach_from_view = theme_irc_detach_from_view;
	theme_class->append_message   = theme_irc_append_message;
	theme_class->append_event     = theme_irc_append_event;
	theme_class->append_timestamp = theme_irc_append_timestamp;
	theme_class->append_spacing   = theme_irc_append_spacing;

	g_object_class_install_property (object_class,
					 PROP_MY_PROP,
					 g_param_spec_int ("my-prop",
							   "",
							   "",
							   0, 1,
							   1,
							   G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EmpathyThemeIrcPriv));
}

static void
empathy_theme_irc_init (EmpathyThemeIrc *presence)
{
	EmpathyThemeIrcPriv *priv;

	priv = GET_PRIV (presence);
}

static void
theme_irc_finalize (GObject *object)
{
	EmpathyThemeIrcPriv *priv;

	priv = GET_PRIV (object);

	(G_OBJECT_CLASS (empathy_theme_irc_parent_class)->finalize) (object);
}

static void
theme_irc_get_property (GObject    *object,
		    guint       param_id,
		    GValue     *value,
		    GParamSpec *pspec)
{
	EmpathyThemeIrcPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_MY_PROP:
		g_value_set_int (value, priv->my_prop);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}
static void
theme_irc_set_property (GObject      *object,
		    guint         param_id,
		    const GValue *value,
		    GParamSpec   *pspec)
{
	EmpathyThemeIrcPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_MY_PROP:
		priv->my_prop = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
theme_irc_fixup_tag_table (EmpathyTheme *theme, EmpathyChatView *view)
{
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	/* IRC style tags. */
	empathy_theme_utils_ensure_tag_by_name (buffer, "irc-nick-self");
	empathy_theme_utils_ensure_tag_by_name (buffer, "irc-body-self");
	empathy_theme_utils_ensure_tag_by_name (buffer, "irc-action-self");

	empathy_theme_utils_ensure_tag_by_name (buffer, "irc-nick-other");
	empathy_theme_utils_ensure_tag_by_name (buffer, "irc-body-other");
	empathy_theme_utils_ensure_tag_by_name (buffer, "irc-action-other");

	empathy_theme_utils_ensure_tag_by_name (buffer, "irc-nick-highlight");
	empathy_theme_utils_ensure_tag_by_name (buffer, "irc-spacing");
	empathy_theme_utils_ensure_tag_by_name (buffer, "irc-time");
	empathy_theme_utils_ensure_tag_by_name (buffer, "irc-event");
	empathy_theme_utils_ensure_tag_by_name (buffer, "irc-link");
}

static void
theme_irc_apply_theme_classic (EmpathyTheme *theme, EmpathyChatView *view)
{
	EmpathyThemeIrcPriv *priv;
	GtkTextBuffer      *buffer;
	GtkTextTagTable    *table;
	GtkTextTag         *tag;

	priv = GET_PRIV (theme);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	table = gtk_text_buffer_get_tag_table (buffer);

	tag = empathy_theme_utils_init_tag_by_name (table, "irc-spacing");
	g_object_set (tag,
		      "size", 2000,
		      NULL);
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "irc-nick-self");
	g_object_set (tag,
		      "foreground", "sea green",
		      NULL);
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "irc-body-self");
	g_object_set (tag,
		      /* To get the default theme color: */
		      "foreground-set", FALSE,
		      NULL);
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "irc-action-self");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      NULL);
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "irc-nick-highlight");
	g_object_set (tag,
		      "foreground", "indian red",
		      "weight", PANGO_WEIGHT_BOLD,
		      NULL);
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "irc-nick-other");
	g_object_set (tag,
		      "foreground", "skyblue4",
		      NULL);
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "irc-body-other");
	g_object_set (tag,
		      /* To get the default theme color: */
		      "foreground-set", FALSE,
		      NULL);
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "irc-action-other");
	g_object_set (tag,
		      "foreground", "brown4",
		      "style", PANGO_STYLE_ITALIC,
		      NULL);
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "irc-time");
	g_object_set (tag,
		      "foreground", "darkgrey",
		      "justification", GTK_JUSTIFY_CENTER,
		      NULL);
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "irc-event");
	g_object_set (tag,
		      "foreground", "PeachPuff4",
		      "justification", GTK_JUSTIFY_LEFT,
		      NULL);
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "invite");
	g_object_set (tag,
		      "foreground", "sienna",
		      NULL);
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "irc-link");
	g_object_set (tag,
		      "foreground", "steelblue",
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL);
	empathy_theme_utils_add_tag (table, tag);
}


static EmpathyThemeContext *
theme_irc_setup_with_view (EmpathyTheme *theme, EmpathyChatView *view)
{
	theme_irc_fixup_tag_table (theme, view);
	theme_irc_apply_theme_classic (theme, view);
	empathy_chat_view_set_margin (view, 3);

	return NULL;
}

static void
theme_irc_detach_from_view (EmpathyTheme        *theme,
			    EmpathyThemeContext *context,
			    EmpathyChatView     *view)
{
	/* Free the context */
}

static void
theme_irc_append_message (EmpathyTheme        *theme,
			  EmpathyThemeContext *context,
			  EmpathyChatView     *view,
			  EmpathyMessage      *message)
{
	GtkTextBuffer *buffer;
	const gchar   *name;
	const gchar   *nick_tag;
	const gchar   *body_tag;
	GtkTextIter    iter;
	gchar         *tmp;
	EmpathyContact *contact;

	empathy_theme_maybe_append_date_and_time (theme, context, view, message);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	contact = empathy_message_get_sender (message);
	name = empathy_contact_get_name (contact);

	if (empathy_message_get_type (message) == EMPATHY_MESSAGE_TYPE_ACTION) {
		if (empathy_contact_is_user (contact)) {
			body_tag = "irc-action-self";
		} else {
			body_tag = "irc-action-other";
		}

		tmp = g_strdup_printf (" * %s %s", 
				       empathy_contact_get_name (contact),
				       empathy_message_get_body (message));
		empathy_theme_append_text (theme, context, view, tmp,
					   body_tag, "irc-link");
		g_free (tmp);
		return;
	}

	if (empathy_contact_is_user (contact)) {
		nick_tag = "irc-nick-self";
		body_tag = "irc-body-self";
	} else {
		if (empathy_chat_should_highlight_nick (message)) {
			nick_tag = "irc-nick-highlight";
		} else {
			nick_tag = "irc-nick-other";
		}

		body_tag = "irc-body-other";
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
	empathy_theme_append_text (theme, context, view, 
				  empathy_message_get_body (message),
				  body_tag, "irc-link");
}

static void
theme_irc_append_event (EmpathyTheme        *theme,
			EmpathyThemeContext *context,
		    EmpathyChatView     *view,
		    const gchar        *str)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;
	gchar         *msg;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	
	empathy_theme_maybe_append_date_and_time (theme, context, view, NULL);

	gtk_text_buffer_get_end_iter (buffer, &iter);

	msg = g_strdup_printf (" - %s\n", str);
	gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
						  msg, -1,
						  "irc-event",
						  NULL);
	g_free (msg);
}

static void
theme_irc_append_timestamp (EmpathyTheme        *theme,
			    EmpathyThemeContext *context,
			    EmpathyChatView     *view,
			    EmpathyMessage      *message,
			    gboolean            show_date,
			    gboolean            show_time)
{
	GtkTextBuffer *buffer;
	time_t         timestamp;
	GDate         *date;
	GtkTextIter    iter;
	GString       *str;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	date = empathy_message_get_date_and_time (message, &timestamp);

	str = g_string_new (NULL);

	if (show_time || show_date) {
		empathy_theme_append_spacing (theme, 
					     context,
					     view);

		g_string_append (str, "- ");
	}

	if (show_date) {
		gchar buf[256];

		g_date_strftime (buf, 256, _("%A %d %B %Y"), date);
		g_string_append (str, buf);

		if (show_time) {
			g_string_append (str, ", ");
		}
	}

	g_date_free (date);

	if (show_time) {
		gchar *tmp;

		tmp = empathy_time_to_string_local (timestamp, EMPATHY_TIME_FORMAT_DISPLAY_SHORT);
		g_string_append (str, tmp);
		g_free (tmp);
	}

	if (show_time || show_date) {
		g_string_append (str, " -\n");

		gtk_text_buffer_get_end_iter (buffer, &iter);
		gtk_text_buffer_insert_with_tags_by_name (buffer,
							  &iter,
							  str->str, -1,
							  "irc-time",
							  NULL);

		empathy_chat_view_set_last_timestamp (view, timestamp);
	}

	g_string_free (str, TRUE);
}

static void
theme_irc_append_spacing (EmpathyTheme        *theme,
			  EmpathyThemeContext *context,
			  EmpathyChatView     *view)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;

	g_return_if_fail (EMPATHY_IS_THEME (theme));
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  "\n",
						  -1,
						  "cut",
						  "irc-spacing",
						  NULL);
}

