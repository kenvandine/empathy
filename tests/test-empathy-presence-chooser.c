/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>

#include <gtk/gtk.h>

#include <libempathy-gtk/empathy-ui-utils.h>
#include <libempathy-gtk/empathy-presence-chooser.h>

int
main (int argc, char **argv)
{
	gtk_init (&argc, &argv);
	empathy_gtk_init ();

	GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	GtkWidget *chooser = empathy_presence_chooser_new ();
	gtk_container_add (GTK_CONTAINER (window), chooser);

	gtk_window_set_default_size (GTK_WINDOW (window), 150, -1);
	gtk_widget_show_all (window);

	g_signal_connect_swapped (window, "destroy",
			G_CALLBACK (gtk_main_quit), NULL);

	gtk_main ();

	return 0;
}
