#include <stdlib.h>

#include <gtk/gtk.h>
#include <libempathy/empathy-contact-manager.h>

static gboolean
time_out (gpointer data)
{
	gtk_main_quit ();

	return FALSE;
}

int
main (int argc, char **argv)
{
	EmpathyContactManager *manager;

	gtk_init (&argc, &argv);

	manager = empathy_contact_manager_new ();
	
	g_timeout_add (5000, time_out, NULL);

	gtk_main ();
	
	g_object_unref (manager);

	return EXIT_SUCCESS;
}
