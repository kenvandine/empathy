/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <libempathy/empathy-utils.h>

#include "empathy-theme-manager.h"
#include "empathy-chat-view.h"
#include "empathy-conf.h"
#include "empathy-chat-text-view.h"
#include "empathy-theme-boxes.h"
#include "empathy-theme-irc.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyThemeManager)
typedef struct {
	gchar       *name;
	guint        name_notify_id;
	GtkSettings *settings;
	GList       *boxes_views;
} EmpathyThemeManagerPriv;

enum {
	THEME_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static const gchar *themes[] = {
	"classic", N_("Classic"),
	"simple", N_("Simple"),
	"clean", N_("Clean"),
	"blue", N_("Blue"),
	NULL
};

G_DEFINE_TYPE (EmpathyThemeManager, empathy_theme_manager, G_TYPE_OBJECT);

static void
theme_manager_gdk_color_to_hex (GdkColor *gdk_color, gchar *str_color)
{
	g_snprintf (str_color, 10, 
		    "#%02x%02x%02x", 
		    gdk_color->red >> 8,
		    gdk_color->green >> 8,
		    gdk_color->blue >> 8);
}

static void
theme_manager_color_hash_notify_cb (EmpathyThemeManager *manager)
{
#if 0

FIXME: Make that work, it should update color when theme changes but it
       doesnt seems to work with all themes.

	g_object_get (priv->settings,
		      "color-hash", &color_hash,
		      NULL);

	/*
	 * base_color: #ffffffffffff
	 * fg_color: #000000000000
	 * bg_color: #e6e6e7e7e8e8
	 * text_color: #000000000000
	 * selected_bg_color: #58589a9adbdb
	 * selected_fg_color: #ffffffffffff
	 */

	color = g_hash_table_lookup (color_hash, "base_color");
	if (color) {
		theme_manager_gdk_color_to_hex (color, color_str);
		g_object_set (priv->simple_theme,
			      "action-foreground", color_str,
			      "link-foreground", color_str,
			      NULL);
	}

	color = g_hash_table_lookup (color_hash, "selected_bg_color");
	if (color) {
		theme_manager_gdk_color_to_hex (color, color_str);
		g_object_set (priv->simple_theme,
			      "header-background", color_str,
			      NULL);
	}

	color = g_hash_table_lookup (color_hash, "bg_color");
	if (color) {
		GdkColor tmp;

		tmp = *color;
		tmp.red /= 2;
		tmp.green /= 2;
		tmp.blue /= 2;
		theme_manager_gdk_color_to_hex (&tmp, color_str);
		g_object_set (priv->simple_theme,
			      "header-line-background", color_str,
			      NULL);
	}

	color = g_hash_table_lookup (color_hash, "selected_fg_color");
	if (color) {
		theme_manager_gdk_color_to_hex (color, color_str);
		g_object_set (priv->simple_theme,
			      "header-foreground", color_str,
			      NULL);
	}

	g_hash_table_unref (color_hash);

#endif
}

static gboolean
theme_manager_ensure_theme_exists (const gchar *name)
{
	gint i;

	if (G_STR_EMPTY (name)) {
		return FALSE;
	}

	for (i = 0; themes[i]; i += 2) {
		if (strcmp (themes[i], name) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static void
theme_manager_notify_name_cb (EmpathyConf *conf,
			      const gchar *key,
			      gpointer     user_data)
{
	EmpathyThemeManager     *manager = EMPATHY_THEME_MANAGER (user_data);
	EmpathyThemeManagerPriv *priv = GET_PRIV (manager);
	gchar                   *name;

	g_free (priv->name);

	name = NULL;
	if (!empathy_conf_get_string (conf, key, &name) ||
	    !theme_manager_ensure_theme_exists (name)) {
		priv->name = g_strdup ("classic");
		g_free (name);
	} else {
		priv->name = name;
	}

	g_signal_emit (manager, signals[THEME_CHANGED], 0, NULL);
}

static void
theme_manager_finalize (GObject *object)
{
	EmpathyThemeManagerPriv *priv = GET_PRIV (object);

	empathy_conf_notify_remove (empathy_conf_get (), priv->name_notify_id);
	g_free (priv->name);

	G_OBJECT_CLASS (empathy_theme_manager_parent_class)->finalize (object);
}

static void
empathy_theme_manager_class_init (EmpathyThemeManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	signals[THEME_CHANGED] =
		g_signal_new ("theme-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_type_class_add_private (object_class, sizeof (EmpathyThemeManagerPriv));

	object_class->finalize = theme_manager_finalize;
}

static void
empathy_theme_manager_init (EmpathyThemeManager *manager)
{
	EmpathyThemeManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
		EMPATHY_TYPE_THEME_MANAGER, EmpathyThemeManagerPriv);

	manager->priv = priv;

	/* Take the theme name and track changes */
	priv->name_notify_id =
		empathy_conf_notify_add (empathy_conf_get (),
					 EMPATHY_PREFS_CHAT_THEME,
					 theme_manager_notify_name_cb,
					 manager);
	theme_manager_notify_name_cb (empathy_conf_get (),
				      EMPATHY_PREFS_CHAT_THEME,
				      manager);

	/* Track GTK color changes */
	priv->settings = gtk_settings_get_default ();
	g_signal_connect_swapped (priv->settings, "notify::color-hash",
				  G_CALLBACK (theme_manager_color_hash_notify_cb),
				  manager);
}

static EmpathyChatView *
theme_manager_create_irc_view (EmpathyThemeManager *manager)
{
	EmpathyChatTextView *view;

	view = EMPATHY_CHAT_TEXT_VIEW (empathy_theme_irc_new ());

	/* Define base tags */
	empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_SPACING,
					"size", 2000,
					NULL);
	empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_TIME,
					"foreground", "darkgrey",
					"justification", GTK_JUSTIFY_CENTER,
					NULL);
	empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_ACTION,
					"foreground", "brown4",
					"style", PANGO_STYLE_ITALIC,
					NULL);
	empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_BODY,
					"foreground-set", FALSE,
					NULL);
	empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_EVENT,
					"foreground", "PeachPuff4",
					"justification", GTK_JUSTIFY_LEFT,
					NULL);
	empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_LINK,
					"foreground", "steelblue",
					"underline", PANGO_UNDERLINE_SINGLE,
					NULL);

	/* Define IRC tags */
	empathy_chat_text_view_tag_set (view, EMPATHY_THEME_IRC_TAG_NICK_SELF,
					"foreground", "sea green",
					NULL);
	empathy_chat_text_view_tag_set (view, EMPATHY_THEME_IRC_TAG_NICK_OTHER,
					"foreground", "skyblue4",
					NULL);
	empathy_chat_text_view_tag_set (view, EMPATHY_THEME_IRC_TAG_NICK_HIGHLIGHT,
					"foreground", "indian red",
					"weight", PANGO_WEIGHT_BOLD,
					NULL);

	return EMPATHY_CHAT_VIEW (view);
}

static void
theme_manager_boxes_weak_notify_cb (gpointer data,
				    GObject *where_the_object_was)
{
	EmpathyThemeManagerPriv *priv = GET_PRIV (data);

	priv->boxes_views = g_list_remove (priv->boxes_views, where_the_object_was);
}

static EmpathyChatView *
theme_manager_create_boxes_view (EmpathyThemeManager *manager)
{
	EmpathyThemeManagerPriv *priv = GET_PRIV (manager);
	EmpathyThemeBoxes       *view;

	view = empathy_theme_boxes_new ();
	priv->boxes_views = g_list_prepend (priv->boxes_views, view);
	g_object_weak_ref (G_OBJECT (view),
			   theme_manager_boxes_weak_notify_cb,
			   manager);

	return EMPATHY_CHAT_VIEW (view);
}

static void
theme_manager_update_boxes_view (EmpathyChatTextView *view,
				 const gchar         *header_foreground,
				 const gchar         *header_background,
				 const gchar         *header_line_background,
				 const gchar         *action_foreground,
				 const gchar         *time_foreground,
				 const gchar         *event_foreground,
				 const gchar         *link_foreground,
				 const gchar         *text_foreground,
				 const gchar         *text_background,
				 const gchar         *highlight_foreground)

{
	GtkTextTag *tag;

	/* FIXME: GtkTextTag don't support to set color properties to NULL.
	 * See bug #542523 */
	#define TAG_SET(prop, value) \
		if (value != NULL) { \
			g_object_set (tag, prop, value, NULL); \
		}

	/* Define base tags */
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_HIGHLIGHT,
					      "weight", PANGO_WEIGHT_BOLD,
					      "pixels-above-lines", 4,
					      NULL);
	TAG_SET ("paragraph-background", text_background);
	TAG_SET ("foreground", highlight_foreground);

	empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_SPACING,
					"size", 3000,
					"pixels-above-lines", 8,
					NULL);
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_TIME,
					      "justification", GTK_JUSTIFY_CENTER,
					      NULL);
	TAG_SET ("foreground", time_foreground);
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_ACTION,
					      "style", PANGO_STYLE_ITALIC,
					      "pixels-above-lines", 4,
					      NULL);
	TAG_SET ("paragraph-background", text_background);
	TAG_SET ("foreground", action_foreground);
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_BODY,
					      "pixels-above-lines", 4,
					      NULL);
	TAG_SET ("paragraph-background", text_background);
	TAG_SET ("foreground", text_foreground);
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_EVENT,
					      "justification", GTK_JUSTIFY_LEFT,
					      NULL);
	TAG_SET ("foreground", event_foreground);
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_LINK,
					      "underline", PANGO_UNDERLINE_SINGLE,
					      NULL);
	TAG_SET ("foreground", link_foreground);

	/* Define BOXES tags */
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_THEME_BOXES_TAG_HEADER,
					      "weight", PANGO_WEIGHT_BOLD,
					      "foreground", header_foreground,
					      "paragraph-background", header_background,
					      NULL);
	TAG_SET ("foreground", header_foreground);
	TAG_SET ("paragraph-background", header_background);
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_THEME_BOXES_TAG_HEADER_LINE,
					      "size", 1,
					      "paragraph-background", header_line_background,
					      NULL);
	TAG_SET ("paragraph-background", header_line_background);

	#undef TAG_SET
}

static void
theme_manager_update_simple_view (EmpathyChatTextView *view)
{
	GtkStyle *style;
	gchar     color1[10];
	gchar     color2[10];
	gchar     color3[10];
	gchar     color4[10];

	style = gtk_widget_get_default_style ();

	theme_manager_gdk_color_to_hex (&style->base[GTK_STATE_SELECTED], color1);
	theme_manager_gdk_color_to_hex (&style->bg[GTK_STATE_SELECTED], color2);
	theme_manager_gdk_color_to_hex (&style->dark[GTK_STATE_SELECTED], color3);
	theme_manager_gdk_color_to_hex (&style->fg[GTK_STATE_SELECTED], color4);

	theme_manager_update_boxes_view (view,
					 color4,     /* header_foreground */
					 color2,     /* header_background */
					 color3,     /* header_line_background */
					 color1,     /* action_foreground */
					 "darkgrey", /* time_foreground */
					 "darkgrey", /* event_foreground */
					 color1,     /* link_foreground */
					 NULL,       /* text_foreground */
					 NULL,       /* text_background */
					 NULL);      /* highlight_foreground */
}

EmpathyChatView *
empathy_theme_manager_create_view (EmpathyThemeManager *manager)
{
	EmpathyThemeManagerPriv *priv = GET_PRIV (manager);
	EmpathyChatView         *view = NULL;

	g_return_val_if_fail (EMPATHY_IS_THEME_MANAGER (manager), NULL);

	DEBUG ("Using theme %s", priv->name);

	if (strcmp (priv->name, "classic") == 0)  {
		view = theme_manager_create_irc_view (manager);
	}
	else if (strcmp (priv->name, "simple") == 0) {
		view = theme_manager_create_boxes_view (manager);
		theme_manager_update_simple_view (EMPATHY_CHAT_TEXT_VIEW (view));
	}
	else if (strcmp (priv->name, "clean") == 0) {
		view = theme_manager_create_boxes_view (manager);
		theme_manager_update_boxes_view (EMPATHY_CHAT_TEXT_VIEW (view),
						 "black",    /* header_foreground */
						 "#efefdf",  /* header_background */
						 "#e3e3d3",  /* header_line_background */
						 "brown4",   /* action_foreground */
						 "darkgrey", /* time_foreground */
						 "darkgrey", /* event_foreground */
						 "#49789e",  /* link_foreground */
						 NULL,       /* text_foreground */
						 NULL,       /* text_background */
						 NULL);      /* highlight_foreground */
	}
	else if (strcmp (priv->name, "blue") == 0) {
		view = theme_manager_create_boxes_view (manager);
		theme_manager_update_boxes_view (EMPATHY_CHAT_TEXT_VIEW (view),
						 "black",    /* header_foreground */
						 "#88a2b4",  /* header_background */
						 "#7f96a4",  /* header_line_background */
						 "brown4",   /* action_foreground */
						 "darkgrey", /* time_foreground */
						 "#7f96a4",  /* event_foreground */
						 "#49789e",  /* link_foreground */
						 "black",    /* text_foreground */
						 "#adbdc8",  /* text_background */
						 "black");   /* highlight_foreground */
	}

	return view;
}

EmpathyThemeManager *
empathy_theme_manager_get (void)
{
	static EmpathyThemeManager *manager = NULL;

	if (!manager) {
		manager = g_object_new (EMPATHY_TYPE_THEME_MANAGER, NULL);
	}

	return manager;
}

const gchar **
empathy_theme_manager_get_themes (void)
{
	return themes;
}

