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

#ifndef __EMPATHY_CONTACT_LIST_H__
#define __EMPATHY_CONTACT_LIST_H__

#include <glib-object.h>

#include "gossip-contact.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_CONTACT_LIST         (empathy_contact_list_get_type ())
#define EMPATHY_CONTACT_LIST(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CONTACT_LIST, EmpathyContactList))
#define EMPATHY_IS_CONTACT_LIST(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CONTACT_LIST))
#define EMPATHY_CONTACT_LIST_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), EMPATHY_TYPE_CONTACT_LIST, EmpathyContactListIface))

typedef struct _EmpathyContactList      EmpathyContactList;
typedef struct _EmpathyContactListIface EmpathyContactListIface;

struct _EmpathyContactListIface {
	GTypeInterface   base_iface;

	/* VTabled */
	void            (*setup)        (EmpathyContactList *list);
	GossipContact * (*find)         (EmpathyContactList *list,
					 const gchar        *id);
	void            (*add)          (EmpathyContactList *list,
					 GossipContact      *contact,
					 const gchar        *message);
	void            (*remove)       (EmpathyContactList *list,
					 GossipContact      *contact,
					 const gchar        *message);
	GList *         (*get_contacts) (EmpathyContactList *list);
};

GType           empathy_contact_list_get_type     (void) G_GNUC_CONST;
void            empathy_contact_list_setup        (EmpathyContactList *list);
GossipContact * empathy_contact_list_find         (EmpathyContactList *list,
						   const gchar        *id);
void            empathy_contact_list_add          (EmpathyContactList *list,
						   GossipContact      *contact,
						   const gchar        *message);
void            empathy_contact_list_remove       (EmpathyContactList *list,
						   GossipContact      *contact,
						   const gchar        *message);
GList *         empathy_contact_list_get_contacts (EmpathyContactList *list);

G_END_DECLS

#endif /* __EMPATHY_CONTACT_LIST_H__ */

