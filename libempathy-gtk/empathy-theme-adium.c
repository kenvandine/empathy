/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008-2009 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <string.h>
#include <glib/gi18n.h>

#include <webkit/webkitnetworkrequest.h>

#include <libempathy/empathy-time.h>
#include <libempathy/empathy-utils.h>

#include "empathy-theme-adium.h"
#include "empathy-smiley-manager.h"
#include "empathy-conf.h"
#include "empathy-ui-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_CHAT
#include <libempathy/empathy-debug.h>

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyThemeAdium)

/* "Join" consecutive messages with timestamps within five minutes */
#define MESSAGE_JOIN_PERIOD 5*60

typedef struct {
	EmpathySmileyManager *smiley_manager;
	EmpathyContact       *last_contact;
	time_t                last_timestamp;
	gboolean              page_loaded;
	GList                *message_queue;
	gchar                *path;
	gchar                *default_avatar_filename;
	gchar                *template_html;
	gchar                *basedir;
	gchar                *in_content_html;
	gsize                 in_content_len;
	gchar                *in_nextcontent_html;
	gsize                 in_nextcontent_len;
	gchar                *out_content_html;
	gsize                 out_content_len;
	gchar                *out_nextcontent_html;
	gsize                 out_nextcontent_len;
	gchar                *status_html;
	gsize                 status_len;
} EmpathyThemeAdiumPriv;

static void theme_adium_iface_init (EmpathyChatViewIface *iface);

enum {
	PROP_0,
	PROP_PATH,
};

G_DEFINE_TYPE_WITH_CODE (EmpathyThemeAdium, empathy_theme_adium,
			 WEBKIT_TYPE_WEB_VIEW,
			 G_IMPLEMENT_INTERFACE (EMPATHY_TYPE_CHAT_VIEW,
						theme_adium_iface_init));

static void
theme_adium_load (EmpathyThemeAdium *theme)
{
	EmpathyThemeAdiumPriv *priv = GET_PRIV (theme);
	gchar                 *file;
	gchar                 *template_html = NULL;
	gsize                  template_len;
	GString               *string;
	gchar                **strv = NULL;
	gchar                 *css_path;
	guint                  len = 0;
	guint                  i = 0;
	gchar                 *basedir_uri;

	priv->basedir = g_strconcat (priv->path, G_DIR_SEPARATOR_S "Contents" G_DIR_SEPARATOR_S "Resources" G_DIR_SEPARATOR_S, NULL);
	basedir_uri = g_strconcat ("file://", priv->basedir, NULL);

	/* Load html files */
	file = g_build_filename (priv->basedir, "Incoming", "Content.html", NULL);
	g_file_get_contents (file, &priv->in_content_html, &priv->in_content_len, NULL);
	g_free (file);

	file = g_build_filename (priv->basedir, "Incoming", "NextContent.html", NULL);
	g_file_get_contents (file, &priv->in_nextcontent_html, &priv->in_nextcontent_len, NULL);
	g_free (file);

	file = g_build_filename (priv->basedir, "Outgoing", "Content.html", NULL);
	g_file_get_contents (file, &priv->out_content_html, &priv->out_content_len, NULL);
	g_free (file);

	file = g_build_filename (priv->basedir, "Outgoing", "NextContent.html", NULL);
	g_file_get_contents (file, &priv->out_nextcontent_html, &priv->out_nextcontent_len, NULL);
	g_free (file);

	file = g_build_filename (priv->basedir, "Status.html", NULL);
	g_file_get_contents (file, &priv->status_html, &priv->status_len, NULL);
	g_free (file);

	css_path = g_build_filename (priv->basedir, "main.css", NULL);

	/* There is 2 formats for Template.html: The old one has 4 parameters,
	 * the new one has 5 parameters. */
	file = g_build_filename (priv->basedir, "Template.html", NULL);
	if (g_file_get_contents (file, &template_html, &template_len, NULL)) {
		strv = g_strsplit (template_html, "%@", -1);
		len = g_strv_length (strv);
	}
	g_free (file);

	if (len != 5 && len != 6) {
		/* Either the theme has no template or it don't have the good
		 * number of parameters. Fallback to use our own template. */
		g_free (template_html);
		g_strfreev (strv);

		file = empathy_file_lookup ("Template.html", "data");
		g_file_get_contents (file, &template_html, &template_len, NULL);
		g_free (file);
		strv = g_strsplit (template_html, "%@", -1);
		len = g_strv_length (strv);
	}

	/* Replace %@ with the needed information in the template html. */
	string = g_string_sized_new (template_len);
	g_string_append (string, strv[i++]);
	g_string_append (string, priv->basedir);
	g_string_append (string, strv[i++]);
	if (len == 6) {
		/* We include main.css by default */
		g_string_append_printf (string, "@import url(\"%s\");", css_path);
		g_string_append (string, strv[i++]);
		/* FIXME: We should set the variant css here */
		g_string_append (string, "");
	} else {
		/* FIXME: We should set main.css OR the variant css */
		g_string_append (string, css_path);
	}
	g_string_append (string, strv[i++]);
	g_string_append (string, ""); /* We don't want header */
	g_string_append (string, strv[i++]);
	g_string_append (string, ""); /* FIXME: We don't support footer yet */
	g_string_append (string, strv[i++]);
	priv->template_html = g_string_free (string, FALSE);

	/* Load the template */
	webkit_web_view_load_html_string (WEBKIT_WEB_VIEW (theme),
					  priv->template_html, basedir_uri);

	g_free (basedir_uri);
	g_free (template_html);
	g_free (css_path);
	g_strfreev (strv);
}

static WebKitNavigationResponse
theme_adium_navigation_requested_cb (WebKitWebView        *view,
				     WebKitWebFrame       *frame,
				     WebKitNetworkRequest *request,
				     gpointer              user_data)
{
	const gchar *uri;

	uri = webkit_network_request_get_uri (request);
	empathy_url_show (GTK_WIDGET (view), uri);

	return WEBKIT_NAVIGATION_RESPONSE_IGNORE;
}

static void
theme_adium_populate_popup_cb (WebKitWebView *view,
			       GtkMenu       *menu,
			       gpointer       user_data)
{
	GtkWidget *item;

	/* Remove default menu items */
	gtk_container_foreach (GTK_CONTAINER (menu),
		(GtkCallback) gtk_widget_destroy, NULL);
	
	/* Select all item */
	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_SELECT_ALL, NULL);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
		
	g_signal_connect_swapped (item, "activate",
				  G_CALLBACK (webkit_web_view_select_all),
				  view);

	/* Copy menu item */
	if (webkit_web_view_can_copy_clipboard (view)) {
		item = gtk_image_menu_item_new_from_stock (GTK_STOCK_COPY, NULL);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
		
		g_signal_connect_swapped (item, "activate",
					  G_CALLBACK (webkit_web_view_copy_clipboard),
					  view);
	}

	/* Clear menu item */
	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
		
	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLEAR, NULL);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
		
	g_signal_connect_swapped (item, "activate",
				  G_CALLBACK (empathy_chat_view_clear),
				  view);

	/* FIXME: Add open_link and copy_link when those bugs are fixed:
	 * https://bugs.webkit.org/show_bug.cgi?id=16092
	 * https://bugs.webkit.org/show_bug.cgi?id=16562
	 */
}

static gchar *
theme_adium_parse_body (EmpathyThemeAdium *theme,
			const gchar       *text)
{
	EmpathyThemeAdiumPriv *priv = GET_PRIV (theme);
	gboolean               use_smileys = FALSE;
	GSList                *smileys, *l;
	GString               *string;
	gint                   i;
	GRegex                *uri_regex;
	GMatchInfo            *match_info;
	gboolean               match;
	gchar                 *ret = NULL;
	gint                   prev;

	empathy_conf_get_bool (empathy_conf_get (),
			       EMPATHY_PREFS_CHAT_SHOW_SMILEYS,
			       &use_smileys);

	if (use_smileys) {
		/* Replace smileys by a <img/> tag */
		string = g_string_sized_new (strlen (text));
		smileys = empathy_smiley_manager_parse (priv->smiley_manager, text);
		for (l = smileys; l; l = l->next) {
			EmpathySmiley *smiley;

			smiley = l->data;
			if (smiley->path) {
				g_string_append_printf (string,
							"<abbr title='%s'><img src=\"%s\"/ alt=\"%s\"/></abbr>",
							smiley->str, smiley->path, smiley->str);
			} else {
				gchar *str;

				str = g_markup_escape_text (smiley->str, -1);
				g_string_append (string, str);
				g_free (str);
			}
			empathy_smiley_free (smiley);
		}
		g_slist_free (smileys);

		g_free (ret);
		text = ret = g_string_free (string, FALSE);
	}

	/* Add <a href></a> arround links */
	uri_regex = empathy_uri_regex_dup_singleton ();
	match = g_regex_match (uri_regex, text, 0, &match_info);
	if (match) {
		gint last = 0;
		gint s = 0, e = 0;

		string = g_string_sized_new (strlen (text));
		do {
			g_match_info_fetch_pos (match_info, 0, &s, &e);

			if (s > last) {
				/* Append the text between last link (or the
				 * start of the message) and this link */
				g_string_append_len (string, text + last, s - last);
			}

			/* Append the link inside <a href=""></a> tag */
			g_string_append (string, "<a href=\"");
			g_string_append_len (string, text + s, e - s);
			g_string_append (string, "\">");
			g_string_append_len (string, text + s, e - s);
			g_string_append (string, "</a>");

			last = e;
		} while (g_match_info_next (match_info, NULL));

		if (e < strlen (text)) {
			/* Append the text after the last link */
			g_string_append_len (string, text + e, strlen (text) - e);
		}

		g_free (ret);
		text = ret = g_string_free (string, FALSE);
	}
	g_match_info_free (match_info);
	g_regex_unref (uri_regex);

	/* Replace \n by <br/> */
	string = NULL;
	prev = 0;
	for (i = 0; text[i] != '\0'; i++) {
		if (text[i] == '\n') {
			if (!string ) {
				string = g_string_sized_new (strlen (text));
			}
			g_string_append_len (string, text + prev, i - prev);
			g_string_append (string, "<br/>");
			prev = i + 1;
		}
	}
	if (string) {
		g_string_append (string, text + prev);
		g_free (ret);
		text = ret = g_string_free (string, FALSE);
	}

	return ret;
}

static void
escape_and_append_len (GString *string, const gchar *str, gint len)
{
	while (*str != '\0' && len != 0) {
		switch (*str) {
		case '\\':
			/* \ becomes \\ */
			g_string_append (string, "\\\\");	
			break;
		case '\"':
			/* " becomes \" */
			g_string_append (string, "\\\"");
			break;
		case '\n':
			/* Remove end of lines */
			break;
		default:
			g_string_append_c (string, *str);
		}

		str++;
		len--;
	}
}

static gboolean
theme_adium_match (const gchar **str, const gchar *match)
{
	gint len;

	len = strlen (match);
	if (strncmp (*str, match, len) == 0) {
		*str += len - 1;
		return TRUE;
	}

	return FALSE;
}

static void
theme_adium_append_html (EmpathyThemeAdium *theme,
			 const gchar       *func,
			 const gchar       *html, gsize len,
		         const gchar       *message,
		         const gchar       *avatar_filename,
		         const gchar       *name,
		         time_t             timestamp)
{
	GString     *string;
	const gchar *cur = NULL;
	gchar       *script;

	/* Make some search-and-replace in the html code */
	string = g_string_sized_new (len + strlen (message));
	g_string_append_printf (string, "%s(\"", func);
	for (cur = html; *cur != '\0'; cur++) {
		const gchar *replace = NULL;
		gchar       *dup_replace = NULL;

		if (theme_adium_match (&cur, "%message%")) {
			replace = message;
		} else if (theme_adium_match (&cur, "%userIconPath%")) {
			replace = avatar_filename;
		} else if (theme_adium_match (&cur, "%sender%")) {
			replace = name;
		} else if (theme_adium_match (&cur, "%time")) {
			gchar *format = NULL;
			gchar *end;

			/* Time can be in 2 formats:
			 * %time% or %time{strftime format}%
			 * Extract the time format if provided. */
			if (cur[1] == '{') {
				cur += 2;
				end = strstr (cur, "}%");
				if (!end) {
					/* Invalid string */
					continue;
				}
				format = g_strndup (cur, end - cur);
				cur = end + 1;
			} else {
				cur++;
			}

			dup_replace = empathy_time_to_string_local (timestamp,
				format ? format : EMPATHY_TIME_FORMAT_DISPLAY_SHORT);
			replace = dup_replace;
			g_free (format);
		} else {
			escape_and_append_len (string, cur, 1);
			continue;
		}

		/* Here we have a replacement to make */
		escape_and_append_len (string, replace, -1);
		g_free (dup_replace);
	}
	g_string_append (string, "\")");

	script = g_string_free (string, FALSE);
	webkit_web_view_execute_script (WEBKIT_WEB_VIEW (theme), script);
	g_free (script);
}

static void
theme_adium_append_message (EmpathyChatView *view,
			    EmpathyMessage  *msg)
{
	EmpathyThemeAdium     *theme = EMPATHY_THEME_ADIUM (view);
	EmpathyThemeAdiumPriv *priv = GET_PRIV (theme);
	EmpathyContact        *sender;
	gchar                 *dup_body = NULL;
	const gchar           *body;
	const gchar           *name;
	EmpathyAvatar         *avatar;
	const gchar           *avatar_filename = NULL;
	time_t                 timestamp;
	gchar                 *html = NULL;
	gsize                  len = 0;
	const gchar           *func;

	if (!priv->page_loaded) {
		priv->message_queue = g_list_prepend (priv->message_queue,
						      g_object_ref (msg));
		return;
	}

	/* Get information */
	sender = empathy_message_get_sender (msg);
	timestamp = empathy_message_get_timestamp (msg);
	body = empathy_message_get_body (msg);
	dup_body = theme_adium_parse_body (theme, body);
	if (dup_body) {
		body = dup_body;
	}
	name = empathy_contact_get_name (sender);

	/* If this is a /me, append an event */
	if (empathy_message_get_tptype (msg) == TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION) {
		gchar *str;

		str = g_strdup_printf ("%s %s", name, body);
		empathy_chat_view_append_event (view, str);
		g_free (str);
		g_free (dup_body);
		return;
	}

	/* Get the avatar filename, or a fallback */
	avatar = empathy_contact_get_avatar (sender);
	if (avatar) {
		avatar_filename = avatar->filename;
	}
	if (!avatar_filename) {
		if (!priv->default_avatar_filename) {
			priv->default_avatar_filename =
				empathy_filename_from_icon_name ("stock_person",
								 GTK_ICON_SIZE_DIALOG);
		}
		avatar_filename = priv->default_avatar_filename;
	}

	/* Get the right html/func to add the message */
	func = "appendMessage";
	/*
	 * To mimick Adium's behavior, we only want to join messages
	 * sent within a 5 minute time frame.
	 */
	if (empathy_contact_equal (priv->last_contact, sender) &&
	    (timestamp - priv->last_timestamp < MESSAGE_JOIN_PERIOD)) {
		func = "appendNextMessage";
		if (empathy_contact_is_user (sender)) {
			html = priv->out_nextcontent_html;
			len = priv->out_nextcontent_len;
		}
		if (!html) {
			html = priv->in_nextcontent_html;
			len = priv->in_nextcontent_len;
		}
	}
	if (!html) {
		if (empathy_contact_is_user (sender)) {
			html = priv->out_content_html;
			len = priv->out_content_len;
		}
		if (!html) {
			html = priv->in_content_html;
			len = priv->in_content_len;
		}
	}

	theme_adium_append_html (theme, func, html, len, body, avatar_filename,
				 name, timestamp);

	/* Keep the sender of the last displayed message */
	if (priv->last_contact) {
		g_object_unref (priv->last_contact);
	}
	priv->last_contact = g_object_ref (sender);
	priv->last_timestamp = timestamp;

	g_free (dup_body);
}

static void
theme_adium_append_event (EmpathyChatView *view,
			  const gchar     *str)
{
	EmpathyThemeAdium     *theme = EMPATHY_THEME_ADIUM (view);
	EmpathyThemeAdiumPriv *priv = GET_PRIV (theme);

	if (priv->status_html) {
		theme_adium_append_html (theme, "appendMessage",
					 priv->status_html, priv->status_len,
					 str, NULL, NULL,
					 empathy_time_get_current ());
	}

	/* There is no last contact */
	if (priv->last_contact) {
		g_object_unref (priv->last_contact);
		priv->last_contact = NULL;
	}
}

static void
theme_adium_scroll (EmpathyChatView *view,
		    gboolean         allow_scrolling)
{
	/* FIXME: Is it possible? I guess we need a js function, but I don't
	 * see any... */
}

static void
theme_adium_scroll_down (EmpathyChatView *view)
{
	webkit_web_view_execute_script (WEBKIT_WEB_VIEW (view), "scrollToBottom()");
}

static gboolean
theme_adium_get_has_selection (EmpathyChatView *view)
{
	return webkit_web_view_has_selection (WEBKIT_WEB_VIEW (view));
}

static void
theme_adium_clear (EmpathyChatView *view)
{
	EmpathyThemeAdiumPriv *priv = GET_PRIV (view);
	gchar *basedir_uri;

	priv->page_loaded = FALSE;
	basedir_uri = g_strconcat ("file://", priv->basedir, NULL);
	webkit_web_view_load_html_string (WEBKIT_WEB_VIEW (view),
					  priv->template_html, basedir_uri);
	g_free (basedir_uri);
}

static gboolean
theme_adium_find_previous (EmpathyChatView *view,
			   const gchar     *search_criteria,
			   gboolean         new_search)
{
	return webkit_web_view_search_text (WEBKIT_WEB_VIEW (view),
					    search_criteria, FALSE,
					    FALSE, TRUE);
}

static gboolean
theme_adium_find_next (EmpathyChatView *view,
		       const gchar     *search_criteria,
		       gboolean         new_search)
{
	return webkit_web_view_search_text (WEBKIT_WEB_VIEW (view),
					    search_criteria, FALSE,
					    TRUE, TRUE);
}

static void
theme_adium_find_abilities (EmpathyChatView *view,
			    const gchar    *search_criteria,
			    gboolean       *can_do_previous,
			    gboolean       *can_do_next)
{
	/* FIXME: Does webkit provide an API for that? We have wrap=true in
	 * find_next and find_previous to work around this problem. */
	if (can_do_previous)
		*can_do_previous = TRUE;
	if (can_do_next)
		*can_do_next = TRUE;
}

static void
theme_adium_highlight (EmpathyChatView *view,
		       const gchar     *text)
{
	webkit_web_view_unmark_text_matches (WEBKIT_WEB_VIEW (view));
	webkit_web_view_mark_text_matches (WEBKIT_WEB_VIEW (view),
					   text, FALSE, 0);
	webkit_web_view_set_highlight_text_matches (WEBKIT_WEB_VIEW (view),
						    TRUE);
}

static void
theme_adium_copy_clipboard (EmpathyChatView *view)
{
	webkit_web_view_copy_clipboard (WEBKIT_WEB_VIEW (view));
}

static void
theme_adium_iface_init (EmpathyChatViewIface *iface)
{
	iface->append_message = theme_adium_append_message;
	iface->append_event = theme_adium_append_event;
	iface->scroll = theme_adium_scroll;
	iface->scroll_down = theme_adium_scroll_down;
	iface->get_has_selection = theme_adium_get_has_selection;
	iface->clear = theme_adium_clear;
	iface->find_previous = theme_adium_find_previous;
	iface->find_next = theme_adium_find_next;
	iface->find_abilities = theme_adium_find_abilities;
	iface->highlight = theme_adium_highlight;
	iface->copy_clipboard = theme_adium_copy_clipboard;
}

static void
theme_adium_load_finished_cb (WebKitWebView  *view,
			      WebKitWebFrame *frame,
			      gpointer        user_data)
{
	EmpathyThemeAdiumPriv *priv = GET_PRIV (view);
	EmpathyChatView       *chat_view = EMPATHY_CHAT_VIEW (view);

	DEBUG ("Page loaded");
	priv->page_loaded = TRUE;

	/* Display queued messages */
	priv->message_queue = g_list_reverse (priv->message_queue);
	while (priv->message_queue) {
		EmpathyMessage *message = priv->message_queue->data;

		theme_adium_append_message (chat_view, message);
		priv->message_queue = g_list_remove (priv->message_queue, message);
		g_object_unref (message);
	}
}

static void
theme_adium_finalize (GObject *object)
{
	EmpathyThemeAdiumPriv *priv = GET_PRIV (object);

	g_free (priv->basedir);
	g_free (priv->template_html);
	g_free (priv->in_content_html);
	g_free (priv->in_nextcontent_html);
	g_free (priv->out_content_html);
	g_free (priv->out_nextcontent_html);
	g_free (priv->default_avatar_filename);
	g_free (priv->path);

	G_OBJECT_CLASS (empathy_theme_adium_parent_class)->finalize (object);
}

static void
theme_adium_dispose (GObject *object)
{
	EmpathyThemeAdiumPriv *priv = GET_PRIV (object);

	if (priv->smiley_manager) {
		g_object_unref (priv->smiley_manager);
		priv->smiley_manager = NULL;
	}

	if (priv->last_contact) {
		g_object_unref (priv->last_contact);
		priv->last_contact = NULL;
	}

	G_OBJECT_CLASS (empathy_theme_adium_parent_class)->dispose (object);
}

static void
theme_adium_constructed (GObject *object)
{
	theme_adium_load (EMPATHY_THEME_ADIUM (object));
}

static void
theme_adium_get_property (GObject    *object,
			  guint       param_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
	EmpathyThemeAdiumPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_PATH:
		g_value_set_string (value, priv->path);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
theme_adium_set_property (GObject      *object,
			  guint         param_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
	EmpathyThemeAdiumPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_PATH:
		g_free (priv->path);
		priv->path = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
empathy_theme_adium_class_init (EmpathyThemeAdiumClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = theme_adium_finalize;
	object_class->dispose = theme_adium_dispose;
	object_class->constructed = theme_adium_constructed;
	object_class->get_property = theme_adium_get_property;
	object_class->set_property = theme_adium_set_property;

	g_object_class_install_property (object_class,
					 PROP_PATH,
					 g_param_spec_string ("path",
							      "The theme path",
							      "Path to the adium theme",
							      g_get_home_dir (),
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_READWRITE));


	g_type_class_add_private (object_class, sizeof (EmpathyThemeAdiumPriv));
}

static void
empathy_theme_adium_init (EmpathyThemeAdium *theme)
{
	EmpathyThemeAdiumPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (theme,
		EMPATHY_TYPE_THEME_ADIUM, EmpathyThemeAdiumPriv);

	theme->priv = priv;	

	priv->smiley_manager = empathy_smiley_manager_dup_singleton ();

	g_signal_connect (theme, "load-finished",
			  G_CALLBACK (theme_adium_load_finished_cb),
			  NULL);
	g_signal_connect (theme, "navigation-requested",
			  G_CALLBACK (theme_adium_navigation_requested_cb),
			  NULL);
	g_signal_connect (theme, "populate-popup",
			  G_CALLBACK (theme_adium_populate_popup_cb),
			  NULL);
}

EmpathyThemeAdium *
empathy_theme_adium_new (const gchar *path)
{
	g_return_val_if_fail (empathy_theme_adium_is_valid (path), NULL);

	return g_object_new (EMPATHY_TYPE_THEME_ADIUM,
			     "path", path,
			     NULL);
}

gboolean
empathy_theme_adium_is_valid (const gchar *path)
{
	gboolean ret;
	gchar   *file;

	/* We ship a default Template.html as fallback if there is any problem
	 * with the one inside the theme. The only other required file is
	 * Content.html for incoming messages (outgoing fallback to use
	 * incoming). */
	file = g_build_filename (path, "Contents", "Resources", "Incoming",
				 "Content.html", NULL);
	ret = g_file_test (file, G_FILE_TEST_EXISTS);
	g_free (file);

	return ret;
}

