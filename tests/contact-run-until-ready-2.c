#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <libempathy/empathy-contact-factory.h>
#include <libempathy/empathy-contact-manager.h>
#include <libmissioncontrol/mc-account.h>

static void
pending_cb (EmpathyContactManager *manager,
	    EmpathyContact        *contact,
	    EmpathyContact        *actor,
	    guint                  reason,
	    gchar                 *message,
	    gboolean               is_pending,
	    gpointer               data)
{
	if (!is_pending) {
		return;
	}

	g_print ("Contact handle=%d alias=%s\n",
		 empathy_contact_get_handle (contact),
		 empathy_contact_get_name (contact));

	empathy_contact_run_until_ready (contact,
					 EMPATHY_CONTACT_READY_NAME,
					 NULL);

	g_print ("Contact ready: handle=%d alias=%s ready=%d\n",
		 empathy_contact_get_handle (contact),
		 empathy_contact_get_name (contact),
		 empathy_contact_get_ready (contact));

	g_object_unref (manager);
	gtk_main_quit ();
}

static gboolean
callback (gpointer data)
{
	EmpathyContactManager *manager;

	manager = empathy_contact_manager_dup_singleton ();
	g_signal_connect (manager, "pendings-changed",
			  G_CALLBACK (pending_cb),
			  NULL);

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

