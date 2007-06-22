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
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>

#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-chan.h>
#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mc-account-monitor.h>
#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-presence.h>
#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-chandler.h>
#include <libempathy/empathy-tp-chat.h>
#include <libempathy/empathy-idle.h>
#include <libempathy-gtk/empathy-main-window.h>
#include <libempathy-gtk/empathy-status-icon.h>
#include <libempathy-gtk/empathy-private-chat.h>
#include <libempathy-gtk/empathy-group-chat.h>

#define DEBUG_DOMAIN "EmpathyMain"

#define BUS_NAME "org.gnome.Empathy.Chat"
#define OBJECT_PATH "/org/freedesktop/Telepathy/ChannelHandler"

static void
service_ended_cb (MissionControl *mc,
		  gpointer        user_data)
{
	empathy_debug (DEBUG_DOMAIN, "Mission Control stopped");
}

static void
operation_error_cb (MissionControl *mc,
		    guint           operation_id,
		    guint           error_code,
		    gpointer        user_data)
{
	empathy_debug (DEBUG_DOMAIN, "Error code %d during operation %d",
		      error_code,
		      operation_id);
}

static void
start_mission_control (EmpathyIdle *idle)
{
	McPresence presence;

	presence = empathy_idle_get_state (idle);

	if (presence > MC_PRESENCE_OFFLINE) {
		/* MC is already running and online, nothing to do */
		return;
	}

	empathy_idle_set_state (idle, MC_PRESENCE_AVAILABLE);
}

static void
account_enabled_cb (McAccountMonitor *monitor,
		    gchar            *unique_name,
		    EmpathyIdle      *idle)
{
	empathy_debug (DEBUG_DOMAIN, "Account enabled: %s", unique_name);
	start_mission_control (idle);
}

static void
new_channel_cb (EmpathyChandler *chandler,
		TpConn          *tp_conn,
		TpChan          *tp_chan,
		MissionControl  *mc)
{
	McAccount  *account;
	EmpathyChat *chat;
	gchar      *id;

	account = mission_control_get_account_for_connection (mc, tp_conn, NULL);
	id = empathy_get_channel_id (account, tp_chan);
	chat = empathy_chat_window_find_chat (account, id);
	g_free (id);

	if (chat) {
		/* The chat already exists */
		if (!empathy_chat_is_connected (chat)) {
			EmpathyTpChat *tp_chat;

			/* The chat died, give him the new text channel */
			tp_chat = empathy_tp_chat_new (account, tp_chan);
			empathy_chat_set_tp_chat (chat, tp_chat);
			g_object_unref (tp_chat);
		}
		empathy_chat_present (chat);

		g_object_unref (account);
		return;
	}

	if (tp_chan->handle_type == TP_HANDLE_TYPE_CONTACT) {
		/* We have a new private chat channel */
		chat = EMPATHY_CHAT (empathy_private_chat_new (account, tp_chan));
	}
	else if (tp_chan->handle_type == TP_HANDLE_TYPE_ROOM) {
		/* We have a new group chat channel */
		chat = EMPATHY_CHAT (empathy_group_chat_new (account, tp_chan));
	}

	empathy_chat_present (EMPATHY_CHAT (chat));

	g_object_unref (chat);
	g_object_unref (account);
}

int
main (int argc, char *argv[])
{
	EmpathyStatusIcon *icon;
	GtkWidget         *window;
	MissionControl    *mc;
	McAccountMonitor  *monitor;
	EmpathyIdle       *idle;
	EmpathyChandler   *chandler;
	GnomeProgram      *program;
	gboolean           no_connect = FALSE;
	GOptionContext    *context;
	GOptionEntry       options[] = {
		{ "no-connect", 'n',
		  0, G_OPTION_ARG_NONE, &no_connect,
		  N_("Don't connect on startup"),
		  NULL },
		{ NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (_("- Empathy Instant Messenger"));
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	g_set_application_name (PACKAGE_NAME);

	program = gnome_program_init ("empathy",
				      PACKAGE_VERSION,
				      LIBGNOMEUI_MODULE,
				      argc, argv,
				      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      "goption-context", context,
				      GNOME_PARAM_HUMAN_READABLE_NAME, PACKAGE_NAME,
				      NULL);

	gtk_window_set_default_icon_name ("empathy");
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   DATADIR G_DIR_SEPARATOR_S "empathy");

	/* Setting up MC */
	monitor = mc_account_monitor_new ();
	mc = empathy_mission_control_new ();
	idle = empathy_idle_new ();
	g_signal_connect (monitor, "account-enabled",
			  G_CALLBACK (account_enabled_cb),
			  idle);
	g_signal_connect (mc, "ServiceEnded",
			  G_CALLBACK (service_ended_cb),
			  NULL);
	g_signal_connect (mc, "Error",
			  G_CALLBACK (operation_error_cb),
			  NULL);

	if (!no_connect) {
		start_mission_control (idle);
	}

	/* Setting up UI */
	window = empathy_main_window_show ();
	icon = empathy_status_icon_new (GTK_WINDOW (window));

	/* Setting up channel handler  */
	chandler = empathy_chandler_new (BUS_NAME, OBJECT_PATH);
	g_signal_connect (chandler, "new-channel",
			  G_CALLBACK (new_channel_cb),
			  mc);

	gtk_main ();

	empathy_idle_set_state (idle, MC_PRESENCE_OFFLINE);

	g_object_unref (chandler);
	g_object_unref (monitor);
	g_object_unref (mc);
	g_object_unref (idle);
	g_object_unref (icon);
	g_object_unref (program);

	return EXIT_SUCCESS;
}

