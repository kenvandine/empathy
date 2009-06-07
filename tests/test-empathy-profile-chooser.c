#include <config.h>

#include <gtk/gtk.h>

#include <libempathy-gtk/empathy-ui-utils.h>
#include <libempathy-gtk/empathy-profile-chooser.h>

int
main (int argc,
    char **argv)
{
  GtkWidget *window;
  GtkWidget *chooser;

  gtk_init (&argc, &argv);
  empathy_gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  chooser = empathy_profile_chooser_new ();
  gtk_container_add (GTK_CONTAINER (window), chooser);

  /*  gtk_window_set_default_size (GTK_WINDOW (window), 150, -1);*/
  gtk_widget_show_all (window);

  g_signal_connect_swapped (window, "destroy",
      G_CALLBACK (gtk_main_quit), NULL);

  gtk_main ();

  return 0;
}
