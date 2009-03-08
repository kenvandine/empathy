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
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <libebook/e-book.h>
#include <libnotify/notify.h>

#include <telepathy-glib/util.h>
#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-idle.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-call-factory.h>
#include <libempathy/empathy-chatroom-manager.h>
#include <libempathy/empathy-dispatcher.h>
#include <libempathy/empathy-dispatch-operation.h>
#include <libempathy/empathy-log-manager.h>
#include <libempathy/empathy-tp-chat.h>
#include <libempathy/empathy-tp-call.h>

#include <libempathy-gtk/empathy-conf.h>
#include <libempathy-gtk/empathy-ui-utils.h>

#include "empathy-accounts-dialog.h"
#include "empathy-main-window.h"
#include "empathy-status-icon.h"
#include "empathy-call-window.h"
#include "empathy-chat-window.h"
#include "empathy-ft-manager.h"
#include "bacon-message-connection.h"

#include "extensions/extensions.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

#include <gst/gst.h>

static BaconMessageConnection *connection = NULL;

static void
dispatch_cb (EmpathyDispatcher *dispatcher,
	     EmpathyDispatchOperation *operation,
	     gpointer           user_data)
{
	GQuark channel_type;

	channel_type = empathy_dispatch_operation_get_channel_type_id (operation);

	if (channel_type == TP_IFACE_QUARK_CHANNEL_TYPE_TEXT) {
		EmpathyTpChat *tp_chat;
		EmpathyChat   *chat = NULL;
		const gchar   *id;

		tp_chat = EMPATHY_TP_CHAT (
			empathy_dispatch_operation_get_channel_wrapper (operation));

		id = empathy_tp_chat_get_id (tp_chat);
		if (!id) {
			EmpathyContact *contact;

			contact = empathy_tp_chat_get_remote_contact (tp_chat);
			if (contact) {
				id = empathy_contact_get_id (contact);
			}
		}

		if (id) {
			McAccount *account;

			account = empathy_tp_chat_get_account (tp_chat);
			chat = empathy_chat_window_find_chat (account, id);
		}

		if (chat) {
			empathy_chat_set_tp_chat (chat, tp_chat);
		} else {
			chat = empathy_chat_new (tp_chat);
		}

		empathy_chat_window_present_chat (chat);

		empathy_dispatch_operation_claim (operation);
	} else if (channel_type == TP_IFACE_QUARK_CHANNEL_TYPE_STREAMED_MEDIA) {
		EmpathyCallFactory *factory;

		factory = empathy_call_factory_get ();
		empathy_call_factory_claim_channel (factory, operation);
	} else if (channel_type == TP_IFACE_QUARK_CHANNEL_TYPE_FILE_TRANSFER) {
		EmpathyFTManager *ft_manager;
		EmpathyTpFile    *tp_file;

		ft_manager = empathy_ft_manager_dup_singleton ();
		tp_file = EMPATHY_TP_FILE (
			empathy_dispatch_operation_get_channel_wrapper (operation));
		empathy_ft_manager_add_tp_file (ft_manager, tp_file);
		empathy_dispatch_operation_claim (operation);
		g_object_unref (ft_manager);
	}
}

static void
service_ended_cb (MissionControl *mc,
		  gpointer        user_data)
{
	DEBUG ("Mission Control stopped");
}

static void
operation_error_cb (MissionControl *mc,
		    guint           operation_id,
		    guint           error_code,
		    gpointer        user_data)
{
	const gchar *message;

	switch (error_code) {
	case MC_DISCONNECTED_ERROR:
		message = "Disconnected";
		break;
	case MC_INVALID_HANDLE_ERROR:
		message = "Invalid handle";
		break;
	case MC_NO_MATCHING_CONNECTION_ERROR:
		message = "No matching connection";
		break;
	case MC_INVALID_ACCOUNT_ERROR:
		message = "Invalid account";
		break;
	case MC_PRESENCE_FAILURE_ERROR:
		message = "Presence failure";
		break;
	case MC_NO_ACCOUNTS_ERROR:
		message = "No accounts";
		break;
	case MC_NETWORK_ERROR:
		message = "Network error";
		break;
	case MC_CONTACT_DOES_NOT_SUPPORT_VOICE_ERROR:
		message = "Contact does not support voice";
		break;
	case MC_LOWMEM_ERROR:
		message = "Lowmem";
		break;
	case MC_CHANNEL_REQUEST_GENERIC_ERROR:
		message = "Channel request generic error";
		break;
	case MC_CHANNEL_BANNED_ERROR:
		message = "Channel banned";
		break;
	case MC_CHANNEL_FULL_ERROR:
		message = "Channel full";
		break;
	case MC_CHANNEL_INVITE_ONLY_ERROR:
		message = "Channel invite only";
		break;
	default:
		message = "Unknown error code";
	}

	DEBUG ("Error during operation %d: %s", operation_id, message);
}

static void
use_nm_notify_cb (EmpathyConf *conf,
		  const gchar *key,
		  gpointer     user_data)
{
	EmpathyIdle *idle = user_data;
	gboolean     use_nm;

	if (empathy_conf_get_bool (conf, key, &use_nm)) {
		empathy_idle_set_use_nm (idle, use_nm);
	}
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
	GError     *error = NULL;

	/* Check if we already created a salut account */
	empathy_conf_get_bool (empathy_conf_get(),
			       EMPATHY_PREFS_SALUT_ACCOUNT_CREATED,
			       &salut_created);
	if (salut_created) {
		return;
	}

	DEBUG ("Try to add a salut account...");

	/* Check if the salut CM is installed */
	profile = mc_profile_lookup ("salut");
	if (!profile) {
		DEBUG ("No salut profile");
		return;
	}
	protocol = mc_profile_get_protocol (profile);
	if (!protocol) {
		DEBUG ("Salut not installed");
		g_object_unref (profile);
		return;
	}
	g_object_unref (protocol);

	/* Get self EContact from EDS */
	if (!e_book_get_self (&contact, &book, &error)) {
		DEBUG ("Failed to get self econtact: %s",
			error ? error->message : "No error given");
		g_clear_error (&error);
		g_object_unref (profile);
		return;
	}

	empathy_conf_set_bool (empathy_conf_get (),
			       EMPATHY_PREFS_SALUT_ACCOUNT_CREATED,
			       TRUE);

	/* Check if there is already a salut account */
	accounts = mc_accounts_list_by_profile (profile);
	if (accounts) {
		DEBUG ("There is already a salut account");
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

	DEBUG ("Salut account created:\nnickname=%s\nfirst-name=%s\n"
		"last-name=%s\nemail=%s\njid=%s\n",
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

/* The code that handles single-instance and startup notification is
 * copied from gedit.
 *
 * Copyright (C) 2005 - Paolo Maggi 
 */
static void
on_bacon_message_received (const char *message,
			   gpointer    data)
{
	GtkWidget *window = data;
	guint32    startup_timestamp;

	g_return_if_fail (message != NULL);

	DEBUG ("Other instance launched, presenting the main window. message='%s'",
		message);

	if (strcmp (message, "accounts") == 0) {
		/* accounts dialog requested */
		empathy_accounts_dialog_show (GTK_WINDOW (window), NULL);
	} else {
		startup_timestamp = atoi (message);

		/* Set the proper interaction time on the window.
		 * Fall back to roundtripping to the X server when we
		 * don't have the timestamp, e.g. when launched from
		 * terminal. We also need to make sure that the window
		 * has been realized otherwise it will not work. lame. */
		if (startup_timestamp == 0) {
			/* Work if launched from the terminal */
			DEBUG ("Using X server timestamp as a fallback");

			if (!GTK_WIDGET_REALIZED (window)) {
				gtk_widget_realize (GTK_WIDGET (window));
			}

			startup_timestamp = gdk_x11_get_server_time (window->window);
		}

		gtk_window_present_with_time (GTK_WINDOW (window), startup_timestamp);
	}
}

static guint32
get_startup_timestamp ()
{
	const gchar *startup_id_env;
	gchar       *startup_id = NULL;
	gchar       *time_str;
	gchar       *end;
	gulong       retval = 0;

	/* we don't unset the env, since startup-notification
	 * may still need it */
	startup_id_env = g_getenv ("DESKTOP_STARTUP_ID");
	if (startup_id_env == NULL) {
		goto out;
	}

	startup_id = g_strdup (startup_id_env);

	time_str = g_strrstr (startup_id, "_TIME");
	if (time_str == NULL) {
		goto out;
	}

	errno = 0;

	/* Skip past the "_TIME" part */
	time_str += 5;

	retval = strtoul (time_str, &end, 0);
	if (end == time_str || errno != 0)
		retval = 0;

 out:
	g_free (startup_id);

	return (retval > 0) ? retval : 0;
}

static gboolean
show_version_cb (const char *option_name,
                 const char *value,
                 gpointer data,
                 GError **error)
{
	g_print ("%s\n", PACKAGE_STRING);

	exit (EXIT_SUCCESS);

	return FALSE;
}

static void
new_call_handler_cb (EmpathyCallFactory *factory, EmpathyCallHandler *handler,
	gboolean outgoing, gpointer user_data)
{
		EmpathyCallWindow *window;

		window = empathy_call_window_new (handler);
		gtk_widget_show (GTK_WIDGET (window));
}

int
main (int argc, char *argv[])
{
	guint32            startup_timestamp;
	EmpathyStatusIcon *icon;
	EmpathyDispatcher *dispatcher;
	EmpathyLogManager *log_manager;
	EmpathyChatroomManager *chatroom_manager;
	EmpathyFTManager  *ft_manager;
	EmpathyCallFactory *call_factory;
	GtkWidget         *window;
	MissionControl    *mc;
	EmpathyIdle       *idle;
	gboolean           autoconnect = TRUE;
	gboolean           no_connect = FALSE; 
	gboolean           hide_contact_list = FALSE;
	gboolean           accounts_dialog = FALSE;
	GError            *error = NULL;
	GOptionEntry       options[] = {
		{ "no-connect", 'n',
		  0, G_OPTION_ARG_NONE, &no_connect,
		  N_("Don't connect on startup"),
		  NULL },
		{ "hide-contact-list", 'h',
		  0, G_OPTION_ARG_NONE, &hide_contact_list,
		  N_("Don't show the contact list on startup"),
		  NULL },
		{ "accounts", 'a',
		  0, G_OPTION_ARG_NONE, &accounts_dialog,
		  N_("Show the accounts dialog"),
		  NULL },
		{ "version", 'v',
		  G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, show_version_cb, NULL, NULL },
		{ NULL }
	};

	/* Init */
	g_thread_init (NULL);
	empathy_init ();

	if (!gtk_init_with_args (&argc, &argv,
				 N_("- Empathy Instant Messenger"),
				 options, GETTEXT_PACKAGE, &error)) {
		g_warning ("Error in empathy init: %s", error->message);
		return EXIT_FAILURE;
	}

	empathy_gtk_init ();
	g_set_application_name (_(PACKAGE_NAME));
	g_setenv("PULSE_PROP_media.role", "phone", TRUE);

	gst_init (&argc, &argv);

	gtk_window_set_default_icon_name ("empathy");
	textdomain (GETTEXT_PACKAGE);

        /* Setting up the bacon connection */
	startup_timestamp = get_startup_timestamp ();
	connection = bacon_message_connection_new ("empathy");
	if (connection != NULL) {
		if (!bacon_message_connection_get_is_server (connection)) {
			gchar *message;

			if (accounts_dialog) {
				DEBUG ("Showing accounts dialog from existing Empathy instance");

				message = g_strdup ("accounts");

			} else {

				DEBUG ("Activating existing instance");

				message = g_strdup_printf ("%" G_GUINT32_FORMAT,
							   startup_timestamp);
			}

			bacon_message_connection_send (connection, message);

			/* We never popup a window, so tell startup-notification
			 * that we are done. */
			gdk_notify_startup_complete ();

			g_free (message);
			bacon_message_connection_free (connection);

			return EXIT_SUCCESS;
		}
	} else {
		g_warning ("Cannot create the 'empathy' bacon connection.");
	}

	/* Setting up MC */
	mc = empathy_mission_control_dup_singleton ();
	g_signal_connect (mc, "ServiceEnded",
			  G_CALLBACK (service_ended_cb),
			  NULL);
	g_signal_connect (mc, "Error",
			  G_CALLBACK (operation_error_cb),
			  NULL);

	if (accounts_dialog) {
		GtkWidget *dialog;

		dialog = empathy_accounts_dialog_show (NULL, NULL);
		g_signal_connect (dialog, "destroy",
				  G_CALLBACK (gtk_main_quit),
				  NULL);

		gtk_main ();
		return 0;
	}

	/* Setting up Idle */
	idle = empathy_idle_dup_singleton ();
	empathy_idle_set_auto_away (idle, TRUE);
	use_nm_notify_cb (empathy_conf_get (), EMPATHY_PREFS_USE_NM, idle);
	empathy_conf_notify_add (empathy_conf_get (), EMPATHY_PREFS_USE_NM,
				 use_nm_notify_cb, idle);

	/* Autoconnect */
	empathy_conf_get_bool (empathy_conf_get(),
			       EMPATHY_PREFS_AUTOCONNECT,
			       &autoconnect);
	if (autoconnect && ! no_connect &&
	    empathy_idle_get_state (idle) <= MC_PRESENCE_OFFLINE) {
		empathy_idle_set_state (idle, MC_PRESENCE_AVAILABLE);
	}
	
	create_salut_account ();

	/* Setting up UI */
	window = empathy_main_window_show ();
	icon = empathy_status_icon_new (GTK_WINDOW (window), hide_contact_list);

	if (connection) {
		/* We se the callback here because we need window */
		bacon_message_connection_set_callback (connection,
						       on_bacon_message_received,
						       window);
	}

	/* Handle channels */
	dispatcher = empathy_dispatcher_dup_singleton ();
	g_signal_connect (dispatcher, "dispatch", G_CALLBACK (dispatch_cb), NULL);

	/* Logging */
	log_manager = empathy_log_manager_dup_singleton ();
	empathy_log_manager_observe (log_manager, dispatcher);

	chatroom_manager = empathy_chatroom_manager_dup_singleton (NULL);
	empathy_chatroom_manager_observe (chatroom_manager, dispatcher);

	ft_manager = empathy_ft_manager_dup_singleton ();

	notify_init (_(PACKAGE_NAME));
	/* Create the call factory */
	call_factory = empathy_call_factory_initialise ();
	g_signal_connect (G_OBJECT (call_factory), "new-call-handler",
		G_CALLBACK (new_call_handler_cb), NULL);

	gtk_main ();

	empathy_idle_set_state (idle, MC_PRESENCE_OFFLINE);

	g_object_unref (mc);
	g_object_unref (idle);
	g_object_unref (icon);
	g_object_unref (log_manager);
	g_object_unref (dispatcher);
	g_object_unref (chatroom_manager);
	g_object_unref (ft_manager);

	notify_uninit ();

	return EXIT_SUCCESS;
}

