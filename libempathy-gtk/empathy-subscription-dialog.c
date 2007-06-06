/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-contact-list.h>

#include "empathy-subscription-dialog.h"
#include "empathy-contact-widget.h"
#include "gossip-ui-utils.h"

static GHashTable *dialogs = NULL;

static void
subscription_dialog_response_cb (GtkDialog     *dialog,
				 gint           response,
				 GossipContact *contact)
{
	EmpathyContactManager *manager;

	manager = empathy_contact_manager_new ();

	if (response == GTK_RESPONSE_YES) {
		empathy_contact_list_add (EMPATHY_CONTACT_LIST (manager),
					  contact,
					  _("I would like to add you to my contact list."));
	}
	else if (response == GTK_RESPONSE_NO) {
		empathy_contact_list_remove (EMPATHY_CONTACT_LIST (manager),
					     contact,
					     _("Sorry, I don't want you in my contact list."));
	}

	g_hash_table_remove (dialogs, contact);
	g_object_unref (manager);
}

void
empathy_subscription_dialog_show (GossipContact *contact,
				  GtkWindow     *parent)
{
	GtkWidget *dialog;
	GtkWidget *hbox_subscription;

	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	if (!dialogs) {
		dialogs = g_hash_table_new_full (gossip_contact_hash,
						 gossip_contact_equal,
						 (GDestroyNotify) g_object_unref,
						 (GDestroyNotify) gtk_widget_destroy);
	}

	dialog = g_hash_table_lookup (dialogs, contact);
	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog));
		return;
	}

	gossip_glade_get_file_simple ("empathy-subscription-dialog.glade",
				      "subscription_request_dialog",
				      NULL,
				      "subscription_request_dialog", &dialog,
				      "hbox_subscription", &hbox_subscription,
				      NULL);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (subscription_dialog_response_cb),
			  contact);

	g_hash_table_insert (dialogs, g_object_ref (contact), dialog);

	gtk_box_pack_end (GTK_BOX (hbox_subscription),
			  empathy_contact_widget_new (contact),
			  TRUE, TRUE,
			  0);

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
	}

	gtk_widget_show (dialog);
}

