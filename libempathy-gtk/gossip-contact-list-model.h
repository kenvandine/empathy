/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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
 */

#ifndef __GOSSIP_CONTACT_LIST_H__
#define __GOSSIP_CONTACT_LIST_H__

#include <gtk/gtktreeview.h>

#include <libempathy/gossip-contact.h>

G_BEGIN_DECLS

/*
 * GossipContactListSort
 */ 
#define GOSSIP_TYPE_CONTACT_LIST_SORT    (gossip_contact_list_sort_get_type ())

typedef enum {
	GOSSIP_CONTACT_LIST_SORT_STATE,
	GOSSIP_CONTACT_LIST_SORT_NAME
} GossipContactListSort;

GType gossip_contact_list_sort_get_type (void) G_GNUC_CONST;

/*
 * GossipContactList 
 */ 
#define GOSSIP_TYPE_CONTACT_LIST         (gossip_contact_list_get_type ())
#define GOSSIP_CONTACT_LIST(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_CONTACT_LIST, GossipContactList))
#define GOSSIP_CONTACT_LIST_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_CONTACT_LIST, GossipContactListClass))
#define GOSSIP_IS_CONTACT_LIST(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_CONTACT_LIST))
#define GOSSIP_IS_CONTACT_LIST_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_CONTACT_LIST))
#define GOSSIP_CONTACT_LIST_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_CONTACT_LIST, GossipContactListClass))

typedef struct _GossipContactList      GossipContactList;
typedef struct _GossipContactListClass GossipContactListClass;
typedef struct _GossipContactListPriv  GossipContactListPriv;

struct _GossipContactList {
	GtkTreeView            parent;
};

struct _GossipContactListClass {
	GtkTreeViewClass       parent_class;
};

GType                 gossip_contact_list_get_type           (void) G_GNUC_CONST;
GossipContactList *   gossip_contact_list_new                (void);
GossipContact *       gossip_contact_list_get_selected       (GossipContactList     *list);
gchar *               gossip_contact_list_get_selected_group (GossipContactList     *list);
gboolean              gossip_contact_list_get_show_offline   (GossipContactList     *list);
gboolean              gossip_contact_list_get_show_avatars   (GossipContactList     *list);
gboolean              gossip_contact_list_get_is_compact     (GossipContactList     *list);
GossipContactListSort gossip_contact_list_get_sort_criterium (GossipContactList     *list);
GtkWidget *           gossip_contact_list_get_contact_menu   (GossipContactList     *list,
							      GossipContact         *contact);
GtkWidget *           gossip_contact_list_get_group_menu     (GossipContactList     *list);
void                  gossip_contact_list_set_show_offline   (GossipContactList     *list,
							      gboolean               show_offline);
void                  gossip_contact_list_set_show_avatars   (GossipContactList     *list,
							      gboolean               show_avatars);
void                  gossip_contact_list_set_is_compact     (GossipContactList     *list,
							      gboolean               is_compact);
void                  gossip_contact_list_set_sort_criterium (GossipContactList     *list,
							      GossipContactListSort  sort_criterium);
void                  gossip_contact_list_set_filter         (GossipContactList     *list,
							      const gchar           *filter);

G_END_DECLS

#endif /* __GOSSIP_CONTACT_LIST_H__ */

