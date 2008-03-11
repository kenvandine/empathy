#include <stdlib.h>

#include <glib.h>
#include <libempathy/empathy-contact-factory.h>
#include <libmissincontrol/mc-account.h>

static gboolean
time_out (gpointer main_loop)
{
	g_main_loop_quit (main_loop);

	return FALSE;
}

int
main (int argc, char **argv)
{
	EmpathyContactFactory *factory;
	GMainLoop             *main_loop;
	McAccount             *account;
	EmpathyContact        *contact;

	g_type_init ();

	main_loop = g_main_loop_new (NULL, FALSE);
	factory = empathy_contact_factory_new ();
	account = mc_account_lookup ("jabber4");
	g_print ("Got account %p\n", account);

	contact = empathy_contact_factory_get_from_id ("testman@jabber.belnet.be");
	g_print ("Got contact with handle %d\n", emapthy_contact_get_handle (contact));

	empathy_contact_run_until_ready (contact, EMPATHY_CONTACT_READY_HANDLE);
	g_print ("Contact handle is now %d\n", emapthy_contact_get_handle (contact));

	g_timeout_add_seconds (5, time_out, main_loop);

	g_main_loop_run (main_loop);

	g_object_unref (manager);
	g_main_loop_unref (main_loop);

	return EXIT_SUCCESS;
}

