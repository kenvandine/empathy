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

	gboolean     irc_style;
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

	priv->clean_theme   = empathy_theme_boxes_new ("clean");
	priv->simple_theme  = empathy_theme_boxes_new ("simple");
	priv->blue_theme    = empathy_theme_boxes_new ("blue");
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

