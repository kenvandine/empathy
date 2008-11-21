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

#ifndef __EMPATHY_CONTACT_MENU_H__
#define __EMPATHY_CONTACT_MENU_H__

#include <gtk/gtkmenu.h>

#include <libempathy/empathy-contact.h>

G_BEGIN_DECLS

typedef enum {
	EMPATHY_CONTACT_FEATURE_NONE = 0,
	EMPATHY_CONTACT_FEATURE_CHAT = 1 << 0,
	EMPATHY_CONTACT_FEATURE_CALL = 1 << 1,
	EMPATHY_CONTACT_FEATURE_LOG = 1 << 2,
	EMPATHY_CONTACT_FEATURE_EDIT = 1 << 3,
	EMPATHY_CONTACT_FEATURE_INFO = 1 << 4,
	EMPATHY_CONTACT_FEATURE_ALL = (1 << 5) - 1,
} EmpathyContactFeatureFlags;

GtkWidget * empathy_contact_menu_new           (EmpathyContact             *contact,
						EmpathyContactFeatureFlags  features);
GtkWidget * empathy_contact_chat_menu_item_new (EmpathyContact             *contact);
GtkWidget * empathy_contact_call_menu_item_new (EmpathyContact             *contact);
GtkWidget * empathy_contact_log_menu_item_new  (EmpathyContact             *contact);
GtkWidget * empathy_contact_info_menu_item_new (EmpathyContact             *contact);
GtkWidget * empathy_contact_edit_menu_item_new (EmpathyContact             *contact);
GtkWidget * empathy_contact_invite_menu_item_new (EmpathyContact *contact);
GtkWidget * empathy_contact_file_transfer_menu_item_new (EmpathyContact    *contact);

G_END_DECLS

#endif /* __EMPATHY_CONTACT_MENU_H__ */

