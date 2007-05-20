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

#include "config.h"

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <libempathy/gossip-debug.h>

#include "gossip-contact-list-store.h"
#include "gossip-contact-groups.h"
#include "gossip-ui-utils.h"

#define DEBUG_DOMAIN "ContactListStore"

/* Active users are those which have recently changed state
 * (e.g. online, offline or from normal to a busy state).
 */

/* Time user is shown as active */
#define ACTIVE_USER_SHOW_TIME 7000

/* Time after connecting which we wait before active users are enabled */
#define ACTIVE_USER_WAIT_TO_ENABLE_TIME 5000

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CONTACT_LIST_STORE, GossipContactListStorePriv))

struct _GossipContactListStorePriv {
	EmpathyContactList         *list;
	gboolean                    show_offline;
	gboolean                    show_avatars;
	gboolean                    is_compact;
	gboolean                    show_active;
	GossipContactListStoreSort  sort_criterium;

	GossipContactGroupsFunc     get_contact_groups;
	gpointer                    get_contact_groups_data;
};

typedef struct {
	GtkTreeIter  iter;
	const gchar *name;
	gboolean     found;
} FindGroup;

typedef struct {
	GossipContact *contact;
	gboolean       found;
	GList         *iters;
} FindContact;

typedef struct {
	GossipContactListStore *store;
	GossipContact          *contact;
	gboolean                remove;
} ShowActiveData;

static void             gossip_contact_list_store_class_init         (GossipContactListStoreClass *klass);
static void             gossip_contact_list_store_init               (GossipContactListStore      *list);
static void             contact_list_store_finalize                  (GObject                     *object);
static void             contact_list_store_get_property              (GObject                     *object,
								      guint                        param_id,
								      GValue                      *value,
								      GParamSpec                  *pspec);
static void             contact_list_store_set_property              (GObject                     *object,
								      guint                        param_id,
								      const GValue                *value,
								      GParamSpec                  *pspec);
static void             contact_list_store_setup                     (GossipContactListStore      *store);
static void             contact_list_store_contact_added_cb          (EmpathyContactList          *list_iface,
								      GossipContact               *contact,
								      GossipContactListStore      *store);
static void             contact_list_store_add_contact               (GossipContactListStore      *store,
								      GossipContact               *contact);
static void             contact_list_store_contact_removed_cb        (EmpathyContactList          *list_iface,
								      GossipContact               *contact,
								      GossipContactListStore      *store);
static void             contact_list_store_remove_contact            (GossipContactListStore      *store,
								      GossipContact               *contact);
static void             contact_list_store_contact_update            (GossipContactListStore      *store,
								      GossipContact               *contact);
static void             contact_list_store_contact_groups_updated_cb (GossipContact               *contact,
								      GParamSpec                  *param,
								      GossipContactListStore      *store);
static void             contact_list_store_contact_updated_cb        (GossipContact               *contact,
								      GParamSpec                  *param,
								      GossipContactListStore      *store);
static void             contact_list_store_contact_set_active        (GossipContactListStore      *store,
								      GossipContact               *contact,
								      gboolean                     active,
								      gboolean                     set_changed);
static ShowActiveData * contact_list_store_contact_active_new        (GossipContactListStore      *store,
								      GossipContact               *contact,
								      gboolean                     remove);
static void             contact_list_store_contact_active_free       (ShowActiveData              *data);
static gboolean         contact_list_store_contact_active_cb         (ShowActiveData              *data);
static gboolean         contact_list_store_get_group_foreach         (GtkTreeModel                *model,
								      GtkTreePath                 *path,
								      GtkTreeIter                 *iter,
								      FindGroup                   *fg);
static void             contact_list_store_get_group                 (GossipContactListStore      *store,
								      const gchar                 *name,
								      GtkTreeIter                 *iter_group_to_set,
								      GtkTreeIter                 *iter_separator_to_set,
								      gboolean                    *created);
static gint             contact_list_store_state_sort_func           (GtkTreeModel                *model,
								      GtkTreeIter                 *iter_a,
								      GtkTreeIter                 *iter_b,
								      gpointer                     user_data);
static gint             contact_list_store_name_sort_func            (GtkTreeModel                *model,
								      GtkTreeIter                 *iter_a,
								      GtkTreeIter                 *iter_b,
								      gpointer                     user_data);
static gboolean         contact_list_store_find_contact_foreach      (GtkTreeModel                *model,
								      GtkTreePath                 *path,
								      GtkTreeIter                 *iter,
								      FindContact                 *fc);
static GList *          contact_list_store_find_contact              (GossipContactListStore      *store,
								      GossipContact               *contact);
static gboolean         contact_list_store_update_list_mode_foreach  (GtkTreeModel                *model,
								      GtkTreePath                 *path,
								      GtkTreeIter                 *iter,
								      GossipContactListStore      *store);

enum {
	PROP_0,
	PROP_SHOW_OFFLINE,
	PROP_SHOW_AVATARS,
	PROP_IS_COMPACT,
	PROP_SORT_CRITERIUM
};

GType
gossip_contact_list_store_sort_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] = {
			{ GOSSIP_CONTACT_LIST_STORE_SORT_NAME, 
			  "GOSSIP_CONTACT_LIST_STORE_SORT_NAME", 
			  "name" },
			{ GOSSIP_CONTACT_LIST_STORE_SORT_STATE, 
			  "GOSSIP_CONTACT_LIST_STORE_SORT_STATE", 
			  "state" },
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("GossipContactListStoreSort", values);
	}

	return etype;
}

G_DEFINE_TYPE (GossipContactListStore, gossip_contact_list_store, GTK_TYPE_TREE_STORE);

static void
gossip_contact_list_store_class_init (GossipContactListStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = contact_list_store_finalize;
	object_class->get_property = contact_list_store_get_property;
	object_class->set_property = contact_list_store_set_property;

	g_object_class_install_property (object_class,
					 PROP_SHOW_OFFLINE,
					 g_param_spec_boolean ("show-offline",
							       "Show Offline",
							       "Whether contact list should display "
							       "offline contacts",
							       FALSE,
							       G_PARAM_READWRITE));
	 g_object_class_install_property (object_class,
					  PROP_SHOW_AVATARS,
					  g_param_spec_boolean ("show-avatars",
								"Show Avatars",
								"Whether contact list should display "
								"avatars for contacts",
								TRUE,
								G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_IS_COMPACT,
					 g_param_spec_boolean ("is-compact",
							       "Is Compact",
							       "Whether the contact list is in compact mode or not",
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_SORT_CRITERIUM,
					 g_param_spec_enum ("sort-criterium",
							    "Sort citerium",
							    "The sort criterium to use for sorting the contact list",
							    GOSSIP_TYPE_CONTACT_LIST_STORE_SORT,
							    GOSSIP_CONTACT_LIST_STORE_SORT_NAME,
							    G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipContactListStorePriv));
}

static void
gossip_contact_list_store_init (GossipContactListStore *store)
{
	GossipContactListStorePriv *priv;

	priv = GET_PRIV (store);

	priv->is_compact = FALSE;
	priv->show_active = TRUE;
	priv->show_avatars = TRUE;
}

static void
contact_list_store_finalize (GObject *object)
{
	GossipContactListStorePriv *priv;

	priv = GET_PRIV (object);

	/* FIXME: disconnect all signals on the list and contacts */

	if (priv->list) {
		g_object_unref (priv->list);
	}

	G_OBJECT_CLASS (gossip_contact_list_store_parent_class)->finalize (object);
}

static void
contact_list_store_get_property (GObject    *object,
				 guint       param_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	GossipContactListStorePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_SHOW_OFFLINE:
		g_value_set_boolean (value, priv->show_offline);
		break;
	case PROP_SHOW_AVATARS:
		g_value_set_boolean (value, priv->show_avatars);
		break;
	case PROP_IS_COMPACT:
		g_value_set_boolean (value, priv->is_compact);
		break;
	case PROP_SORT_CRITERIUM:
		g_value_set_enum (value, priv->sort_criterium);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
contact_list_store_set_property (GObject      *object,
				 guint         param_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	GossipContactListStorePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_SHOW_OFFLINE:
		gossip_contact_list_store_set_show_offline (GOSSIP_CONTACT_LIST_STORE (object),
							    g_value_get_boolean (value));
		break;
	case PROP_SHOW_AVATARS:
		gossip_contact_list_store_set_show_avatars (GOSSIP_CONTACT_LIST_STORE (object),
							    g_value_get_boolean (value));
		break;
	case PROP_IS_COMPACT:
		gossip_contact_list_store_set_is_compact (GOSSIP_CONTACT_LIST_STORE (object),
							  g_value_get_boolean (value));
		break;
	case PROP_SORT_CRITERIUM:
		gossip_contact_list_store_set_sort_criterium (GOSSIP_CONTACT_LIST_STORE (object),
							      g_value_get_enum (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

GossipContactListStore *
gossip_contact_list_store_new (EmpathyContactList *list_iface)
{
	GossipContactListStore     *store;
	GossipContactListStorePriv *priv;
	GList                      *contacts, *l;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list_iface), NULL);

	store = g_object_new (GOSSIP_TYPE_CONTACT_LIST_STORE, NULL);
	priv = GET_PRIV (store);

	contact_list_store_setup (store);
	priv->list = g_object_ref (list_iface);

	/* Signal connection. */
	g_signal_connect (priv->list,
			  "contact-added",
			  G_CALLBACK (contact_list_store_contact_added_cb),
			  store);
	g_signal_connect (priv->list,
			  "contact-removed",
			  G_CALLBACK (contact_list_store_contact_removed_cb),
			  store);

	/* Add contacts already created */
	contacts = empathy_contact_list_get_contacts (priv->list);
	for (l = contacts; l; l = l->next) {
		GossipContact *contact;

		contact = l->data;

		contact_list_store_contact_added_cb (priv->list, contact, store);

		g_object_unref (contact);
	}
	g_list_free (contacts);

	return store;
}

EmpathyContactList *
gossip_contact_list_store_get_list_iface (GossipContactListStore *store)
{
	GossipContactListStorePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST_STORE (store), FALSE);

	priv = GET_PRIV (store);

	return priv->list;
}

gboolean
gossip_contact_list_store_get_show_offline (GossipContactListStore *store)
{
	GossipContactListStorePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST_STORE (store), FALSE);

	priv = GET_PRIV (store);

	return priv->show_offline;
}

void
gossip_contact_list_store_set_show_offline (GossipContactListStore *store,
					    gboolean                show_offline)
{
	GossipContactListStorePriv *priv;
	GList                      *contacts, *l;
	gboolean                    show_active;

	g_return_if_fail (GOSSIP_IS_CONTACT_LIST_STORE (store));

	priv = GET_PRIV (store);

	priv->show_offline = show_offline;
	show_active = priv->show_active;

	/* Disable temporarily. */
	priv->show_active = FALSE;

	contacts = empathy_contact_list_get_contacts (priv->list);
	for (l = contacts; l; l = l->next) {
		GossipContact *contact;

		contact = GOSSIP_CONTACT (l->data);

		contact_list_store_contact_update (store, contact);
		
		g_object_unref (contact);
	}
	g_list_free (contacts);

	/* Restore to original setting. */
	priv->show_active = show_active;
}

gboolean
gossip_contact_list_store_get_show_avatars (GossipContactListStore *store)
{
	GossipContactListStorePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST_STORE (store), TRUE);

	priv = GET_PRIV (store);

	return priv->show_avatars;
}

void
gossip_contact_list_store_set_show_avatars (GossipContactListStore *store,
					    gboolean                show_avatars)
{
	GossipContactListStorePriv *priv;
	GtkTreeModel               *model;

	g_return_if_fail (GOSSIP_IS_CONTACT_LIST_STORE (store));

	priv = GET_PRIV (store);

	priv->show_avatars = show_avatars;

	model = GTK_TREE_MODEL (store);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc)
				contact_list_store_update_list_mode_foreach,
				store);
}

gboolean
gossip_contact_list_store_get_is_compact (GossipContactListStore *store)
{
	GossipContactListStorePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST_STORE (store), TRUE);

	priv = GET_PRIV (store);

	return priv->is_compact;
}

void
gossip_contact_list_store_set_is_compact (GossipContactListStore *store,
					  gboolean                is_compact)
{
	GossipContactListStorePriv *priv;
	GtkTreeModel               *model;

	g_return_if_fail (GOSSIP_IS_CONTACT_LIST_STORE (store));

	priv = GET_PRIV (store);

	priv->is_compact = is_compact;

	model = GTK_TREE_MODEL (store);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc)
				contact_list_store_update_list_mode_foreach,
				store);
}

GossipContactListStoreSort
gossip_contact_list_store_get_sort_criterium (GossipContactListStore *store)
{
	GossipContactListStorePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST_STORE (store), 0);

	priv = GET_PRIV (store);

	return priv->sort_criterium;
}

void
gossip_contact_list_store_set_sort_criterium (GossipContactListStore     *store,
					      GossipContactListStoreSort  sort_criterium)
{
	GossipContactListStorePriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT_LIST_STORE (store));

	priv = GET_PRIV (store);

	priv->sort_criterium = sort_criterium;

	switch (sort_criterium) {
	case GOSSIP_CONTACT_LIST_STORE_SORT_STATE:
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
						      COL_STATUS,
						      GTK_SORT_ASCENDING);
		break;
		
	case GOSSIP_CONTACT_LIST_STORE_SORT_NAME:
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
						      COL_NAME,
						      GTK_SORT_ASCENDING);
		break;
	}
}

gboolean
gossip_contact_list_store_row_separator_func (GtkTreeModel *model,
					      GtkTreeIter  *iter,
					      gpointer      data)
{
	gboolean is_separator = FALSE;

	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);

	gtk_tree_model_get (model, iter,
			    COL_IS_SEPARATOR, &is_separator,
			    -1);

	return is_separator;
}

gchar *
gossip_contact_list_store_get_parent_group (GtkTreeModel *model,
					    GtkTreePath  *path,
					    gboolean     *path_is_group)
{
	GtkTreeIter  parent_iter, iter;
	gchar       *name = NULL;
	gboolean     is_group;

	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);

	if (path_is_group) {
		*path_is_group = FALSE;
	}

	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter,
			    COL_IS_GROUP, &is_group,
			    COL_NAME, &name,
			    -1);

	if (!is_group) {
		g_free (name);
		name = NULL;

		if (!gtk_tree_model_iter_parent (model, &parent_iter, &iter)) {
			return NULL;
		}

		iter = parent_iter;

		gtk_tree_model_get (model, &iter,
				    COL_IS_GROUP, &is_group,
				    COL_NAME, &name,
				    -1);
		if (!is_group) {
			g_free (name);
			return NULL;
		}
	}

	if (path_is_group) {
		*path_is_group = TRUE;
	}

	return name;
}

gboolean
gossip_contact_list_store_search_equal_func (GtkTreeModel *model,
					     gint          column,
					     const gchar  *key,
					     GtkTreeIter  *iter,
					     gpointer      search_data)
{
	gchar    *name, *name_folded;
	gchar    *key_folded;
	gboolean  ret;

	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);

	if (!key) {
		return FALSE;
	}

	gtk_tree_model_get (model, iter, COL_NAME, &name, -1);

	if (!name) {
		return FALSE;
	}

	name_folded = g_utf8_casefold (name, -1);
	key_folded = g_utf8_casefold (key, -1);

	if (name_folded && key_folded && 
	    strstr (name_folded, key_folded)) {
		ret = FALSE;
	} else {
		ret = TRUE;
	}

	g_free (name);
	g_free (name_folded);
	g_free (key_folded);

	return ret;
}

void
gossip_contact_list_store_set_contact_groups_func (GossipContactListStore  *store,
						   GossipContactGroupsFunc  func,
						   gpointer                 user_data)
{
	GossipContactListStorePriv *priv;
	GList                      *contacts, *l;

	g_return_if_fail (GOSSIP_IS_CONTACT_LIST_STORE (store));

	priv = GET_PRIV (store);

	if (func) {
		priv->get_contact_groups = func;
		priv->get_contact_groups_data = user_data;
	} else {
		priv->get_contact_groups = NULL;
		priv->get_contact_groups_data = NULL;
	}

	/* If we set a custom function to get contacts groups  we have to
	 * disconnect our default notify::groups signal and wait for the user
	 * to call himself gossip_contact_list_store_update_contact_groups ()
	 * when needed. If func is NULL we come back to default.
	 */
	contacts = empathy_contact_list_get_contacts (priv->list);
	for (l = contacts; l; l = l->next) {
		GossipContact *contact;

		contact = l->data;

		if (func) {
			g_signal_handlers_disconnect_by_func (contact, 
							      G_CALLBACK (contact_list_store_contact_groups_updated_cb),
							      store);
		} else {
			g_signal_connect (contact, "notify::groups",
					  G_CALLBACK (contact_list_store_contact_groups_updated_cb),
					  store);
		}

		gossip_contact_list_store_update_contact_groups (store, contact);

		g_object_unref (contact);
	}
	g_list_free (contacts);
}

void
gossip_contact_list_store_update_contact_groups (GossipContactListStore *store,
						 GossipContact          *contact)
{
	gossip_debug (DEBUG_DOMAIN, "Contact:'%s' updating groups",
		      gossip_contact_get_name (contact));

	/* We do this to make sure the groups are correct, if not, we
	 * would have to check the groups already set up for each
	 * contact and then see what has been updated.
	 */
	contact_list_store_remove_contact (store, contact);
	contact_list_store_add_contact (store, contact);
}

static void
contact_list_store_setup (GossipContactListStore *store)
{
	GossipContactListStorePriv *priv;
	GType                       types[] = {G_TYPE_STRING,       /* Status icon-name */
					       GDK_TYPE_PIXBUF,     /* Avatar pixbuf */
					       G_TYPE_BOOLEAN,      /* Avatar pixbuf visible */
					       G_TYPE_STRING,       /* Name */
					       G_TYPE_STRING,       /* Status string */
					       G_TYPE_BOOLEAN,      /* Show status */
					       GOSSIP_TYPE_CONTACT, /* Contact type */
					       G_TYPE_BOOLEAN,      /* Is group */
					       G_TYPE_BOOLEAN,      /* Is active */
					       G_TYPE_BOOLEAN,      /* Is online */
					       G_TYPE_BOOLEAN};     /* Is separator */
	
	priv = GET_PRIV (store);

	gtk_tree_store_set_column_types (GTK_TREE_STORE (store), COL_COUNT, types);

	/* Set up sorting */
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store),
					 COL_NAME,
					 contact_list_store_name_sort_func,
					 store, NULL);
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store),
					 COL_STATUS,
					 contact_list_store_state_sort_func,
					 store, NULL);

	gossip_contact_list_store_set_sort_criterium (store, priv->sort_criterium);
}

static void
contact_list_store_contact_added_cb (EmpathyContactList     *list_iface,
				     GossipContact          *contact,
				     GossipContactListStore *store)
{
	GossipContactListStorePriv *priv;

	priv = GET_PRIV (store);

	gossip_debug (DEBUG_DOMAIN, 
		      "Contact:'%s' added",
		      gossip_contact_get_name (contact));

	if (!priv->get_contact_groups) {
		g_signal_connect (contact, "notify::groups",
				  G_CALLBACK (contact_list_store_contact_groups_updated_cb),
				  store);
	}
	g_signal_connect (contact, "notify::presence",
			  G_CALLBACK (contact_list_store_contact_updated_cb),
			  store);
	g_signal_connect (contact, "notify::name",
			  G_CALLBACK (contact_list_store_contact_updated_cb),
			  store);
	g_signal_connect (contact, "notify::avatar",
			  G_CALLBACK (contact_list_store_contact_updated_cb),
			  store);
	g_signal_connect (contact, "notify::type",
			  G_CALLBACK (contact_list_store_contact_updated_cb),
			  store);

	contact_list_store_add_contact (store, contact);
}

static void
contact_list_store_add_contact (GossipContactListStore *store,
				GossipContact          *contact)
{
	GossipContactListStorePriv *priv;
	GtkTreeIter                 iter;
	GList                      *groups, *l;

	priv = GET_PRIV (store);
	
	if (!priv->show_offline && !gossip_contact_is_online (contact)) {
		return;
	}

	/* If no groups just add it at the top level. */
	if (priv->get_contact_groups) {
		groups = priv->get_contact_groups (contact,
						   priv->get_contact_groups_data);
	} else {
		groups = gossip_contact_get_groups (contact);
	}

	if (!groups) {
		gtk_tree_store_append (GTK_TREE_STORE (store), &iter, NULL);
		gtk_tree_store_set (GTK_TREE_STORE (store), &iter,
				    COL_CONTACT, contact,
				    -1);
	}

	/* Else add to each group. */
	for (l = groups; l; l = l->next) {
		GtkTreeIter  iter_group;
		const gchar *name;

		name = l->data;
		if (!name) {
			continue;
		}

		contact_list_store_get_group (store, name, &iter_group, NULL, NULL);

		gtk_tree_store_insert_after (GTK_TREE_STORE (store), &iter,
					     &iter_group, NULL);
		gtk_tree_store_set (GTK_TREE_STORE (store), &iter,
				    COL_CONTACT, contact,
				    -1);
	}

	contact_list_store_contact_update (store, contact);
}

static void
contact_list_store_contact_removed_cb (EmpathyContactList     *list_iface,
				       GossipContact          *contact,
				       GossipContactListStore *store)
{
	gossip_debug (DEBUG_DOMAIN, "Contact:'%s' removed",
		      gossip_contact_get_name (contact));

	/* Disconnect signals */
	g_signal_handlers_disconnect_by_func (contact, 
					      G_CALLBACK (contact_list_store_contact_groups_updated_cb),
					      store);
	g_signal_handlers_disconnect_by_func (contact,
					      G_CALLBACK (contact_list_store_contact_updated_cb),
					      store);

	contact_list_store_remove_contact (store, contact);
}

static void
contact_list_store_remove_contact (GossipContactListStore *store,
				   GossipContact          *contact)
{
	GossipContactListStorePriv *priv;
	GtkTreeModel               *model;
	GList                      *iters, *l;

	priv = GET_PRIV (store);

	iters = contact_list_store_find_contact (store, contact);
	if (!iters) {
		return;
	}
	
	/* Clean up model */
	model = GTK_TREE_MODEL (store);

	for (l = iters; l; l = l->next) {
		GtkTreeIter parent;

		/* NOTE: it is only <= 2 here because we have
		 * separators after the group name, otherwise it
		 * should be 1. 
		 */
		if (gtk_tree_model_iter_parent (model, &parent, l->data) &&
		    gtk_tree_model_iter_n_children (model, &parent) <= 2) {
			gtk_tree_store_remove (GTK_TREE_STORE (store), &parent);
		} else {
			gtk_tree_store_remove (GTK_TREE_STORE (store), l->data);
		}
	}

	g_list_foreach (iters, (GFunc) gtk_tree_iter_free, NULL);
	g_list_free (iters);
}

static void
contact_list_store_contact_update (GossipContactListStore *store,
				   GossipContact          *contact)
{
	GossipContactListStorePriv *priv;
	ShowActiveData             *data;
	GtkTreeModel               *model;
	GList                      *iters, *l;
	gboolean                    in_list;
	gboolean                    should_be_in_list;
	gboolean                    was_online = TRUE;
	gboolean                    now_online = FALSE;
	gboolean                    set_model = FALSE;
	gboolean                    do_remove = FALSE;
	gboolean                    do_set_active = FALSE;
	gboolean                    do_set_refresh = FALSE;
	GdkPixbuf                  *pixbuf_avatar;

	priv = GET_PRIV (store);

	model = GTK_TREE_MODEL (store);

	iters = contact_list_store_find_contact (store, contact);
	if (!iters) {
		in_list = FALSE;
	} else {
		in_list = TRUE;
	}

	/* Get online state now. */
	now_online = gossip_contact_is_online (contact);

	if (priv->show_offline || now_online) {
		should_be_in_list = TRUE;
	} else {
		should_be_in_list = FALSE;
	}

	if (!in_list && !should_be_in_list) {
		/* Nothing to do. */
		gossip_debug (DEBUG_DOMAIN,
			      "Contact:'%s' in list:NO, should be:NO",
			      gossip_contact_get_name (contact));

		g_list_foreach (iters, (GFunc) gtk_tree_iter_free, NULL);
		g_list_free (iters);
		return;
	}
	else if (in_list && !should_be_in_list) {
		gossip_debug (DEBUG_DOMAIN,
			      "Contact:'%s' in list:YES, should be:NO",
			      gossip_contact_get_name (contact));

		if (priv->show_active) {
			do_remove = TRUE;
			do_set_active = TRUE;
			do_set_refresh = TRUE;

			set_model = TRUE;
			gossip_debug (DEBUG_DOMAIN, "Remove item (after timeout)");
		} else {
			gossip_debug (DEBUG_DOMAIN, "Remove item (now)!");
			contact_list_store_remove_contact (store, contact);
		}
	}
	else if (!in_list && should_be_in_list) {
		gossip_debug (DEBUG_DOMAIN,
			      "Contact:'%s' in list:NO, should be:YES",
			      gossip_contact_get_name (contact));

		contact_list_store_add_contact (store, contact);

		if (priv->show_active) {
			do_set_active = TRUE;

			gossip_debug (DEBUG_DOMAIN, "Set active (contact added)");
		}
	} else {
		gossip_debug (DEBUG_DOMAIN,
			      "Contact:'%s' in list:YES, should be:YES",
			      gossip_contact_get_name (contact));

		/* Get online state before. */
		if (iters && g_list_length (iters) > 0) {
			gtk_tree_model_get (model, iters->data,
					    COL_IS_ONLINE, &was_online,
					    -1);
		}

		/* Is this really an update or an online/offline. */
		if (priv->show_active) {
			if (was_online != now_online) {
				do_set_active = TRUE;
				do_set_refresh = TRUE;

				gossip_debug (DEBUG_DOMAIN, "Set active (contact updated %s)",
					      was_online ? "online  -> offline" :
							   "offline -> online");
			} else {
				/* Was TRUE for presence updates. */
				/* do_set_active = FALSE;  */
				do_set_refresh = TRUE;

				gossip_debug (DEBUG_DOMAIN, "Set active (contact updated)");
			}
		}

		set_model = TRUE;
	}

	pixbuf_avatar = gossip_pixbuf_avatar_from_contact_scaled (contact, 32, 32);
	for (l = iters; l && set_model; l = l->next) {
		gtk_tree_store_set (GTK_TREE_STORE (store), l->data,
				    COL_ICON_STATUS, gossip_icon_name_for_contact (contact),
				    COL_PIXBUF_AVATAR, pixbuf_avatar,
				    COL_PIXBUF_AVATAR_VISIBLE, priv->show_avatars,
				    COL_NAME, gossip_contact_get_name (contact),
				    COL_STATUS, gossip_contact_get_status (contact),
				    COL_STATUS_VISIBLE, !priv->is_compact,
				    COL_IS_GROUP, FALSE,
				    COL_IS_ONLINE, now_online,
				    COL_IS_SEPARATOR, FALSE,
				    -1);
	}

	if (pixbuf_avatar) {
		g_object_unref (pixbuf_avatar);
	}

	if (priv->show_active && do_set_active) {
		contact_list_store_contact_set_active (store, contact, do_set_active, do_set_refresh);

		if (do_set_active) {
			data = contact_list_store_contact_active_new (store, contact, do_remove);
			g_timeout_add (ACTIVE_USER_SHOW_TIME,
				       (GSourceFunc) contact_list_store_contact_active_cb,
				       data);
		}
	}

	/* FIXME: when someone goes online then offline quickly, the
	 * first timeout sets the user to be inactive and the second
	 * timeout removes the user from the contact list, really we
	 * should remove the first timeout.
	 */
	g_list_foreach (iters, (GFunc) gtk_tree_iter_free, NULL);
	g_list_free (iters);
}

static void
contact_list_store_contact_groups_updated_cb (GossipContact          *contact,
					      GParamSpec             *param,
					      GossipContactListStore *store)
{
	gossip_contact_list_store_update_contact_groups (store, contact);
}

static void
contact_list_store_contact_updated_cb (GossipContact          *contact,
				       GParamSpec             *param,
				       GossipContactListStore *store)
{
	gossip_debug (DEBUG_DOMAIN,
		      "Contact:'%s' updated, checking roster is in sync...",
		      gossip_contact_get_name (contact));

	contact_list_store_contact_update (store, contact);
}

static void
contact_list_store_contact_set_active (GossipContactListStore *store,
				       GossipContact          *contact,
				       gboolean                active,
				       gboolean                set_changed)
{
	GossipContactListStorePriv *priv;
	GtkTreeModel               *model;
	GList                      *iters, *l;

	priv = GET_PRIV (store);
	model = GTK_TREE_MODEL (store);

	iters = contact_list_store_find_contact (store, contact);
	for (l = iters; l; l = l->next) {
		GtkTreePath *path;

		gtk_tree_store_set (GTK_TREE_STORE (store), l->data,
				    COL_IS_ACTIVE, active,
				    -1);

		gossip_debug (DEBUG_DOMAIN, "Set item %s", active ? "active" : "inactive");

		if (set_changed) {
			path = gtk_tree_model_get_path (model, l->data);
			gtk_tree_model_row_changed (model, path, l->data);
			gtk_tree_path_free (path);
		}
	}

	g_list_foreach (iters, (GFunc) gtk_tree_iter_free, NULL);
	g_list_free (iters);

}

static ShowActiveData *
contact_list_store_contact_active_new (GossipContactListStore *store,
				       GossipContact          *contact,
				       gboolean                remove)
{
	ShowActiveData *data;

	gossip_debug (DEBUG_DOMAIN, 
		      "Contact:'%s' now active, and %s be removed",
		      gossip_contact_get_name (contact), 
		      remove ? "WILL" : "WILL NOT");
	
	data = g_slice_new0 (ShowActiveData);

	data->store = g_object_ref (store);
	data->contact = g_object_ref (contact);
	data->remove = remove;

	return data;
}

static void
contact_list_store_contact_active_free (ShowActiveData *data)
{
	g_object_unref (data->contact);
	g_object_unref (data->store);

	g_slice_free (ShowActiveData, data);
}

static gboolean
contact_list_store_contact_active_cb (ShowActiveData *data)
{
	GossipContactListStorePriv *priv;

	priv = GET_PRIV (data->store);

	if (data->remove &&
	    !priv->show_offline &&
	    !gossip_contact_is_online (data->contact)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Contact:'%s' active timeout, removing item",
			      gossip_contact_get_name (data->contact));
		contact_list_store_remove_contact (data->store, data->contact);
	}

	gossip_debug (DEBUG_DOMAIN, 
		      "Contact:'%s' no longer active",
		      gossip_contact_get_name (data->contact));

	contact_list_store_contact_set_active (data->store,
					       data->contact,
					       FALSE,
					       TRUE);

	contact_list_store_contact_active_free (data);

	return FALSE;
}

static gboolean
contact_list_store_get_group_foreach (GtkTreeModel *model,
				      GtkTreePath  *path,
				      GtkTreeIter  *iter,
				      FindGroup    *fg)
{
	gchar    *str;
	gboolean  is_group;

	/* Groups are only at the top level. */
	if (gtk_tree_path_get_depth (path) != 1) {
		return FALSE;
	}

	gtk_tree_model_get (model, iter,
			    COL_NAME, &str,
			    COL_IS_GROUP, &is_group,
			    -1);

	if (is_group && strcmp (str, fg->name) == 0) {
		fg->found = TRUE;
		fg->iter = *iter;
	}

	g_free (str);

	return fg->found;
}

static void
contact_list_store_get_group (GossipContactListStore *store,
			      const gchar       *name,
			      GtkTreeIter       *iter_group_to_set,
			      GtkTreeIter       *iter_separator_to_set,
			      gboolean          *created)
{
	GossipContactListStorePriv *priv;
	GtkTreeModel               *model;
	GtkTreeIter                 iter_group;
	GtkTreeIter                 iter_separator;
	FindGroup                   fg;

	priv = GET_PRIV (store);

	memset (&fg, 0, sizeof (fg));

	fg.name = name;

	model = GTK_TREE_MODEL (store);
	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) contact_list_store_get_group_foreach,
				&fg);

	if (!fg.found) {
		if (created) {
			*created = TRUE;
		}

		gtk_tree_store_append (GTK_TREE_STORE (store), &iter_group, NULL);
		gtk_tree_store_set (GTK_TREE_STORE (store), &iter_group,
				    COL_ICON_STATUS, NULL,
				    COL_NAME, name,
				    COL_IS_GROUP, TRUE,
				    COL_IS_ACTIVE, FALSE,
				    COL_IS_SEPARATOR, FALSE,
				    -1);

		if (iter_group_to_set) {
			*iter_group_to_set = iter_group;
		}

		gtk_tree_store_append (GTK_TREE_STORE (store),
				       &iter_separator, 
				       &iter_group);
		gtk_tree_store_set (GTK_TREE_STORE (store), &iter_separator,
				    COL_IS_SEPARATOR, TRUE,
				    -1);

		if (iter_separator_to_set) {
			*iter_separator_to_set = iter_separator;
		}
	} else {
		if (created) {
			*created = FALSE;
		}

		if (iter_group_to_set) {
			*iter_group_to_set = fg.iter;
		}

		iter_separator = fg.iter;

		if (gtk_tree_model_iter_next (model, &iter_separator)) {
			gboolean is_separator;

			gtk_tree_model_get (model, &iter_separator,
					    COL_IS_SEPARATOR, &is_separator,
					    -1);

			if (is_separator && iter_separator_to_set) {
				*iter_separator_to_set = iter_separator;
			}
		}
	}
}

static gint
contact_list_store_state_sort_func (GtkTreeModel *model,
				    GtkTreeIter  *iter_a,
				    GtkTreeIter  *iter_b,
				    gpointer      user_data)
{
	gint            ret_val = 0;
	gchar          *name_a, *name_b;
	gboolean        is_separator_a, is_separator_b;
	GossipContact  *contact_a, *contact_b;
	GossipPresence *presence_a, *presence_b;
	McPresence      state_a, state_b;

	gtk_tree_model_get (model, iter_a,
			    COL_NAME, &name_a,
			    COL_CONTACT, &contact_a,
			    COL_IS_SEPARATOR, &is_separator_a,
			    -1);
	gtk_tree_model_get (model, iter_b,
			    COL_NAME, &name_b,
			    COL_CONTACT, &contact_b,
			    COL_IS_SEPARATOR, &is_separator_b,
			    -1);

	/* Separator or group? */
	if (is_separator_a || is_separator_b) {
		if (is_separator_a) {
			ret_val = -1;
		} else if (is_separator_b) {
			ret_val = 1;
		}
	} else if (!contact_a && contact_b) {
		ret_val = 1;
	} else if (contact_a && !contact_b) {
		ret_val = -1;
	} else if (!contact_a && !contact_b) {
		/* Handle groups */
		ret_val = g_utf8_collate (name_a, name_b);
	}

	if (ret_val) {
		goto free_and_out;
	}

	/* If we managed to get this far, we can start looking at
	 * the presences.
	 */
	presence_a = gossip_contact_get_presence (GOSSIP_CONTACT (contact_a));
	presence_b = gossip_contact_get_presence (GOSSIP_CONTACT (contact_b));

	if (!presence_a && presence_b) {
		ret_val = 1;
	} else if (presence_a && !presence_b) {
		ret_val = -1;
	} else if (!presence_a && !presence_b) {
		/* Both offline, sort by name */
		ret_val = g_utf8_collate (name_a, name_b);
	} else {
		state_a = gossip_presence_get_state (presence_a);
		state_b = gossip_presence_get_state (presence_b);

		if (state_a < state_b) {
			ret_val = -1;
		} else if (state_a > state_b) {
			ret_val = 1;
		} else {
			/* Fallback: compare by name */
			ret_val = g_utf8_collate (name_a, name_b);
		}
	}

free_and_out:
	g_free (name_a);
	g_free (name_b);

	if (contact_a) {
		g_object_unref (contact_a);
	}

	if (contact_b) {
		g_object_unref (contact_b);
	}

	return ret_val;
}

static gint
contact_list_store_name_sort_func (GtkTreeModel *model,
				   GtkTreeIter  *iter_a,
				   GtkTreeIter  *iter_b,
				   gpointer      user_data)
{
	gchar         *name_a, *name_b;
	GossipContact *contact_a, *contact_b;
	gboolean       is_separator_a, is_separator_b;
	gint           ret_val;

	gtk_tree_model_get (model, iter_a,
			    COL_NAME, &name_a,
			    COL_CONTACT, &contact_a,
			    COL_IS_SEPARATOR, &is_separator_a,
			    -1);
	gtk_tree_model_get (model, iter_b,
			    COL_NAME, &name_b,
			    COL_CONTACT, &contact_b,
			    COL_IS_SEPARATOR, &is_separator_b,
			    -1);

	/* If contact is NULL it means it's a group. */

	if (is_separator_a || is_separator_b) {
		if (is_separator_a) {
			ret_val = -1;
		} else if (is_separator_b) {
			ret_val = 1;
		}
	} else if (!contact_a && contact_b) {
		ret_val = 1;
	} else if (contact_a && !contact_b) {
		ret_val = -1;
	} else {
		ret_val = g_utf8_collate (name_a, name_b);
	}

	g_free (name_a);
	g_free (name_b);

	if (contact_a) {
		g_object_unref (contact_a);
	}

	if (contact_b) {
		g_object_unref (contact_b);
	}

	return ret_val;
}

static gboolean
contact_list_store_find_contact_foreach (GtkTreeModel *model,
					 GtkTreePath  *path,
					 GtkTreeIter  *iter,
					 FindContact  *fc)
{
	GossipContact *contact;

	gtk_tree_model_get (model, iter,
			    COL_CONTACT, &contact,
			    -1);

	if (!contact) {
		return FALSE;
	}

	if (gossip_contact_equal (contact, fc->contact)) {
		fc->found = TRUE;
		fc->iters = g_list_append (fc->iters, gtk_tree_iter_copy (iter));
	}
	g_object_unref (contact);

	return FALSE;
}

static GList *
contact_list_store_find_contact (GossipContactListStore *store,
				 GossipContact          *contact)
{
	GossipContactListStorePriv *priv;
	GtkTreeModel              *model;
	GList                     *l = NULL;
	FindContact                fc;

	priv = GET_PRIV (store);

	memset (&fc, 0, sizeof (fc));

	fc.contact = contact;

	model = GTK_TREE_MODEL (store);
	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) contact_list_store_find_contact_foreach,
				&fc);

	if (fc.found) {
		l = fc.iters;
	}

	return l;
}

static gboolean
contact_list_store_update_list_mode_foreach (GtkTreeModel           *model,
					     GtkTreePath            *path,
					     GtkTreeIter            *iter,
					     GossipContactListStore *store)
{
	GossipContactListStorePriv *priv;
	gboolean                    show_avatar = FALSE;

	priv = GET_PRIV (store);

	if (priv->show_avatars && !priv->is_compact) {
		show_avatar = TRUE;
	}

	gtk_tree_store_set (GTK_TREE_STORE (store), iter,
			    COL_PIXBUF_AVATAR_VISIBLE, show_avatar,
			    COL_STATUS_VISIBLE, !priv->is_compact,
			    -1);

	return FALSE;
}

