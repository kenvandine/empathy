/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Xavier Claessens <xclaesse@gmail.com>
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

#ifndef __EMPATHY_TP_CONTACT_LIST_H__
#define __EMPATHY_TP_CONTACT_LIST_H__

#include <glib.h>
#include <libmissioncontrol/mc-account.h>


G_BEGIN_DECLS

#define EMPATHY_TYPE_TP_CONTACT_LIST         (empathy_tp_contact_list_get_type ())
#define EMPATHY_TP_CONTACT_LIST(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CONTACT_LIST, EmpathyTpContactList))
#define EMPATHY_TP_CONTACT_LIST_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_CONTACT_LIST, EmpathyTpContactListClass))
#define EMPATHY_IS_TP_CONTACT_LIST(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CONTACT_LIST))
#define EMPATHY_IS_TP_CONTACT_LIST_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CONTACT_LIST))
#define EMPATHY_TP_CONTACT_LIST_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CONTACT_LIST, EmpathyTpContactListClass))

typedef struct _EmpathyTpContactList      EmpathyTpContactList;
typedef struct _EmpathyTpContactListClass EmpathyTpContactListClass;

struct _EmpathyTpContactList {
	GObject parent;
	gpointer priv;
};

struct _EmpathyTpContactListClass {
	GObjectClass parent_class;
};

GType                  empathy_tp_contact_list_get_type    (void) G_GNUC_CONST;
EmpathyTpContactList * empathy_tp_contact_list_new         (McAccount            *account);
McAccount *            empathy_tp_contact_list_get_account (EmpathyTpContactList *list);
gboolean               empathy_tp_contact_list_can_add     (EmpathyTpContactList *list);

G_END_DECLS

#endif /* __EMPATHY_TP_CONTACT_LIST_H__ */
