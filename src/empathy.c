/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007-2008 Collabora Ltd.
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

#include <libebook/e-book.h>
#include <libgnomevfs/gnome-vfs.h>

#include <telepathy-glib/util.h>
#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mc-account-monitor.h>
#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-idle.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-debug.h>

#include <libempathy-gtk/empathy-conf.h>
#include <libempathy-gtk/empathy-preferences.h>
#include <libempathy-gtk/empathy-main-window.h>
#include <libempathy-gtk/empathy-status-icon.h>

#define DEBUG_DOMAIN "EmpathyMain"

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
create_salut_account (void)
{
	McProfile  *profile;
	McProtocol *protocol;
	gboolean    salut_created = FALSE;
	McAccount  *account;
	GList      *accounts;
	EBook      *book;
	EContact   *contact;
	gchar      *nickname = NULL;
	gchar      *first_name = NULL;
	gchar      *last_name = NULL;
	gchar      *email = NULL;
	gchar      *jid = NULL;

	/* Check if we already created a salut account */
	empathy_conf_get_bool (empathy_conf_get(),
			       EMPATHY_PREFS_SALUT_ACCOUNT_CREATED,
			       &salut_created);
	if (salut_created) {
		return;
	}

	empathy_debug (DEBUG_DOMAIN, "Try to add a salut account...");

	/* Check if the salut CM is installed */
	profile = mc_profile_lookup ("salut");
	protocol = mc_profile_get_protocol (profile);
	if (!protocol) {
		empathy_debug (DEBUG_DOMAIN, "Salut not installed");
		g_object_unref (profile);
		return;
	}
	g_object_unref (protocol);

	/* Get self EContact from EDS */
	if (!e_book_get_self (&contact, &book, NULL)) {
		empathy_debug (DEBUG_DOMAIN, "Failed to get self econtact");
		g_object_unref (profile);
		return;
	}

	empathy_conf_set_bool (empathy_conf_get (),
			       EMPATHY_PREFS_SALUT_ACCOUNT_CREATED,
			       TRUE);

	/* Check if there is already a salut account */
	accounts = mc_accounts_list_by_profile (profile);
	if (accounts) {
		empathy_debug (DEBUG_DOMAIN, "There is already a salut account");
		mc_accounts_list_free (accounts);
		g_object_unref (profile);
		return;
	}

	account = mc_account_create (profile);
	mc_account_set_display_name (account, _("People nearby"));
	
	nickname = e_contact_get (contact, E_CONTACT_NICKNAME);
	first_name = e_contact_get (contact, E_CONTACT_GIVEN_NAME);
	last_name = e_contact_get (contact, E_CONTACT_FAMILY_NAME);
	email = e_contact_get (contact, E_CONTACT_EMAIL_1);
	jid = e_contact_get (contact, E_CONTACT_IM_JABBER_HOME_1);
	
	if (!tp_strdiff (nickname, "nickname")) {
		g_free (nickname);
		nickname = NULL;
	}

	empathy_debug (DEBUG_DOMAIN, "Salut account created:\n"
				     "  nickname=%s\n"
				     "  first-name=%s\n"
				     "  last-name=%s\n"
				     "  email=%s\n"
				     "  jid=%s\n",
		       nickname, first_name, last_name, email, jid);

	mc_account_set_param_string (account, "nickname", nickname ? nickname : "");
	mc_account_set_param_string (account, "first-name", first_name ? first_name : "");
	mc_account_set_param_string (account, "last-name", last_name ? last_name : "");
	mc_account_set_param_string (account, "email", email ? email : "");
	mc_account_set_param_string (account, "jid", jid ? jid : "");

	g_free (nickname);
	g_free (first_name);
	g_free (last_name);
	g_free (email);
	g_free (jid);
	g_object_unref (account);
	g_object_unref (profile);
	g_object_unref (contact);
	g_object_unref (book);
}

int
main (int argc, char *argv[])
{
	EmpathyStatusIcon *icon;
	GtkWidget         *window;
	MissionControl    *mc;
	McAccountMonitor  *monitor;
	EmpathyIdle       *idle;
	gboolean           autoconnect = TRUE;
	GError            *error = NULL;

	empathy_debug_set_log_file_from_env ();

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	if (!gtk_init_with_args (&argc, &argv,
				 _("- Empathy Instant Messenger"),
				 NULL, GETTEXT_PACKAGE, &error)) {
		empathy_debug (DEBUG_DOMAIN, error->message);
		return EXIT_FAILURE;
	}

	g_set_application_name (PACKAGE_NAME);

	gtk_window_set_default_icon_name ("empathy");
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   PKGDATADIR G_DIR_SEPARATOR_S "icons");
	gnome_vfs_init ();

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

	empathy_conf_get_bool (empathy_conf_get(),
			       EMPATHY_PREFS_AUTOCONNECT,
			       &autoconnect);
			       
	if (autoconnect) {
		start_mission_control (idle);
	}
	
	create_salut_account ();

	/* Setting up UI */
	window = empathy_main_window_show ();
	icon = empathy_status_icon_new (GTK_WINDOW (window));

	gtk_main ();

	empathy_idle_set_state (idle, MC_PRESENCE_OFFLINE);

	g_object_unref (monitor);
	g_object_unref (mc);
	g_object_unref (idle);
	g_object_unref (icon);

	return EXIT_SUCCESS;
}

