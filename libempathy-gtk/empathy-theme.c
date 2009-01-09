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

#include <config.h>

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <libempathy/empathy-utils.h>

#include "empathy-chat.h"
#include "empathy-conf.h"
#include "empathy-theme.h"
#include "empathy-smiley-manager.h"

/* Number of seconds between timestamps when using normal mode, 5 minutes. */
#define TIMESTAMP_INTERVAL 300

#define SCHEMES "(https?|ftps?|nntp|news|javascript|about|ghelp|apt|telnet|"\
		"file|webcal|mailto)"
#define BODY "([^\\ ]+)"
#define END_BODY "([^\\ ]*[^,;\?><()\\ \"\\.])"
#define URI_REGEX "("SCHEMES"://"END_BODY")" \
		  "|((mailto:)?"BODY"@"BODY"\\."END_BODY")"\
		  "|((www|ftp)\\."END_BODY")"
static GRegex *uri_regex = NULL;

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyTheme)

typedef struct {
	EmpathySmileyManager *smiley_manager;
	gboolean show_avatars;
} EmpathyThemePriv;

static void         theme_finalize            (GObject            *object);
static void         theme_get_property        (GObject            *object,
					       guint               param_id,
					       GValue             *value,
					       GParamSpec         *pspec);
static void         theme_set_property        (GObject            *object,
					       guint               param_id,
					       const GValue       *value,
					       GParamSpec         *pspec);


G_DEFINE_TYPE (EmpathyTheme, empathy_theme, G_TYPE_OBJECT);

enum {
	PROP_0,
	PROP_SHOW_AVATARS
};

static void
empathy_theme_class_init (EmpathyThemeClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize     = theme_finalize;
	object_class->get_property = theme_get_property;
	object_class->set_property = theme_set_property;

	class->update_view      = NULL;
	class->append_message   = NULL;
	class->append_event     = NULL;
	class->append_timestamp = NULL;
	class->append_spacing   = NULL;

	g_object_class_install_property (object_class,
					 PROP_SHOW_AVATARS,
					 g_param_spec_boolean ("show-avatars",
							       "", "",
							       TRUE,
							       G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EmpathyThemePriv));
}

static void
empathy_theme_init (EmpathyTheme *theme)
{
	EmpathyThemePriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (theme,
		EMPATHY_TYPE_THEME, EmpathyThemePriv);

	theme->priv = priv;
	priv->smiley_manager = empathy_smiley_manager_dup_singleton ();
}

static void
theme_finalize (GObject *object)
{
	EmpathyThemePriv *priv;

	priv = GET_PRIV (object);

	if (priv->smiley_manager) {
		g_object_unref (priv->smiley_manager);
	}

	(G_OBJECT_CLASS (empathy_theme_parent_class)->finalize) (object);
}

static void
theme_get_property (GObject    *object,
		    guint       param_id,
		    GValue     *value,
		    GParamSpec *pspec)
{
	EmpathyThemePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_SHOW_AVATARS:
		g_value_set_boolean (value, priv->show_avatars);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
theme_set_property (GObject      *object,
                    guint         param_id,
                    const GValue *value,
                    GParamSpec   *pspec)
{
	EmpathyThemePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_SHOW_AVATARS:
		empathy_theme_set_show_avatars (EMPATHY_THEME (object),
						g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

void
empathy_theme_maybe_append_date_and_time (EmpathyTheme        *theme,
					 EmpathyChatView     *view,
					 EmpathyMessage      *message)
{
	time_t    timestamp;
	GDate    *date, *last_date;
	gboolean  append_date, append_time;

	date = empathy_message_get_date_and_time (message, &timestamp);

	last_date = g_date_new ();
	g_date_set_time_t (last_date, empathy_chat_view_get_last_timestamp (view));

	append_date = FALSE;
	append_time = FALSE;

	if (g_date_compare (date, last_date) > 0) {
		append_date = TRUE;
		append_time = TRUE;
	}
	
	g_date_free (last_date);
	g_date_free (date);

	if (empathy_chat_view_get_last_timestamp (view) + TIMESTAMP_INTERVAL < timestamp) {
		append_time = TRUE;
	}

	if (append_time || append_date) {
		empathy_theme_append_timestamp (theme, view, message,
					       append_date, append_time);
	}
}

void
empathy_theme_update_view (EmpathyTheme    *theme,
			   EmpathyChatView *view)
{
	if (!EMPATHY_THEME_GET_CLASS(theme)->update_view) {
		g_error ("Theme must override update_view");
	}

	EMPATHY_THEME_GET_CLASS(theme)->update_view (theme, view);
}

void
empathy_theme_append_message (EmpathyTheme        *theme,
			     EmpathyChatView     *view,
			     EmpathyMessage      *message)
{
	if (!EMPATHY_THEME_GET_CLASS(theme)->append_message) {
		g_warning ("Theme should override append_message");
		return;
	}

	EMPATHY_THEME_GET_CLASS(theme)->append_message (theme, view, message);
}

static void
theme_insert_text_with_emoticons (GtkTextBuffer *buf,
				  GtkTextIter   *iter,
				  const gchar   *str,
				  EmpathySmileyManager *smiley_manager)
{
	gboolean             use_smileys = FALSE;
	GSList              *smileys, *l;

	empathy_conf_get_bool (empathy_conf_get (),
			      EMPATHY_PREFS_CHAT_SHOW_SMILEYS,
			      &use_smileys);

	if (!use_smileys) {
		gtk_text_buffer_insert (buf, iter, str, -1);
		return;
	}

	smileys = empathy_smiley_manager_parse (smiley_manager, str);
	for (l = smileys; l; l = l->next) {
		EmpathySmiley *smiley;

		smiley = l->data;
		if (smiley->pixbuf) {
			gtk_text_buffer_insert_pixbuf (buf, iter, smiley->pixbuf);
		} else {
			gtk_text_buffer_insert (buf, iter, smiley->str, -1);
		}
		empathy_smiley_free (smiley);
	}
	g_slist_free (smileys);
}

void
empathy_theme_append_text (EmpathyTheme        *theme,
			  EmpathyChatView     *view,
			  const gchar        *body,
			  const gchar        *tag,
			  const gchar        *link_tag)
{
	EmpathyThemePriv *priv;
	GtkTextBuffer   *buffer;
	GtkTextIter      start_iter, end_iter;
	GtkTextMark     *mark;
	GtkTextIter      iter;
	GMatchInfo      *match_info;
	gboolean         match;
	gint             last = 0;
	gint             s = 0, e = 0;
	gchar           *tmp;

	priv = GET_PRIV (theme);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gtk_text_buffer_get_end_iter (buffer, &start_iter);
	mark = gtk_text_buffer_create_mark (buffer, NULL, &start_iter, TRUE);

	if (!uri_regex) {
		uri_regex = g_regex_new (URI_REGEX, 0, 0, NULL);
	}

	for (match = g_regex_match (uri_regex, body, 0, &match_info); match;
	     match = g_match_info_next (match_info, NULL)) {
		if (!g_match_info_fetch_pos (match_info, 0, &s, &e))
			continue;

		if (s > last) {
			tmp = empathy_substring (body, last, s);

			gtk_text_buffer_get_end_iter (buffer, &iter);
			theme_insert_text_with_emoticons (buffer,
							  &iter,
							  tmp,
							  priv->smiley_manager);
			g_free (tmp);
		}

		tmp = empathy_substring (body, s, e);

		gtk_text_buffer_get_end_iter (buffer, &iter);
		if (!link_tag) {
			gtk_text_buffer_insert (buffer, &iter,
						tmp, -1);
		} else {
			gtk_text_buffer_insert_with_tags_by_name (buffer,
								  &iter,
								  tmp,
								  -1,
								  link_tag,
								  "link",
								  NULL);
		}

		g_free (tmp);
		last = e;
	}
	g_match_info_free (match_info);

	if (last < strlen (body)) {
		gtk_text_buffer_get_end_iter (buffer, &iter);
		theme_insert_text_with_emoticons (buffer,
						  &iter,
						  body + last,
						  priv->smiley_manager);
	}

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert (buffer, &iter, "\n", 1);

	/* Apply the style to the inserted text. */
	gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, mark);
	gtk_text_buffer_get_end_iter (buffer, &end_iter);

	gtk_text_buffer_apply_tag_by_name (buffer,
					   tag,
					   &start_iter,
					   &end_iter);

	gtk_text_buffer_delete_mark (buffer, mark);
}

void 
empathy_theme_append_event (EmpathyTheme        *theme,
			   EmpathyChatView     *view,
			   const gchar        *str)
{
	if (!EMPATHY_THEME_GET_CLASS(theme)->append_event) {
		return;
	}

	EMPATHY_THEME_GET_CLASS(theme)->append_event (theme, view, str);
}

void
empathy_theme_append_spacing (EmpathyTheme        *theme, 
			     EmpathyChatView     *view)
{
	if (!EMPATHY_THEME_GET_CLASS(theme)->append_spacing) {
		return;
	}

	EMPATHY_THEME_GET_CLASS(theme)->append_spacing (theme, view);
}


void 
empathy_theme_append_timestamp (EmpathyTheme        *theme,
			       EmpathyChatView     *view,
			       EmpathyMessage      *message,
			       gboolean            show_date,
			       gboolean            show_time)
{
	if (!EMPATHY_THEME_GET_CLASS(theme)->append_timestamp) {
		return;
	}

	EMPATHY_THEME_GET_CLASS(theme)->append_timestamp (theme, view,
							 message, show_date,
							 show_time);
}

gboolean
empathy_theme_get_show_avatars (EmpathyTheme *theme)
{
	EmpathyThemePriv *priv;

	g_return_val_if_fail (EMPATHY_IS_THEME (theme), FALSE);

	priv = GET_PRIV (theme);

	return priv->show_avatars;
}

void
empathy_theme_set_show_avatars (EmpathyTheme *theme, gboolean show)
{
	EmpathyThemePriv *priv;

	g_return_if_fail (EMPATHY_IS_THEME (theme));

	priv = GET_PRIV (theme);

	priv->show_avatars = show;

	g_object_notify (G_OBJECT (theme), "show-avatars");
}

