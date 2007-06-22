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
	}

	return type;
}

static void
contact_list_base_init (gpointer klass)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		g_signal_new ("contact-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, EMPATHY_TYPE_CONTACT);

		g_signal_new ("contact-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, EMPATHY_TYPE_CONTACT);

		g_signal_new ("local-pending",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__OBJECT_STRING,
			      G_TYPE_NONE,
			      2, EMPATHY_TYPE_CONTACT, G_TYPE_STRING);

		initialized = TRUE;
	}
}

EmpathyContactListInfo *
empathy_contact_list_info_new (EmpathyContact *contact,
			       const gchar   *message)
{
	EmpathyContactListInfo *info;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	info = g_slice_new0 (EmpathyContactListInfo);
	info->contact = g_object_ref (contact);
	info->message = g_strdup (message);

	return info;
}			       

void
empathy_contact_list_info_free (EmpathyContactListInfo *info)
{
	if (!info) {
		return;
	}

	if (info->contact) {
		g_object_unref (info->contact);
	}
	g_free (info->message);

	g_slice_free (EmpathyContactListInfo, info);
}

void
empathy_contact_list_setup (EmpathyContactList *list)
{
	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->setup) {
		EMPATHY_CONTACT_LIST_GET_IFACE (list)->setup (list);
	}
}

EmpathyContact *
empathy_contact_list_find (EmpathyContactList *list,
			   const gchar        *id)
{
	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->find) {
		return EMPATHY_CONTACT_LIST_GET_IFACE (list)->find (list, id);
	}

	return NULL;
}

void
empathy_contact_list_add (EmpathyContactList *list,
			  EmpathyContact      *contact,
			  const gchar        *message)
{
	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->add) {
		EMPATHY_CONTACT_LIST_GET_IFACE (list)->add (list, contact, message);
	}
}

void
empathy_contact_list_remove (EmpathyContactList *list,
			     EmpathyContact      *contact,
			     const gchar        *message)
{
	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));

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

GList *
empathy_contact_list_get_local_pending (EmpathyContactList *list)
{
	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->get_local_pending) {
		return EMPATHY_CONTACT_LIST_GET_IFACE (list)->get_local_pending (list);
	}

	return NULL;
}

void
empathy_contact_list_process_pending (EmpathyContactList *list,
				      EmpathyContact      *contact,
				      gboolean            accept)
{
	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->process_pending) {
		EMPATHY_CONTACT_LIST_GET_IFACE (list)->process_pending (list,
									contact,
									accept);
	}
}

