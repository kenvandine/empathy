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

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <libempathy-gtk/empathy-accounts-dialog.h>

static void
destroy_cb (GtkWidget *dialog,
	    gpointer   user_data)
{
	gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
	GtkWidget *dialog;

	gtk_init (&argc, &argv);

	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   PKGDATADIR G_DIR_SEPARATOR_S "icons");
	dialog = empathy_accounts_dialog_show (NULL);

	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (destroy_cb),
			  NULL);

	gtk_main ();

	return EXIT_SUCCESS;
}

