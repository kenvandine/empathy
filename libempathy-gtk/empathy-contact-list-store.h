/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
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

#ifndef __EMPATHY_CONTACT_LIST_STORE_H__
#define __EMPATHY_CONTACT_LIST_STORE_H__

#include <gtk/gtktreestore.h>

#include <libempathy/empathy-contact-list.h>
#include <libempathy/empathy-contact.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_CONTACT_LIST_STORE         (empathy_contact_list_store_get_type ())
#define EMPATHY_CONTACT_LIST_STORE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CONTACT_LIST_STORE, EmpathyContactListStore))
#define EMPATHY_CONTACT_LIST_STORE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_CONTACT_LIST_STORE, EmpathyContactListStoreClass))
#define EMPATHY_IS_CONTACT_LIST_STORE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CONTACT_LIST_STORE))
#define EMPATHY_IS_CONTACT_LIST_STORE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CONTACT_LIST_STORE))
#define EMPATHY_CONTACT_LIST_STORE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CONTACT_LIST_STORE, EmpathyContactListStoreClass))

typedef struct _EmpathyContactListStore      EmpathyContactListStore;
typedef struct _EmpathyContactListStoreClass EmpathyContactListStoreClass;

typedef enum {
	EMPATHY_CONTACT_LIST_STORE_SORT_STATE,
	EMPATHY_CONTACT_LIST_STORE_SORT_NAME
} EmpathyContactListStoreSort;

typedef enum {
	EMPATHY_CONTACT_LIST_STORE_COL_ICON_STATUS,
	EMPATHY_CONTACT_LIST_STORE_COL_PIXBUF_AVATAR,
	EMPATHY_CONTACT_LIST_STORE_COL_PIXBUF_AVATAR_VISIBLE,
	EMPATHY_CONTACT_LIST_STORE_COL_NAME,
	EMPATHY_CONTACT_LIST_STORE_COL_STATUS,
	EMPATHY_CONTACT_LIST_STORE_COL_STATUS_VISIBLE,
	EMPATHY_CONTACT_LIST_STORE_COL_CONTACT,
	EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP,
	EMPATHY_CONTACT_LIST_STORE_COL_IS_ACTIVE,
	EMPATHY_CONTACT_LIST_STORE_COL_IS_ONLINE,
	EMPATHY_CONTACT_LIST_STORE_COL_IS_SEPARATOR,
	EMPATHY_CONTACT_LIST_STORE_COL_CAN_VOIP,
	EMPATHY_CONTACT_LIST_STORE_COL_COUNT
} EmpathyContactListStoreCol;

struct _EmpathyContactListStore {
	GtkTreeStore parent;
	gpointer priv;
};

struct _EmpathyContactListStoreClass {
	GtkTreeStoreClass parent_class;
};

GType                      empathy_contact_list_store_get_type           (void) G_GNUC_CONST;
EmpathyContactListStore *  empathy_contact_list_store_new                (EmpathyContactList         *list_iface);
EmpathyContactList *       empathy_contact_list_store_get_list_iface     (EmpathyContactListStore     *store);
gboolean                   empathy_contact_list_store_get_show_offline   (EmpathyContactListStore     *store);
void                       empathy_contact_list_store_set_show_offline   (EmpathyContactListStore     *store,
									 gboolean                    show_offline);
gboolean                   empathy_contact_list_store_get_show_avatars   (EmpathyContactListStore     *store);
void                       empathy_contact_list_store_set_show_avatars   (EmpathyContactListStore     *store,
									 gboolean                    show_avatars);
gboolean                   empathy_contact_list_store_get_show_groups   (EmpathyContactListStore     *store);
void                       empathy_contact_list_store_set_show_groups   (EmpathyContactListStore     *store,
									 gboolean                    show_groups);
gboolean                   empathy_contact_list_store_get_is_compact     (EmpathyContactListStore     *store);
void                       empathy_contact_list_store_set_is_compact     (EmpathyContactListStore     *store,
									 gboolean                    is_compact);
EmpathyContactListStoreSort empathy_contact_list_store_get_sort_criterium (EmpathyContactListStore     *store);
void                       empathy_contact_list_store_set_sort_criterium (EmpathyContactListStore     *store,
									 EmpathyContactListStoreSort  sort_criterium);
gboolean                   empathy_contact_list_store_row_separator_func (GtkTreeModel               *model,
									 GtkTreeIter                *iter,
									 gpointer                    data);
gchar *                    empathy_contact_list_store_get_parent_group   (GtkTreeModel               *model,
									 GtkTreePath                *path,
									 gboolean                   *path_is_group);
gboolean                   empathy_contact_list_store_search_equal_func  (GtkTreeModel               *model,
									 gint                        column,
									 const gchar                *key,
									 GtkTreeIter                *iter,
									 gpointer                    search_data);

G_END_DECLS

#endif /* __EMPATHY_CONTACT_LIST_STORE_H__ */

