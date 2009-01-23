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

#include <telepathy-glib/util.h>
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

static EmpathyThemeIrc *
theme_manager_create_irc_view (EmpathyThemeManager *manager)
{
	EmpathyChatTextView *view;
	EmpathyThemeIrc     *theme;

	theme = empathy_theme_irc_new ();
	view = EMPATHY_CHAT_TEXT_VIEW (theme);

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

	return theme;
}

static void
theme_manager_boxes_weak_notify_cb (gpointer data,
				    GObject *where_the_object_was)
{
	EmpathyThemeManagerPriv *priv = GET_PRIV (data);

	priv->boxes_views = g_list_remove (priv->boxes_views, where_the_object_was);
}

static EmpathyThemeBoxes *
theme_manager_create_boxes_view (EmpathyThemeManager *manager)
{
	EmpathyThemeManagerPriv *priv = GET_PRIV (manager);
	EmpathyThemeBoxes       *theme;

	theme = empathy_theme_boxes_new ();
	priv->boxes_views = g_list_prepend (priv->boxes_views, theme);
	g_object_weak_ref (G_OBJECT (theme),
			   theme_manager_boxes_weak_notify_cb,
			   manager);

	return theme;
}

static void
theme_manager_update_boxes_tags (EmpathyThemeBoxes *theme,
				 const gchar       *header_foreground,
				 const gchar       *header_background,
				 const gchar       *header_line_background,
				 const gchar       *action_foreground,
				 const gchar       *time_foreground,
				 const gchar       *event_foreground,
				 const gchar       *link_foreground,
				 const gchar       *text_foreground,
				 const gchar       *text_background,
				 const gchar       *highlight_foreground)

{
	EmpathyChatTextView *view = EMPATHY_CHAT_TEXT_VIEW (theme);
	GtkTextTag          *tag;

	DEBUG ("Update view with new colors:\n"
		"header_foreground = %s\n"
		"header_background = %s\n"
		"header_line_background = %s\n"
		"action_foreground = %s\n"
		"time_foreground = %s\n"
		"event_foreground = %s\n"
		"link_foreground = %s\n"
		"text_foreground = %s\n"
		"text_background = %s\n"
		"highlight_foreground = %s\n",
		header_foreground, header_background, header_line_background,
		action_foreground, time_foreground, event_foreground,
		link_foreground, text_foreground, text_background,
		highlight_foreground);


	/* FIXME: GtkTextTag don't support to set color properties to NULL.
	 * See bug #542523 */
	
	#define TAG_SET(prop, prop_set, value) \
		if (value != NULL) { \
			g_object_set (tag, prop, value, NULL); \
		} else { \
			g_object_set (tag, prop_set, FALSE, NULL); \
		}

	/* Define base tags */
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_HIGHLIGHT,
					      "weight", PANGO_WEIGHT_BOLD,
					      "pixels-above-lines", 4,
					      NULL);
	TAG_SET ("paragraph-background", "paragraph-background-set", text_background);
	TAG_SET ("foreground", "foreground-set",highlight_foreground);

	empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_SPACING,
					"size", 3000,
					"pixels-above-lines", 8,
					NULL);
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_TIME,
					      "justification", GTK_JUSTIFY_CENTER,
					      NULL);
	TAG_SET ("foreground", "foreground-set", time_foreground);
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_ACTION,
					      "style", PANGO_STYLE_ITALIC,
					      "pixels-above-lines", 4,
					      NULL);
	TAG_SET ("paragraph-background", "paragraph-background-set", text_background);
	TAG_SET ("foreground", "foreground-set", action_foreground);
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_BODY,
					      "pixels-above-lines", 4,
					      NULL);
	TAG_SET ("paragraph-background", "paragraph-background-set", text_background);
	TAG_SET ("foreground", "foreground-set", text_foreground);
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_EVENT,
					      "justification", GTK_JUSTIFY_LEFT,
					      NULL);
	TAG_SET ("foreground", "foreground-set", event_foreground);
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_CHAT_TEXT_VIEW_TAG_LINK,
					      "underline", PANGO_UNDERLINE_SINGLE,
					      NULL);
	TAG_SET ("foreground", "foreground-set", link_foreground);

	/* Define BOXES tags */
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_THEME_BOXES_TAG_HEADER,
					      "weight", PANGO_WEIGHT_BOLD,
					      "foreground", header_foreground,
					      "paragraph-background", header_background,
					      NULL);
	TAG_SET ("foreground", "foreground-set", header_foreground);
	TAG_SET ("paragraph-background", "paragraph-background-set", header_background);
	tag = empathy_chat_text_view_tag_set (view, EMPATHY_THEME_BOXES_TAG_HEADER_LINE,
					      "size", 1,
					      "paragraph-background", header_line_background,
					      NULL);
	TAG_SET ("paragraph-background", "paragraph-background-set", header_line_background);

	#undef TAG_SET
}

static void
theme_manager_update_simple_tags (EmpathyThemeBoxes *theme)
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

	theme_manager_update_boxes_tags (theme,
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

static void
theme_manager_update_boxes_theme (EmpathyThemeManager *manager,
				  EmpathyThemeBoxes   *theme)
{
	EmpathyThemeManagerPriv *priv = GET_PRIV (manager);

	if (strcmp (priv->name, "simple") == 0) {
		theme_manager_update_simple_tags (theme);
	}
	else if (strcmp (priv->name, "clean") == 0) {
		theme_manager_update_boxes_tags (theme,
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
		theme_manager_update_boxes_tags (theme,
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
}

EmpathyChatView *
empathy_theme_manager_create_view (EmpathyThemeManager *manager)
{
	EmpathyThemeManagerPriv *priv = GET_PRIV (manager);
	EmpathyThemeBoxes       *theme;

	g_return_val_if_fail (EMPATHY_IS_THEME_MANAGER (manager), NULL);

	DEBUG ("Using theme %s", priv->name);

	if (strcmp (priv->name, "classic") == 0)  {
		return EMPATHY_CHAT_VIEW (theme_manager_create_irc_view (manager));
	}

	theme = theme_manager_create_boxes_view (manager);
	theme_manager_update_boxes_theme (manager, theme);

	return EMPATHY_CHAT_VIEW (theme);
}

static void
theme_manager_color_hash_notify_cb (EmpathyThemeManager *manager)
{
	EmpathyThemeManagerPriv *priv = GET_PRIV (manager);

	/* FIXME: Make that work, it should update color when theme changes but
	 * it doesnt seems to work with all themes. */

	if (strcmp (priv->name, "simple") == 0) {
		GList *l;

		/* We are using the simple theme which use the GTK theme color,
		 * Update views to use the new color. */
		for (l = priv->boxes_views; l; l = l->next) {
			theme_manager_update_simple_tags (EMPATHY_THEME_BOXES (l->data));
		}
	}
}

static gboolean
theme_manager_ensure_theme_exists (const gchar *name)
{
	gint i;

	if (EMP_STR_EMPTY (name)) {
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
	gchar                   *name = NULL;

	if (!empathy_conf_get_string (conf, key, &name) ||
	    !theme_manager_ensure_theme_exists (name) ||
	    !tp_strdiff (priv->name, name)) {
		if (!priv->name) {
			priv->name = g_strdup ("classic");
		}

		g_free (name);
		return;
	}

	g_free (priv->name);
	priv->name = name;

	if (!tp_strdiff (priv->name, "simple") ||
	    !tp_strdiff (priv->name, "clean") ||
	    !tp_strdiff (priv->name, "blue")) {
		GList *l;

		/* The theme changes to a boxed one, we can update boxed views */
		for (l = priv->boxes_views; l; l = l->next) {
			theme_manager_update_boxes_theme (manager,
							  EMPATHY_THEME_BOXES (l->data));
		}
	}

	g_signal_emit (manager, signals[THEME_CHANGED], 0, NULL);
}

static void
theme_manager_finalize (GObject *object)
{
	EmpathyThemeManagerPriv *priv = GET_PRIV (object);
	GList                   *l;

	empathy_conf_notify_remove (empathy_conf_get (), priv->name_notify_id);
	g_free (priv->name);

	for (l = priv->boxes_views; l; l = l->next) {
		g_object_weak_unref (G_OBJECT (l->data),
				     theme_manager_boxes_weak_notify_cb,
				     object);
	}
	g_list_free (priv->boxes_views);

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

