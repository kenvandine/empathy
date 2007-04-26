/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Xavier Claessens <xclaesse@gmail.com>
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

#include <glib.h>
#include <libmissioncontrol/mc-account.h>

#include "gossip-contact.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_CONTACT_LIST         (empathy_contact_list_get_type ())
#define EMPATHY_CONTACT_LIST(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CONTACT_LIST, EmpathyContactList))
#define EMPATHY_CONTACT_LIST_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_CONTACT_LIST, EmpathyContactListClass))
#define EMPATHY_IS_CONTACT_LIST(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CONTACT_LIST))
#define EMPATHY_IS_CONTACT_LIST_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CONTACT_LIST))
#define EMPATHY_CONTACT_LIST_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CONTACT_LIST, EmpathyContactListClass))

typedef struct _EmpathyContactList      EmpathyContactList;
typedef struct _EmpathyContactListClass EmpathyContactListClass;
typedef struct _EmpathyContactListPriv  EmpathyContactListPriv;

struct _EmpathyContactList {
	GObject      parent;
};

struct _EmpathyContactListClass {
	GObjectClass parent_class;
};

GType                empathy_contact_list_get_type         (void) G_GNUC_CONST;
EmpathyContactList * empathy_contact_list_new              (McAccount          *account);
void                 empathy_contact_list_setup            (EmpathyContactList *list);
McAccount *          empathy_contact_list_get_account      (EmpathyContactList *list);
GossipContact *      empathy_contact_list_get_own          (EmpathyContactList *list);
GossipContact *      empathy_contact_list_find             (EmpathyContactList *list,
							    const gchar        *id);
void                 empathy_contact_list_add              (EmpathyContactList *list,
							    guint               handle,
							    const gchar        *message);
void                 empathy_contact_list_remove           (EmpathyContactList *list,
							    guint               handle);
GossipContact *      empathy_contact_list_get_from_id      (EmpathyContactList *list,
							    const gchar        *id);
GossipContact *      empathy_contact_list_get_from_handle  (EmpathyContactList *list,
							    guint               handle);
GList *              empathy_contact_list_get_from_handles (EmpathyContactList *list,
							    GArray             *handles);
void                 empathy_contact_list_rename_group     (EmpathyContactList *list,
							    const gchar        *old_group,
							    const gchar        *new_group);
GList *              empathy_contact_list_get_groups       (EmpathyContactList *list);
GList *              empathy_contact_list_get_contacts     (EmpathyContactList *list);

G_END_DECLS

#endif /* __EMPATHY_CONTACT_LIST_H__ */
