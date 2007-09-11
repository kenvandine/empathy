/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_CONTACT_LIST_VIEW_H__
#define __EMPATHY_CONTACT_LIST_VIEW_H__

#include <gtk/gtktreeview.h>

#include <libempathy/empathy-contact.h>

#include "empathy-contact-list-store.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_CONTACT_LIST_VIEW         (empathy_contact_list_view_get_type ())
#define EMPATHY_CONTACT_LIST_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CONTACT_LIST_VIEW, EmpathyContactListView))
#define EMPATHY_CONTACT_LIST_VIEW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_CONTACT_LIST_VIEW, EmpathyContactListViewClass))
#define EMPATHY_IS_CONTACT_LIST_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CONTACT_LIST_VIEW))
#define EMPATHY_IS_CONTACT_LIST_VIEW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CONTACT_LIST_VIEW))
#define EMPATHY_CONTACT_LIST_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CONTACT_LIST_VIEW, EmpathyContactListViewClass))

typedef struct _EmpathyContactListView      EmpathyContactListView;
typedef struct _EmpathyContactListViewClass EmpathyContactListViewClass;
typedef struct _EmpathyContactListViewPriv  EmpathyContactListViewPriv;

struct _EmpathyContactListView {
	GtkTreeView            parent;
};

struct _EmpathyContactListViewClass {
	GtkTreeViewClass       parent_class;
};

GType                   empathy_contact_list_view_get_type           (void) G_GNUC_CONST;
EmpathyContactListView *empathy_contact_list_view_new                (EmpathyContactListStore *store);
void                    empathy_contact_list_view_set_interactive    (EmpathyContactListView  *view,
								      gboolean                 interactive);
gboolean                empathy_contact_list_view_get_interactive    (EmpathyContactListView  *view);
EmpathyContact *        empathy_contact_list_view_get_selected       (EmpathyContactListView  *view);
gchar *                 empathy_contact_list_view_get_selected_group (EmpathyContactListView  *view);
GtkWidget *             empathy_contact_list_view_get_contact_menu   (EmpathyContactListView  *view,
								      EmpathyContact          *contact);
GtkWidget *             empathy_contact_list_view_get_group_menu     (EmpathyContactListView  *view);

G_END_DECLS

#endif /* __EMPATHY_CONTACT_LIST_VIEW_H__ */

