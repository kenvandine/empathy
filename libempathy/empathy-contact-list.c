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

#include "config.h"

#include "empathy-contact-list.h"
#include "empathy-marshal.h"

static void contact_list_base_init (gpointer klass);

GType
empathy_contact_list_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo type_info = {
			sizeof (EmpathyContactListIface),
			contact_list_base_init,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EmpathyContactList",
					       &type_info, 0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

static void
contact_list_base_init (gpointer klass)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		g_signal_new ("members-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      _empathy_marshal_VOID__OBJECT_OBJECT_UINT_STRING_BOOLEAN,
			      G_TYPE_NONE,
			      5, EMPATHY_TYPE_CONTACT, EMPATHY_TYPE_CONTACT,
			      G_TYPE_UINT, G_TYPE_STRING, G_TYPE_BOOLEAN);

		g_signal_new ("pendings-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      _empathy_marshal_VOID__OBJECT_OBJECT_UINT_STRING_BOOLEAN,
			      G_TYPE_NONE,
			      5, EMPATHY_TYPE_CONTACT, EMPATHY_TYPE_CONTACT,
			      G_TYPE_UINT, G_TYPE_STRING, G_TYPE_BOOLEAN);

		g_signal_new ("groups-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      _empathy_marshal_VOID__OBJECT_STRING_BOOLEAN,
			      G_TYPE_NONE,
			      3, EMPATHY_TYPE_CONTACT, G_TYPE_STRING, G_TYPE_BOOLEAN);

		initialized = TRUE;
	}
}

void
empathy_contact_list_add (EmpathyContactList *list,
			  EmpathyContact     *contact,
			  const gchar        *message)
{
	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->add) {
		EMPATHY_CONTACT_LIST_GET_IFACE (list)->add (list, contact, message);
	}
}

void
empathy_contact_list_remove (EmpathyContactList *list,
			     EmpathyContact     *contact,
			     const gchar        *message)
{
	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->remove) {
		EMPATHY_CONTACT_LIST_GET_IFACE (list)->remove (list, contact, message);
	}
}

GList *
empathy_contact_list_get_members (EmpathyContactList *list)
{
	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->get_members) {
		return EMPATHY_CONTACT_LIST_GET_IFACE (list)->get_members (list);
	}

	return NULL;
}

EmpathyContactMonitor *
empathy_contact_list_get_monitor (EmpathyContactList *list)
{
	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->get_monitor) {
		return EMPATHY_CONTACT_LIST_GET_IFACE (list)->get_monitor (list);
	}

	return NULL;
}

GList *
empathy_contact_list_get_pendings (EmpathyContactList *list)
{
	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->get_pendings) {
		return EMPATHY_CONTACT_LIST_GET_IFACE (list)->get_pendings (list);
	}

	return NULL;
}

GList *
empathy_contact_list_get_all_groups (EmpathyContactList *list)
{
	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->get_all_groups) {
		return EMPATHY_CONTACT_LIST_GET_IFACE (list)->get_all_groups (list);
	}

	return NULL;
}

GList *
empathy_contact_list_get_groups (EmpathyContactList *list,
				 EmpathyContact     *contact)
{
	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);
	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->get_groups) {
		return EMPATHY_CONTACT_LIST_GET_IFACE (list)->get_groups (list, contact);
	}

	return NULL;
}

void
empathy_contact_list_add_to_group (EmpathyContactList *list,
				   EmpathyContact     *contact,
				   const gchar        *group)
{
	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));
	g_return_if_fail (group != NULL);

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->add_to_group) {
		EMPATHY_CONTACT_LIST_GET_IFACE (list)->add_to_group (list, contact, group);
	}
}

void
empathy_contact_list_remove_from_group (EmpathyContactList *list,
					EmpathyContact     *contact,
					const gchar        *group)
{
	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));
	g_return_if_fail (group != NULL);

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->remove_from_group) {
		EMPATHY_CONTACT_LIST_GET_IFACE (list)->remove_from_group (list, contact, group);
	}
}

void
empathy_contact_list_rename_group (EmpathyContactList *list,
				   const gchar        *old_group,
				   const gchar        *new_group)
{
	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));
	g_return_if_fail (old_group != NULL);
	g_return_if_fail (new_group != NULL);

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->rename_group) {
		EMPATHY_CONTACT_LIST_GET_IFACE (list)->rename_group (list, old_group, new_group);
	}
}

void
empathy_contact_list_remove_group (EmpathyContactList *list,
				   const gchar *group)
{
	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));
	g_return_if_fail (group != NULL);

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->remove_group) {
		EMPATHY_CONTACT_LIST_GET_IFACE (list)->remove_group (list, group);
	}
}

