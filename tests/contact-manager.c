#include <stdlib.h>

#include <glib.h>
#include <libempathy/empathy-contact-manager.h>

static gboolean
time_out (gpointer main_loop)
{
	g_main_loop_quit (main_loop);

	return FALSE;
}

int
main (int argc, char **argv)
{
	EmpathyContactManager *manager;
	GMainLoop             *main_loop;

	g_type_init ();

	main_loop = g_main_loop_new (NULL, FALSE);
	manager = empathy_contact_manager_new ();
	
	g_timeout_add (5000, time_out, main_loop);

	g_main_loop_run (main_loop);
	
	g_object_unref (manager);
	g_main_loop_unref (main_loop);

	return EXIT_SUCCESS;
}

