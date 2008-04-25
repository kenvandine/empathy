/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Collabora Ltd.
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

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-log-manager.h>

#include "empathy-contact-menu.h"
#include "empathy-images.h"
#include "empathy-log-window.h"
#include "empathy-contact-dialogs.h"

#define DEBUG_DOMAIN "ContactMenu"

GtkWidget *
empathy_contact_menu_new (EmpathyContact             *contact,
			  EmpathyContactFeatureFlags  features)
{
	GtkWidget    *menu;
	GtkMenuShell *shell;
	GtkWidget    *item;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	if (features == EMPATHY_CONTACT_FEATURE_NONE) {
		return NULL;
	}

	menu = gtk_menu_new ();
	shell = GTK_MENU_SHELL (menu);

	/* Chat */
	if (features & EMPATHY_CONTACT_FEATURE_CHAT) {
		item = empathy_contact_chat_menu_item_new (contact);
		gtk_menu_shell_append (shell, item);
		gtk_widget_show (item);
	}

	/* Call */
	if (features & EMPATHY_CONTACT_FEATURE_CALL) {
		item = empathy_contact_call_menu_item_new (contact);
		gtk_menu_shell_append (shell, item);
		gtk_widget_show (item);
	}

	/* Log */
	if (features & EMPATHY_CONTACT_FEATURE_LOG) {
		item = empathy_contact_log_menu_item_new (contact);
		gtk_menu_shell_append (shell, item);
		gtk_widget_show (item);
	}

	/* Separator */
	if (features & (EMPATHY_CONTACT_FEATURE_EDIT |
			EMPATHY_CONTACT_FEATURE_INFO)) {
		item = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (shell, item);
		gtk_widget_show (item);
	}

	/* Edit */
	if (features & EMPATHY_CONTACT_FEATURE_EDIT) {
		item = empathy_contact_edit_menu_item_new (contact);
		gtk_menu_shell_append (shell, item);
		gtk_widget_show (item);
	}

	/* Info */
	if (features & EMPATHY_CONTACT_FEATURE_INFO) {
		item = empathy_contact_info_menu_item_new (contact);
		gtk_menu_shell_append (shell, item);
		gtk_widget_show (item);
	}

	return menu;
}

GtkWidget *
empathy_contact_chat_menu_item_new (EmpathyContact *contact)
{
	GtkWidget *item;
	GtkWidget *image;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Chat"));
	image = gtk_image_new_from_icon_name (EMPATHY_IMAGE_MESSAGE,
					      GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	gtk_widget_show (image);

	g_signal_connect_swapped (item, "activate",
				  G_CALLBACK (empathy_chat_with_contact),
				  contact);
	
	return item;
}

GtkWidget *
empathy_contact_call_menu_item_new (EmpathyContact *contact)
{
	GtkWidget *item;
	GtkWidget *image;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Call"));
	image = gtk_image_new_from_icon_name (EMPATHY_IMAGE_VOIP,
					      GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	gtk_widget_set_sensitive (item, empathy_contact_can_voip (contact));
	gtk_widget_show (image);

	g_signal_connect_swapped (item, "activate",
				  G_CALLBACK (empathy_call_with_contact),
				  contact);
	
	return item;
}

static void
contact_log_menu_item_activate_cb (EmpathyContact *contact)
{
	empathy_log_window_show (empathy_contact_get_account (contact),
				 empathy_contact_get_id (contact),
				 FALSE, NULL);
}

GtkWidget *
empathy_contact_log_menu_item_new (EmpathyContact *contact)
{
	EmpathyLogManager *manager;
	gboolean           have_log;
	GtkWidget         *item;
	GtkWidget         *image;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	manager = empathy_log_manager_new ();
	have_log = empathy_log_manager_exists (manager,
					       empathy_contact_get_account (contact),
					       empathy_contact_get_id (contact),
					       FALSE);
	g_object_unref (manager);

	item = gtk_image_menu_item_new_with_mnemonic (_("_View Previous Conversations"));
	image = gtk_image_new_from_icon_name (EMPATHY_IMAGE_LOG,
					      GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	gtk_widget_set_sensitive (item, have_log);
	gtk_widget_show (image);

	g_signal_connect_swapped (item, "activate",
				  G_CALLBACK (contact_log_menu_item_activate_cb),
				  contact);
	
	return item;
}

static void
contact_info_menu_item_activate_cb (EmpathyContact *contact)
{
	empathy_contact_information_dialog_show (contact, NULL, FALSE, FALSE);
}

GtkWidget *
empathy_contact_info_menu_item_new (EmpathyContact *contact)
{
	GtkWidget *item;
	GtkWidget *image;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	item = gtk_image_menu_item_new_with_mnemonic (_("Infor_mation"));
	image = gtk_image_new_from_icon_name (EMPATHY_IMAGE_CONTACT_INFORMATION,
					      GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	gtk_widget_show (image);

	g_signal_connect_swapped (item, "activate",
				  G_CALLBACK (contact_info_menu_item_activate_cb),
				  contact);
	
	return item;
}

static void
contact_edit_menu_item_activate_cb (EmpathyContact *contact)
{
	empathy_contact_information_dialog_show (contact, NULL, TRUE, FALSE);
}

GtkWidget *
empathy_contact_edit_menu_item_new (EmpathyContact *contact)
{
	GtkWidget *item;
	GtkWidget *image;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Edit"));
	image = gtk_image_new_from_icon_name (GTK_STOCK_EDIT,
					      GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	gtk_widget_show (image);

	g_signal_connect_swapped (item, "activate",
				  G_CALLBACK (contact_edit_menu_item_activate_cb),
				  contact);
	
	return item;
}

