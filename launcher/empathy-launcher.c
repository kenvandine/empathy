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

#include <libtelepathy/tp-helpers.h>
#include <libmissioncontrol/mc-account-monitor.h>
#include <libmissioncontrol/mission-control.h>

#include <libempathy/gossip-debug.h>

#define DEBUG_DOMAIN "Launcher"

static void error_cb              (MissionControl *mc,
				   GError         *error,
				   gpointer        data);
static void service_ended_cb      (MissionControl *mc,
				   gpointer        user_data);
static void start_mission_control (MissionControl *mc);




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

	if (presence != MC_PRESENCE_UNSET &&
	    presence != MC_PRESENCE_OFFLINE) {
		/* MC is already running and online, nothing to do */
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Starting Mission Control...");

	/* FIXME: Save/Restore status message */
	mission_control_set_presence (mc, MC_PRESENCE_AVAILABLE,
				      NULL,
				      (McCallback) error_cb,
				      NULL);

	mission_control_connect_all_with_default_presence (mc,
							   (McCallback) error_cb,
							   NULL);
}

int
main (int argc, char *argv[])
{
	GMainLoop        *main_loop;
	MissionControl   *mc;
	McAccountMonitor *monitor;

	g_type_init ();

	main_loop = g_main_loop_new (NULL, FALSE);
	monitor = mc_account_monitor_new ();
	mc = mission_control_new (tp_get_bus ());

	g_signal_connect (monitor, "account-enabled",
			  G_CALLBACK (account_enabled_cb),
			  mc);
	g_signal_connect (mc, "ServiceEnded",
			  G_CALLBACK (service_ended_cb),
			  NULL);

	start_mission_control (mc);

	g_main_loop_run (main_loop);

	g_object_unref (monitor);
	g_object_unref (mc);

	return 0;
}

