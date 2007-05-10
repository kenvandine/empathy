/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
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

#include <config.h>

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <libtelepathy/tp-helpers.h>

#include <libmissioncontrol/mission-control.h>

#include <libempathy/gossip-debug.h>
#include <libempathy/gossip-utils.h>

#include "empathy-status-icon.h"
#include "gossip-ui-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_STATUS_ICON, EmpathyStatusIconPriv))

#define DEBUG_DOMAIN "StatusIcon"

struct _EmpathyStatusIconPriv {
	MissionControl *mc;
	GtkStatusIcon  *icon;
	GtkWindow      *window;
};

static void empathy_status_icon_class_init  (EmpathyStatusIconClass *klass);
static void empathy_status_icon_init        (EmpathyStatusIcon      *icon);
static void status_icon_finalize            (GObject                *object);
static void status_icon_presence_changed_cb (MissionControl         *mc,
					     McPresence              state,
					     EmpathyStatusIcon      *icon);
static void status_icon_toggle_visibility   (EmpathyStatusIcon      *icon);
static void status_icon_activate_cb         (GtkStatusIcon          *status_icon,
					     EmpathyStatusIcon      *icon);

G_DEFINE_TYPE (EmpathyStatusIcon, empathy_status_icon, G_TYPE_OBJECT);

static void
empathy_status_icon_class_init (EmpathyStatusIconClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = status_icon_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyStatusIconPriv));
}

static void
empathy_status_icon_init (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;
	McPresence             state;

	priv = GET_PRIV (icon);

	priv->icon = gtk_status_icon_new ();
	priv->mc = mission_control_new (tp_get_bus ());
	state = mission_control_get_presence_actual (priv->mc, NULL);
	status_icon_presence_changed_cb (priv->mc, state, icon);

	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->mc),
				     "PresenceStatusActual",
				     G_CALLBACK (status_icon_presence_changed_cb),
				     icon, NULL);
	g_signal_connect (priv->icon, "activate",
			  G_CALLBACK (status_icon_activate_cb),
			  icon);
/*	g_signal_connect (priv->icon, "popup-menu",
			  G_CALLBACK (status_icon_popup_menu_cb),
			  icon);
*/
}

static void
status_icon_finalize (GObject *object)
{
	EmpathyStatusIconPriv *priv;

	priv = GET_PRIV (object);

	dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (priv->mc),
					"PresenceStatusActual",
					G_CALLBACK (status_icon_presence_changed_cb),
					object);

	g_object_unref (priv->mc);
	g_object_unref (priv->icon);
	g_object_unref (priv->window);
}

EmpathyStatusIcon *
empathy_status_icon_new (GtkWindow *window)
{
	EmpathyStatusIconPriv *priv;
	EmpathyStatusIcon     *icon;

	g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

	icon = g_object_new (EMPATHY_TYPE_STATUS_ICON, NULL);
	priv = GET_PRIV (icon);

	priv->window = g_object_ref (window);

	return icon;
}

static void
status_icon_presence_changed_cb (MissionControl    *mc,
				 McPresence         state,
				 EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;
	const gchar           *icon_name;
	gchar                 *status;

	priv = GET_PRIV (icon);

	icon_name = gossip_icon_name_for_presence_state (state);
	status = mission_control_get_presence_message_actual (priv->mc, NULL);
	if (G_STR_EMPTY (status)) {
		g_free (status);
		status = g_strdup (gossip_presence_state_get_default_status (state));
	}

	gtk_status_icon_set_from_icon_name (priv->icon, icon_name);
	gtk_status_icon_set_tooltip (priv->icon, status);

	g_free (status);
}

static void
status_icon_toggle_visibility (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;
	gboolean               visible;

	priv = GET_PRIV (icon);

	visible = gossip_window_get_is_visible (GTK_WINDOW (priv->window));

	if (visible) {
		gtk_widget_hide (GTK_WIDGET (priv->window));
	} else {
		gossip_window_present (GTK_WINDOW (priv->window), TRUE);
	}
}

static void
status_icon_activate_cb (GtkStatusIcon     *status_icon,
			 EmpathyStatusIcon *icon)
{
	status_icon_toggle_visibility (icon);
}
#if 0
static void
status_icon_popup_menu_cb (GtkStatusIcon     *status_icon,
			   guint              button,
			   guint              activate_time,
			   EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;
	GtkWidget             *submenu;
	gboolean               show;

	priv = GET_PRIV (icon);

	show = gossip_window_get_is_visible (GTK_WINDOW (priv->window));

	g_signal_handlers_block_by_func (priv->show_window_item,
					 contact_list_show_hide_window_cb,
					 icon);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (priv->show_window_item),
					show);
	g_signal_handlers_unblock_by_func (priv->show_window_item,
					   contact_list_show_hide_window_cb,
					   icon);

	submenu = gossip_presence_chooser_create_menu (
		GOSSIP_PRESENCE_CHOOSER (priv->presence_chooser));
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->popup_menu_status_item),
				   submenu);

	gtk_menu_popup (GTK_MENU (priv->popup_menu),
			NULL, NULL,
			gtk_status_icon_position_menu,
			priv->status_icon,
			button,
			activate_time);
}

static void
app_status_icon_create_menu (void)
{
	GossipAppPriv *priv;
	GladeXML      *glade;
	GtkWidget     *message_item;

	priv = GET_PRIV (app);

	glade = gossip_glade_get_file ("main.glade",
				       "tray_menu",
				       NULL,
				       "tray_menu", &priv->popup_menu,
				       "tray_show_list", &priv->popup_menu_show_list_item,
				       "tray_new_message", &message_item,
				       "tray_status", &priv->popup_menu_status_item,
				       NULL);

	gossip_glade_connect (glade,
			      app,
			      "tray_new_message", "activate", app_popup_new_message_cb,
			      "tray_quit", "activate", app_chat_quit_cb,
			      NULL);

	g_signal_connect (priv->popup_menu_show_list_item, "toggled",
			  G_CALLBACK (app_show_hide_list_cb), app);

	priv->widgets_connected = g_list_prepend (priv->widgets_connected,
						  priv->popup_menu_status_item);

	priv->widgets_connected = g_list_prepend (priv->widgets_connected,
						  message_item);

	g_object_unref (glade);
}
#endif

