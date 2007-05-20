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

#ifndef __GOSSIP_CONTACT_LIST_VIEW_H__
#define __GOSSIP_CONTACT_LIST_VIEW_H__

#include <gtk/gtktreeview.h>

#include <libempathy/gossip-contact.h>

#include "gossip-contact-list-store.h"

G_BEGIN_DECLS

#define GOSSIP_TYPE_CONTACT_LIST_VIEW         (gossip_contact_list_view_get_type ())
#define GOSSIP_CONTACT_LIST_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CONTACT_LIST_VIEW, GossipContactListView))
#define GOSSIP_CONTACT_LIST_VIEW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_CONTACT_LIST_VIEW, GossipContactListViewClass))
#define GOSSIP_IS_CONTACT_LIST_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CONTACT_LIST_VIEW))
#define GOSSIP_IS_CONTACT_LIST_VIEW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CONTACT_LIST_VIEW))
#define GOSSIP_CONTACT_LIST_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CONTACT_LIST_VIEW, GossipContactListViewClass))

typedef struct _GossipContactListView      GossipContactListView;
typedef struct _GossipContactListViewClass GossipContactListViewClass;
typedef struct _GossipContactListViewPriv  GossipContactListViewPriv;

struct _GossipContactListView {
	GtkTreeView            parent;
};

struct _GossipContactListViewClass {
	GtkTreeViewClass       parent_class;
};

typedef void           (*GossipContactListViewDragReceivedFunc)        (GossipContact *contact,
									GdkDragAction  action,
									const gchar   *old_group,
									const gchar   *new_group,
									gpointer       user_data);

GType                  gossip_contact_list_view_get_type               (void) G_GNUC_CONST;
GossipContactListView *gossip_contact_list_view_new                    (GossipContactListStore                *store);
GossipContact *        gossip_contact_list_view_get_selected           (GossipContactListView                 *view);
gchar *                gossip_contact_list_view_get_selected_group     (GossipContactListView                 *view);
GtkWidget *            gossip_contact_list_view_get_contact_menu       (GossipContactListView                 *view,
									GossipContact                         *contact);
GtkWidget *            gossip_contact_list_view_get_group_menu         (GossipContactListView                 *view);
void                   gossip_contact_list_view_set_filter             (GossipContactListView                 *view,
									const gchar                           *filter);
void                   gossip_contact_list_view_set_drag_received_func (GossipContactListView                 *view,
									GossipContactListViewDragReceivedFunc  func,
									gpointer                               user_data);

G_END_DECLS

#endif /* __GOSSIP_CONTACT_LIST_VIEW_H__ */

