/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007-2008 Collabora Ltd.
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

#include <config.h>

#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-idle.h>

#include <libempathy-gtk/empathy-presence-chooser.h>
#include <libempathy-gtk/empathy-conf.h>
#include <libempathy-gtk/empathy-ui-utils.h>
#include <libempathy-gtk/empathy-accounts-dialog.h>
#include <libempathy-gtk/empathy-images.h>
#include <libempathy-gtk/empathy-new-message-dialog.h>

#include "empathy-status-icon.h"
#include "empathy-preferences.h"
#include "empathy-filter.h"

#define DEBUG_FLAG EMPATHY_DEBUG_FILTER
#include <libempathy/empathy-debug.h>

/* Number of ms to wait when blinking */
#define BLINK_TIMEOUT 500

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyStatusIcon)
typedef struct {
	GtkStatusIcon      *icon;
	EmpathyIdle        *idle;
	MissionControl     *mc;
	EmpathyFilter      *filter;
	EmpathyFilterEvent *event;
	gboolean            showing_event_icon;
	guint               blink_timeout;
	gpointer            token;

	GtkWindow          *window;
	GtkWidget          *popup_menu;
	GtkWidget          *show_window_item;
	GtkWidget          *message_item;
	GtkWidget          *status_item;
} EmpathyStatusIconPriv;

G_DEFINE_TYPE (EmpathyStatusIcon, empathy_status_icon, G_TYPE_OBJECT);

static void
status_icon_set_visibility (EmpathyStatusIcon *icon,
			    gboolean           visible,
			    gboolean           store)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);

	if (store) {
		empathy_conf_set_bool (empathy_conf_get (),
				       EMPATHY_PREFS_UI_MAIN_WINDOW_HIDDEN, !visible);
	}

	if (!visible) {
		empathy_window_iconify (priv->window, priv->icon);
	} else {
		GList *accounts;

		empathy_window_present (GTK_WINDOW (priv->window), TRUE);
	
		/* Show the accounts dialog if there is no enabled accounts */
		accounts = mc_accounts_list_by_enabled (TRUE);
		if (accounts) {
			mc_accounts_list_free (accounts);
		} else {
			DEBUG ("No enabled account, Showing account dialog");
			empathy_accounts_dialog_show (GTK_WINDOW (priv->window));
		}
	}
}

static void
status_icon_notify_visibility_cb (EmpathyConf *conf,
				  const gchar *key,
				  gpointer     user_data)
{
	EmpathyStatusIcon *icon = user_data;
	gboolean           hidden = FALSE;

	if (empathy_conf_get_bool (conf, key, &hidden)) {
		status_icon_set_visibility (icon, !hidden, FALSE);
	}
}

static void
status_icon_toggle_visibility (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	gboolean               visible;

	visible = gtk_window_is_active (priv->window);
	status_icon_set_visibility (icon, !visible, TRUE);
}

static void
status_icon_update_tooltip (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	const gchar           *tooltip = NULL;

	if (priv->event) {
		tooltip = priv->event->message;
	}

	if (!tooltip) {
		tooltip = empathy_idle_get_status (priv->idle);
	}

	gtk_status_icon_set_tooltip (priv->icon, tooltip);	
}

static void
status_icon_update_icon (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	const gchar           *icon_name;

	if (priv->event && priv->showing_event_icon) {
		icon_name = priv->event->icon_name;
	} else {
		McPresence state;

		state = empathy_idle_get_state (priv->idle);
		icon_name = empathy_icon_name_for_presence (state);
	}

	gtk_status_icon_set_from_icon_name (priv->icon, icon_name);
}

static void
status_icon_idle_notify_cb (EmpathyStatusIcon *icon)
{
	status_icon_update_icon (icon);
	status_icon_update_tooltip (icon);
}

static gboolean
status_icon_delete_event_cb (GtkWidget         *widget,
			     GdkEvent          *event,
			     EmpathyStatusIcon *icon)
{
	status_icon_set_visibility (icon, FALSE, TRUE);
	return TRUE;
}

static void
status_icon_activate_cb (GtkStatusIcon     *status_icon,
			 EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);

	DEBUG ("Activated: %s", priv->event ? "event" : "toggle");

	if (priv->event) {
		empathy_filter_activate_event (priv->filter, priv->event);
		priv->event = empathy_filter_get_top_event (priv->filter);
		status_icon_update_tooltip (icon);
		status_icon_update_icon (icon);

		if (!priv->event && priv->blink_timeout) {
			g_source_remove (priv->blink_timeout);
			priv->blink_timeout = 0;
		}
	} else {
		status_icon_toggle_visibility (icon);
	}
}

static void
status_icon_show_hide_window_cb (GtkWidget         *widget,
				 EmpathyStatusIcon *icon)
{
	gboolean visible;

	visible = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget));
	status_icon_set_visibility (icon, visible, TRUE);
}

static void
status_icon_new_message_cb (GtkWidget         *widget,
			    EmpathyStatusIcon *icon)
{
	empathy_new_message_dialog_show (NULL);
}

static void
status_icon_quit_cb (GtkWidget         *window,
		     EmpathyStatusIcon *icon)
{
	gtk_main_quit ();
}

static void
status_icon_popup_menu_cb (GtkStatusIcon     *status_icon,
			   guint              button,
			   guint              activate_time,
			   EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	GtkWidget             *submenu;
	gboolean               show;

	show = empathy_window_get_is_visible (GTK_WINDOW (priv->window));

	g_signal_handlers_block_by_func (priv->show_window_item,
					 status_icon_show_hide_window_cb,
					 icon);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (priv->show_window_item),
					show);
	g_signal_handlers_unblock_by_func (priv->show_window_item,
					   status_icon_show_hide_window_cb,
					   icon);

	submenu = empathy_presence_chooser_create_menu ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->status_item),
				   submenu);

	gtk_menu_popup (GTK_MENU (priv->popup_menu),
			NULL, NULL,
			gtk_status_icon_position_menu,
			priv->icon,
			button,
			activate_time);
}

static void
status_icon_create_menu (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	GladeXML              *glade;
	gchar                 *filename;

	filename = empathy_file_lookup ("empathy-status-icon.glade", "src");
	glade = empathy_glade_get_file (filename,
				       "tray_menu",
				       NULL,
				       "tray_menu", &priv->popup_menu,
				       "tray_show_list", &priv->show_window_item,
				       "tray_new_message", &priv->message_item,
				       "tray_status", &priv->status_item,
				       NULL);
	g_free (filename);

	empathy_glade_connect (glade,
			      icon,
			      "tray_show_list", "toggled", status_icon_show_hide_window_cb,
			      "tray_new_message", "activate", status_icon_new_message_cb,
			      "tray_quit", "activate", status_icon_quit_cb,
			      NULL);

	g_object_unref (glade);
}

static gboolean
status_icon_blink_timeout_cb (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);

	priv->showing_event_icon = !priv->showing_event_icon;
	status_icon_update_icon (icon);

	return TRUE;
}

static void
status_icon_top_event_notify_cb (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);

	priv->event = empathy_filter_get_top_event (priv->filter);
	priv->showing_event_icon = priv->event != NULL;
	status_icon_update_icon (icon);
	status_icon_update_tooltip (icon);

	if (!priv->blink_timeout) {
		priv->blink_timeout = g_timeout_add (BLINK_TIMEOUT,
						     (GSourceFunc) status_icon_blink_timeout_cb,
						     icon);
	}
}

static void
status_icon_finalize (GObject *object)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (object);

	if (priv->blink_timeout) {
		g_source_remove (priv->blink_timeout);
	}

	empathy_disconnect_account_status_changed (priv->token);

	g_object_unref (priv->icon);
	g_object_unref (priv->idle);
	g_object_unref (priv->filter);
	g_object_unref (priv->mc);
}

static void
empathy_status_icon_class_init (EmpathyStatusIconClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = status_icon_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyStatusIconPriv));
}

static void
status_icon_status_changed_cb (MissionControl           *mc,
			       TpConnectionStatus        status,
			       McPresence                presence,
			       TpConnectionStatusReason  reason,
			       const gchar              *unique_name,
			       EmpathyStatusIcon        *icon)
{
	GList                 *accounts, *l;
	guint                  connection_status = 1;
	EmpathyStatusIconPriv *priv;

	priv = GET_PRIV (icon);

	/* Check for a connected account */
	accounts = mc_accounts_list_by_enabled (TRUE);
	for (l = accounts; l; l = l->next) {
		connection_status = mission_control_get_connection_status (priv->mc,
									   l->data,
									   NULL);
		if (connection_status == 0)
			break;
	}
	mc_accounts_list_free (accounts);

	gtk_widget_set_sensitive (priv->message_item, connection_status == 0);
}

static void
empathy_status_icon_init (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (icon,
		EMPATHY_TYPE_STATUS_ICON, EmpathyStatusIconPriv);

	icon->priv = priv;
	priv->icon = gtk_status_icon_new ();
	priv->mc = empathy_mission_control_new ();
	priv->idle = empathy_idle_new ();
	priv->filter = empathy_filter_new ();
	priv->token = empathy_connect_to_account_status_changed (priv->mc,
			G_CALLBACK (status_icon_status_changed_cb),
			icon, NULL);

	/* make icon listen and respond to MAIN_WINDOW_HIDDEN changes */
	empathy_conf_notify_add (empathy_conf_get (),
				 EMPATHY_PREFS_UI_MAIN_WINDOW_HIDDEN,
				 status_icon_notify_visibility_cb,
				 icon);

	status_icon_create_menu (icon);
	status_icon_idle_notify_cb (icon);

	g_signal_connect_swapped (priv->idle, "notify",
				  G_CALLBACK (status_icon_idle_notify_cb),
				  icon);
	g_signal_connect_swapped (priv->filter, "notify::top-event",
				  G_CALLBACK (status_icon_top_event_notify_cb),
				  icon);
	g_signal_connect (priv->icon, "activate",
			  G_CALLBACK (status_icon_activate_cb),
			  icon);
	g_signal_connect (priv->icon, "popup-menu",
			  G_CALLBACK (status_icon_popup_menu_cb),
			  icon);
}

EmpathyStatusIcon *
empathy_status_icon_new (GtkWindow *window)
{
	EmpathyStatusIconPriv *priv;
	EmpathyStatusIcon     *icon;
	gboolean               should_hide;

	g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

	icon = g_object_new (EMPATHY_TYPE_STATUS_ICON, NULL);
	priv = GET_PRIV (icon);

	priv->window = g_object_ref (window);

	g_signal_connect (priv->window, "delete-event",
			  G_CALLBACK (status_icon_delete_event_cb),
			  icon);

	empathy_conf_get_bool (empathy_conf_get (),
			      EMPATHY_PREFS_UI_MAIN_WINDOW_HIDDEN,
			      &should_hide);

	if (gtk_window_is_active (priv->window) == should_hide) {
		status_icon_set_visibility (icon, !should_hide, FALSE);
	}

	return icon;
}

