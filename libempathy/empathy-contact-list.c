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
			      1, GOSSIP_TYPE_CONTACT);

		g_signal_new ("contact-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CONTACT);

		initialized = TRUE;
	}
}

void
empathy_contact_list_setup (EmpathyContactList *list)
{
	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->setup) {
		EMPATHY_CONTACT_LIST_GET_IFACE (list)->setup (list);
	}
}

GossipContact *
empathy_contact_list_find (EmpathyContactList *list,
				 const gchar             *id)
{
	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->find) {
		return EMPATHY_CONTACT_LIST_GET_IFACE (list)->find (list, id);
	}

	return NULL;
}

void
empathy_contact_list_add (EmpathyContactList *list,
			  GossipContact      *contact,
			  const gchar        *message)
{
	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->add) {
		EMPATHY_CONTACT_LIST_GET_IFACE (list)->add (list, contact, message);
	}
}

void
empathy_contact_list_remove (EmpathyContactList *list,
			     GossipContact      *contact,
			     const gchar        *message)
{
	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->remove) {
		EMPATHY_CONTACT_LIST_GET_IFACE (list)->remove (list, contact, message);
	}
}

GList *
empathy_contact_list_get_contacts (EmpathyContactList *list)
{
	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);

	if (EMPATHY_CONTACT_LIST_GET_IFACE (list)->get_contacts) {
		return EMPATHY_CONTACT_LIST_GET_IFACE (list)->get_contacts (list);
	}

	return NULL;
}

