#include "config.h"

#include <gtk/gtk.h>

#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-dispatcher.h>

#include <libempathy-gtk/empathy-contact-list-store.h>
#include <libempathy-gtk/empathy-contact-selector.h>

static GtkWidget *window = NULL;

static void
destroy_cb (GtkWidget *widget,
            gpointer data)
{
  gtk_main_quit ();
}


static void
chat_cb (EmpathyDispatchOperation *dispatch,
         const GError *error,
         gpointer user_data)
{
  GtkWidget *dialog;

  if (error != NULL)
    {
      dialog = gtk_message_dialog_new (GTK_WINDOW (window), GTK_DIALOG_MODAL,
          GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
          error->message ? error->message : "No error message");

      gtk_dialog_run (GTK_DIALOG (dialog));
    }

  gtk_widget_destroy (window);
}

static void
clicked_cb (GtkButton *button,
            gpointer data)
{
  EmpathyContactSelector *selector = EMPATHY_CONTACT_SELECTOR (data);
  EmpathyContact *contact;

  contact = empathy_contact_selector_get_selected (selector);

  if (!contact)
    return;

  /* This is required otherwise the dispatcher isn't ref'd, and so it
   * disappears by the time the callback gets called. It's deliberately not
   * freed otherwise it segfaults... sigh */
  empathy_dispatcher_dup_singleton ();
  empathy_dispatcher_chat_with_contact (contact, chat_cb, NULL);
}

int main (int argc,
          char *argv[])
{
  EmpathyContactManager *manager;
  EmpathyContactListStore *store;
  EmpathyContactSelector *selector;
  GtkWidget *vbox, *button;
  gchar *icon_path;

  gtk_init (&argc, &argv);

  icon_path = g_build_path (G_DIR_SEPARATOR_S, PKGDATADIR, "icons", NULL);
  gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (), icon_path);
  g_free (icon_path);

  manager = empathy_contact_manager_dup_singleton ();
  store = empathy_contact_list_store_new (EMPATHY_CONTACT_LIST (manager));

  vbox = gtk_vbox_new (FALSE, 2);

  selector = empathy_contact_selector_new (store);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (selector), FALSE, FALSE, 5);

  button = gtk_button_new_with_label ("Chat");
  g_signal_connect (G_OBJECT (button), "clicked",
      G_CALLBACK (clicked_cb), (gpointer) selector);
  gtk_box_pack_start(GTK_BOX (vbox), button, FALSE, FALSE, 5);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "destroy",
      G_CALLBACK (destroy_cb), NULL);
  gtk_window_set_title (GTK_WINDOW (window),"Empetit");
  gtk_container_set_border_width (GTK_CONTAINER (window), 5);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show_all (window);

  gtk_main ();

  g_object_unref (store);
  g_object_unref (manager);

  return 0;
}
