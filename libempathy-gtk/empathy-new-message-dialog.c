/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007-2008 Collabora Ltd.
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

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n-lib.h>

#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-call-factory.h>
#include <libempathy/empathy-contact-factory.h>
#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-dispatcher.h>
#include <libempathy/empathy-utils.h>

#define DEBUG_FLAG EMPATHY_DEBUG_CONTACT
#include <libempathy/empathy-debug.h>

#include <libempathy-gtk/empathy-ui-utils.h>

#include "empathy-new-message-dialog.h"
#include "empathy-account-chooser.h"

typedef struct {
	GtkWidget *dialog;
	GtkWidget *table_contact;
	GtkWidget *account_chooser;
	GtkWidget *entry_id;
	GtkWidget *button_chat;
	GtkWidget *button_call;
	EmpathyContactManager *contact_manager;
} EmpathyNewMessageDialog;

enum {
	COMPLETION_COL_TEXT,
	COMPLETION_COL_ID,
	COMPLETION_COL_NAME,
} CompletionCol;

static void
new_message_dialog_account_changed_cb (GtkWidget               *widget,
				       EmpathyNewMessageDialog *dialog)
{
	EmpathyAccountChooser *chooser;
	McAccount            *account;
	EmpathyTpContactList *contact_list;
	GList                *members, *l;
	GtkListStore         *store;
	GtkEntryCompletion   *completion;
	GtkTreeIter           iter;
	gchar                *tmpstr;

	chooser = EMPATHY_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = empathy_account_chooser_get_account (chooser);
	contact_list = empathy_contact_manager_get_list (dialog->contact_manager,
							 account);
	members = empathy_contact_list_get_members (EMPATHY_CONTACT_LIST (contact_list));
	completion = gtk_entry_get_completion (GTK_ENTRY (dialog->entry_id));
	store = GTK_LIST_STORE (gtk_entry_completion_get_model (completion));
	gtk_list_store_clear (store);

	for (l = members; l; l = l->next) {
		EmpathyContact *contact = l->data;

		if (!empathy_contact_is_online (contact)) {
			continue;
		}

		DEBUG ("Adding contact ID %s, Name %s",
		       empathy_contact_get_id (contact),
		       empathy_contact_get_name (contact));

		tmpstr = g_strdup_printf ("%s (%s)",
					  empathy_contact_get_name (contact),
					  empathy_contact_get_id (contact));

		gtk_list_store_insert_with_values (store, &iter, -1,
			COMPLETION_COL_TEXT, tmpstr,
			COMPLETION_COL_ID, empathy_contact_get_id (contact),
			COMPLETION_COL_NAME, empathy_contact_get_name (contact),
			-1);

		g_free (tmpstr);
	}

	g_object_unref (account);
}

static gboolean
new_message_dialog_match_selected_cb (GtkEntryCompletion *widget,
				      GtkTreeModel       *model,
				      GtkTreeIter        *iter,
				      EmpathyNewMessageDialog *dialog)
{
	gchar *id;

	if (!iter || !model) {
		return FALSE;
	}

	gtk_tree_model_get (model, iter, COMPLETION_COL_ID, &id, -1);
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_id), id);

	DEBUG ("Got selected match **%s**", id);

	g_free (id);

	return TRUE;
}

static gboolean
new_message_dialog_match_func (GtkEntryCompletion *completion,
			       const gchar        *key,
			       GtkTreeIter        *iter,
			       gpointer            user_data)
{
	GtkTreeModel *model;
	gchar        *id;
	gchar        *name;

	model = gtk_entry_completion_get_model (completion);
	if (!model || !iter) {
		return FALSE;
	}

	gtk_tree_model_get (model, iter, COMPLETION_COL_NAME, &name, -1);
	if (strstr (name, key)) {
		DEBUG ("Key %s is matching name **%s**", key, name);
		g_free (name);
		return TRUE;
	}
	g_free (name);

	gtk_tree_model_get (model, iter, COMPLETION_COL_ID, &id, -1);
	if (strstr (id, key)) {
		DEBUG ("Key %s is matching ID **%s**", key, id);
		g_free (id);
		return TRUE;
	}
	g_free (id);

	return FALSE;
}

static void
new_message_dialog_response_cb (GtkWidget               *widget,
				gint                    response,
				EmpathyNewMessageDialog *dialog)
{
	McAccount   *account;
	const gchar *id;

	account = empathy_account_chooser_get_account (EMPATHY_ACCOUNT_CHOOSER (dialog->account_chooser));
	id = gtk_entry_get_text (GTK_ENTRY (dialog->entry_id));
	if (!account || EMP_STR_EMPTY (id)) {
		if (account) {
			g_object_unref (account);
		}
		gtk_widget_destroy (widget);
		return;
	}

	if (response == 1) {
		EmpathyContactFactory *factory;
		EmpathyContact *contact;
		EmpathyCallFactory *call_factory;

		factory = empathy_contact_factory_dup_singleton ();
		contact = empathy_contact_factory_get_from_id (factory, account, id);

		call_factory = empathy_call_factory_get();
		empathy_call_factory_new_call (call_factory, contact);

		g_object_unref (contact);
		g_object_unref (factory);
	} else if (response == 2) {
		empathy_dispatcher_chat_with_contact_id (account, id, NULL, NULL);
	}

	g_object_unref (account);
	gtk_widget_destroy (widget);
}

static void
new_message_change_state_button_cb  (GtkEditable             *editable,
				     EmpathyNewMessageDialog *dialog)  
{
	const gchar *id;
	gboolean     sensitive;

	id = gtk_entry_get_text (GTK_ENTRY (editable));
	sensitive = !EMP_STR_EMPTY (id);
	
	gtk_widget_set_sensitive (dialog->button_chat, sensitive);
	gtk_widget_set_sensitive (dialog->button_call, sensitive);
}

static void
new_message_dialog_destroy_cb (GtkWidget               *widget,
			       EmpathyNewMessageDialog *dialog)
{
	g_object_unref (dialog->contact_manager);
	g_free (dialog);
}

GtkWidget *
empathy_new_message_dialog_show (GtkWindow *parent)
{
	static EmpathyNewMessageDialog *dialog = NULL;
	GladeXML                       *glade;
	gchar                          *filename;
	GtkEntryCompletion             *completion;
	GtkListStore                   *model;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->dialog));
		return dialog->dialog;
	}

	dialog = g_new0 (EmpathyNewMessageDialog, 1);

	/* create a contact manager */
	dialog->contact_manager = empathy_contact_manager_dup_singleton ();

	filename = empathy_file_lookup ("empathy-new-message-dialog.glade",
					"libempathy-gtk");
	glade = empathy_glade_get_file (filename,
				        "new_message_dialog",
				        NULL,
				        "new_message_dialog", &dialog->dialog,
				        "table_contact", &dialog->table_contact,
				        "entry_id", &dialog->entry_id,
					"button_chat", &dialog->button_chat,
					"button_call",&dialog->button_call,
				        NULL);
	g_free (filename);

	/* text completion */
	completion = gtk_entry_completion_new ();
	model = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	gtk_entry_completion_set_text_column (completion, COMPLETION_COL_TEXT);
	gtk_entry_completion_set_match_func (completion,
					     new_message_dialog_match_func,
					     NULL, NULL);
	gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (model));
	gtk_entry_set_completion (GTK_ENTRY (dialog->entry_id), completion);
	g_signal_connect (completion, "match-selected",
			  G_CALLBACK (new_message_dialog_match_selected_cb),
			  dialog);
	g_object_unref(completion);
	g_object_unref(model);

	empathy_glade_connect (glade, dialog,
			       "new_message_dialog", "destroy", new_message_dialog_destroy_cb,
			       "new_message_dialog", "response", new_message_dialog_response_cb,
			       "entry_id", "changed", new_message_change_state_button_cb,
			       NULL);

	g_object_add_weak_pointer (G_OBJECT (dialog->dialog), (gpointer) &dialog);

	g_object_unref (glade);

	/* Create account chooser */
	dialog->account_chooser = empathy_account_chooser_new ();
	gtk_table_attach_defaults (GTK_TABLE (dialog->table_contact),
				   dialog->account_chooser,
				   1, 2, 0, 1);
	empathy_account_chooser_set_filter (EMPATHY_ACCOUNT_CHOOSER (dialog->account_chooser),
					    empathy_account_chooser_filter_is_connected,
					    NULL);
	gtk_widget_show (dialog->account_chooser);

	new_message_dialog_account_changed_cb (dialog->account_chooser, dialog);
	g_signal_connect (dialog->account_chooser, "changed", 
			  G_CALLBACK (new_message_dialog_account_changed_cb),
			  dialog);

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog),
					      GTK_WINDOW (parent));
	}

	gtk_widget_set_sensitive (dialog->button_chat, FALSE);
	gtk_widget_set_sensitive (dialog->button_call, FALSE);

	gtk_widget_show (dialog->dialog);

	return dialog->dialog;
}
