/*
 * Copyright (C) 2007-2008 Guillaume Desmottes
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
 * Authors: Guillaume Desmottes <gdesmott@gnome.org>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mc-protocol.h>

#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-irc-network-manager.h>

#include "empathy-irc-network-dialog.h"
#include "empathy-account-widget.h"
#include "empathy-account-widget-irc.h"
#include "empathy-ui-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_ACCOUNT | EMPATHY_DEBUG_IRC
#include <libempathy/empathy-debug.h>

#define IRC_NETWORKS_FILENAME "irc-networks.xml"

typedef struct {
  McAccount *account;
  EmpathyIrcNetworkManager *network_manager;

  GtkWidget *vbox_settings;

  GtkWidget *combobox_network;
  GtkWidget *button_add_network;
  GtkWidget *button_network;
  GtkWidget *button_remove;
} EmpathyAccountWidgetIrc;

enum {
  COL_NETWORK_OBJ,
  COL_NETWORK_NAME,
};

static void
account_widget_irc_destroy_cb (GtkWidget *widget,
                               EmpathyAccountWidgetIrc *settings)
{
  g_object_unref (settings->network_manager);
  g_object_unref (settings->account);
  g_slice_free (EmpathyAccountWidgetIrc, settings);
}

static void
unset_server_params (EmpathyAccountWidgetIrc *settings)
{
  DEBUG ("Unset server, port and use-ssl");
  mc_account_unset_param (settings->account, "server");
  mc_account_unset_param (settings->account, "port");
  mc_account_unset_param (settings->account, "use-ssl");
}

static void
update_server_params (EmpathyAccountWidgetIrc *settings)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  EmpathyIrcNetwork *network;
  GSList *servers;
  gchar *charset;

  if (!gtk_combo_box_get_active_iter (
        GTK_COMBO_BOX (settings->combobox_network), &iter))
    {
      unset_server_params (settings);
      return;
    }

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (settings->combobox_network));
  gtk_tree_model_get (model, &iter, COL_NETWORK_OBJ, &network, -1);

  g_assert (network != NULL);

  g_object_get (network, "charset", &charset, NULL);
  DEBUG ("Setting charset to %s", charset);
  mc_account_set_param_string (settings->account, "charset", charset);
  g_free (charset);

  servers = empathy_irc_network_get_servers (network);
  if (g_slist_length (servers) > 0)
    {
      /* set the first server as CM server */
      EmpathyIrcServer *server = servers->data;
      gchar *address;
      guint port;
      gboolean ssl;

      g_object_get (server,
          "address", &address,
          "port", &port,
          "ssl", &ssl,
          NULL);

      DEBUG ("Setting server to %s", address);
      mc_account_set_param_string (settings->account, "server", address);
      DEBUG ("Setting port to %u", port);
      mc_account_set_param_int (settings->account, "port", port);
      DEBUG ("Setting use-ssl to %s", ssl ? "TRUE": "FALSE" );
      mc_account_set_param_boolean (settings->account, "use-ssl", ssl);

      g_free (address);
    }
  else
    {
      /* No server. Unset values */
      unset_server_params (settings);
    }

  g_slist_foreach (servers, (GFunc) g_object_unref, NULL);
  g_slist_free (servers);
  g_object_unref (network);
}

static void
irc_network_dialog_destroy_cb (GtkWidget *widget,
                               EmpathyAccountWidgetIrc *settings)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  EmpathyIrcNetwork *network;
  gchar *name;

  /* name could be changed */
  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (settings->combobox_network),
      &iter);
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (settings->combobox_network));
  gtk_tree_model_get (model, &iter, COL_NETWORK_OBJ, &network, -1);

  g_object_get (network, "name", &name, NULL);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
      COL_NETWORK_NAME, name, -1);

  update_server_params (settings);

  g_object_unref (network);
  g_free (name);
}

static void
display_irc_network_dialog (EmpathyAccountWidgetIrc *settings,
                            EmpathyIrcNetwork *network)
{
  GtkWindow *window;
  GtkWidget *dialog;

  window = empathy_get_toplevel_window (settings->vbox_settings);
  dialog = empathy_irc_network_dialog_show (network, GTK_WIDGET (window));
  g_signal_connect (dialog, "destroy",
      G_CALLBACK (irc_network_dialog_destroy_cb), settings);
}

static void
account_widget_irc_button_edit_network_clicked_cb (
    GtkWidget *button,
    EmpathyAccountWidgetIrc *settings)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  EmpathyIrcNetwork *network;

  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (settings->combobox_network),
      &iter);
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (settings->combobox_network));
  gtk_tree_model_get (model, &iter, COL_NETWORK_OBJ, &network, -1);

  g_assert (network != NULL);

  display_irc_network_dialog (settings, network);

  g_object_unref (network);
}

static void
account_widget_irc_button_remove_clicked_cb (GtkWidget *button,
                                             EmpathyAccountWidgetIrc *settings)
{
  EmpathyIrcNetwork *network;
  GtkTreeIter iter;
  GtkTreeModel *model;
  gchar *name;

  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (settings->combobox_network),
      &iter);
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (settings->combobox_network));
  gtk_tree_model_get (model, &iter, COL_NETWORK_OBJ, &network, -1);

  g_assert (network != NULL);

  g_object_get (network, "name", &name, NULL);
  DEBUG ("Remove network %s", name);

  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
  empathy_irc_network_manager_remove (settings->network_manager, network);

  /* Select the first network */
  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      gtk_combo_box_set_active_iter (
          GTK_COMBO_BOX (settings->combobox_network), &iter);
    }

  g_free (name);
  g_object_unref (network);
}

static void
account_widget_irc_button_add_network_clicked_cb (GtkWidget *button,
                                                  EmpathyAccountWidgetIrc *settings)
{
  EmpathyIrcNetwork *network;
  GtkTreeModel *model;
  GtkListStore *store;
  gchar *name;
  GtkTreeIter iter;

  network = empathy_irc_network_new (_("New Network"));
  empathy_irc_network_manager_add (settings->network_manager, network);

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (settings->combobox_network));
  store = GTK_LIST_STORE (model);

  g_object_get (network, "name", &name, NULL);

  gtk_list_store_insert_with_values (store, &iter, -1,
      COL_NETWORK_OBJ, network,
      COL_NETWORK_NAME, name,
      -1);

  gtk_combo_box_set_active_iter (GTK_COMBO_BOX (settings->combobox_network),
      &iter);

  display_irc_network_dialog (settings, network);

  g_free (name);
  g_object_unref (network);
}

static void
account_widget_irc_combobox_network_changed_cb (GtkWidget *combobox,
                                                EmpathyAccountWidgetIrc *settings)
{
  update_server_params (settings);
}

static void
fill_networks_model (EmpathyAccountWidgetIrc *settings,
                     EmpathyIrcNetwork *network_to_select)
{
  GSList *networks, *l;
  GtkTreeModel *model;
  GtkListStore *store;

  networks = empathy_irc_network_manager_get_networks (
      settings->network_manager);

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (settings->combobox_network));
  store = GTK_LIST_STORE (model);

  for (l = networks; l != NULL; l = g_slist_next (l))
    {
      gchar *name;
      EmpathyIrcNetwork *network = l->data;
      GtkTreeIter iter;

      g_object_get (network, "name", &name, NULL);

      gtk_list_store_insert_with_values (store, &iter, -1,
          COL_NETWORK_OBJ, network,
          COL_NETWORK_NAME, name,
          -1);

       if (network == network_to_select)
         {
           gtk_combo_box_set_active_iter (
               GTK_COMBO_BOX (settings->combobox_network), &iter);
         }

      g_free (name);
      g_object_unref (network);
    }

  if (network_to_select == NULL)
    {
      /* Select the first network */
      GtkTreeIter iter;

      if (gtk_tree_model_get_iter_first (model, &iter))
        {
          gtk_combo_box_set_active_iter (
              GTK_COMBO_BOX (settings->combobox_network), &iter);

          update_server_params (settings);
        }
    }

  g_slist_free (networks);
}

static void
account_widget_irc_setup (EmpathyAccountWidgetIrc *settings)
{
  gchar *nick = NULL;
  gchar *fullname = NULL;
  gchar *server = NULL;
  gint port = 6667;
  gchar *charset;
  gboolean ssl = FALSE;
  EmpathyIrcNetwork *network = NULL;

  mc_account_get_param_string (settings->account, "account", &nick);
  mc_account_get_param_string (settings->account, "fullname", &fullname);
  mc_account_get_param_string (settings->account, "server", &server);
  mc_account_get_param_string (settings->account, "charset", &charset);
  mc_account_get_param_int (settings->account, "port", &port);
  mc_account_get_param_boolean (settings->account, "use-ssl", &ssl);

  if (!nick)
    {
      nick = g_strdup (g_get_user_name ());
      mc_account_set_param_string (settings->account, "account", nick);
    }

  if (!fullname)
    {
      fullname = g_strdup (g_get_real_name ());
      if (!fullname)
        {
          fullname = g_strdup (nick);
        }
      mc_account_set_param_string (settings->account, "fullname", fullname);
    }

  if (server != NULL)
    {
      GtkListStore *store;

      network = empathy_irc_network_manager_find_network_by_address (
          settings->network_manager, server);


      store = GTK_LIST_STORE (gtk_combo_box_get_model (
            GTK_COMBO_BOX (settings->combobox_network)));

      if (network != NULL)
        {
          gchar *name;

          g_object_set (network, "charset", charset, NULL);

          g_object_get (network, "name", &name, NULL);
          DEBUG ("Account use network %s", name);

          g_free (name);
        }
      else
        {
          /* We don't have this network. Let's create it */
          EmpathyIrcServer *srv;
          GtkTreeIter iter;

          DEBUG ("Create a network %s", server);
          network = empathy_irc_network_new (server);
          srv = empathy_irc_server_new (server, port, ssl);

          empathy_irc_network_append_server (network, srv);
          empathy_irc_network_manager_add (settings->network_manager, network);

          gtk_list_store_insert_with_values (store, &iter, -1,
              COL_NETWORK_OBJ, network,
              COL_NETWORK_NAME, server,
              -1);

          gtk_combo_box_set_active_iter (
              GTK_COMBO_BOX (settings->combobox_network), &iter);

          g_object_unref (srv);
          g_object_unref (network);
        }
    }


  fill_networks_model (settings, network);

  g_free (nick);
  g_free (fullname);
  g_free (server);
  g_free (charset);
}

/**
 * empathy_account_widget_irc_new:
 * @account: the #McAccount to configure
 *
 * Creates a new IRC account widget to configure a given #McAccount
 *
 * Returns: The toplevel container of the configuration widget
 */
GtkWidget *
empathy_account_widget_irc_new (McAccount *account)
{
  EmpathyAccountWidgetIrc *settings;
  gchar *dir, *user_file_with_path, *global_file_with_path;
  GladeXML *glade;
  GtkListStore *store;
  GtkCellRenderer *renderer;
  gchar *filename;

  settings = g_slice_new0 (EmpathyAccountWidgetIrc);
  settings->account = g_object_ref (account);

  dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
  g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
  user_file_with_path = g_build_filename (dir, IRC_NETWORKS_FILENAME, NULL);
  g_free (dir);

  global_file_with_path = g_build_filename (g_getenv ("EMPATHY_SRCDIR"),
      "libempathy-gtk", IRC_NETWORKS_FILENAME, NULL);
  if (!g_file_test (global_file_with_path, G_FILE_TEST_EXISTS))
    {
      g_free (global_file_with_path);
      global_file_with_path = g_build_filename (DATADIR, "empathy",
          IRC_NETWORKS_FILENAME, NULL);
    }

  settings->network_manager = empathy_irc_network_manager_new (
      global_file_with_path,
      user_file_with_path);

  g_free (global_file_with_path);
  g_free (user_file_with_path);

  filename = empathy_file_lookup ("empathy-account-widget-irc.glade",
      "libempathy-gtk");
  glade = empathy_glade_get_file (filename,
      "vbox_irc_settings",
      NULL,
      "vbox_irc_settings", &settings->vbox_settings,
      "combobox_network", &settings->combobox_network,
      "button_network", &settings->button_network,
      "button_add_network", &settings->button_add_network,
      "button_remove", &settings->button_remove,
      NULL);
  g_free (filename);

  /* Fill the networks combobox */
  store = gtk_list_store_new (2, G_TYPE_OBJECT, G_TYPE_STRING);

  gtk_cell_layout_clear (GTK_CELL_LAYOUT (settings->combobox_network)); 
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (settings->combobox_network),
      renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (settings->combobox_network),
      renderer,
      "text", COL_NETWORK_NAME,
      NULL);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
      COL_NETWORK_NAME,
      GTK_SORT_ASCENDING);

  gtk_combo_box_set_model (GTK_COMBO_BOX (settings->combobox_network),
      GTK_TREE_MODEL (store));
  g_object_unref (store);

  account_widget_irc_setup (settings);

  empathy_account_widget_handle_params (account, glade,
      "entry_nick", "account",
      "entry_fullname", "fullname",
      "entry_password", "password",
      "entry_quit_message", "quit-message",
      NULL);

  empathy_glade_connect (glade, settings,
      "vbox_irc_settings", "destroy", account_widget_irc_destroy_cb,
      "button_network", "clicked", account_widget_irc_button_edit_network_clicked_cb,
      "button_add_network", "clicked", account_widget_irc_button_add_network_clicked_cb,
      "button_remove", "clicked", account_widget_irc_button_remove_clicked_cb,
      "combobox_network", "changed", account_widget_irc_combobox_network_changed_cb,
      NULL);

  g_object_unref (glade);

  return settings->vbox_settings;
}
