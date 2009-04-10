#include "config.h"

#include <gtk/gtk.h>

#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-dispatcher.h>

#include <libempathy-gtk/empathy-ui-utils.h>
#include <libempathy-gtk/empathy-contact-list-store.h>
#include <libempathy-gtk/empathy-contact-selector.h>

static GtkWidget *window = NULL;

static void
chat_cb (EmpathyDispatchOperation *dispatch,
         const GError *error,
         gpointer user_data)
{
  GtkWidget *dialog;

  if (error != NULL)
    {
      dialog = gtk_message_dialog_new (GTK_WINDOW (window), GTK_DIALOG_MODAL,
          GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s",
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

  contact = empathy_contact_selector_dup_selected (selector);

  if (!contact)
    return;

  empathy_dispatcher_chat_with_contact (contact, chat_cb, NULL);

  g_object_unref (contact);
}

int main (int argc,
          char *argv[])
{
  EmpathyContactManager *manager;
  GtkWidget *vbox, *button, *selector;

  gtk_init (&argc, &argv);

  empathy_gtk_init ();

  manager = empathy_contact_manager_dup_singleton ();
  selector = empathy_contact_selector_new (EMPATHY_CONTACT_LIST (manager));

  empathy_contact_selector_set_visible (EMPATHY_CONTACT_SELECTOR (selector),
      (EmpathyContactSelectorFilterFunc) empathy_contact_can_send_files, NULL);

  vbox = gtk_vbox_new (FALSE, 2);

  gtk_box_pack_start (GTK_BOX (vbox), selector, FALSE, FALSE, 5);

  button = gtk_button_new_with_label ("Chat");
  g_signal_connect (G_OBJECT (button), "clicked",
      G_CALLBACK (clicked_cb), (gpointer) selector);
  gtk_box_pack_start(GTK_BOX (vbox), button, FALSE, FALSE, 5);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "destroy",
      gtk_main_quit, NULL);
  gtk_window_set_title (GTK_WINDOW (window),"Empetit");
  gtk_container_set_border_width (GTK_CONTAINER (window), 5);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_widget_show_all (window);

  gtk_main ();

  g_object_unref (manager);

  return 0;
}
