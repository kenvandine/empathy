/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007-2009 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#if HAVE_LIBCHAMPLAIN
#include <champlain/champlain.h>
#include <champlain-gtk/champlain-gtk.h>
#endif

#include <libmissioncontrol/mc-account.h>
#include <telepathy-glib/util.h>

#include <libempathy/empathy-tp-contact-factory.h>
#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-contact-list.h>
#include <libempathy/empathy-location.h>
#include <libempathy/empathy-utils.h>

#include "empathy-contact-widget.h"
#include "empathy-account-chooser.h"
#include "empathy-avatar-chooser.h"
#include "empathy-avatar-image.h"
#include "empathy-ui-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_CONTACT
#include <libempathy/empathy-debug.h>

/**
 * SECTION:empathy-contact-widget
 * @title:EmpathyContactWidget
 * @short_description: A widget used to display and edit details about a contact
 * @include: libempathy-empathy-contact-widget.h
 *
 * #EmpathyContactWidget is a widget which displays appropriate widgets
 * with details about a contact, also allowing changing these details,
 * if desired.
 */

/**
 * EmpathyContactWidget:
 * @parent: parent object
 *
 * Widget which displays appropriate widgets with details about a contact,
 * also allowing changing these details, if desired.
 */

/* Delay before updating the widget when the id entry changed (seconds) */
#define ID_CHANGED_TIMEOUT 1

typedef struct
{
  EmpathyTpContactFactory *factory;
  EmpathyContactManager *manager;
  EmpathyContact *contact;
  EmpathyContactWidgetFlags flags;
  guint widget_id_timeout;

  GtkWidget *vbox_contact_widget;

  /* Contact */
  GtkWidget *vbox_contact;
  GtkWidget *widget_avatar;
  GtkWidget *widget_account;
  GtkWidget *widget_id;
  GtkWidget *widget_alias;
  GtkWidget *label_alias;
  GtkWidget *entry_alias;
  GtkWidget *hbox_presence;
  GtkWidget *image_state;
  GtkWidget *label_status;
  GtkWidget *table_contact;
  GtkWidget *vbox_avatar;

  /* Location */
  GtkWidget *vbox_location;
  GtkWidget *subvbox_location;
  GtkWidget *table_location;
  GtkWidget *label_location;
#if HAVE_LIBCHAMPLAIN
  GtkWidget *viewport_map;
  GtkWidget *map_view_embed;
  ClutterActor *map_view;
#endif

  /* Groups */
  GtkWidget *vbox_groups;
  GtkWidget *entry_group;
  GtkWidget *button_group;
  GtkWidget *treeview_groups;

  /* Details */
  GtkWidget *vbox_details;
  GtkWidget *table_details;
  GtkWidget *hbox_details_requested;

  /* Client */
  GtkWidget *vbox_client;
  GtkWidget *table_client;
  GtkWidget *hbox_client_requested;
} EmpathyContactWidget;

typedef struct
{
  EmpathyContactWidget *information;
  const gchar *name;
  gboolean found;
  GtkTreeIter found_iter;
} FindName;

static void contact_widget_destroy_cb (GtkWidget *widget,
    EmpathyContactWidget *information);
static void contact_widget_remove_contact (EmpathyContactWidget *information);
static void contact_widget_set_contact (EmpathyContactWidget *information,
    EmpathyContact *contact);
static void contact_widget_contact_setup (EmpathyContactWidget *information);
static void contact_widget_contact_update (EmpathyContactWidget *information);
static void contact_widget_change_contact (EmpathyContactWidget *information);
static void contact_widget_avatar_changed_cb (EmpathyAvatarChooser *chooser,
    EmpathyContactWidget *information);
static gboolean contact_widget_id_focus_out_cb (GtkWidget *widget,
    GdkEventFocus *event, EmpathyContactWidget *information);
static gboolean contact_widget_entry_alias_focus_event_cb (
    GtkEditable *editable, GdkEventFocus *event,
    EmpathyContactWidget *information);
static void contact_widget_name_notify_cb (EmpathyContactWidget *information);
static void contact_widget_presence_notify_cb (
    EmpathyContactWidget *information);
static void contact_widget_avatar_notify_cb (
    EmpathyContactWidget *information);
static void contact_widget_groups_setup (
    EmpathyContactWidget *information);
static void contact_widget_groups_update (EmpathyContactWidget *information);
static void contact_widget_model_setup (EmpathyContactWidget *information);
static void contact_widget_model_populate_columns (
    EmpathyContactWidget *information);
static void contact_widget_groups_populate_data (
    EmpathyContactWidget *information);
static void contact_widget_groups_notify_cb (
    EmpathyContactWidget *information);
static gboolean contact_widget_model_find_name (
    EmpathyContactWidget *information,const gchar *name, GtkTreeIter *iter);
static gboolean contact_widget_model_find_name_foreach (GtkTreeModel *model,
    GtkTreePath *path, GtkTreeIter *iter, FindName *data);
static void contact_widget_cell_toggled (GtkCellRendererToggle *cell,
    gchar *path_string, EmpathyContactWidget *information);
static void contact_widget_entry_group_changed_cb (GtkEditable *editable,
    EmpathyContactWidget *information);
static void contact_widget_entry_group_activate_cb (GtkEntry *entry,
    EmpathyContactWidget *information);
static void contact_widget_button_group_clicked_cb (GtkButton *button,
    EmpathyContactWidget *information);
static void contact_widget_details_setup (EmpathyContactWidget *information);
static void contact_widget_details_update (EmpathyContactWidget *information);
static void contact_widget_client_setup (EmpathyContactWidget *information);
static void contact_widget_client_update (EmpathyContactWidget *information);
static void contact_widget_location_update (EmpathyContactWidget *information);

enum
{
  COL_NAME,
  COL_ENABLED,
  COL_EDITABLE,
  COL_COUNT
};

/**
 * empathy_contact_widget_new:
 * @contact: an #EmpathyContact
 * @flags: #EmpathyContactWidgetFlags for the new contact widget
 *
 * Creates a new #EmpathyContactWidget.
 *
 * Return value: a new #EmpathyContactWidget
 */
GtkWidget *
empathy_contact_widget_new (EmpathyContact *contact,
                            EmpathyContactWidgetFlags flags)
{
  EmpathyContactWidget *information;
  GtkBuilder *gui;
  gchar *filename;

  g_return_val_if_fail (contact == NULL || EMPATHY_IS_CONTACT (contact), NULL);

  information = g_slice_new0 (EmpathyContactWidget);
  information->flags = flags;

  filename = empathy_file_lookup ("empathy-contact-widget.ui",
      "libempathy-gtk");
  gui = empathy_builder_get_file (filename,
       "vbox_contact_widget", &information->vbox_contact_widget,
       "vbox_contact", &information->vbox_contact,
       "hbox_presence", &information->hbox_presence,
       "label_alias", &information->label_alias,
       "image_state", &information->image_state,
       "label_status", &information->label_status,
       "table_contact", &information->table_contact,
       "vbox_avatar", &information->vbox_avatar,
       "vbox_location", &information->vbox_location,
       "subvbox_location", &information->subvbox_location,
       "label_location", &information->label_location,
#if HAVE_LIBCHAMPLAIN
       "viewport_map", &information->viewport_map,
#endif
       "vbox_groups", &information->vbox_groups,
       "entry_group", &information->entry_group,
       "button_group", &information->button_group,
       "treeview_groups", &information->treeview_groups,
       "vbox_details", &information->vbox_details,
       "table_details", &information->table_details,
       "hbox_details_requested", &information->hbox_details_requested,
       "vbox_client", &information->vbox_client,
       "table_client", &information->table_client,
       "hbox_client_requested", &information->hbox_client_requested,
       NULL);
  g_free (filename);

  empathy_builder_connect (gui, information,
      "vbox_contact_widget", "destroy", contact_widget_destroy_cb,
      "entry_group", "changed", contact_widget_entry_group_changed_cb,
      "entry_group", "activate", contact_widget_entry_group_activate_cb,
      "button_group", "clicked", contact_widget_button_group_clicked_cb,
      NULL);
  information->table_location = NULL;

  g_object_set_data (G_OBJECT (information->vbox_contact_widget),
      "EmpathyContactWidget",
      information);

  /* Create widgets */
  contact_widget_contact_setup (information);
  contact_widget_groups_setup (information);
  contact_widget_details_setup (information);
  contact_widget_client_setup (information);

  if (contact != NULL)
    contact_widget_set_contact (information, contact);

  else if (information->flags & EMPATHY_CONTACT_WIDGET_EDIT_ACCOUNT ||
      information->flags & EMPATHY_CONTACT_WIDGET_EDIT_ID)
    contact_widget_change_contact (information);

  return empathy_builder_unref_and_keep_widget (gui,
    information->vbox_contact_widget);
}

/**
 * empathy_contact_widget_get_contact:
 * @widget: an #EmpathyContactWidget
 *
 * Get the #EmpathyContact related with the #EmpathyContactWidget @widget.
 *
 * Returns: the #EmpathyContact associated with @widget
 */
EmpathyContact *
empathy_contact_widget_get_contact (GtkWidget *widget)
{
  EmpathyContactWidget *information;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  information = g_object_get_data (G_OBJECT (widget), "EmpathyContactWidget");
  if (!information)
      return NULL;

  return information->contact;
}

/**
 * empathy_contact_widget_set_contact:
 * @widget: an #EmpathyContactWidget
 * @contact: a different #EmpathyContact
 *
 * Change the #EmpathyContact related with the #EmpathyContactWidget @widget.
 */
void
empathy_contact_widget_set_contact (GtkWidget *widget,
                                    EmpathyContact *contact)
{
  EmpathyContactWidget *information;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (EMPATHY_IS_CONTACT (contact));

  information = g_object_get_data (G_OBJECT (widget), "EmpathyContactWidget");
  if (!information)
    return;

  contact_widget_set_contact (information, contact);
}

/**
 * empathy_contact_widget_set_account_filter:
 * @widget: an #EmpathyContactWidget
 * @filter: a #EmpathyAccountChooserFilterFunc
 * @user_data: user data to pass to @filter, or %NULL
 *
 * Set a filter on the #EmpathyAccountChooser included in the
 * #EmpathyContactWidget.
 */
void
empathy_contact_widget_set_account_filter (
    GtkWidget *widget,
    EmpathyAccountChooserFilterFunc filter,
    gpointer user_data)
{
  EmpathyContactWidget *information;
  EmpathyAccountChooser *chooser;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  information = g_object_get_data (G_OBJECT (widget), "EmpathyContactWidget");
  if (!information)
    return;

  chooser = EMPATHY_ACCOUNT_CHOOSER (information->widget_account);
  if (chooser)
      empathy_account_chooser_set_filter (chooser, filter, user_data);
}

static void
contact_widget_destroy_cb (GtkWidget *widget,
                           EmpathyContactWidget *information)
{
  contact_widget_remove_contact (information);

  if (information->widget_id_timeout != 0)
    {
      g_source_remove (information->widget_id_timeout);
    }
  if (information->manager)
    {
      g_object_unref (information->manager);
    }

  g_slice_free (EmpathyContactWidget, information);
}

static void
contact_widget_remove_contact (EmpathyContactWidget *information)
{
  if (information->contact)
    {
      g_signal_handlers_disconnect_by_func (information->contact,
          contact_widget_name_notify_cb, information);
      g_signal_handlers_disconnect_by_func (information->contact,
          contact_widget_presence_notify_cb, information);
      g_signal_handlers_disconnect_by_func (information->contact,
          contact_widget_avatar_notify_cb, information);
      g_signal_handlers_disconnect_by_func (information->contact,
          contact_widget_groups_notify_cb, information);

      g_object_unref (information->contact);
      g_object_unref (information->factory);
      information->contact = NULL;
      information->factory = NULL;
    }
}

static void
contact_widget_set_contact (EmpathyContactWidget *information,
                            EmpathyContact *contact)
{
  if (contact == information->contact)
    return;

  contact_widget_remove_contact (information);
  if (contact)
    {
      TpConnection *connection;

      connection = empathy_contact_get_connection (contact);
      information->contact = g_object_ref (contact);
      information->factory = empathy_tp_contact_factory_dup_singleton (connection);
    }

  /* Update information for widgets */
  contact_widget_contact_update (information);
  contact_widget_groups_update (information);
  contact_widget_details_update (information);
  contact_widget_client_update (information);
  contact_widget_location_update (information);
}

static gboolean
contact_widget_id_activate_timeout (EmpathyContactWidget *self)
{
  contact_widget_change_contact (self);
  return FALSE;
}

static void
contact_widget_id_changed_cb (GtkEntry *entry,
                              EmpathyContactWidget *self)
{
  if (self->widget_id_timeout != 0)
    {
      g_source_remove (self->widget_id_timeout);
    }

  self->widget_id_timeout =
    g_timeout_add_seconds (ID_CHANGED_TIMEOUT,
        (GSourceFunc) contact_widget_id_activate_timeout, self);
}

static void
save_avatar_menu_activate_cb (GtkWidget *widget,
                              EmpathyContactWidget *information)
{
  GtkWidget *dialog;
  EmpathyAvatar *avatar;
  gchar *ext = NULL, *filename;

  dialog = gtk_file_chooser_dialog_new (_("Save Avatar"),
      NULL,
      GTK_FILE_CHOOSER_ACTION_SAVE,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
      NULL);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog),
      TRUE);

  /* look for the avatar extension */
  avatar = empathy_contact_get_avatar (information->contact);
  if (avatar->format != NULL)
    {
      gchar **splitted;

      splitted = g_strsplit (avatar->format, "/", 2);
      if (splitted[0] != NULL && splitted[1] != NULL)
          ext = g_strdup (splitted[1]);

      g_strfreev (splitted);
    }
  else
    {
      /* Avatar was loaded from the cache so was converted to PNG */
      ext = g_strdup ("png");
    }

  if (ext != NULL)
    {
      gchar *id;

      id = tp_escape_as_identifier (empathy_contact_get_id (
            information->contact));

      filename = g_strdup_printf ("%s.%s", id, ext);
      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), filename);

      g_free (id);
      g_free (ext);
      g_free (filename);
    }

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      GError *error = NULL;

      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

      if (!empathy_avatar_save_to_file (avatar, filename, &error))
        {
          /* Save error */
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (NULL, 0,
              GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
              _("Unable to save avatar"));

          gtk_message_dialog_format_secondary_text (
              GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

          g_signal_connect (dialog, "response",
              G_CALLBACK (gtk_widget_destroy), NULL);

          gtk_window_present (GTK_WINDOW (dialog));

          g_clear_error (&error);
        }

      g_free (filename);
    }

  gtk_widget_destroy (dialog);
}

static void
popup_avatar_menu (EmpathyContactWidget *information,
                   GtkWidget *parent,
                   GdkEventButton *event)
{
  GtkWidget *menu, *item;
  gint button, event_time;

  if (information->contact == NULL ||
      empathy_contact_get_avatar (information->contact) == NULL)
      return;

  menu = gtk_menu_new ();

  /* Add "Save as..." entry */
  item = gtk_image_menu_item_new_from_stock (GTK_STOCK_SAVE_AS, NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  g_signal_connect (item, "activate",
      G_CALLBACK (save_avatar_menu_activate_cb), information);

  if (event)
    {
      button = event->button;
      event_time = event->time;
    }
  else
    {
      button = 0;
      event_time = gtk_get_current_event_time ();
    }

  gtk_menu_attach_to_widget (GTK_MENU (menu), parent, NULL);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
      button, event_time);
}

static gboolean
widget_avatar_popup_menu_cb (GtkWidget *widget,
                             EmpathyContactWidget *information)
{
  popup_avatar_menu (information, widget, NULL);

  return TRUE;
}

static gboolean
widget_avatar_button_press_event_cb (GtkWidget *widget,
                                     GdkEventButton *event,
                                     EmpathyContactWidget *information)
{
  /* Ignore double-clicks and triple-clicks */
  if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
      popup_avatar_menu (information, widget, event);
      return TRUE;
    }

  return FALSE;
}

static void
update_avatar_chooser_account_cb (EmpathyAccountChooser *account_chooser,
                                  EmpathyAvatarChooser *avatar_chooser)
{
  TpConnection *connection;

  connection = empathy_account_chooser_get_connection (account_chooser);
  g_object_set (avatar_chooser, "connection", connection, NULL);
}

static void
contact_widget_contact_setup (EmpathyContactWidget *information)
{
  /* Setup account label/chooser */
  if (information->flags & EMPATHY_CONTACT_WIDGET_EDIT_ACCOUNT)
    {
      information->widget_account = empathy_account_chooser_new ();

      g_signal_connect_swapped (information->widget_account, "changed",
            G_CALLBACK (contact_widget_change_contact),
            information);
    }
  else
    {
      information->widget_account = gtk_label_new (NULL);
      if (!(information->flags & EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP)) {
        gtk_label_set_selectable (GTK_LABEL (information->widget_account), TRUE);
      }
      gtk_misc_set_alignment (GTK_MISC (information->widget_account), 0, 0.5);
    }
  gtk_table_attach_defaults (GTK_TABLE (information->table_contact),
           information->widget_account,
           1, 2, 0, 1);
  gtk_widget_show (information->widget_account);

  /* Set up avatar chooser/display */
  if (information->flags & EMPATHY_CONTACT_WIDGET_EDIT_AVATAR)
    {
      information->widget_avatar = empathy_avatar_chooser_new ();
      g_signal_connect (information->widget_avatar, "changed",
            G_CALLBACK (contact_widget_avatar_changed_cb),
            information);
      if (information->flags & EMPATHY_CONTACT_WIDGET_EDIT_ACCOUNT)
        {
          g_signal_connect (information->widget_account, "changed",
              G_CALLBACK (update_avatar_chooser_account_cb),
              information->widget_avatar);
          update_avatar_chooser_account_cb (
              EMPATHY_ACCOUNT_CHOOSER (information->widget_account),
              EMPATHY_AVATAR_CHOOSER (information->widget_avatar));
        }
    }
  else
    {
      information->widget_avatar = empathy_avatar_image_new ();

      g_signal_connect (information->widget_avatar, "popup-menu",
          G_CALLBACK (widget_avatar_popup_menu_cb), information);
      g_signal_connect (information->widget_avatar, "button-press-event",
          G_CALLBACK (widget_avatar_button_press_event_cb), information);
    }

  gtk_box_pack_start (GTK_BOX (information->vbox_avatar),
          information->widget_avatar,
          FALSE, FALSE,
          6);
  gtk_widget_show (information->widget_avatar);

  /* Setup id label/entry */
  if (information->flags & EMPATHY_CONTACT_WIDGET_EDIT_ID)
    {
      information->widget_id = gtk_entry_new ();
      g_signal_connect (information->widget_id, "focus-out-event",
            G_CALLBACK (contact_widget_id_focus_out_cb),
            information);
      g_signal_connect (information->widget_id, "changed",
            G_CALLBACK (contact_widget_id_changed_cb),
            information);
    }
  else
    {
      information->widget_id = gtk_label_new (NULL);
      if (!(information->flags & EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP)) {
        gtk_label_set_selectable (GTK_LABEL (information->widget_id), TRUE);
      }
      gtk_misc_set_alignment (GTK_MISC (information->widget_id), 0, 0.5);
    }
  gtk_table_attach_defaults (GTK_TABLE (information->table_contact),
           information->widget_id,
           1, 2, 1, 2);
  gtk_widget_show (information->widget_id);

  /* Setup alias label/entry */
  if (information->flags & EMPATHY_CONTACT_WIDGET_EDIT_ALIAS)
    {
      information->widget_alias = gtk_entry_new ();
      g_signal_connect (information->widget_alias, "focus-out-event",
            G_CALLBACK (contact_widget_entry_alias_focus_event_cb),
            information);
      /* Make return activate the window default (the Close button) */
      gtk_entry_set_activates_default (GTK_ENTRY (information->widget_alias),
          TRUE);
    }
  else
    {
      information->widget_alias = gtk_label_new (NULL);
      if (!(information->flags & EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP)) {
        gtk_label_set_selectable (GTK_LABEL (information->widget_alias), TRUE);
      }
      gtk_misc_set_alignment (GTK_MISC (information->widget_alias), 0, 0.5);
    }
  gtk_table_attach_defaults (GTK_TABLE (information->table_contact),
           information->widget_alias,
           1, 2, 2, 3);
  if (information->flags & EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP) {
    gtk_label_set_selectable (GTK_LABEL (information->label_status), FALSE);
  }
  gtk_widget_show (information->widget_alias);
}

static void
contact_widget_contact_update (EmpathyContactWidget *information)
{
  McAccount *account = NULL;
  const gchar *id = NULL;

  /* Connect and get info from new contact */
  if (information->contact)
    {
      g_signal_connect_swapped (information->contact, "notify::name",
          G_CALLBACK (contact_widget_name_notify_cb), information);
      g_signal_connect_swapped (information->contact, "notify::presence",
          G_CALLBACK (contact_widget_presence_notify_cb), information);
      g_signal_connect_swapped (information->contact,
          "notify::presence-message",
          G_CALLBACK (contact_widget_presence_notify_cb), information);
      g_signal_connect_swapped (information->contact, "notify::avatar",
          G_CALLBACK (contact_widget_avatar_notify_cb), information);

      account = empathy_contact_get_account (information->contact);
      id = empathy_contact_get_id (information->contact);
    }

  /* Update account widget */
  if (information->flags & EMPATHY_CONTACT_WIDGET_EDIT_ACCOUNT)
    {
      if (account)
        {
          g_signal_handlers_block_by_func (information->widget_account,
                   contact_widget_change_contact,
                   information);
          empathy_account_chooser_set_account (
              EMPATHY_ACCOUNT_CHOOSER (information->widget_account), account);
          g_signal_handlers_unblock_by_func (information->widget_account,
              contact_widget_change_contact, information);
        }
    }
  else
    {
      if (account)
        {
          const gchar *name;

          name = mc_account_get_display_name (account);
          gtk_label_set_label (GTK_LABEL (information->widget_account), name);
        }
    }

  /* Update id widget */
  if (information->flags & EMPATHY_CONTACT_WIDGET_EDIT_ID)
      gtk_entry_set_text (GTK_ENTRY (information->widget_id), id ? id : "");
  else
      gtk_label_set_label (GTK_LABEL (information->widget_id), id ? id : "");

  /* Update other widgets */
  if (information->contact)
    {
      contact_widget_name_notify_cb (information);
      contact_widget_presence_notify_cb (information);
      contact_widget_avatar_notify_cb (information);

      gtk_widget_show (information->label_alias);
      gtk_widget_show (information->widget_alias);
      gtk_widget_show (information->hbox_presence);
      gtk_widget_show (information->widget_avatar);
    }
  else
    {
      gtk_widget_hide (information->label_alias);
      gtk_widget_hide (information->widget_alias);
      gtk_widget_hide (information->hbox_presence);
      gtk_widget_hide (information->widget_avatar);
    }
}

static void
contact_widget_got_contact_cb (EmpathyTpContactFactory *factory,
                               EmpathyContact *contact,
                               const GError *error,
                               gpointer user_data,
                               GObject *weak_object)
{
  EmpathyContactWidget *information = user_data;

  if (error != NULL)
    {
      DEBUG ("Error: %s", error->message);
      return;
    }

  contact_widget_set_contact (information, contact);
}

static void
contact_widget_change_contact (EmpathyContactWidget *information)
{
  EmpathyTpContactFactory *factory;
  TpConnection *connection;

  connection = empathy_account_chooser_get_connection (
      EMPATHY_ACCOUNT_CHOOSER (information->widget_account));
  if (!connection)
      return;

  factory = empathy_tp_contact_factory_dup_singleton (connection);
  if (information->flags & EMPATHY_CONTACT_WIDGET_EDIT_ID)
    {
      const gchar *id;

      id = gtk_entry_get_text (GTK_ENTRY (information->widget_id));
      if (!EMP_STR_EMPTY (id))
        {
          empathy_tp_contact_factory_get_from_id (factory, id,
              contact_widget_got_contact_cb, information, NULL,
              G_OBJECT (information->vbox_contact_widget));
        }
    }
  else
    {
      empathy_tp_contact_factory_get_from_handle (factory,
          tp_connection_get_self_handle (connection),
          contact_widget_got_contact_cb, information, NULL,
          G_OBJECT (information->vbox_contact_widget));
    }

  g_object_unref (factory);
}

static void
contact_widget_avatar_changed_cb (EmpathyAvatarChooser *chooser,
                                  EmpathyContactWidget *information)
{
  const gchar *data;
  gsize size;
  const gchar *mime_type;

  empathy_avatar_chooser_get_image_data (
      EMPATHY_AVATAR_CHOOSER (information->widget_avatar),
      &data, &size, &mime_type);
  empathy_tp_contact_factory_set_avatar (information->factory,
      data, size, mime_type);
}

static gboolean
contact_widget_id_focus_out_cb (GtkWidget *widget,
                                GdkEventFocus *event,
                                EmpathyContactWidget *information)
{
  contact_widget_change_contact (information);
  return FALSE;
}

static gboolean
contact_widget_entry_alias_focus_event_cb (GtkEditable *editable,
                                           GdkEventFocus *event,
                                           EmpathyContactWidget *information)
{
  if (information->contact)
    {
      const gchar *alias;

      alias = gtk_entry_get_text (GTK_ENTRY (editable));
      empathy_tp_contact_factory_set_alias (information->factory,
          information->contact, alias);
    }

  return FALSE;
}

static void
contact_widget_name_notify_cb (EmpathyContactWidget *information)
{
  if (GTK_IS_ENTRY (information->widget_alias))
      gtk_entry_set_text (GTK_ENTRY (information->widget_alias),
          empathy_contact_get_name (information->contact));
  else
      gtk_label_set_label (GTK_LABEL (information->widget_alias),
          empathy_contact_get_name (information->contact));
}

static void
contact_widget_presence_notify_cb (EmpathyContactWidget *information)
{
  gtk_label_set_text (GTK_LABEL (information->label_status),
      empathy_contact_get_status (information->contact));
  gtk_image_set_from_icon_name (GTK_IMAGE (information->image_state),
      empathy_icon_name_for_contact (information->contact),
      GTK_ICON_SIZE_BUTTON);
}

static void
contact_widget_avatar_notify_cb (EmpathyContactWidget *information)
{
  EmpathyAvatar *avatar = NULL;

  if (information->contact)
      avatar = empathy_contact_get_avatar (information->contact);

  if (information->flags & EMPATHY_CONTACT_WIDGET_EDIT_AVATAR)
    {
      g_signal_handlers_block_by_func (information->widget_avatar,
          contact_widget_avatar_changed_cb,
          information);
      empathy_avatar_chooser_set (
          EMPATHY_AVATAR_CHOOSER (information->widget_avatar), avatar);
      g_signal_handlers_unblock_by_func (information->widget_avatar,
          contact_widget_avatar_changed_cb, information);
    }
  else
      empathy_avatar_image_set (
          EMPATHY_AVATAR_IMAGE (information->widget_avatar), avatar);
}

static void
contact_widget_groups_setup (EmpathyContactWidget *information)
{
  if (information->flags & EMPATHY_CONTACT_WIDGET_EDIT_GROUPS)
    {
      information->manager = empathy_contact_manager_dup_singleton ();
      contact_widget_model_setup (information);
    }
}

static void
contact_widget_groups_update (EmpathyContactWidget *information)
{
  if (information->flags & EMPATHY_CONTACT_WIDGET_EDIT_GROUPS &&
      information->contact)
    {
      g_signal_connect_swapped (information->contact, "notify::groups",
          G_CALLBACK (contact_widget_groups_notify_cb), information);
      contact_widget_groups_populate_data (information);

      gtk_widget_show (information->vbox_groups);
    }
  else
      gtk_widget_hide (information->vbox_groups);
}

static void
contact_widget_model_setup (EmpathyContactWidget *information)
{
  GtkTreeView *view;
  GtkListStore *store;
  GtkTreeSelection *selection;

  view = GTK_TREE_VIEW (information->treeview_groups);

  store = gtk_list_store_new (COL_COUNT,
      G_TYPE_STRING,   /* name */
      G_TYPE_BOOLEAN,  /* enabled */
      G_TYPE_BOOLEAN); /* editable */

  gtk_tree_view_set_model (view, GTK_TREE_MODEL (store));

  selection = gtk_tree_view_get_selection (view);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  contact_widget_model_populate_columns (information);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
      COL_NAME, GTK_SORT_ASCENDING);

  g_object_unref (store);
}

static void
contact_widget_model_populate_columns (EmpathyContactWidget *information)
{
  GtkTreeView *view;
  GtkTreeModel *model;
  GtkTreeViewColumn *column;
  GtkCellRenderer  *renderer;
  guint col_offset;

  view = GTK_TREE_VIEW (information->treeview_groups);
  model = gtk_tree_view_get_model (view);

  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (renderer, "toggled",
      G_CALLBACK (contact_widget_cell_toggled), information);

  column = gtk_tree_view_column_new_with_attributes (_("Select"), renderer,
      "active", COL_ENABLED, NULL);

  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_fixed_width (column, 50);
  gtk_tree_view_append_column (view, column);

  renderer = gtk_cell_renderer_text_new ();
  col_offset = gtk_tree_view_insert_column_with_attributes (view,
      -1, _("Group"),
      renderer,
      "text", COL_NAME,
      /* "editable", COL_EDITABLE, */
      NULL);

  g_object_set_data (G_OBJECT (renderer),
      "column", GINT_TO_POINTER (COL_NAME));

  column = gtk_tree_view_get_column (view, col_offset - 1);
  gtk_tree_view_column_set_sort_column_id (column, COL_NAME);
  gtk_tree_view_column_set_resizable (column,FALSE);
  gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);
}

static void
contact_widget_groups_populate_data (EmpathyContactWidget *information)
{
  GtkTreeView *view;
  GtkListStore *store;
  GtkTreeIter iter;
  GList *my_groups, *l;
  GList *all_groups;

  view = GTK_TREE_VIEW (information->treeview_groups);
  store = GTK_LIST_STORE (gtk_tree_view_get_model (view));
  gtk_list_store_clear (store);

  all_groups = empathy_contact_list_get_all_groups (
      EMPATHY_CONTACT_LIST (information->manager));
  my_groups = empathy_contact_list_get_groups (
      EMPATHY_CONTACT_LIST (information->manager),
      information->contact);

  for (l = all_groups; l; l = l->next)
    {
      const gchar *group_str;
      gboolean enabled;

      group_str = l->data;

      enabled = g_list_find_custom (my_groups,
          group_str, (GCompareFunc) strcmp) != NULL;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
          COL_NAME, group_str,
          COL_EDITABLE, TRUE,
          COL_ENABLED, enabled,
          -1);
    }

  g_list_foreach (all_groups, (GFunc) g_free, NULL);
  g_list_foreach (my_groups, (GFunc) g_free, NULL);
  g_list_free (all_groups);
  g_list_free (my_groups);
}

static void
contact_widget_groups_notify_cb (EmpathyContactWidget *information)
{
  /* FIXME: not implemented */
}

static gboolean
contact_widget_model_find_name (EmpathyContactWidget *information,
                                const gchar *name,
                                GtkTreeIter *iter)
{
  GtkTreeView *view;
  GtkTreeModel *model;
  FindName data;

  if (EMP_STR_EMPTY (name))
      return FALSE;

  data.information = information;
  data.name = name;
  data.found = FALSE;

  view = GTK_TREE_VIEW (information->treeview_groups);
  model = gtk_tree_view_get_model (view);

  gtk_tree_model_foreach (model,
      (GtkTreeModelForeachFunc) contact_widget_model_find_name_foreach,
      &data);

  if (data.found == TRUE)
    {
      *iter = data.found_iter;
      return TRUE;
    }

  return FALSE;
}

static gboolean
contact_widget_model_find_name_foreach (GtkTreeModel *model,
                                        GtkTreePath *path,
                                        GtkTreeIter *iter,
                                        FindName *data)
{
  gchar *name;

  gtk_tree_model_get (model, iter,
      COL_NAME, &name,
      -1);

  if (!name)
      return FALSE;

  if (data->name && strcmp (data->name, name) == 0)
    {
      data->found = TRUE;
      data->found_iter = *iter;

      g_free (name);

      return TRUE;
    }

  g_free (name);

  return FALSE;
}

static void
contact_widget_cell_toggled (GtkCellRendererToggle *cell,
                             gchar *path_string,
                             EmpathyContactWidget *information)
{
  GtkTreeView *view;
  GtkTreeModel *model;
  GtkListStore *store;
  GtkTreePath *path;
  GtkTreeIter iter;
  gboolean enabled;
  gchar *group;

  view = GTK_TREE_VIEW (information->treeview_groups);
  model = gtk_tree_view_get_model (view);
  store = GTK_LIST_STORE (model);

  path = gtk_tree_path_new_from_string (path_string);

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter,
      COL_ENABLED, &enabled,
      COL_NAME, &group,
      -1);

  gtk_list_store_set (store, &iter, COL_ENABLED, !enabled, -1);
  gtk_tree_path_free (path);

  if (group)
    {
      if (enabled)
        {
          empathy_contact_list_remove_from_group (
              EMPATHY_CONTACT_LIST (information->manager), information->contact,
              group);
        }
      else
        {
          empathy_contact_list_add_to_group (
              EMPATHY_CONTACT_LIST (information->manager), information->contact,
              group);
        }
      g_free (group);
    }
}

static void
contact_widget_entry_group_changed_cb (GtkEditable *editable,
                                       EmpathyContactWidget *information)
{
  GtkTreeIter iter;
  const gchar *group;

  group = gtk_entry_get_text (GTK_ENTRY (information->entry_group));

  if (contact_widget_model_find_name (information, group, &iter))
      gtk_widget_set_sensitive (GTK_WIDGET (information->button_group), FALSE);
  else
      gtk_widget_set_sensitive (GTK_WIDGET (information->button_group),
          !EMP_STR_EMPTY (group));
}

static void
contact_widget_entry_group_activate_cb (GtkEntry *entry,
                                        EmpathyContactWidget  *information)
{
  gtk_widget_activate (GTK_WIDGET (information->button_group));
}

static void
contact_widget_button_group_clicked_cb (GtkButton *button,
                                        EmpathyContactWidget *information)
{
  GtkTreeView *view;
  GtkListStore *store;
  GtkTreeIter iter;
  const gchar *group;

  view = GTK_TREE_VIEW (information->treeview_groups);
  store = GTK_LIST_STORE (gtk_tree_view_get_model (view));

  group = gtk_entry_get_text (GTK_ENTRY (information->entry_group));

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
      COL_NAME, group,
      COL_ENABLED, TRUE,
      -1);

  empathy_contact_list_add_to_group (
      EMPATHY_CONTACT_LIST (information->manager), information->contact,
      group);
}

static void
contact_widget_details_setup (EmpathyContactWidget *information)
{
  /* FIXME: Needs new telepathy spec */
  gtk_widget_hide (information->vbox_details);
}

static void
contact_widget_details_update (EmpathyContactWidget *information)
{
  /* FIXME: Needs new telepathy spec */
}

static void
contact_widget_client_setup (EmpathyContactWidget *information)
{
  /* FIXME: Needs new telepathy spec */
  gtk_widget_hide (information->vbox_client);
}

static void
contact_widget_client_update (EmpathyContactWidget *information)
{
  /* FIXME: Needs new telepathy spec */
}

static void
contact_widget_location_update (EmpathyContactWidget *information)
{
  GHashTable *location;
  GValue *value;
  gdouble lat, lon;

  location = empathy_contact_get_location (information->contact);
  if (location == NULL)
    {
      gtk_widget_hide (information->vbox_location);
      return;
    }

  value = g_hash_table_lookup (location, EMPATHY_LOCATION_LAT);
  if (value == NULL)
    {
      gtk_widget_hide (information->vbox_location);
      return;
    }
  lat = g_value_get_double (value);

  value = g_hash_table_lookup (location, EMPATHY_LOCATION_LON);
  if (value == NULL)
    {
      gtk_widget_hide (information->vbox_location);
      return;
    }
  lon = g_value_get_double (value);

  value = g_hash_table_lookup (location, EMPATHY_LOCATION_TIMESTAMP);
  if (value == NULL)
    gtk_label_set_markup (GTK_LABEL (information->label_location), _("<b>Location</b>"));
  else
    {
      gchar user_date [100];
      gint64 stamp;
      struct tm *ptm;
      time_t time;

      stamp = g_value_get_int64 (value);
      time = stamp;
      ptm = gmtime (&time);

      if (strftime (user_date, 100, _("%B %e, %Y at %R UTC"), ptm) > 0)
        {
          gchar *text;
          text = g_strconcat ( _("<b>Location</b> on "), user_date, NULL);
          gtk_label_set_markup (GTK_LABEL (information->label_location), text);
          g_free (text);
        }
      else
        gtk_label_set_markup (GTK_LABEL (information->label_location), _("<b>Location</b>"));
    }

  if (information->flags & EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP ||
      information->flags & EMPATHY_CONTACT_WIDGET_SHOW_LOCATION)
    {
      GtkWidget *label;
      guint row = 0;
      GHashTableIter iter;
      gpointer key, value;

      if (information->table_location != NULL)
        {
          gtk_widget_destroy (information->table_location);
        }

      information->table_location = gtk_table_new (1, 2, FALSE);
      gtk_box_pack_start (GTK_BOX (information->subvbox_location),
          information->table_location, FALSE, FALSE, 5);

      g_hash_table_iter_init (&iter, location);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          const gchar *skey;
          GValue *gvalue;
          char *svalue = NULL;

          skey = (const gchar *) key;
          gvalue = (GValue*) value;

          label = gtk_label_new (_(skey));
          gtk_table_attach_defaults (GTK_TABLE (information->table_location),
              label, 0, 1, row, row + 1);
          gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
          gtk_widget_show (label);

          if (G_VALUE_TYPE (gvalue) == G_TYPE_DOUBLE)
            {
              gdouble dvalue;
              dvalue = g_value_get_double (gvalue);
              svalue = g_strdup_printf ("%f", dvalue);
            }
          else if (G_VALUE_TYPE (gvalue) == G_TYPE_STRING)
            {
              svalue = g_value_dup_string (gvalue);
            }

          if (svalue != NULL)
            {
              label = gtk_label_new (svalue);
              gtk_table_attach_defaults (GTK_TABLE (information->table_location),
                  label, 1, 2, row, row + 1);
              gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
              gtk_widget_show (label);
            }

          g_free (svalue);
          row++;
        }

      gtk_widget_show (information->table_location);
    }

  if (/* information->flags & EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP || */
      information->flags & EMPATHY_CONTACT_WIDGET_SHOW_LOCATION)
    {
      ClutterActor *marker;
      ChamplainLayer *layer;

      information->map_view = champlain_view_new (CHAMPLAIN_VIEW_MODE_KINETIC);
      information->map_view_embed = champlain_view_embed_new (
          CHAMPLAIN_VIEW (information->map_view));

      gtk_container_add (GTK_CONTAINER (information->viewport_map),
          information->map_view_embed);
      g_object_set (G_OBJECT (information->map_view), "show-license", FALSE,
          NULL);

      layer = champlain_layer_new ();
      champlain_view_add_layer (CHAMPLAIN_VIEW (information->map_view), layer);

      marker = champlain_marker_new_with_label (
          empathy_contact_get_name (information->contact), NULL, NULL, NULL);
      champlain_marker_set_position (CHAMPLAIN_MARKER (marker), lat, lon);
      clutter_container_add (CLUTTER_CONTAINER (layer), marker, NULL);

      champlain_view_center_on (CHAMPLAIN_VIEW(information->map_view), lat, lon);
    }

    gtk_widget_show (information->vbox_location);
}
