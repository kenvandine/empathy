/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Collabora Ltd.
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
 * Authors: Davyd Madeley <davyd.madeley@collabora.co.uk>
 */

#include <config.h>

#include <gtk/gtk.h>

#include <libempathy/empathy-status-presets.h>

#include <libempathy-gtk/empathy-ui-utils.h>
#include <libempathy-gtk/empathy-status-preset-dialog.h>

int
main (int argc, char **argv)
{
	GtkWidget *dialog;

	gtk_init (&argc, &argv);
	empathy_gtk_init ();

	empathy_status_presets_get_all ();

	dialog = empathy_status_preset_dialog_new (NULL);

	gtk_widget_show (dialog);

	gtk_main ();

	return 0;
}
