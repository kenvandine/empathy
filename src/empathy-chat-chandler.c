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

#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-chandler.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-tp-chat.h>
#include <libempathy/empathy-tp-chatroom.h>
#include <libempathy/empathy-debug.h>

#include <libempathy-gtk/empathy-chat.h>
#include <libempathy-gtk/empathy-private-chat.h>
#include <libempathy-gtk/empathy-group-chat.h>
#include <libempathy-gtk/empathy-chat-window.h>

#define DEBUG_DOMAIN "EmpathyChat"

#define BUS_NAME "org.gnome.Empathy.ChatChandler"
#define OBJECT_PATH "/org/gnome/Empathy/ChatChandler"

static guint nb_chats = 0;

static void
chat_chandler_weak_notify (gpointer  data,
			   GObject  *where_the_object_was)
{
	nb_chats--;
	if (nb_chats == 0) {
		empathy_debug (DEBUG_DOMAIN, "No more chats, leaving...");
		gtk_main_quit ();
	}
}

static void
chat_chandler_new_channel_cb (EmpathyChandler *chandler,
			      TpConn          *tp_conn,
			      TpChan          *tp_chan,
			      MissionControl  *mc)
{
	EmpathyTpChat *tp_chat;
	McAccount     *account;
	EmpathyChat   *chat;
	gchar         *id;

	account = mission_control_get_account_for_connection (mc, tp_conn, NULL);
	id = empathy_inspect_channel (account, tp_chan);
	chat = empathy_chat_window_find_chat (account, id);
	g_free (id);

	if (chat) {
		/* The chat already exists */
		if (!empathy_chat_is_connected (chat)) {
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
		tp_chat = empathy_tp_chat_new (account, tp_chan);
		chat = EMPATHY_CHAT (empathy_private_chat_new (tp_chat));
	}
	else if (tp_chan->handle_type == TP_HANDLE_TYPE_ROOM) {
		/* We have a new group chat channel */
		tp_chat = EMPATHY_TP_CHAT (empathy_tp_chatroom_new (account, tp_chan));
		chat = EMPATHY_CHAT (empathy_group_chat_new (EMPATHY_TP_CHATROOM (tp_chat)));
	} else {
		empathy_debug (DEBUG_DOMAIN,
			       "Unknown handle type (%d) for Text channel",
			       tp_chan->handle_type);
		g_object_unref (account);
		return;
	}

	nb_chats++;
	g_object_weak_ref (G_OBJECT (chat), chat_chandler_weak_notify, NULL);
	empathy_chat_present (chat);

	g_object_unref (chat);
	g_object_unref (account);
	g_object_unref (tp_chat);
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
			  G_CALLBACK (chat_chandler_new_channel_cb),
			  mc);

	empathy_debug (DEBUG_DOMAIN, "Ready to handle new text channels");

	gtk_main ();

	g_object_unref (chandler);
	g_object_unref (mc);

	return EXIT_SUCCESS;
}

