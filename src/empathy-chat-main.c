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

#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-helpers.h>
#include <libmissioncontrol/mc-account.h>

#include <libempathy/gossip-contact.h>
#include <libempathy/gossip-debug.h>
#include <libempathy/empathy-chandler.h>
#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-contact-list.h>
#include <libempathy-gtk/gossip-private-chat.h>

#define DEBUG_DOMAIN "ChatMain"

#define BUS_NAME "org.gnome.Empathy.Chat"
#define OBJECT_PATH "/org/freedesktop/Telepathy/ChannelHandler"

/* Time to wait before exit, in seconds */
#define EXIT_TIMEOUT 5


static guint chat_count = 0;
static guint exit_timeout = 0;


static gboolean
exit_timeout_cb (gpointer user_data)
{
	gossip_debug (DEBUG_DOMAIN, "Timeout, exiting");

	gtk_main_quit ();

	return FALSE;
}

static void
exit_timeout_start (void)
{
	if (exit_timeout) {
		return;
	}

	exit_timeout = g_timeout_add (EXIT_TIMEOUT * 1000,
				      (GSourceFunc) exit_timeout_cb,
				      NULL);
}

static void
exit_timeout_stop (void)
{
	if (exit_timeout) {
		gossip_debug (DEBUG_DOMAIN, "Exit timeout canceled");
		g_source_remove (exit_timeout);
		exit_timeout = 0;
	}
}

static void
chat_finalized_cb (gpointer    user_data,
		   GossipChat *chat)
{
	chat_count--;
	if (chat_count == 0) {
		gossip_debug (DEBUG_DOMAIN, "No more chat, start exit timeout");
		exit_timeout_start ();
	}
}

static void
new_channel_cb (EmpathyChandler *chandler,
		TpConn          *tp_conn,
		TpChan          *tp_chan,
		gpointer         user_data)
{
	if (tp_chan->handle_type == TP_HANDLE_TYPE_CONTACT) {
		MissionControl        *mc;
		McAccount             *account;
		EmpathyContactManager *manager;
		EmpathyContactList    *list;
		GossipContact         *contact;
		GossipPrivateChat     *chat;

		/* We have a private chat channel */
		mc = mission_control_new (tp_get_bus ());
		account = mission_control_get_account_for_connection (mc, tp_conn, NULL);
		manager = empathy_contact_manager_new ();
		list = empathy_contact_manager_get_list (manager, account);
		contact = empathy_contact_list_get_from_handle (list, tp_chan->handle);

		chat = gossip_private_chat_new_with_channel (contact, tp_chan);
		g_object_weak_ref (G_OBJECT (chat),
				   (GWeakNotify) chat_finalized_cb,
				   NULL);

		exit_timeout_stop ();
		chat_count++;

		gossip_chat_present (GOSSIP_CHAT (chat));

		g_object_unref (mc);
		g_object_unref (account);
		g_object_unref (contact);
		g_object_unref (chat);
		g_object_unref (manager);
	}
}

int
main (int argc, char *argv[])
{
	EmpathyChandler *chandler;

	gtk_init (&argc, &argv);

	exit_timeout_start ();
	chandler = empathy_chandler_new (BUS_NAME, OBJECT_PATH);

	g_signal_connect (chandler, "new-channel",
			  G_CALLBACK (new_channel_cb),
			  NULL);

	gtk_main ();

	return EXIT_SUCCESS;
}

