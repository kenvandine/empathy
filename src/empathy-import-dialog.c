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
#include <glib/gi18n.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <libmissioncontrol/mc-account.h>
#include <telepathy-glib/util.h>

#include "empathy-import-dialog.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

/* Pidgin to MC map */
typedef struct
{
  gchar *protocol;
  gchar *pidgin_name;
  gchar *mc_name;
} PidginMcMapItem;

static PidginMcMapItem pidgin_mc_map[] =
{
  { "msn", "server", "server" },
  { "msn", "port", "port" },

  { "jabber", "connect_server", "server" },
  { "jabber", "port", "port" },
  { "jabber", "require_tls", "require-encryption" },
  { "jabber", "old_ssl", "old-ssl" },

  { "aim", "server", "server" },
  { "aim", "port", "port" },

  { "salut", "first", "first-name" },
  { "salut", "last", "last-name" },
  { "salut", "jid", "jid" },
  { "salut", "email", "email" },

  { "groupwise", "server", "server" },
  { "groupwise", "port", "port" },

  { "icq", "server", "server" },
  { "icq", "port", "port" },

  { "irc", "realname", "fullname" },
  { "irc", "ssl", "use-ssl" },
  { "irc", "port", "port" },

  { "yahoo", "server", "server" },
  { "yahoo", "port", "port" },
  { "yahoo", "xfer_port", "xfer-port" },
  { "yahoo", "ignore_invites", "ignore-invites" },
  { "yahoo", "yahoojp", "yahoojp" },
  { "yahoo", "xferjp_host", "xferjp-host" },
  { "yahoo", "serverjp", "serverjp" },
  { "yahoo", "xfer_host", "xfer-host" },
};

typedef struct
{
  GHashTable *settings;
  gchar *protocol;
} AccountData;

typedef struct
{
  GtkWidget *window;
  GtkWidget *label_select;
  GtkWidget *combo;
} EmpathyImportDialog;

#define PIDGIN_ACCOUNT_TAG_NAME "name"
#define PIDGIN_ACCOUNT_TAG_ACCOUNT "account"
#define PIDGIN_ACCOUNT_TAG_PROTOCOL "protocol"
#define PIDGIN_ACCOUNT_TAG_PASSWORD "password"
#define PIDGIN_ACCOUNT_TAG_SETTINGS "settings"
#define PIDGIN_SETTING_PROP_TYPE "type"
#define PIDGIN_PROTOCOL_BONJOUR "bonjour"
#define PIDGIN_PROTOCOL_NOVELL "novell"

static void
import_dialog_account_data_free (AccountData *data)
{
	g_free (data->protocol);
	g_hash_table_destroy (data->settings);
}

static gboolean
import_dialog_add_account (AccountData *data)
{
  McProfile *profile;
  McAccount *account;
  GHashTableIter iter;
  gpointer key, value;
  gchar *display_name;
  gchar *username;

  DEBUG ("Looking up profile with protocol '%s'", data->protocol);
  profile = mc_profile_lookup (data->protocol);

  if (profile == NULL)
    return FALSE;

  account = mc_account_create (profile);

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
  mc_account_get_param_string (account, "account", &username);
  display_name = g_strdup_printf ("%s (%s)",
    mc_profile_get_display_name (profile), username);
  mc_account_set_display_name (account, display_name);

  g_free (username);
  g_free (display_name);
  g_object_unref (account);
  g_object_unref (profile);

  return TRUE;
}

static void
import_dialog_pidgin_parse_setting (AccountData *data,
                                    xmlNodePtr setting)
{
  PidginMcMapItem *item = NULL;
  gchar *tag_name;
  gchar *type = NULL;
  gchar *content;
  gint i;
  GValue *value = NULL;

  /* We can't do anything if we didn't discovered the protocol yet */
  if (!data->protocol)
    return;

  /* We can't do anything if the setting don't have a name */
  tag_name = (gchar *) xmlGetProp (setting, PIDGIN_ACCOUNT_TAG_NAME);
  if (!tag_name)
    return;

  /* Search for the map corresponding to setting we are parsing */
  for (i = 0; i < G_N_ELEMENTS (pidgin_mc_map); i++)
    {
      if (strcmp (data->protocol, pidgin_mc_map[i].protocol) != 0 &&
          strcmp (tag_name, pidgin_mc_map[i].pidgin_name) == 0)
        {
          item = pidgin_mc_map + i;
          break;
        }
    }
  g_free (tag_name);

  /* If we didn't find the item, there is nothing we can do */
  if (!item)
    return;

  type = (gchar *) xmlGetProp (setting, PIDGIN_SETTING_PROP_TYPE);
  content = (gchar *) xmlNodeGetContent (setting);

  if (strcmp (type, "bool") == 0)
    {
      sscanf (content, "%i", &i);
      value = tp_g_value_slice_new (G_TYPE_BOOLEAN);
      g_value_set_boolean (value, i != 0);
    }
  else if (strcmp (type, "int") == 0)
    {
      sscanf (content, "%i", &i);
      value = tp_g_value_slice_new (G_TYPE_INT);
      g_value_set_int (value, i);
    }
  else if (strcmp (type, "string") == 0)
    {
      value = tp_g_value_slice_new (G_TYPE_STRING);
      g_value_set_string (value, content);
    }

  if (value)
    g_hash_table_insert (data->settings, item->mc_name, value);

  g_free (type);
  g_free (content);
}

static GList *
import_dialog_pidgin_load (void)
{
  xmlNodePtr rootnode, node, child, setting;
  xmlParserCtxtPtr ctxt;
  xmlDocPtr doc;
  gchar *filename;
  GList *accounts = NULL;

  /* Load pidgin accounts xml */
  ctxt = xmlNewParserCtxt ();
  filename = g_build_filename (g_get_home_dir (), ".purple", "accounts.xml",
      NULL);
  doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);
  g_free (filename);

  rootnode = xmlDocGetRootElement (doc);
  if (rootnode == NULL)
    goto OUT;

  for (node = rootnode->children; node; node = node->next)
    {
      AccountData *data;

      /* If it is not an account node, skip */
      if (strcmp ((gchar *) node->name, PIDGIN_ACCOUNT_TAG_ACCOUNT) != 0)
        continue;

      /* Create account data struct */
      data = g_slice_new0 (AccountData);
      data->settings = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
        (GDestroyNotify) tp_g_value_slice_free);

      /* Parse account's child nodes to fill the account data struct */
      for (child = node->children; child; child = child->next)
        {
          GValue *value;

          /* Protocol */
          if (strcmp ((gchar *) child->name,
              PIDGIN_ACCOUNT_TAG_PROTOCOL) == 0)
            {
              const gchar *protocol;
              gchar *content;

              protocol = content = (gchar *) xmlNodeGetContent (child);

              if (g_str_has_prefix (protocol, "prpl-"))
                protocol += 5;

              if (strcmp (protocol, PIDGIN_PROTOCOL_BONJOUR) == 0)
                protocol = "salut";
              else if (strcmp (protocol, PIDGIN_PROTOCOL_NOVELL) == 0)
                protocol = "groupwise";

              data->protocol = g_strdup (protocol);
              g_free (content);
            }
          /* Username and IRC server */
          else if (strcmp ((gchar *) child->name,
              PIDGIN_ACCOUNT_TAG_NAME) == 0)
            {
              gchar *name;
              GStrv name_resource = NULL;
              GStrv nick_server = NULL;
              const gchar *username;

              name = (gchar *) xmlNodeGetContent (child);

              /* Split "username/resource" */
              if (g_strrstr (name, "/") != NULL)
                {
                  name_resource = g_strsplit (name, "/", 2);
                  username = name_resource[0];
                }
              else
                username = name;

             /* Split "username@server" if it is an IRC account */
             if (data->protocol && strstr (name, "@") &&
                 strcmp (data->protocol, "irc") == 0)
              {
                nick_server = g_strsplit (name, "@", 2);
                username = nick_server[0];

                /* Add the server setting */
                value = tp_g_value_slice_new (G_TYPE_STRING);
                g_value_set_string (value, nick_server[1]);
                g_hash_table_insert (data->settings, "server", value);
              }

              /* Add the account setting */
              value = tp_g_value_slice_new (G_TYPE_STRING);
              g_value_set_string (value, username);
              g_hash_table_insert (data->settings, "account", value);

              g_strfreev (name_resource);
              g_strfreev (nick_server);
              g_free (name);
            }
          /* Password */
          else if (strcmp ((gchar *) child->name,
              PIDGIN_ACCOUNT_TAG_PASSWORD) == 0)
            {
              gchar *password;

              password = (gchar *) xmlNodeGetContent (child);

              /* Add the password setting */
              value = tp_g_value_slice_new (G_TYPE_STRING);
              g_value_set_string (value, password);
              g_hash_table_insert (data->settings, "password", value);

              g_free (password);
            }
          /* Other settings */
          else if (strcmp ((gchar *) child->name,
              PIDGIN_ACCOUNT_TAG_SETTINGS) == 0)
              for (setting = child->children; setting; setting = setting->next)
                import_dialog_pidgin_parse_setting (data, setting);
        }

      /* If we have the needed settings, add the account data to the list,
       * otherwise free the data */
      if (data->protocol && g_hash_table_size (data->settings) > 0)
        accounts = g_list_prepend (accounts, data);
      else
        import_dialog_account_data_free (data);
    }

OUT:
  xmlFreeDoc(doc);
  xmlFreeParserCtxt (ctxt);

  return accounts;
}

static void
import_dialog_response_cb (GtkDialog *dialog_window,
                           gint response,
                           EmpathyImportDialog *dialog)
{
  gchar *from = NULL;
  if (response == GTK_RESPONSE_OK)
    {
      from = gtk_combo_box_get_active_text (GTK_COMBO_BOX (dialog->combo));

      if (strcmp (from, "Pidgin") == 0)
        import_dialog_pidgin_import_accounts ();

      g_free (from);
    }

  gtk_widget_hide (GTK_WIDGET (dialog_window));
}

void
empathy_import_dialog_show (GtkWindow *parent)
{
  static EmpathyImportDialog *dialog = NULL;

  if (dialog)
    {
      gtk_window_present (GTK_WINDOW (dialog->window));
      return;
    }

  dialog = g_slice_new0 (EmpathyImportDialog);

  dialog->window = gtk_dialog_new_with_buttons (_("Import accounts"),
      NULL,
      GTK_DIALOG_MODAL,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      GTK_STOCK_OK, GTK_RESPONSE_OK,
      NULL);

  g_signal_connect (G_OBJECT (dialog->window), "response",
      G_CALLBACK (import_dialog_response_cb),
      dialog);

  dialog->label_select = gtk_label_new (
      _("Select the program to import accounts from:"));
  gtk_widget_show (dialog->label_select);

  dialog->combo = gtk_combo_box_new_text ();
  gtk_widget_show (dialog->combo);

  gtk_combo_box_append_text (GTK_COMBO_BOX (dialog->combo), "Pidgin");
  gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->combo), 0);

  gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (dialog->window)->vbox),
      dialog->label_select);

  gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (dialog->window)->vbox),
      dialog->combo);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (dialog->window), parent);

  gtk_widget_show (dialog->window);
}
