#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <libempathy/empathy-contact-factory.h>
#include <libmissioncontrol/mc-account.h>

static gboolean
callback (gpointer data)
{
	EmpathyContactFactory *factory;
	McAccount             *account;
	EmpathyContact        *contact;
	EmpathyContactReady    ready_flags;

	factory = empathy_contact_factory_dup_singleton ();
	account = mc_account_lookup ("jabber0");
	contact = empathy_contact_factory_get_from_handle (factory, account, 2);

	g_print ("Contact handle=%d alias=%s\n",
		 empathy_contact_get_handle (contact),
		 empathy_contact_get_name (contact));

	ready_flags = EMPATHY_CONTACT_READY_HANDLE | EMPATHY_CONTACT_READY_NAME;
	empathy_contact_run_until_ready (contact, ready_flags, NULL);

	g_print ("Contact ready: handle=%d alias=%s ready=%d needed-ready=%d\n",
		 empathy_contact_get_handle (contact),
		 empathy_contact_get_name (contact),
		 empathy_contact_get_ready (contact),
		 ready_flags);

	g_object_unref (factory);
	g_object_unref (account);
	g_object_unref (contact);

	gtk_main_quit ();

	return FALSE;
}

int
main (int argc, char **argv)
{
	gtk_init (&argc, &argv);

	g_idle_add (callback, NULL);

	gtk_main ();

	return EXIT_SUCCESS;
}

