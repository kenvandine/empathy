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

typedef enum {
        EMPATHY_IMPORT_SETTING_TYPE_STRING,
        EMPATHY_IMPORT_SETTING_TYPE_BOOL,
        EMPATHY_IMPORT_SETTING_TYPE_INT,
} EmpathyImportSettingType;

typedef struct {
        gpointer     value;
        EmpathyImportSettingType  type;
} EmpathyImportSetting;


/* Pidgin to MC map */
const struct {
        gchar *protocol;
        gchar *pidgin_name;
        gchar *mc_name;
} pidgin_mc_map[] = {
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

#define PIDGIN_MC_MAP_ITEMS 27

typedef struct {
        GtkWidget       *window;
        GtkWidget       *label_select;
        GtkWidget       *combo;
} EmpathyImportDialog;

static void     empathy_import_dialog_add_setting               (GHashTable               *settings,
                                                                 gchar                    *key,
                                                                 gpointer                  value,
                                                                 EmpathyImportSettingType  type);
static gboolean empathy_import_dialog_add_account               (gchar                    *protocol_name,
                                                                 GHashTable               *settings);
static void     empathy_import_dialog_pidgin_parse_setting      (gchar                    *protocol,
                                                                 xmlNodePtr                setting,
                                                                 GHashTable               *settings);
static void     empathy_import_dialog_pidgin_import_accounts    ();
static void     empathy_import_dialog_response_cb               (GtkDialog                *dialog_window,
                                                                 gint                      response,
                                                                 EmpathyImportDialog      *dialog);

static EmpathyImportDialog *dialog_p = NULL;

static void
empathy_import_dialog_add_setting (GHashTable              *settings,
                                   gchar                   *key,
                                   gpointer                 value,
                                   EmpathyImportSettingType type)
{
        EmpathyImportSetting *set = g_new0 (EmpathyImportSetting, 1);

        set->value = value;
        set->type = type;

        g_hash_table_insert (settings, key, set);
}

static gboolean
empathy_import_dialog_add_account (gchar      *protocol_name,
                                   GHashTable *settings)
{
        McProfile *profile;
        McAccount *account;

        DEBUG ("Looking up profile with protocol '%s'", protocol_name);
        profile = mc_profile_lookup (protocol_name);

        if (profile != NULL) {
                account = mc_account_create (profile);

                if (account != NULL) {
                        const gchar *unique_name;
                        GHashTableIter iter;
                        gpointer key, value;
                        EmpathyImportSetting *set;

                        unique_name = mc_account_get_unique_name (account);
                        mc_account_set_display_name (account, unique_name);

                        g_hash_table_iter_init (&iter, settings);
                        while (g_hash_table_iter_next (&iter, &key, &value)) {
                                set = (EmpathyImportSetting *) value;
                                switch (((EmpathyImportSetting *) value)->type) {
                                        case EMPATHY_IMPORT_SETTING_TYPE_STRING:
                                                DEBUG ("Setting %s to (string) %s",
                                                        (gchar *) key, (gchar *) set->value);
                                                mc_account_set_param_string (account,
                                                        (gchar *) key, (gchar *) set->value);
                                                break;

                                        case EMPATHY_IMPORT_SETTING_TYPE_BOOL:
                                                DEBUG ("Setting %s to (bool) %i",
                                                        (gchar *) key, (gboolean) set->value);
                                                mc_account_set_param_boolean (account,
                                                        (gchar *) key, (gboolean) set->value);
                                                break;

                                        case EMPATHY_IMPORT_SETTING_TYPE_INT:
                                                DEBUG ("Setting %s to (int) %i",
                                                        (gchar *) key, (gint) set->value);
                                                mc_account_set_param_int (account,
                                                        (gchar *) key, (gint) set->value);
                                                break;
                                }
                        }
                        g_object_unref (account);
                }

                g_object_unref (profile);
                return TRUE;
        }
        return FALSE;
}

static void
empathy_import_dialog_pidgin_parse_setting (gchar *protocol,
                                            xmlNodePtr setting,
                                            GHashTable *settings)
{
        int i;

        if (!xmlHasProp (setting, (xmlChar *) "name"))
                return;

        for (i = 0; i < PIDGIN_MC_MAP_ITEMS; i++) {
                if (strcmp(protocol, pidgin_mc_map[i].protocol) != 0) {
                        continue;
                }

                if (strcmp ((gchar *) xmlGetProp (setting, (xmlChar *) "name"),
                        pidgin_mc_map[i].pidgin_name) == 0) {

                        int arg;
                        gchar *type = NULL;

                        type = (gchar *) xmlGetProp (setting, (xmlChar *) "type");

                        if (strcmp (type, "bool") == 0) {
                                sscanf ((gchar *) xmlNodeGetContent (setting),"%i", &arg);
                                empathy_import_dialog_add_setting (settings, pidgin_mc_map[i].mc_name,
                                        (gpointer) arg,
                                        EMPATHY_IMPORT_SETTING_TYPE_BOOL);
                        } else if (strcmp (type, "int") == 0) {
                                sscanf ((gchar *) xmlNodeGetContent (setting),
                                        "%i", &arg);
                                empathy_import_dialog_add_setting (settings, pidgin_mc_map[i].mc_name,
                                        (gpointer) arg,
                                        EMPATHY_IMPORT_SETTING_TYPE_INT);
                        } else if (strcmp (type, "string") == 0) {
                                empathy_import_dialog_add_setting (settings, pidgin_mc_map[i].mc_name,
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

        filename = g_build_filename (g_get_home_dir (), ".purple", "accounts.xml", NULL);

        doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);

        g_free (filename);

        rootnode = xmlDocGetRootElement (doc);

        if (rootnode == NULL) {
                return;
        }

        node = rootnode->children;

        while (node) {
                if (strcmp ((gchar *) node->name, "account") == 0) {
                        child = node->children;

                        settings = g_hash_table_new (g_str_hash, g_str_equal);

                        while (child) {

                                if (strcmp ((gchar *) child->name, "protocol") == 0) {
                                        protocol = (gchar *) xmlNodeGetContent (child);

                                        if (g_str_has_prefix (protocol, "prpl-")) {
                                                protocol = strchr (protocol, '-') + 1;
                                        }

                                        if (strcmp (protocol, "bonjour") == 0) {
                                                protocol = "salut";
                                        } else if (strcmp (protocol, "novell") == 0) {
                                                protocol = "groupwise";
                                        }

                                        empathy_import_dialog_add_setting (settings, "protocol",
                                                (gpointer) protocol,
                                                EMPATHY_IMPORT_SETTING_TYPE_STRING);

                                } else if (strcmp ((gchar *) child->name, "name") == 0) {
                                        name = (gchar *) xmlNodeGetContent (child);

                                        if (g_strrstr (name, "/") != NULL) {
                                                gchar **name_resource;
                                                name_resource = g_strsplit (name, "/", 2);
                                                username = name_resource[0];
                                        } else {
                                                username = name;
                                        }

                                        if (strstr (name, "@") && strcmp (protocol, "irc") == 0) {
                                                gchar **nick_server;
                                                nick_server = g_strsplit (name, "@", 2);
                                                username = nick_server[0];
                                                empathy_import_dialog_add_setting (settings,
                                                        "server", (gpointer) nick_server[1],
                                                        EMPATHY_IMPORT_SETTING_TYPE_STRING);
                                        }

                                        empathy_import_dialog_add_setting (settings, "account",
                                                (gpointer) username, EMPATHY_IMPORT_SETTING_TYPE_STRING);

                                } else if (strcmp ((gchar *) child->name, "password") == 0) {
                                        empathy_import_dialog_add_setting (settings, "password",
                                                (gpointer) xmlNodeGetContent (child),
                                                EMPATHY_IMPORT_SETTING_TYPE_STRING);

                                } else if (strcmp ((gchar *) child->name, "settings") == 0) {

                                        setting = child->children;

                                        while (setting) {
                                                empathy_import_dialog_pidgin_parse_setting (protocol,
                                                        setting, settings);
                                                setting = setting->next;
                                        }

                                }
                                child = child->next;
                        }

                        if (g_hash_table_size (settings) > 0) {
                                empathy_import_dialog_add_account (protocol, settings);
                        }
                        g_free (username);
                        g_hash_table_unref (settings);

                }
                node = node->next;
        }

        xmlFreeDoc(doc);
        xmlFreeParserCtxt (ctxt);

}

static void
empathy_import_dialog_response_cb (GtkDialog           *dialog_window,
                                   gint                 response,
                                   EmpathyImportDialog *dialog)
{
        gchar *from = NULL;
        if (response == GTK_RESPONSE_OK) {

                from = gtk_combo_box_get_active_text (GTK_COMBO_BOX (dialog->combo));

                if (strcmp (from, "Pidgin") == 0) {
                        empathy_import_dialog_pidgin_import_accounts ();
                }
        }

        gtk_widget_destroy (GTK_WIDGET (dialog_window));
        dialog_p = NULL;
        g_free (dialog);
}

void
empathy_import_dialog_show (GtkWindow *parent)
{
        EmpathyImportDialog *dialog;

        if (dialog_p) {
                gtk_window_present (GTK_WINDOW (dialog_p->window));
                return;
        }

        dialog_p = dialog = g_new0 (EmpathyImportDialog, 1);

        dialog->window = gtk_dialog_new_with_buttons (_("Import accounts"),
                                                      NULL,
                                                      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                      GTK_STOCK_OK, GTK_RESPONSE_OK,
                                                      NULL);

        g_signal_connect (G_OBJECT (dialog->window), "response",
                          G_CALLBACK (empathy_import_dialog_response_cb),
                          dialog);

        dialog->label_select = gtk_label_new (_("Select the program to import accounts from:"));

        dialog->combo = gtk_combo_box_new_text ();

        gtk_combo_box_append_text (GTK_COMBO_BOX (dialog->combo), "Pidgin");
        gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->combo), 0);

        gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (dialog->window)->vbox),
                dialog->label_select);

        gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (dialog->window)->vbox),
                dialog->combo);


        if (parent) {
                gtk_window_set_transient_for (GTK_WINDOW (dialog->window),
                                              parent);
        }

        gtk_widget_show_all (dialog->window);
}
