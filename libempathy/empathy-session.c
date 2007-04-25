/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Xavier Claessens <xclaesse@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <glib.h>

#include <libtelepathy/tp-helpers.h>

#include <libmissioncontrol/mc-account-monitor.h>

#include "empathy-session.h"
#include "gossip-debug.h"

#define DEBUG_DOMAIN "Session"

static void session_start_mission_control (void);
static void session_error_cb              (MissionControl   *mc,
					   GError           *error,
					   gpointer          data);
static void session_account_enabled_cb    (McAccountMonitor *monitor,
					   gchar            *unique_name,
					   gpointer          user_data);
static void session_service_ended_cb      (MissionControl   *mc,
					   gpointer          user_data);

static MissionControl        *mission_control = NULL;
static EmpathyContactManager *contact_manager = NULL;

void
empathy_session_connect (void)
{
	MissionControl   *mc;
	McAccountMonitor *monitor;
	static gboolean   started = FALSE;

	if (started) {
		return;
	}

	mc = empathy_session_get_mission_control ();
	monitor = mc_account_monitor_new ();

	g_signal_connect (monitor, "account-enabled",
			  G_CALLBACK (session_account_enabled_cb),
			  NULL);
	g_signal_connect (mc, "ServiceEnded",
			  G_CALLBACK (session_service_ended_cb),
			  NULL);

	g_object_unref (monitor);
	session_start_mission_control ();

	started = TRUE;
}

void
empathy_session_finalize (void)
{
	if (mission_control) {
		g_object_unref (mission_control);
		mission_control = NULL;
	}

	if (contact_manager) {
		g_object_unref (contact_manager);
		contact_manager = NULL;
	}
}

MissionControl *
empathy_session_get_mission_control (void)
{
	if (!mission_control) {
		mission_control = mission_control_new (tp_get_bus ());
	}

	return mission_control;
}

EmpathyContactManager *
empathy_session_get_contact_manager (void)
{
	if (!contact_manager) {
		contact_manager = empathy_contact_manager_new ();
	}

	return contact_manager;
}

static void
session_start_mission_control (void)
{
	MissionControl *mc;
	McPresence      presence;

	mc = empathy_session_get_mission_control ();
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
				      (McCallback) session_error_cb,
				      NULL);

	mission_control_connect_all_with_default_presence (mc,
							   (McCallback) session_error_cb,
							   NULL);
}

static void
session_error_cb (MissionControl *mc,
		  GError         *error,
		  gpointer        data)
{
	if (error) {
		gossip_debug (DEBUG_DOMAIN, "Error: %s", error->message);
	}
}

static void
session_account_enabled_cb (McAccountMonitor *monitor,
			    gchar            *unique_name,
			    gpointer          user_data)
{
	gossip_debug (DEBUG_DOMAIN, "Account enabled: %s", unique_name);
	session_start_mission_control ();
}

static void
session_service_ended_cb (MissionControl *mc,
			  gpointer        user_data)
{
	gossip_debug (DEBUG_DOMAIN, "Mission Control stopped");
}

