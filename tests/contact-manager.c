#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-debug.h>

#include <libempathy-gtk/empathy-contact-list-store.h>
#include <libempathy-gtk/empathy-ui-utils.h>

int
main (int argc, char **argv)
{
	EmpathyContactManager *manager;
	GMainLoop             *main_loop;
	EmpathyContactListStore *store;
	GtkWidget *combo;
	GtkWidget *window;
	GtkCellRenderer *renderer;

	gtk_init (&argc, &argv);
	empathy_gtk_init ();

	empathy_debug_set_flags (g_getenv ("EMPATHY_DEBUG"));
	main_loop = g_main_loop_new (NULL, FALSE);
	manager = empathy_contact_manager_dup_singleton ();
	store = empathy_contact_list_store_new (EMPATHY_CONTACT_LIST (manager));
	empathy_contact_list_store_set_is_compact (store, TRUE);
	empathy_contact_list_store_set_show_groups (store, FALSE);
	combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), renderer, "text", EMPATHY_CONTACT_LIST_STORE_COL_NAME);
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_add (GTK_CONTAINER (window), combo);
	gtk_widget_show (combo);
	gtk_widget_show (window);
	g_object_unref (manager);

	gtk_main ();

	return EXIT_SUCCESS;
}

