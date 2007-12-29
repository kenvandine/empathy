/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-conf.h>

#include "empathy-chat-view.h"
#include "empathy-preferences.h"
#include "empathy-theme.h"
#include "empathy-theme-boxes.h"
#include "empathy-theme-irc.h"
#include "empathy-theme-manager.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EMPATHY_TYPE_THEME_MANAGER, EmpathyThemeManagerPriv))

typedef struct {
	gchar       *name;
	guint        name_notify_id;
	guint        room_notify_id;

	gboolean     show_avatars;
	guint        show_avatars_notify_id;

	EmpathyTheme *clean_theme;
	EmpathyTheme *simple_theme;
	EmpathyTheme *blue_theme;
	EmpathyTheme *classic_theme;

	GtkSettings  *settings;
} EmpathyThemeManagerPriv;

static void        theme_manager_finalize                 (GObject            *object);
static void        theme_manager_notify_name_cb           (EmpathyConf         *conf,
							   const gchar        *key,
							   gpointer            user_data);
static void        theme_manager_notify_room_cb           (EmpathyConf         *conf,
							   const gchar        *key,
							   gpointer            user_data);
static void        theme_manager_notify_show_avatars_cb   (EmpathyConf         *conf,
							   const gchar        *key,
							   gpointer            user_data);
static void        theme_manager_apply_theme              (EmpathyThemeManager *manager,
							   EmpathyChatView     *view,
							   const gchar        *name);

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
	EmpathyThemeManagerPriv *priv;
	GtkStyle                *style;
	gchar                    color[10];

	priv = GET_PRIV (manager);

	style = gtk_widget_get_default_style ();

	g_object_freeze_notify (G_OBJECT (priv->simple_theme));

	theme_manager_gdk_color_to_hex (&style->base[GTK_STATE_SELECTED], color);
	g_object_set (priv->simple_theme,
		      "action-foreground", color,
		      "link-foreground", color,
		      NULL);
 
	theme_manager_gdk_color_to_hex (&style->bg[GTK_STATE_SELECTED], color);
	g_object_set (priv->simple_theme,
		      "header-background", color,
		      NULL);

	theme_manager_gdk_color_to_hex (&style->dark[GTK_STATE_SELECTED], color);
	g_object_set (priv->simple_theme,
		      "header-line-background", color,
		      NULL);

	theme_manager_gdk_color_to_hex (&style->fg[GTK_STATE_SELECTED], color);
	g_object_set (priv->simple_theme,
		      "header-foreground", color,
		      NULL);

	g_object_thaw_notify (G_OBJECT (priv->simple_theme));

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
	EmpathyThemeManagerPriv *priv;

	priv = GET_PRIV (manager);

	priv->name_notify_id =
		empathy_conf_notify_add (empathy_conf_get (),
					EMPATHY_PREFS_CHAT_THEME,
					theme_manager_notify_name_cb,
					manager);

	priv->room_notify_id =
		empathy_conf_notify_add (empathy_conf_get (),
					EMPATHY_PREFS_CHAT_THEME_CHAT_ROOM,
					theme_manager_notify_room_cb,
					manager);

	empathy_conf_get_string (empathy_conf_get (),
				EMPATHY_PREFS_CHAT_THEME,
				&priv->name);

	/* Unused right now, but will be used soon. */
	priv->show_avatars_notify_id =
		empathy_conf_notify_add (empathy_conf_get (),
					EMPATHY_PREFS_UI_SHOW_AVATARS,
					theme_manager_notify_show_avatars_cb,
					manager);

	empathy_conf_get_bool (empathy_conf_get (),
			      EMPATHY_PREFS_UI_SHOW_AVATARS,
			      &priv->show_avatars);

	priv->settings = gtk_settings_get_default ();
	g_signal_connect_swapped (priv->settings, "notify::color-hash",
				  G_CALLBACK (theme_manager_color_hash_notify_cb),
				  manager);

	priv->simple_theme = g_object_new (EMPATHY_TYPE_THEME_BOXES, NULL);
	theme_manager_color_hash_notify_cb (manager);

	priv->clean_theme = g_object_new (EMPATHY_TYPE_THEME_BOXES,
					  "header-foreground", "black",
					  "header-background", "#efefdf",
					  "header_line_background", "#e3e3d3",
					  "action_foreground", "brown4",
					  "time_foreground", "darkgrey",
					  "event_foreground", "darkgrey",
					  "invite_foreground", "sienna",
					  "link_foreground","#49789e",
					  NULL);

	priv->blue_theme = g_object_new (EMPATHY_TYPE_THEME_BOXES,
					 "header_foreground", "black",
					 "header_background", "#88a2b4",
					 "header_line_background", "#7f96a4",
					 "text_foreground", "black",
					 "text_background", "#adbdc8",
					 "highlight_foreground", "black",
					 "action_foreground", "brown4",
					 "time_foreground", "darkgrey",
					 "event_foreground", "#7f96a4",
					 "invite_foreground", "sienna",
					 "link_foreground", "#49789e",
					 NULL);

	priv->classic_theme = g_object_new (EMPATHY_TYPE_THEME_IRC, NULL);
}

static void
theme_manager_finalize (GObject *object)
{
	EmpathyThemeManagerPriv *priv;

	priv = GET_PRIV (object);

	empathy_conf_notify_remove (empathy_conf_get (), priv->name_notify_id);
	empathy_conf_notify_remove (empathy_conf_get (), priv->room_notify_id);
	empathy_conf_notify_remove (empathy_conf_get (), priv->show_avatars_notify_id);

	g_free (priv->name);

	g_object_unref (priv->clean_theme);
	g_object_unref (priv->simple_theme);
	g_object_unref (priv->blue_theme);
	g_object_unref (priv->classic_theme);

	G_OBJECT_CLASS (empathy_theme_manager_parent_class)->finalize (object);
}

static void
theme_manager_notify_name_cb (EmpathyConf  *conf,
			      const gchar *key,
			      gpointer     user_data)
{
	EmpathyThemeManager     *manager;
	EmpathyThemeManagerPriv *priv;
	gchar                  *name;

	manager = user_data;
	priv = GET_PRIV (manager);

	g_free (priv->name);

	name = NULL;
	if (!empathy_conf_get_string (conf, key, &name) ||
	    name == NULL || name[0] == 0) {
		priv->name = g_strdup ("classic");
		g_free (name);
	} else {
		priv->name = name;
	}

	g_signal_emit (manager, signals[THEME_CHANGED], 0, NULL);
}

static void
theme_manager_notify_room_cb (EmpathyConf  *conf,
			      const gchar *key,
			      gpointer     user_data)
{
	g_signal_emit (user_data, signals[THEME_CHANGED], 0, NULL);
}

static void
theme_manager_notify_show_avatars_cb (EmpathyConf  *conf,
				      const gchar *key,
				      gpointer     user_data)
{
	EmpathyThemeManager     *manager;
	EmpathyThemeManagerPriv *priv;
	gboolean                value;

	manager = user_data;
	priv = GET_PRIV (manager);

	if (!empathy_conf_get_bool (conf, key, &value)) {
		priv->show_avatars = FALSE;
	} else {
		priv->show_avatars = value;
	}
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
theme_manager_apply_theme (EmpathyThemeManager *manager,
			   EmpathyChatView     *view,
			   const gchar        *name)
{
	EmpathyThemeManagerPriv *priv;
	EmpathyTheme            *theme;

	priv = GET_PRIV (manager);

	/* Make sure all tags are present. Note: not useful now but when we have
	 * user defined theme it will be.
	 */
	if (theme_manager_ensure_theme_exists (name)) {
		if (strcmp (name, "clean") == 0) {
			theme = priv->clean_theme;
		}
		else if (strcmp (name, "simple") == 0) {
			theme = priv->simple_theme;
		}
		else if (strcmp (name, "blue") == 0) {
			theme = priv->blue_theme;
		} else {
			theme = priv->classic_theme;
		}
	} else {
		theme = priv->classic_theme;
	}

	empathy_chat_view_set_theme (view, theme);
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

void
empathy_theme_manager_apply (EmpathyThemeManager *manager,
			    EmpathyChatView     *view,
			    const gchar        *name)
{
	EmpathyThemeManagerPriv *priv;

	priv = GET_PRIV (manager);

	theme_manager_apply_theme (manager, view, name);
}

void
empathy_theme_manager_apply_saved (EmpathyThemeManager *manager,
				  EmpathyChatView     *view)
{
	EmpathyThemeManagerPriv *priv;

	priv = GET_PRIV (manager);

	theme_manager_apply_theme (manager, view, priv->name);
}

