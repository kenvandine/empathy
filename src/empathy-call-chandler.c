/*
 *  Copyright (C) 2007 Elliot Fairweather
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Authors: Elliot Fairweather <elliot.fairweather@collabora.co.uk>
 */

#include <config.h>

#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libgnomevfs/gnome-vfs.h>

#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-chandler.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-tp-call.h>
#include <libempathy/empathy-debug.h>

#include <libempathy-gtk/empathy-call-window.h>

#define DEBUG_DOMAIN "EmpathyCall"

#define BUS_NAME "org.gnome.Empathy.CallChandler"
#define OBJECT_PATH "/org/gnome/Empathy/CallChandler"

static void
call_chandler_new_channel_cb (EmpathyChandler *chandler,
			      TpConn          *tp_conn,
			      TpChan          *tp_chan,
			      MissionControl  *mc)
{
	EmpathyTpCall *call;
	McAccount     *account;

	account = mission_control_get_account_for_connection (mc, tp_conn, NULL);

	call = empathy_tp_call_new (account, tp_chan);
	empathy_call_window_show (call);
	g_object_unref (account);
	g_object_unref (call);
}

int
main (int argc, char *argv[])
{
	EmpathyChandler *chandler;
	MissionControl  *mc;

	empathy_debug_set_log_file_from_env ();

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	gtk_window_set_default_icon_name ("empathy");
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   PKGDATADIR G_DIR_SEPARATOR_S "icons");

	mc = empathy_mission_control_new ();
	chandler = empathy_chandler_new (BUS_NAME, OBJECT_PATH);
	g_signal_connect (chandler, "new-channel",
			  G_CALLBACK (call_chandler_new_channel_cb),
			  mc);

	empathy_debug (DEBUG_DOMAIN, "Ready to handle new streamed media channels");

	gtk_main ();

	g_object_unref (chandler);
	g_object_unref (mc);

	return EXIT_SUCCESS;
}

