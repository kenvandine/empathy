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

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <libempathy/empathy-session.h>
#include <libempathy-gtk/gossip-accounts-dialog.h>

static void
destroy_cb (GtkWidget *dialog,
	    gpointer   user_data)
{
	empathy_session_finalize ();
	gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
	GtkWidget *dialog;

	gtk_init (&argc, &argv);

	dialog = gossip_accounts_dialog_show ();

	gtk_widget_show (dialog);
	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (destroy_cb),
			  NULL);

	gtk_main ();

	return EXIT_SUCCESS;
}

