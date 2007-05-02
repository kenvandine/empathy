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

#include <libmissioncontrol/mc-account.h>

#include <libempathy/empathy-session.h>
#include <libempathy-gtk/empathy-main-window.h>
#include <libempathy-gtk/gossip-stock.h>
#include <libempathy-gtk/gossip-accounts-dialog.h>

static void
destroy_cb (GtkWidget *window,
	    gpointer   user_data)
{
	gossip_stock_finalize ();
	empathy_session_finalize ();
	gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
	GtkWidget *window;
	GList     *accounts;

	gtk_init (&argc, &argv);

	window = empathy_main_window_show ();
	gossip_stock_init (window);

	g_signal_connect (window, "destroy",
			  G_CALLBACK (destroy_cb),
			  NULL);

	/* Show the accounts dialog if there is no enabled accounts */
	accounts = mc_accounts_list_by_enabled (TRUE);
	if (accounts) {
		mc_accounts_list_free (accounts);
	} else {
		gossip_accounts_dialog_show ();
	}

	gtk_main ();

	return EXIT_SUCCESS;
}

