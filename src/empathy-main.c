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

#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <libtelepathy/tp-helpers.h>

#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mc-account-monitor.h>
#include <libmissioncontrol/mission-control.h>

#include <libempathy/gossip-debug.h>
#include <libempathy/gossip-presence.h>
#include <libempathy-gtk/empathy-main-window.h>
#include <libempathy-gtk/empathy-status-icon.h>
#include <libempathy-gtk/gossip-accounts-dialog.h>

#include "empathy-filter.h"

#define DEBUG_DOMAIN "EmpathyMain"

static void error_cb              (MissionControl    *mc,
				   GError            *error,
				   gpointer           data);
static void service_ended_cb      (MissionControl    *mc,
				   gpointer           user_data);
static void operation_error_cb    (MissionControl    *mc,
				   guint              operation_id,
				   guint              error_code,
				   gpointer           user_data);
static void start_mission_control (MissionControl    *mc);
static void destroy_cb            (GtkWidget         *window,
				   MissionControl    *mc);
static void icon_activate_cb      (EmpathyStatusIcon *icon,
				   GtkWidget         *window);

static void
error_cb (MissionControl *mc,
	  GError         *error,
	  gpointer        data)
{
	if (error) {
		gossip_debug (DEBUG_DOMAIN, "Error: %s", error->message);
	}
}

static void
service_ended_cb (MissionControl *mc,
		  gpointer        user_data)
{
	gossip_debug (DEBUG_DOMAIN, "Mission Control stopped");
}

static void
operation_error_cb (MissionControl *mc,
		    guint           operation_id,
		    guint           error_code,
		    gpointer        user_data)
{
	gossip_debug (DEBUG_DOMAIN, "Error code %d during operation %d",
		      error_code,
		      operation_id);
}

static void
account_enabled_cb (McAccountMonitor *monitor,
		    gchar            *unique_name,
		    MissionControl   *mc)
{
	gossip_debug (DEBUG_DOMAIN, "Account enabled: %s", unique_name);
	start_mission_control (mc);
}

static void
start_mission_control (MissionControl *mc)
{
	McPresence presence;

	presence = mission_control_get_presence_actual (mc, NULL);

	if (presence > MC_PRESENCE_OFFLINE) {
		/* MC is already running and online, nothing to do */
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Starting Mission Control...");

	mission_control_set_presence (mc,
				      MC_PRESENCE_AVAILABLE,
				      NULL,
				      (McCallback) error_cb,
				      NULL);
}

static void
destroy_cb (GtkWidget      *window,
	    MissionControl *mc)
{
	mission_control_set_presence (mc,
				      MC_PRESENCE_OFFLINE,
				      NULL, NULL, NULL);

	gtk_main_quit ();
}

static void
icon_activate_cb (EmpathyStatusIcon *icon,
		  GtkWidget         *window)
{
	if (GTK_WIDGET_VISIBLE (window)) {
		gtk_widget_hide (window);
	} else {
		gtk_widget_show (window);
	}
}

static void
new_channel_cb (EmpathyFilter *filter,
		TpConn        *tp_conn,
		TpChan        *tp_chan,
		guint          context_handle,
		gpointer       user_data)
{
	gossip_debug (DEBUG_DOMAIN, "Filtering context handle: %d",
		      context_handle);
	empathy_filter_process (filter, context_handle, TRUE);
}

int
main (int argc, char *argv[])
{
	GList             *accounts;
	EmpathyStatusIcon *icon;
	GtkWidget         *window;
	MissionControl    *mc;
	McAccountMonitor  *monitor;
	EmpathyFilter     *filter;

	gtk_init (&argc, &argv);

	/* Setting up channel filter */
	filter = empathy_filter_new ();
	g_signal_connect (filter, "new-channel",
			  G_CALLBACK (new_channel_cb),
			  NULL);

	/* Setting up MC */
	monitor = mc_account_monitor_new ();
	mc = mission_control_new (tp_get_bus ());
	g_signal_connect (monitor, "account-enabled",
			  G_CALLBACK (account_enabled_cb),
			  mc);
	g_signal_connect (mc, "ServiceEnded",
			  G_CALLBACK (service_ended_cb),
			  NULL);
	g_signal_connect (mc, "Error",
			  G_CALLBACK (operation_error_cb),
			  NULL);
	start_mission_control (mc);

	/* Setting up the main window */
	window = empathy_main_window_show ();
	g_signal_connect (window, "destroy",
			  G_CALLBACK (destroy_cb),
			  mc);
	g_signal_connect (window, "delete-event",
			  G_CALLBACK (gtk_widget_hide_on_delete),
			  NULL);

	/* Setting up the status icon */
	icon = empathy_status_icon_new ();
	g_signal_connect (icon, "activate",
			  G_CALLBACK (icon_activate_cb),
			  window);

	/* Show the accounts dialog if there is no enabled accounts */
	accounts = mc_accounts_list_by_enabled (TRUE);
	if (accounts) {
		mc_accounts_list_free (accounts);
	} else {
		gossip_accounts_dialog_show ();
	}

	gtk_main ();

	g_object_unref (monitor);
	g_object_unref (mc);
	g_object_unref (icon);

	return EXIT_SUCCESS;
}

