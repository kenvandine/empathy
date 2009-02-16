/*
 * Copyright (C) 2008 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Jonny Lamb <jonny.lamb@collabora.co.uk>
 * */

#include <config.h>

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libmissioncontrol/mc-account.h>
#include <telepathy-glib/util.h>

#include "empathy-import-dialog.h"
#include "empathy-import-pidgin.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-utils.h>

#include <libempathy-gtk/empathy-ui-utils.h>

typedef struct
{
  GtkWidget *window;
  GtkWidget *treeview;
  GtkWidget *button_ok;
  GtkWidget *button_cancel;
  GList *accounts;
} EmpathyImportDialog;

enum
{
  COL_IMPORT = 0,
  COL_PROTOCOL,
  COL_NAME,
  COL_SOURCE,
  COL_ACCOUNT_DATA,
  COL_COUNT
};

EmpathyImportAccountData *
empathy_import_account_data_new (const gchar *source)
{
  EmpathyImportAccountData *data;

  g_return_val_if_fail (!EMP_STR_EMPTY (source), NULL);

  data = g_slice_new0 (EmpathyImportAccountData);
  data->settings = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
    (GDestroyNotify) tp_g_value_slice_free);
  data->source = g_strdup (source);

  return data;
}

void
empathy_import_account_data_free (EmpathyImportAccountData *data)
{
  if (data == NULL)
    return;
  if (data->profile != NULL)
    g_object_unref (data->profile);
  if (data->settings != NULL)
    g_hash_table_destroy (data->settings);
  if (data->source != NULL)
    g_free (data->source);

  g_slice_free (EmpathyImportAccountData, data);
}

static void
import_dialog_add_account (EmpathyImportAccountData *data)
{
  McAccount *account;
  GHashTableIter iter;
  gpointer key, value;
  gchar *display_name;
  GValue *username;

  account = mc_account_create (data->profile);

  g_hash_table_iter_init (&iter, data->settings);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const gchar *param = key;
      GValue *gvalue = value;

      switch (G_VALUE_TYPE (gvalue))
        {
          case G_TYPE_STRING:
            DEBUG ("Set param '%s' to '%s' (string)",
                param, g_value_get_string (gvalue));
            mc_account_set_param_string (account,
                param, g_value_get_string (gvalue));
            break;

          case G_TYPE_BOOLEAN:
            DEBUG ("Set param '%s' to %s (boolean)",
                param, g_value_get_boolean (gvalue) ? "TRUE" : "FALSE");
            mc_account_set_param_boolean (account,
                param, g_value_get_boolean (gvalue));
            break;

          case G_TYPE_INT:
            DEBUG ("Set param '%s' to '%i' (integer)",
                param, g_value_get_int (gvalue));
            mc_account_set_param_int (account,
                param, g_value_get_int (gvalue));
            break;
        }
    }

  /* Set the display name of the account */
  username = g_hash_table_lookup (data->settings, "account");
  display_name = g_strdup_printf ("%s (%s)",
      mc_profile_get_display_name (data->profile),
      g_value_get_string (username));
  mc_account_set_display_name (account, display_name);

  g_free (display_name);
  g_object_unref (account);
}

static gboolean
import_dialog_account_id_in_list (GList *accounts,
                                  const gchar *account_id)
{
  GList *l;

  for (l = accounts; l; l = l->next)
    {
      McAccount *account = l->data;
      gchar *value;
      gboolean result;

      if (mc_account_get_param_string (account, "account", &value)
          == MC_ACCOUNT_SETTING_ABSENT)
        continue;

      result = tp_strdiff (value, account_id);

      g_free (value);

      if (!result)
        return TRUE;
    }

  return FALSE;
}

static void
import_dialog_add_accounts_to_model (EmpathyImportDialog *dialog)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GList *l;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->treeview));

  for (l = dialog->accounts; l; l = l->next)
    {
      GValue *value;
      EmpathyImportAccountData *data = l->data;
      gboolean import;
      GList *accounts;

      value = g_hash_table_lookup (data->settings, "account");

      accounts = mc_accounts_list_by_profile (data->profile);

      /* Only set the "Import" cell to be active if there isn't already an
       * account set up with the same account id. */
      import = !import_dialog_account_id_in_list (accounts,
          g_value_get_string (value));

      mc_accounts_list_free (accounts);

      gtk_list_store_append (GTK_LIST_STORE (model), &iter);

      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
          COL_IMPORT, import,
          COL_PROTOCOL, mc_profile_get_display_name (data->profile),
          COL_NAME, g_value_get_string (value),
          COL_SOURCE, data->source,
          COL_ACCOUNT_DATA, data,
          -1);
    }
}

static void
import_dialog_cell_toggled_cb (GtkCellRendererToggle *cell_renderer,
                               const gchar *path_str,
                               EmpathyImportDialog *dialog)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;

  path = gtk_tree_path_new_from_string (path_str);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->treeview));

  gtk_tree_model_get_iter (model, &iter, path);

  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
      COL_IMPORT, !gtk_cell_renderer_toggle_get_active (cell_renderer),
      -1);

  gtk_tree_path_free (path);
}

static void
import_dialog_set_up_account_list (EmpathyImportDialog *dialog)
{
  GtkListStore *store;
  GtkTreeView *view;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;

  store = gtk_list_store_new (COL_COUNT, G_TYPE_BOOLEAN, G_TYPE_STRING,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

  gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->treeview),
      GTK_TREE_MODEL (store));

  g_object_unref (store);

  view = GTK_TREE_VIEW (dialog->treeview);
  gtk_tree_view_set_headers_visible (view, TRUE);

  /* Import column */
  cell = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_insert_column_with_attributes (view, -1,
      /* Translators: this is the header of a treeview column */
      _("Import"), cell,
      "active", COL_IMPORT,
      NULL);

  g_signal_connect (cell, "toggled",
      G_CALLBACK (import_dialog_cell_toggled_cb), dialog);

  /* Protocol column */
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Protocol"));
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (view, column);

  cell = gtk_cell_renderer_text_new ();
  g_object_set (cell,
      "editable", FALSE,
      NULL);
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_add_attribute (column, cell, "text", COL_PROTOCOL);

  /* Account column */
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Account"));
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (view, column);

  cell = gtk_cell_renderer_text_new ();
  g_object_set (cell,
      "editable", FALSE,
      NULL);
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_add_attribute (column, cell, "text", COL_NAME);

  /* Source column */
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Source"));
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_append_column (view, column);

  cell = gtk_cell_renderer_text_new ();
  g_object_set (cell,
      "editable", FALSE,
      NULL);
  gtk_tree_view_column_pack_start (column, cell, TRUE);
  gtk_tree_view_column_add_attribute (column, cell, "text", COL_SOURCE);

  import_dialog_add_accounts_to_model (dialog);
}

static gboolean
import_dialog_tree_model_foreach (GtkTreeModel *model,
                                  GtkTreePath *path,
                                  GtkTreeIter *iter,
                                  gpointer user_data)
{
  gboolean to_import;
  EmpathyImportAccountData *data;

  gtk_tree_model_get (model, iter,
      COL_IMPORT, &to_import,
      COL_ACCOUNT_DATA, &data,
      -1);

  if (to_import)
    import_dialog_add_account (data);

  return FALSE;
}

static void
import_dialog_response_cb (GtkWidget *widget,
                           gint response,
                           EmpathyImportDialog *dialog)
{
  if (response == GTK_RESPONSE_OK)
    {
      GtkTreeModel *model;

      model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->treeview));
      gtk_tree_model_foreach (model, import_dialog_tree_model_foreach, dialog);
    }

  gtk_widget_destroy (dialog->window);
}

static void
import_dialog_destroy_cb (GtkWidget *widget,
                          EmpathyImportDialog *dialog)
{
  g_list_foreach (dialog->accounts, (GFunc) empathy_import_account_data_free,
    NULL);
  g_list_free (dialog->accounts);
  g_slice_free (EmpathyImportDialog, dialog);
}

void
empathy_import_dialog_show (GtkWindow *parent,
                            gboolean warning)
{
  static EmpathyImportDialog *dialog = NULL;
  GladeXML *glade;
  gchar *filename;
  GList *accounts = NULL;

  /* This window is a singleton. If it already exist, present it */
  if (dialog)
    {
      gtk_window_present (GTK_WINDOW (dialog->window));
      return;
    }

  /* Load all accounts from all supported applications */
  accounts = g_list_concat (accounts, empathy_import_pidgin_load ());

  /* Check if we have accounts to import before creating the window */
  if (!accounts)
    {
      GtkWidget *message;

      if (warning)
        {
          message = gtk_message_dialog_new (parent,
              GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
              _("No accounts to import could be found. Empathy currently only "
                "supports importing accounts from Pidgin."));

          gtk_dialog_run (GTK_DIALOG (message));
          gtk_widget_destroy (message);
        }
      else
        DEBUG ("No accounts to import; closing dialog silently.");

      return;
    }
  
  /* We have accounts, let's display the window with them */
  dialog = g_slice_new0 (EmpathyImportDialog);
  dialog->accounts = accounts;  

  filename = empathy_file_lookup ("empathy-import-dialog.glade", "src");
  glade = empathy_glade_get_file (filename,
      "import_dialog",
      NULL,
      "import_dialog", &dialog->window,
      "treeview", &dialog->treeview,
      NULL);

  empathy_glade_connect (glade,
      dialog,
      "import_dialog", "destroy", import_dialog_destroy_cb,
      "import_dialog", "response", import_dialog_response_cb,
      NULL);

  g_object_add_weak_pointer (G_OBJECT (dialog->window), (gpointer) &dialog);

  g_free (filename);
  g_object_unref (glade);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (dialog->window), parent);

  import_dialog_set_up_account_list (dialog);

  gtk_widget_show (dialog->window);
}

