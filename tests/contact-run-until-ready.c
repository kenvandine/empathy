#include <stdlib.h>

#include <glib.h>
#include <libempathy/empathy-contact-factory.h>
#include <libmissioncontrol/mc-account.h>

int
main (int argc, char **argv)
{
	EmpathyContactFactory *factory;
	McAccount             *account;
	EmpathyContact        *contact;
	EmpathyContactReady    ready_flags;

	g_type_init ();

	factory = empathy_contact_factory_new ();
	account = mc_account_lookup ("jabber4");
	contact = empathy_contact_factory_get_from_id (factory, account,
						       "testman@jabber.belnet.be");

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

	return EXIT_SUCCESS;
}

