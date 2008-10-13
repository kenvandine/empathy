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

#include <libempathy/empathy-log-manager.h>
#include <libempathy/empathy-dispatcher.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-chatroom-manager.h>

#include "empathy-contact-menu.h"
#include "empathy-images.h"
#include "empathy-log-window.h"
#include "empathy-contact-dialogs.h"

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

  /* Invite */
  item = empathy_contact_invite_menu_item_new (contact);
	gtk_menu_shell_append (shell, item);
	gtk_widget_show (item);

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
				  G_CALLBACK (empathy_dispatcher_chat_with_contact),
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
				  G_CALLBACK (empathy_dispatcher_call_with_contact),
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

typedef struct 
{
  EmpathyContact *contact;
  TpChannel *channel;
} contact_room_sub_menu_item_activate_cb_ctx;

static contact_room_sub_menu_item_activate_cb_ctx *
contact_room_sub_menu_item_activate_cb_ctx_new (EmpathyContact *contact,
                                                TpChannel *channel)
{
  contact_room_sub_menu_item_activate_cb_ctx *ctx;

  ctx = g_slice_new (contact_room_sub_menu_item_activate_cb_ctx);

  ctx->contact = g_object_ref (contact);
  ctx->channel = g_object_ref (channel);

  return ctx;
}

static void
contact_room_sub_menu_item_activate_cb_ctx_free (
    contact_room_sub_menu_item_activate_cb_ctx *ctx)
{
  /* FIXME: seems this is never called... */
  g_object_unref (ctx->contact);
  g_object_unref (ctx->channel);

  g_slice_free (contact_room_sub_menu_item_activate_cb_ctx, ctx);
}

static void
contact_room_sub_menu_item_activate_cb (
    GtkWidget *item,
    contact_room_sub_menu_item_activate_cb_ctx *ctx)
{
  GArray *handles;
  TpHandle handle;

  /* send invitation */
  handles = g_array_new (FALSE, FALSE, sizeof (TpHandle));
  handle = empathy_contact_get_handle (ctx->contact);
  g_array_append_val (handles, handle);

  tp_cli_channel_interface_group_call_add_members (ctx->channel, -1, handles,
      _("Inviting to this room"), NULL, NULL, NULL, NULL);

  g_array_free (handles, TRUE);
}

static GtkWidget *
create_room_sub_menu_item (EmpathyContact *contact,
                           EmpathyChatroom *chatroom)
{
	GtkWidget *item;
  TpChannel *channel;
  contact_room_sub_menu_item_activate_cb_ctx *ctx;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

  item = gtk_menu_item_new_with_label (empathy_chatroom_get_name (chatroom));

  g_object_get (chatroom, "tp-channel", &channel, NULL);
  g_assert (channel != NULL);

  ctx = contact_room_sub_menu_item_activate_cb_ctx_new (contact, channel);

  g_signal_connect_data (item, "activate",
      G_CALLBACK (contact_room_sub_menu_item_activate_cb), ctx,
      (GClosureNotify) contact_room_sub_menu_item_activate_cb_ctx_free, 0);

  g_object_unref (channel);
	
	return item;
}

GtkWidget *
empathy_contact_invite_menu_item_new (EmpathyContact *contact)
{
	GtkWidget *item;
	GtkWidget *image;
  GtkWidget *room_item;
  EmpathyChatroomManager *mgr;
  GList *rooms, *l;
  GtkWidget *submenu;
  GtkMenuShell *submenu_shell;
  gboolean have_rooms = FALSE;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	item = gtk_image_menu_item_new_with_mnemonic (_("_Invite to..."));
	image = gtk_image_new_from_icon_name (EMPATHY_IMAGE_GROUP_MESSAGE,
					      GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  mgr = empathy_chatroom_manager_new ();
  rooms = empathy_chatroom_manager_get_chatrooms (mgr,
      empathy_contact_get_account (contact));

  /* create rooms sub menu */
  submenu = gtk_menu_new ();
  submenu_shell = GTK_MENU_SHELL (submenu);

  for (l = rooms; l != NULL; l = g_list_next (l))
    {
      EmpathyChatroom *chatroom = l->data;
      TpChannel *channel;

      g_object_get (chatroom, "tp-channel", &channel, NULL);
      if (channel != NULL)
        {
          have_rooms = TRUE;

          room_item = create_room_sub_menu_item (contact, chatroom);
          gtk_menu_shell_append (submenu_shell, room_item);
          gtk_widget_show (room_item);

          g_object_unref (channel);
        }
    }

  if (have_rooms)
    {
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
    }
  else
    {
      gtk_widget_set_sensitive (item, FALSE);
      gtk_widget_destroy (submenu);
    }

	gtk_widget_show (image);

  g_object_unref (mgr);
  g_list_free (rooms);

	return item;
}
