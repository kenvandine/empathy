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

#include "empathy-import-dialog.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

typedef enum
{
  EMPATHY_IMPORT_SETTING_TYPE_STRING,
  EMPATHY_IMPORT_SETTING_TYPE_BOOL,
  EMPATHY_IMPORT_SETTING_TYPE_INT,
} EmpathyImportSettingType;

typedef struct
{
  gpointer     value;
  EmpathyImportSettingType  type;
} EmpathyImportSetting;


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

static void empathy_import_dialog_add_setting (GHashTable *settings,
    gchar *key, gpointer value, EmpathyImportSettingType  type);
static gboolean empathy_import_dialog_add_account (gchar *protocol_name,
    GHashTable *settings);
static void empathy_import_dialog_pidgin_parse_setting (gchar *protocol,
    xmlNodePtr setting, GHashTable *settings);
static void empathy_import_dialog_pidgin_import_accounts ();
static void empathy_import_dialog_response_cb (GtkDialog *dialog_window,
    gint response, EmpathyImportDialog *dialog);


static void
empathy_import_dialog_add_setting (GHashTable *settings,
                                   gchar *key,
                                   gpointer value,
                                   EmpathyImportSettingType type)
{
  EmpathyImportSetting *set = g_slice_new0 (EmpathyImportSetting);

  set->value = value;
  set->type = type;

  g_hash_table_insert (settings, key, set);
}

static gboolean
empathy_import_dialog_add_account (gchar *protocol_name,
                                   GHashTable *settings)
{
  McProfile *profile;
  McAccount *account;
  gchar *key_char;
  GHashTableIter iter;
  gpointer key, value;
  EmpathyImportSetting *set;
  gchar *display_name;
  gchar *username;

  DEBUG ("Looking up profile with protocol '%s'", protocol_name);
  profile = mc_profile_lookup (protocol_name);

  if (profile == NULL)
    return FALSE;

  account = mc_account_create (profile);

  g_hash_table_iter_init (&iter, settings);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      set = (EmpathyImportSetting *) value;
      key_char = (gchar *) key;
      switch (((EmpathyImportSetting *) value)->type)
        {
          case EMPATHY_IMPORT_SETTING_TYPE_STRING:
            DEBUG ("Setting %s to (string) %s",
                key_char, (gchar *) set->value);
            mc_account_set_param_string (account,
                key_char, (gchar *) set->value);
            break;

          case EMPATHY_IMPORT_SETTING_TYPE_BOOL:
            DEBUG ("Setting %s to (bool) %i",
                key_char, (gboolean) set->value);
            mc_account_set_param_boolean (account,
                key_char, (gboolean) set->value);
            break;

          case EMPATHY_IMPORT_SETTING_TYPE_INT:
            DEBUG ("Setting %s to (int) %i",
                key_char, (gint) set->value);
            mc_account_set_param_int (account,
                key_char, (gint) set->value);
            break;
        }
    }

  mc_account_get_param_string (account, "account", &username);
  display_name = g_strdup_printf ("%s (%s)", username,
      mc_profile_get_display_name (profile));
  mc_account_set_display_name (account, display_name);

  g_free (username);
  g_free (display_name);
  g_object_unref (account);
  g_object_unref (profile);
  return TRUE;
}

static void
empathy_import_dialog_pidgin_parse_setting (gchar *protocol,
                                            xmlNodePtr setting,
                                            GHashTable *settings)
{
  int i;

  if (!xmlHasProp (setting, PIDGIN_ACCOUNT_TAG_NAME))
    return;

  for (i = 0; i < G_N_ELEMENTS (pidgin_mc_map); i++)
    {
      if (strcmp(protocol, pidgin_mc_map[i].protocol) != 0)
        continue;

      if (strcmp ((gchar *) xmlGetProp (setting, PIDGIN_ACCOUNT_TAG_NAME),
        pidgin_mc_map[i].pidgin_name) == 0)
        {
          gint arg;
          gchar *type = NULL;

          type = (gchar *) xmlGetProp (setting, PIDGIN_SETTING_PROP_TYPE);

          if (strcmp (type, "bool") == 0)
            {
              sscanf ((gchar *) xmlNodeGetContent (setting),"%i", &arg);
              empathy_import_dialog_add_setting (settings,
                  pidgin_mc_map[i].mc_name,
                  (gpointer) arg,
                  EMPATHY_IMPORT_SETTING_TYPE_BOOL);
            }
          else if (strcmp (type, "int") == 0)
            {
              sscanf ((gchar *) xmlNodeGetContent (setting),
                  "%i", &arg);
              empathy_import_dialog_add_setting (settings,
                  pidgin_mc_map[i].mc_name,
                  (gpointer) arg,
                  EMPATHY_IMPORT_SETTING_TYPE_INT);
            }
          else if (strcmp (type, "string") == 0)
            {
              empathy_import_dialog_add_setting (settings,
                  pidgin_mc_map[i].mc_name,
                  (gpointer) xmlNodeGetContent (setting),
                  EMPATHY_IMPORT_SETTING_TYPE_STRING);
            }
        }
    }
}

static void
empathy_import_dialog_pidgin_import_accounts ()
{
  xmlNodePtr rootnode, node, child, setting;
  xmlParserCtxtPtr ctxt;
  xmlDocPtr doc;
  gchar *filename;
  gchar *protocol = NULL;
  gchar *name = NULL;
  gchar *username = NULL;
  GHashTable *settings;

  ctxt = xmlNewParserCtxt ();
  filename = g_build_filename (g_get_home_dir (), ".purple", "accounts.xml",
      NULL);
  doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);
  g_free (filename);

  rootnode = xmlDocGetRootElement (doc);
  if (rootnode == NULL)
    return;

  node = rootnode->children;
  while (node)
    {
      if (strcmp ((gchar *) node->name, PIDGIN_ACCOUNT_TAG_ACCOUNT) == 0)
        {
          child = node->children;

          settings = g_hash_table_new (g_str_hash, g_str_equal);

          while (child)
            {

              if (strcmp ((gchar *) child->name,
                  PIDGIN_ACCOUNT_TAG_PROTOCOL) == 0)
                {
                  protocol = (gchar *) xmlNodeGetContent (child);

                  if (g_str_has_prefix (protocol, "prpl-"))
                    protocol = strchr (protocol, '-') + 1;

                  if (strcmp (protocol, PIDGIN_PROTOCOL_BONJOUR) == 0)
                    protocol = "salut";
                  else if (strcmp (protocol, PIDGIN_PROTOCOL_NOVELL) == 0)
                    protocol = "groupwise";

                  empathy_import_dialog_add_setting (settings, "protocol",
                      (gpointer) protocol,
                      EMPATHY_IMPORT_SETTING_TYPE_STRING);

                }
              else if (strcmp ((gchar *) child->name,
                  PIDGIN_ACCOUNT_TAG_NAME) == 0)
                {
                  name = (gchar *) xmlNodeGetContent (child);

                  if (g_strrstr (name, "/") != NULL)
                    {
                      gchar **name_resource;
                      name_resource = g_strsplit (name, "/", 2);
                      username = g_strdup(name_resource[0]);
                      g_free (name_resource);
                    }
                  else
                    username = name;

                 if (strstr (name, "@") && strcmp (protocol, "irc") == 0)
                  {
                    gchar **nick_server;
                    nick_server = g_strsplit (name, "@", 2);
                    username = nick_server[0];
                    empathy_import_dialog_add_setting (settings,
                        "server", (gpointer) nick_server[1],
                        EMPATHY_IMPORT_SETTING_TYPE_STRING);
                  }

                  empathy_import_dialog_add_setting (settings, "account",
                      (gpointer) username, EMPATHY_IMPORT_SETTING_TYPE_STRING);

                }
              else if (strcmp ((gchar *) child->name,
                  PIDGIN_ACCOUNT_TAG_PASSWORD) == 0)
                {
                  empathy_import_dialog_add_setting (settings, "password",
                      (gpointer) xmlNodeGetContent (child),
                      EMPATHY_IMPORT_SETTING_TYPE_STRING);

                }
              else if (strcmp ((gchar *) child->name,
                  PIDGIN_ACCOUNT_TAG_SETTINGS) == 0)
                {
                  setting = child->children;

                  while (setting)
                    {
                      empathy_import_dialog_pidgin_parse_setting (protocol,
                          setting, settings);
                          setting = setting->next;
                    }

                }
              child = child->next;
            }

          if (g_hash_table_size (settings) > 0)
              empathy_import_dialog_add_account (protocol, settings);

          g_free (username);
          g_hash_table_unref (settings);
        }

      node = node->next;
    }

  xmlFreeDoc(doc);
  xmlFreeParserCtxt (ctxt);
}

static void
empathy_import_dialog_response_cb (GtkDialog *dialog_window,
                                   gint response,
                                   EmpathyImportDialog *dialog)
{
  gchar *from = NULL;
  if (response == GTK_RESPONSE_OK)
    {
      from = gtk_combo_box_get_active_text (GTK_COMBO_BOX (dialog->combo));

      if (strcmp (from, "Pidgin") == 0)
        empathy_import_dialog_pidgin_import_accounts ();

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
      G_CALLBACK (empathy_import_dialog_response_cb),
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
