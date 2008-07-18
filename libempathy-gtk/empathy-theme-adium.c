/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Collabora Ltd.
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

typedef struct {
	EmpathySmileyManager *smiley_manager;
	EmpathyContact       *last_contact;
	gboolean              page_loaded;
	GList                *message_queue;
	gchar                *path;
	gchar                *default_avatar_filename;
	gchar                *in_content_html;
	gsize                 in_content_len;
	gchar                *in_nextcontent_html;
	gsize                 in_nextcontent_len;
	gchar                *out_content_html;
	gsize                 out_content_len;
	gchar                *out_nextcontent_html;
	gsize                 out_nextcontent_len;
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
	gchar                 *basedir;
	gchar                 *file;
	gchar                 *template_html = NULL;
	gsize                  template_len;
	GString               *string;
	gchar                **strv = NULL;
	gchar                 *content;
	gchar                 *css_path;
	guint                  len = 0;
	guint                  i = 0;

	basedir = g_build_filename (priv->path, "Contents", "Resources", NULL);

	/* Load html files */
	file = g_build_filename (basedir, "Incoming", "Content.html", NULL);
	g_file_get_contents (file, &priv->in_content_html, &priv->in_content_len, NULL);
	g_free (file);

	file = g_build_filename (basedir, "Incoming", "NextContent.html", NULL);
	g_file_get_contents (file, &priv->in_nextcontent_html, &priv->in_nextcontent_len, NULL);
	g_free (file);

	file = g_build_filename (basedir, "Outgoing", "Content.html", NULL);
	g_file_get_contents (file, &priv->out_content_html, &priv->out_content_len, NULL);
	g_free (file);

	file = g_build_filename (basedir, "Outgoing", "NextContent.html", NULL);
	g_file_get_contents (file, &priv->out_nextcontent_html, &priv->out_nextcontent_len, NULL);
	g_free (file);

	css_path = g_build_filename (basedir, "main.css", NULL);

	/* There is 2 formats for Template.html: The old one has 4 parameters,
	 * the new one has 5 parameters. */
	file = g_build_filename (basedir, "Template.html", NULL);
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
	g_string_append (string, basedir);
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
	content = g_string_free (string, FALSE);

	/* Load the template */
	webkit_web_view_load_html_string (WEBKIT_WEB_VIEW (theme),
					  content, basedir);

	g_free (basedir);
	g_free (content);
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

static gchar *
theme_adium_escape_script (const gchar *text)
{
	const gchar *cur = text;
	GString     *string;

	string = g_string_sized_new (strlen (text));
	while (!G_STR_EMPTY (cur)) {
		switch (*cur) {
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
			g_string_append_c (string, *cur);
		}
		cur++;
	}

	return g_string_free (string, FALSE);
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

	ret = g_markup_escape_text (text, -1);

	empathy_conf_get_bool (empathy_conf_get (),
			       EMPATHY_PREFS_CHAT_SHOW_SMILEYS,
			       &use_smileys);

	if (use_smileys) {
		/* Replace smileys by a <img/> tag */
		string = g_string_sized_new (strlen (ret));
		smileys = empathy_smiley_manager_parse (priv->smiley_manager, ret);
		for (l = smileys; l; l = l->next) {
			EmpathySmiley *smiley;

			smiley = l->data;
			if (smiley->path) {
				g_string_append_printf (string,
							"<abbr title='%s'><img src=\"%s\"/ alt=\"%s\"/></abbr>",
							smiley->str, smiley->path, smiley->str);
			} else {
				g_string_append (string, smiley->str);
			}
			empathy_smiley_free (smiley);
		}
		g_slist_free (smileys);

		g_free (ret);
		ret = g_string_free (string, FALSE);
	}

	/* Add <a href></a> arround links */
	uri_regex = empathy_uri_regex_dup_singleton ();
	match = g_regex_match (uri_regex, cur, 0, &match_info);
	if (match) {
		gint last = 0;
		gint s = 0, e = 0;

		string = g_string_sized_new (strlen (ret));
		do {
			g_match_info_fetch_pos (match_info, 0, &s, &e);

			if (s > last) {
				/* Append the text between last link (or the
				 * start of the message) and this link */
				g_string_append_len (string, ret + last, s - last);
			}

			/* Append the link inside <a href=""></a> tag */
			g_string_append (string, "<a href=\"");
			g_string_append_len (string, ret + s, e - s);
			g_string_append (string, "\">");
			g_string_append_len (string, ret + s, e - s);
			g_string_append (string, "</a>");

			last = e;
		} while (g_match_info_next (match_info, NULL));

		if (e < strlen (ret)) {
			/* Append the text after the last link */
			g_string_append_len (string, ret + e, strlen (ret) - e);
		}

		g_free (ret);
		ret = g_string_free (string, FALSE);
	}
	g_match_info_free (match_info);
	g_regex_unref (uri_regex);

	/* Replace \n by <br/> */
	string = NULL;
	prev = 0;
	for (i = 0; ret[i] != '\0'; i++) {
		if (ret[i] == '\n') {
			if (!string ) {
				string = g_string_sized_new (strlen (ret));
			}
			g_string_append_len (string, ret + prev, i - prev);
			g_string_append (string, "<br/>");
			prev = i + 1;
		}
	}
	if (string) {
		g_string_append (string, ret + prev);
		g_free (ret);
		ret = g_string_free (string, FALSE);
	}

	return ret;
}

static void
theme_adium_scroll_down (EmpathyChatView *view)
{
	/* Not implemented */
}

#define FOLLOW(cur, str) (!strncmp (cur, str, strlen (str)))
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
	gsize                  len;
	GString               *string;
	gchar                 *cur = NULL;
	gchar                 *prev;
	gchar                 *script;
	gchar                 *escape;
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
	if (priv->last_contact &&
	    empathy_contact_equal (priv->last_contact, sender)) {
		func = "appendNextMessage";
		if (empathy_contact_is_user (sender)) {
			cur = priv->out_nextcontent_html;
			len = priv->out_nextcontent_len;
		}
		if (!cur) {
			cur = priv->in_nextcontent_html;
			len = priv->in_nextcontent_len;
		}
	}
	if (!cur) {
		func = "appendMessage";
		if (empathy_contact_is_user (sender)) {
			cur = priv->out_content_html;
			len = priv->out_content_len;
		}
		if (!cur) {
			cur = priv->in_content_html;
			len = priv->in_content_len;
		}
	}

	/* Make some search-and-replace in the html code */
	prev = cur;
	string = g_string_sized_new (len + strlen (body));
	while ((cur = strchr (cur, '%'))) {
		const gchar *replace = NULL;
		gchar       *dup_replace = NULL;
		gchar       *fin = NULL;

		if (FOLLOW (cur, "%message%")) {
			replace = body;
		} else if (FOLLOW (cur, "%userIconPath%")) {
			replace = avatar_filename;
		} else if (FOLLOW (cur, "%sender%")) {
			replace = name;
		} else if (FOLLOW (cur, "%time")) {
			gchar *format = NULL;
			gchar *start;
			gchar *end;

			/* Extract the time format it provided. */
			if (*(start = cur + strlen("%time")) == '{') {
				start++;
				end = strstr (start, "}%");
				if (!end) /* Invalid string */
					continue;
				format = g_strndup (start, end - start);
				fin = end + 1;
			} 

			dup_replace = empathy_time_to_string_local (timestamp,
				format ? format : EMPATHY_TIME_FORMAT_DISPLAY_SHORT);
			replace = dup_replace;
			g_free (format);
		} else {
			cur++;
			continue;
		}

		/* Here we have a replacement to make */
		g_string_append_len (string, prev, cur - prev);
		g_string_append (string, replace);
		g_free (dup_replace);

		/* And update the pointers */
		if (fin) {
			prev = cur = fin + 1;
		} else {
			prev = cur = strchr (cur + 1, '%') + 1;
		}
	}
	g_string_append (string, prev);

	/* Execute a js to add the message */
	cur = g_string_free (string, FALSE);
	escape = theme_adium_escape_script (cur);
	script = g_strdup_printf("%s(\"%s\")", func, escape);
	webkit_web_view_execute_script (WEBKIT_WEB_VIEW (view), script);

	/* Keep the sender of the last displayed message */
	if (priv->last_contact) {
		g_object_unref (priv->last_contact);
	}
	priv->last_contact = g_object_ref (sender);

	g_free (dup_body);
	g_free (cur);
	g_free (script);
}
#undef FOLLOW

static void
theme_adium_append_event (EmpathyChatView *view,
			  const gchar     *str)
{
	/* Not implemented */
}

static void
theme_adium_scroll (EmpathyChatView *view,
		    gboolean         allow_scrolling)
{
	/* Not implemented */
}

static gboolean
theme_adium_get_has_selection (EmpathyChatView *view)
{
	/* Not implemented */
	return FALSE;
}

static void
theme_adium_clear (EmpathyChatView *view)
{
	/* Not implemented */
}

static gboolean
theme_adium_find_previous (EmpathyChatView *view,
			   const gchar     *search_criteria,
			   gboolean         new_search)
{
	/* Not implemented */
	return FALSE;
}

static gboolean
theme_adium_find_next (EmpathyChatView *view,
		       const gchar     *search_criteria,
		       gboolean         new_search)
{
	/* Not implemented */
	return FALSE;
}

static void
theme_adium_find_abilities (EmpathyChatView *view,
			    const gchar    *search_criteria,
			    gboolean       *can_do_previous,
			    gboolean       *can_do_next)
{
	/* Not implemented */
}

static void
theme_adium_highlight (EmpathyChatView *view,
		       const gchar     *text)
{
	/* Not implemented */
}

static void
theme_adium_copy_clipboard (EmpathyChatView *view)
{
	/* Not implemented */
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

	g_free (priv->in_content_html);
	g_free (priv->in_nextcontent_html);
	g_free (priv->out_content_html);
	g_free (priv->out_nextcontent_html);
	g_free (priv->default_avatar_filename);
	g_free (priv->path);
	g_object_unref (priv->smiley_manager);

	if (priv->last_contact) {
		g_object_unref (priv->last_contact);
	}

	G_OBJECT_CLASS (empathy_theme_adium_parent_class)->finalize (object);
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

	priv->smiley_manager = empathy_smiley_manager_new ();

	g_signal_connect (theme, "load-finished",
			  G_CALLBACK (theme_adium_load_finished_cb),
			  NULL);
	g_signal_connect (theme, "navigation-requested",
			  G_CALLBACK (theme_adium_navigation_requested_cb),
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

