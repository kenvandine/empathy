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
#include <glib/gi18n-lib.h>

#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-account-manager.h>
#include <libempathy/empathy-contact-list.h>
#include <libempathy/empathy-utils.h>

#include "empathy-contact-dialogs.h"
#include "empathy-contact-widget.h"
#include "empathy-ui-utils.h"

static GList *subscription_dialogs = NULL;
static GList *information_dialogs = NULL;
static GList *edit_dialogs = NULL;
static GtkWidget *personal_dialog = NULL;
static GtkWidget *new_contact_dialog = NULL;

static gint
contact_dialogs_find (GtkDialog      *dialog,
		      EmpathyContact *contact)
{
	GtkWidget     *contact_widget;
	EmpathyContact *this_contact;

	contact_widget = g_object_get_data (G_OBJECT (dialog), "contact_widget");
	this_contact = empathy_contact_widget_get_contact (contact_widget);

	return contact != this_contact;
}

/*
 *  Subscription dialog
 */

static void
subscription_dialog_response_cb (GtkDialog *dialog,
				 gint       response,
				 GtkWidget *contact_widget)
{
	EmpathyContactManager *manager;
	EmpathyContact        *contact;

	manager = empathy_contact_manager_dup_singleton ();
	contact = empathy_contact_widget_get_contact (contact_widget);

	if (response == GTK_RESPONSE_YES) {
		empathy_contact_list_add (EMPATHY_CONTACT_LIST (manager),
					  contact, "");
	}
	else if (response == GTK_RESPONSE_NO) {
		empathy_contact_list_remove (EMPATHY_CONTACT_LIST (manager),
					     contact, "");
	}

	subscription_dialogs = g_list_remove (subscription_dialogs, dialog);
	gtk_widget_destroy (GTK_WIDGET (dialog));
	g_object_unref (manager);
}

void
empathy_subscription_dialog_show (EmpathyContact *contact,
				  GtkWindow     *parent)
{
	GtkBuilder *gui;
	GtkWidget *dialog;
	GtkWidget *hbox_subscription;
	GtkWidget *contact_widget;
	GList     *l;
	gchar     *filename;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	l = g_list_find_custom (subscription_dialogs,
				contact,
				(GCompareFunc) contact_dialogs_find);
	if (l) {
		gtk_window_present (GTK_WINDOW (l->data));
		return;
	}

	filename = empathy_file_lookup ("empathy-contact-dialogs.ui",
					"libempathy-gtk");
	gui = empathy_builder_get_file (filename,
				      "subscription_request_dialog", &dialog,
				      "hbox_subscription", &hbox_subscription,
				      NULL);
	g_free (filename);
	g_object_unref (gui);

	/* Contact info widget */
	contact_widget = empathy_contact_widget_new (contact,
						     EMPATHY_CONTACT_WIDGET_EDIT_ALIAS |
						     EMPATHY_CONTACT_WIDGET_EDIT_GROUPS);
	gtk_box_pack_end (GTK_BOX (hbox_subscription),
			  contact_widget,
			  TRUE, TRUE,
			  0);
	gtk_widget_show (contact_widget);

	g_object_set_data (G_OBJECT (dialog), "contact_widget", contact_widget);
	subscription_dialogs = g_list_prepend (subscription_dialogs, dialog);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (subscription_dialog_response_cb),
			  contact_widget);

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
	}

	gtk_widget_show (dialog);
}

/*
 *  Information dialog
 */

static void
contact_dialogs_response_cb (GtkDialog *dialog,
			     gint       response,
			     GList    **dialogs)
{
	*dialogs = g_list_remove (*dialogs, dialog);
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
empathy_contact_information_dialog_show (EmpathyContact *contact,
					 GtkWindow      *parent)
{
	GtkWidget *dialog;
	GtkWidget *button;
	GtkWidget *contact_widget;
	GList     *l;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	l = g_list_find_custom (information_dialogs,
				contact,
				(GCompareFunc) contact_dialogs_find);
	if (l) {
		gtk_window_present (GTK_WINDOW (l->data));
		return;
	}

	/* Create dialog */
	dialog = gtk_dialog_new ();
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Edit Contact Information"));

	/* Close button */
	button = gtk_button_new_with_label (GTK_STOCK_CLOSE);
	gtk_button_set_use_stock (GTK_BUTTON (button), TRUE);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
				      button,
				      GTK_RESPONSE_CLOSE);
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_window_set_default (GTK_WINDOW (dialog), button);
	gtk_widget_show (button);

	/* Contact info widget */
	contact_widget = empathy_contact_widget_new (contact,
		EMPATHY_CONTACT_WIDGET_SHOW_LOCATION |
		EMPATHY_CONTACT_WIDGET_EDIT_NONE);
	gtk_container_set_border_width (GTK_CONTAINER (contact_widget), 8);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    contact_widget,
			    TRUE, TRUE, 0);
	gtk_widget_show (contact_widget);

	g_object_set_data (G_OBJECT (dialog), "contact_widget", contact_widget);
	information_dialogs = g_list_prepend (information_dialogs, dialog);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (contact_dialogs_response_cb),
			  &information_dialogs);

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
	}

	gtk_widget_show (dialog);
}

void
empathy_contact_edit_dialog_show (EmpathyContact *contact,
				  GtkWindow      *parent)
{
	GtkWidget *dialog;
	GtkWidget *button;
	GtkWidget *contact_widget;
	GList     *l;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	l = g_list_find_custom (edit_dialogs,
				contact,
				(GCompareFunc) contact_dialogs_find);
	if (l) {
		gtk_window_present (GTK_WINDOW (l->data));
		return;
	}

	/* Create dialog */
	dialog = gtk_dialog_new ();
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Edit Contact Information"));

	/* Close button */
	button = gtk_button_new_with_label (GTK_STOCK_CLOSE);
	gtk_button_set_use_stock (GTK_BUTTON (button), TRUE);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
				      button,
				      GTK_RESPONSE_CLOSE);
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_window_set_default (GTK_WINDOW (dialog), button);
	gtk_widget_show (button);

	/* Contact info widget */
	contact_widget = empathy_contact_widget_new (contact,
		EMPATHY_CONTACT_WIDGET_EDIT_ALIAS |
		EMPATHY_CONTACT_WIDGET_EDIT_GROUPS);
	gtk_container_set_border_width (GTK_CONTAINER (contact_widget), 8);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    contact_widget,
			    TRUE, TRUE, 0);
	gtk_widget_show (contact_widget);

	g_object_set_data (G_OBJECT (dialog), "contact_widget", contact_widget);
	edit_dialogs = g_list_prepend (edit_dialogs, dialog);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (contact_dialogs_response_cb),
			  &edit_dialogs);

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
	}

	gtk_widget_show (dialog);
}

void
empathy_contact_personal_dialog_show (GtkWindow *parent)
{
	GtkWidget *button;
	GtkWidget *contact_widget;

	if (personal_dialog) {
		gtk_window_present (GTK_WINDOW (personal_dialog));
		return;
	}

	/* Create dialog */
	personal_dialog = gtk_dialog_new ();
	gtk_dialog_set_has_separator (GTK_DIALOG (personal_dialog), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (personal_dialog), FALSE);
	gtk_window_set_title (GTK_WINDOW (personal_dialog), _("Personal Information"));

	/* Close button */
	button = gtk_button_new_with_label (GTK_STOCK_CLOSE);
	gtk_button_set_use_stock (GTK_BUTTON (button), TRUE);
	gtk_dialog_add_action_widget (GTK_DIALOG (personal_dialog),
				      button,
				      GTK_RESPONSE_CLOSE);
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_window_set_default (GTK_WINDOW (personal_dialog), button);
	gtk_widget_show (button);

	/* Contact info widget */
	contact_widget = empathy_contact_widget_new (NULL,
		EMPATHY_CONTACT_WIDGET_EDIT_ACCOUNT |
		EMPATHY_CONTACT_WIDGET_EDIT_ALIAS |
		EMPATHY_CONTACT_WIDGET_EDIT_AVATAR);
	gtk_container_set_border_width (GTK_CONTAINER (contact_widget), 8);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (personal_dialog)->vbox),
			    contact_widget,
			    TRUE, TRUE, 0);
	empathy_contact_widget_set_account_filter (contact_widget,
		empathy_account_chooser_filter_is_connected, NULL);
	gtk_widget_show (contact_widget);

	g_signal_connect (personal_dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	g_object_add_weak_pointer (G_OBJECT (personal_dialog),
				   (gpointer) &personal_dialog);

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (personal_dialog), parent);
	}

	gtk_widget_show (personal_dialog);
}

/*
 *  New contact dialog
 */

static gboolean
can_add_contact_to_account (EmpathyAccount *account,
			    gpointer   user_data)
{
	EmpathyContactManager *contact_manager;
	TpConnection          *connection;
	gboolean               result;

	connection = empathy_account_get_connection (account);
	if (connection == NULL)
		return FALSE;

	contact_manager = empathy_contact_manager_dup_singleton ();
	result = empathy_contact_manager_can_add (contact_manager, connection);
	g_object_unref (contact_manager);

	return result;
}

static void
new_contact_response_cb (GtkDialog *dialog,
			 gint       response,
			 GtkWidget *contact_widget)
{
	EmpathyContactManager *manager;
	EmpathyContact         *contact;

	manager = empathy_contact_manager_dup_singleton ();
	contact = empathy_contact_widget_get_contact (contact_widget);

	if (contact && response == GTK_RESPONSE_OK) {
		empathy_contact_list_add (EMPATHY_CONTACT_LIST (manager),
					  contact, "");
	}

	new_contact_dialog = NULL;
	gtk_widget_destroy (GTK_WIDGET (dialog));
	g_object_unref (manager);
}

void
empathy_new_contact_dialog_show (GtkWindow *parent)
{
	GtkWidget *dialog;
	GtkWidget *button;
	GtkWidget *contact_widget;

	if (new_contact_dialog) {
		gtk_window_present (GTK_WINDOW (new_contact_dialog));
		return;
	}

	/* Create dialog */
	dialog = gtk_dialog_new ();
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("New Contact"));

	/* Cancel button */
	button = gtk_button_new_with_label (GTK_STOCK_CANCEL);
	gtk_button_set_use_stock (GTK_BUTTON (button), TRUE);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
				      button,
				      GTK_RESPONSE_CANCEL);
	gtk_widget_show (button);

	/* Add button */
	button = gtk_button_new_with_label (GTK_STOCK_ADD);
	gtk_button_set_use_stock (GTK_BUTTON (button), TRUE);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
				      button,
				      GTK_RESPONSE_OK);
	gtk_widget_show (button);

	/* Contact info widget */
	contact_widget = empathy_contact_widget_new (NULL,
						     EMPATHY_CONTACT_WIDGET_EDIT_ALIAS |
						     EMPATHY_CONTACT_WIDGET_EDIT_ACCOUNT |
						     EMPATHY_CONTACT_WIDGET_EDIT_ID |
						     EMPATHY_CONTACT_WIDGET_EDIT_GROUPS);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    contact_widget,
			    TRUE, TRUE, 0);
	empathy_contact_widget_set_account_filter (contact_widget,
						   can_add_contact_to_account,
						   NULL);
	gtk_widget_show (contact_widget);

	new_contact_dialog = dialog;

	g_signal_connect (dialog, "response",
			  G_CALLBACK (new_contact_response_cb),
			  contact_widget);

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
	}

	gtk_widget_show (dialog);
}

