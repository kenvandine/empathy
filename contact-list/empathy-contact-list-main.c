/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Xavier Claessens <xclaesse@gmail.com>
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
 */

#include <config.h>

#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <libempathy/gossip-contact.h>
#include <libempathy/empathy-session.h>
#include <libempathy-gtk/gossip-contact-list.h>
#include <libempathy-gtk/gossip-private-chat.h>
#include <libempathy-gtk/gossip-stock.h>

static void
destroy_cb (GtkWidget *window,
	    gpointer   user_data)
{
	gossip_stock_finalize ();
	empathy_session_finalize ();
	gtk_main_quit ();
}

static void
contact_chat_cb (GtkWidget     *list,
		 GossipContact *contact,
		 gpointer       user_data)
{
	mission_control_request_channel (empathy_session_get_mission_control (),
					 gossip_contact_get_account (contact),
					 TP_IFACE_CHANNEL_TYPE_TEXT,
					 gossip_contact_get_handle (contact),
					 TP_HANDLE_TYPE_CONTACT,
					 NULL, NULL);
}

int
main (int argc, char *argv[])
{
	GtkWidget *window;
	GtkWidget *list;
	GtkWidget *sw;

	gtk_init (&argc, &argv);

	empathy_session_connect ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gossip_stock_init (window);

	list = GTK_WIDGET (gossip_contact_list_new ());
	sw = gtk_scrolled_window_new (NULL, NULL);	
	gtk_container_add (GTK_CONTAINER (window), sw);
	gtk_container_add (GTK_CONTAINER (sw), list);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (sw, 200, 400);

	g_signal_connect (window, "destroy",
			  G_CALLBACK (destroy_cb),
			  NULL);
	g_signal_connect (list, "contact-chat",
			  G_CALLBACK (contact_chat_cb),
			  NULL);

	gtk_widget_show_all (window);

	gtk_main ();

	return EXIT_SUCCESS;
}

