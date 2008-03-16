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
#include <glib/gi18n.h>

#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-contact-factory.h>
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-utils.h>

#include <libempathy-gtk/empathy-ui-utils.h>

#include "empathy-new-message-dialog.h"
#include "empathy-account-chooser.h"

#define DEBUG_DOMAIN "NewMessageDialog"

typedef struct {
	GtkWidget *dialog;
	GtkWidget *table_contact;
	GtkWidget *account_chooser;
	GtkWidget *entry_id;
	GtkWidget *button_chat;
	GtkWidget *button_call;
} EmpathyNewMessageDialog;

static void
new_message_dialog_response_cb (GtkWidget               *widget,
				gint                    response,
				EmpathyNewMessageDialog *dialog)
{
	McAccount   *account;
	const gchar *id;

	account = empathy_account_chooser_get_account (EMPATHY_ACCOUNT_CHOOSER (dialog->account_chooser));
	id = gtk_entry_get_text (GTK_ENTRY (dialog->entry_id));
	if (!account || G_STR_EMPTY (id)) {
		if (account) {
			g_object_unref (account);
		}
		gtk_widget_destroy (widget);
		return;
	}

	if (response == 1) {
		empathy_call_with_contact_id (account, id);
	}	
	else if (response == 2) {
		empathy_chat_with_contact_id (account, id);
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
	sensitive = !G_STR_EMPTY (id);
	
	gtk_widget_set_sensitive (dialog->button_chat, sensitive);
	gtk_widget_set_sensitive (dialog->button_call, sensitive);
}

static void
new_message_dialog_destroy_cb (GtkWidget               *widget,
			       EmpathyNewMessageDialog *dialog)
{	
	g_free (dialog);
}

GtkWidget *
empathy_new_message_dialog_show (GtkWindow *parent)
{
	static EmpathyNewMessageDialog *dialog = NULL;
	GladeXML                       *glade;
	gchar                          *filename;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->dialog));
		return dialog->dialog;
	}

	dialog = g_new0 (EmpathyNewMessageDialog, 1);

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

	empathy_glade_connect (glade,
			       dialog,
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

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog),
					      GTK_WINDOW (parent));
	}

	gtk_widget_set_sensitive (dialog->button_chat, FALSE);
	gtk_widget_set_sensitive (dialog->button_call, FALSE);

#ifndef HAVE_VOIP
	gtk_widget_hide (dialog->button_call);
#endif

	gtk_widget_show (dialog->dialog);

	return dialog->dialog;
}

