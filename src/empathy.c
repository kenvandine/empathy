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

#include <libebook/e-book.h>

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
#include <libempathy/empathy-tp-chatroom.h>
#include <libempathy/empathy-idle.h>
#include <libempathy/empathy-conf.h>
#include <libempathy-gtk/empathy-preferences.h>
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
	id = empathy_inspect_channel (account, tp_chan);
	chat = empathy_chat_window_find_chat (account, id);
	g_free (id);

	if (chat) {
		/* The chat already exists */
		if (!empathy_chat_is_connected (chat)) {
			EmpathyTpChat *tp_chat;

			/* The chat died, give him the new text channel */
			if (empathy_chat_is_group_chat (chat)) {
				tp_chat = EMPATHY_TP_CHAT (empathy_tp_chatroom_new (account, tp_chan));
			} else {
				tp_chat = empathy_tp_chat_new (account, tp_chan);
			}
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

static void
create_salut_account (void)
{
	McProfile  *profile;
	McProtocol *protocol;
	gboolean    salut_created;
	McAccount  *account;
	GList      *accounts;
	EBook      *book;
	EContact   *contact;
	gchar      *nickname = NULL;
	gchar      *published_name = NULL;
	gchar      *first_name = NULL;
	gchar      *last_name = NULL;
	gchar      *email = NULL;
	gchar      *jid = NULL;

	/* Check if we already created a salut account */
	if (!empathy_conf_get_bool (empathy_conf_get(),
				    EMPATHY_PREFS_SALUT_ACCOUNT_CREATED,
				    &salut_created)) {
		return;
	}
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
	published_name = e_contact_get (contact, E_CONTACT_FULL_NAME);
	first_name = e_contact_get (contact, E_CONTACT_GIVEN_NAME);
	last_name = e_contact_get (contact, E_CONTACT_FAMILY_NAME);
	email = e_contact_get (contact, E_CONTACT_EMAIL_1);
	jid = e_contact_get (contact, E_CONTACT_IM_JABBER_HOME_1);
	
	if (G_STR_EMPTY (nickname) || !empathy_strdiff (nickname, "nickname")) {
		g_free (nickname);
		nickname = g_strdup (g_get_user_name ());
	}
	if (G_STR_EMPTY (published_name)) {
		g_free (published_name);
		published_name = g_strdup (g_get_real_name ());
	}

	empathy_debug (DEBUG_DOMAIN, "Salut account created:\n"
				     "  nickname=%s\n"
				     "  published-name=%s\n"
				     "  first-name=%s\n"
				     "  last-name=%s\n"
				     "  email=%s\n"
				     "  jid=%s\n",
		       nickname, published_name, first_name, last_name, email, jid);

	mc_account_set_param_string (account, "nickname", nickname ? nickname : "");
	mc_account_set_param_string (account, "published-name", published_name ? published_name : "");
	mc_account_set_param_string (account, "first-name", first_name ? first_name : "");
	mc_account_set_param_string (account, "last-name", last_name ? last_name : "");
	mc_account_set_param_string (account, "email", email ? email : "");
	mc_account_set_param_string (account, "jid", jid ? jid : "");

	g_free (nickname);
	g_free (published_name);
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
	EmpathyChandler   *chandler;
	gboolean           no_connect = FALSE;
	GOptionContext    *context;
	GOptionEntry       options[] = {
		{ "no-connect", 'n',
		  0, G_OPTION_ARG_NONE, &no_connect,
		  N_("Don't connect on startup"),
		  NULL },
		{ NULL }
	};

	empathy_debug_set_log_file_from_env ();

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (_("- Empathy Instant Messenger"));
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	g_set_application_name (PACKAGE_NAME);

	gtk_init (&argc, &argv);
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
	
	create_salut_account ();

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

	return EXIT_SUCCESS;
}

