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

#ifndef __GOSSIP_CONTACT_LIST_STORE_H__
#define __GOSSIP_CONTACT_LIST_STORE_H__

#include <gtk/gtktreestore.h>

#include <libempathy/empathy-contact-list.h>
#include <libempathy/gossip-contact.h>

G_BEGIN_DECLS

/*
 * GossipContactListStoreSort
 */ 
#define GOSSIP_TYPE_CONTACT_LIST_STORE_SORT    (gossip_contact_list_store_sort_get_type ())

typedef enum {
	GOSSIP_CONTACT_LIST_STORE_SORT_STATE,
	GOSSIP_CONTACT_LIST_STORE_SORT_NAME
} GossipContactListStoreSort;

GType gossip_contact_list_store_sort_get_type (void) G_GNUC_CONST;

/*
 * GossipContactListStore 
 */ 
#define GOSSIP_TYPE_CONTACT_LIST_STORE         (gossip_contact_list_store_get_type ())
#define GOSSIP_CONTACT_LIST_STORE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CONTACT_LIST_STORE, GossipContactListStore))
#define GOSSIP_CONTACT_LIST_STORE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_CONTACT_LIST_STORE, GossipContactListStoreClass))
#define GOSSIP_IS_CONTACT_LIST_STORE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CONTACT_LIST_STORE))
#define GOSSIP_IS_CONTACT_LIST_STORE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CONTACT_LIST_STORE))
#define GOSSIP_CONTACT_LIST_STORE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CONTACT_LIST_STORE, GossipContactListStoreClass))

typedef struct _GossipContactListStore      GossipContactListStore;
typedef struct _GossipContactListStoreClass GossipContactListStoreClass;
typedef struct _GossipContactListStorePriv  GossipContactListStorePriv;

enum {
	COL_ICON_STATUS,
	COL_PIXBUF_AVATAR,
	COL_PIXBUF_AVATAR_VISIBLE,
	COL_NAME,
	COL_STATUS,
	COL_STATUS_VISIBLE,
	COL_CONTACT,
	COL_IS_GROUP,
	COL_IS_ACTIVE,
	COL_IS_ONLINE,
	COL_IS_SEPARATOR,
	COL_COUNT
} GossipContactListStoreCol;

struct _GossipContactListStore {
	GtkTreeStore            parent;
};

struct _GossipContactListStoreClass {
	GtkTreeStoreClass       parent_class;
};

GType                      gossip_contact_list_store_get_type           (void) G_GNUC_CONST;
GossipContactListStore *   gossip_contact_list_store_new                (EmpathyContactList         *list_iface);
EmpathyContactList *       gossip_contact_list_store_get_list_iface     (GossipContactListStore     *store);
gboolean                   gossip_contact_list_store_get_show_offline   (GossipContactListStore     *store);
void                       gossip_contact_list_store_set_show_offline   (GossipContactListStore     *store,
									 gboolean                    show_offline);
gboolean                   gossip_contact_list_store_get_show_avatars   (GossipContactListStore     *store);
void                       gossip_contact_list_store_set_show_avatars   (GossipContactListStore     *store,
									 gboolean                    show_avatars);
gboolean                   gossip_contact_list_store_get_is_compact     (GossipContactListStore     *store);
void                       gossip_contact_list_store_set_is_compact     (GossipContactListStore     *store,
									 gboolean                    is_compact);
GossipContactListStoreSort gossip_contact_list_store_get_sort_criterium (GossipContactListStore     *store);
void                       gossip_contact_list_store_set_sort_criterium (GossipContactListStore     *store,
									 GossipContactListStoreSort  sort_criterium);
gboolean                   gossip_contact_list_store_row_separator_func (GtkTreeModel               *model,
									 GtkTreeIter                *iter,
									 gpointer                    data);
gchar *                    gossip_contact_list_store_get_parent_group   (GtkTreeModel               *model,
									 GtkTreePath                *path,
									 gboolean                   *path_is_group);
gboolean                   gossip_contact_list_store_search_equal_func  (GtkTreeModel               *model,
									 gint                        column,
									 const gchar                *key,
									 GtkTreeIter                *iter,
									 gpointer                    search_data);

G_END_DECLS

#endif /* __GOSSIP_CONTACT_LIST_STORE_H__ */

