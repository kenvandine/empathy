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

#include "config.h"

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <telepathy-glib/util.h>

#include <libempathy/empathy-utils.h>
#include "empathy-contact-list-store.h"
#include "empathy-ui-utils.h"
#include "empathy-gtk-enum-types.h"

#define DEBUG_FLAG EMPATHY_DEBUG_CONTACT
#include <libempathy/empathy-debug.h>

/* Active users are those which have recently changed state
 * (e.g. online, offline or from normal to a busy state).
 */

/* Time in seconds user is shown as active */
#define ACTIVE_USER_SHOW_TIME 7

/* Time in seconds after connecting which we wait before active users are enabled */
#define ACTIVE_USER_WAIT_TO_ENABLE_TIME 5

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyContactListStore)
typedef struct {
	EmpathyContactList         *list;
	gboolean                    show_offline;
	gboolean                    show_avatars;
	gboolean                    show_groups;
	gboolean                    is_compact;
	gboolean                    show_active;
	EmpathyContactListStoreSort sort_criterium;
	guint                       inhibit_active;
} EmpathyContactListStorePriv;

typedef struct {
	GtkTreeIter  iter;
	const gchar *name;
	gboolean     found;
} FindGroup;

typedef struct {
	EmpathyContact *contact;
	gboolean       found;
	GList         *iters;
} FindContact;

typedef struct {
	EmpathyContactListStore *store;
	EmpathyContact          *contact;
	gboolean                remove;
} ShowActiveData;

static void             contact_list_store_finalize                  (GObject                       *object);
static void             contact_list_store_get_property              (GObject                       *object,
								      guint                          param_id,
								      GValue                        *value,
								      GParamSpec                    *pspec);
static void             contact_list_store_set_property              (GObject                       *object,
								      guint                          param_id,
								      const GValue                  *value,
								      GParamSpec                    *pspec);
static void             contact_list_store_setup                     (EmpathyContactListStore       *store);
static gboolean         contact_list_store_inibit_active_cb          (EmpathyContactListStore       *store);
static void             contact_list_store_members_changed_cb        (EmpathyContactList            *list_iface,
								      EmpathyContact                *contact,
								      EmpathyContact                *actor,
								      guint                          reason,
								      gchar                         *message,
								      gboolean                       is_member,
								      EmpathyContactListStore       *store);
static void             contact_list_store_groups_changed_cb         (EmpathyContactList            *list_iface,
								      EmpathyContact                *contact,
								      gchar                         *group,
								      gboolean                       is_member,
								      EmpathyContactListStore       *store);
static void             contact_list_store_add_contact               (EmpathyContactListStore       *store,
								      EmpathyContact                *contact);
static void             contact_list_store_remove_contact            (EmpathyContactListStore       *store,
								      EmpathyContact                *contact);
static void             contact_list_store_contact_update            (EmpathyContactListStore       *store,
								      EmpathyContact                *contact);
static void             contact_list_store_contact_updated_cb        (EmpathyContact                *contact,
								      GParamSpec                    *param,
								      EmpathyContactListStore       *store);
static void             contact_list_store_contact_set_active        (EmpathyContactListStore       *store,
								      EmpathyContact                *contact,
								      gboolean                       active,
								      gboolean                       set_changed);
static ShowActiveData * contact_list_store_contact_active_new        (EmpathyContactListStore       *store,
								      EmpathyContact                *contact,
								      gboolean                       remove);
static void             contact_list_store_contact_active_free       (ShowActiveData                *data);
static gboolean         contact_list_store_contact_active_cb         (ShowActiveData                *data);
static gboolean         contact_list_store_get_group_foreach         (GtkTreeModel                  *model,
								      GtkTreePath                   *path,
								      GtkTreeIter                   *iter,
								      FindGroup                     *fg);
static void             contact_list_store_get_group                 (EmpathyContactListStore       *store,
								      const gchar                   *name,
								      GtkTreeIter                   *iter_group_to_set,
								      GtkTreeIter                   *iter_separator_to_set,
								      gboolean                      *created);
static gint             contact_list_store_state_sort_func           (GtkTreeModel                  *model,
								      GtkTreeIter                   *iter_a,
								      GtkTreeIter                   *iter_b,
								      gpointer                       user_data);
static gint             contact_list_store_name_sort_func            (GtkTreeModel                  *model,
								      GtkTreeIter                   *iter_a,
								      GtkTreeIter                   *iter_b,
								      gpointer                       user_data);
static gboolean         contact_list_store_find_contact_foreach      (GtkTreeModel                  *model,
								      GtkTreePath                   *path,
								      GtkTreeIter                   *iter,
								      FindContact                   *fc);
static GList *          contact_list_store_find_contact              (EmpathyContactListStore       *store,
								      EmpathyContact                *contact);
static gboolean         contact_list_store_update_list_mode_foreach  (GtkTreeModel                  *model,
								      GtkTreePath                   *path,
								      GtkTreeIter                   *iter,
								      EmpathyContactListStore       *store);

enum {
	PROP_0,
	PROP_CONTACT_LIST,
	PROP_SHOW_OFFLINE,
	PROP_SHOW_AVATARS,
	PROP_SHOW_GROUPS,
	PROP_IS_COMPACT,
	PROP_SORT_CRITERIUM
};

G_DEFINE_TYPE (EmpathyContactListStore, empathy_contact_list_store, GTK_TYPE_TREE_STORE);

static gboolean
contact_list_store_iface_setup (gpointer user_data)
{
	EmpathyContactListStore     *store = user_data;
	EmpathyContactListStorePriv *priv = GET_PRIV (store);
	GList                       *contacts, *l;

	/* Signal connection. */
	g_signal_connect (priv->list,
			  "members-changed",
			  G_CALLBACK (contact_list_store_members_changed_cb),
			  store);
	g_signal_connect (priv->list,
			  "groups-changed",
			  G_CALLBACK (contact_list_store_groups_changed_cb),
			  store);

	/* Add contacts already created. */
	contacts = empathy_contact_list_get_members (priv->list);
	for (l = contacts; l; l = l->next) {
		contact_list_store_members_changed_cb (priv->list, l->data,
						       NULL, 0, NULL,
						       TRUE,
						       store);

		g_object_unref (l->data);
	}
	g_list_free (contacts);

	return FALSE;
}


static void
contact_list_store_set_contact_list (EmpathyContactListStore *store,
				     EmpathyContactList      *list_iface)
{
	EmpathyContactListStorePriv *priv = GET_PRIV (store);

	priv->list = g_object_ref (list_iface);

	/* Let a chance to have all properties set before populating */
	g_idle_add (contact_list_store_iface_setup, store);
}

static void
empathy_contact_list_store_class_init (EmpathyContactListStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = contact_list_store_finalize;
	object_class->get_property = contact_list_store_get_property;
	object_class->set_property = contact_list_store_set_property;

	g_object_class_install_property (object_class,
					 PROP_CONTACT_LIST,
					 g_param_spec_object ("contact-list",
							      "The contact list iface",
							      "The contact list iface",
							      EMPATHY_TYPE_CONTACT_LIST,
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_READWRITE));
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
					  PROP_SHOW_GROUPS,
					  g_param_spec_boolean ("show-groups",
								"Show Groups",
								"Whether contact list should display "
								"contact groups",
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
							    EMPATHY_TYPE_CONTACT_LIST_STORE_SORT,
							    EMPATHY_CONTACT_LIST_STORE_SORT_NAME,
							    G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EmpathyContactListStorePriv));
}

static void
empathy_contact_list_store_init (EmpathyContactListStore *store)
{
	EmpathyContactListStorePriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (store,
		EMPATHY_TYPE_CONTACT_LIST_STORE, EmpathyContactListStorePriv);

	store->priv = priv;
	priv->show_avatars = TRUE;
	priv->show_groups = TRUE;
	priv->inhibit_active = g_timeout_add_seconds (ACTIVE_USER_WAIT_TO_ENABLE_TIME,
						      (GSourceFunc) contact_list_store_inibit_active_cb,
						      store);
	contact_list_store_setup (store);
}

static void
contact_list_store_finalize (GObject *object)
{
	EmpathyContactListStorePriv *priv = GET_PRIV (object);
	GList                       *contacts, *l;

	contacts = empathy_contact_list_get_members (priv->list);
	for (l = contacts; l; l = l->next) {
		g_signal_handlers_disconnect_by_func (l->data,
						      G_CALLBACK (contact_list_store_contact_updated_cb),
						      object);

		g_object_unref (l->data);
	}
	g_list_free (contacts);

	g_signal_handlers_disconnect_by_func (priv->list,
					      G_CALLBACK (contact_list_store_members_changed_cb),
					      object);
	g_signal_handlers_disconnect_by_func (priv->list,
					      G_CALLBACK (contact_list_store_groups_changed_cb),
					      object);
	g_object_unref (priv->list);

	if (priv->inhibit_active) {
		g_source_remove (priv->inhibit_active);
	}

	G_OBJECT_CLASS (empathy_contact_list_store_parent_class)->finalize (object);
}

static void
contact_list_store_get_property (GObject    *object,
				 guint       param_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	EmpathyContactListStorePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_CONTACT_LIST:
		g_value_set_object (value, priv->list);
		break;
	case PROP_SHOW_OFFLINE:
		g_value_set_boolean (value, priv->show_offline);
		break;
	case PROP_SHOW_AVATARS:
		g_value_set_boolean (value, priv->show_avatars);
		break;
	case PROP_SHOW_GROUPS:
		g_value_set_boolean (value, priv->show_groups);
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
	EmpathyContactListStorePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_CONTACT_LIST:
		contact_list_store_set_contact_list (EMPATHY_CONTACT_LIST_STORE (object),
						     g_value_get_object (value));
		break;
	case PROP_SHOW_OFFLINE:
		empathy_contact_list_store_set_show_offline (EMPATHY_CONTACT_LIST_STORE (object),
							    g_value_get_boolean (value));
		break;
	case PROP_SHOW_AVATARS:
		empathy_contact_list_store_set_show_avatars (EMPATHY_CONTACT_LIST_STORE (object),
							    g_value_get_boolean (value));
		break;
	case PROP_SHOW_GROUPS:
		empathy_contact_list_store_set_show_groups (EMPATHY_CONTACT_LIST_STORE (object),
							    g_value_get_boolean (value));
		break;
	case PROP_IS_COMPACT:
		empathy_contact_list_store_set_is_compact (EMPATHY_CONTACT_LIST_STORE (object),
							  g_value_get_boolean (value));
		break;
	case PROP_SORT_CRITERIUM:
		empathy_contact_list_store_set_sort_criterium (EMPATHY_CONTACT_LIST_STORE (object),
							      g_value_get_enum (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

EmpathyContactListStore *
empathy_contact_list_store_new (EmpathyContactList *list_iface)
{
	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list_iface), NULL);

	return g_object_new (EMPATHY_TYPE_CONTACT_LIST_STORE,
			     "contact-list", list_iface,
			     NULL);
}

EmpathyContactList *
empathy_contact_list_store_get_list_iface (EmpathyContactListStore *store)
{
	EmpathyContactListStorePriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST_STORE (store), FALSE);

	priv = GET_PRIV (store);

	return priv->list;
}

gboolean
empathy_contact_list_store_get_show_offline (EmpathyContactListStore *store)
{
	EmpathyContactListStorePriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST_STORE (store), FALSE);

	priv = GET_PRIV (store);

	return priv->show_offline;
}

void
empathy_contact_list_store_set_show_offline (EmpathyContactListStore *store,
					    gboolean                show_offline)
{
	EmpathyContactListStorePriv *priv;
	GList                      *contacts, *l;
	gboolean                    show_active;

	g_return_if_fail (EMPATHY_IS_CONTACT_LIST_STORE (store));

	priv = GET_PRIV (store);

	priv->show_offline = show_offline;
	show_active = priv->show_active;

	/* Disable temporarily. */
	priv->show_active = FALSE;

	contacts = empathy_contact_list_get_members (priv->list);
	for (l = contacts; l; l = l->next) {
		contact_list_store_contact_update (store, l->data);

		g_object_unref (l->data);
	}
	g_list_free (contacts);

	/* Restore to original setting. */
	priv->show_active = show_active;

	g_object_notify (G_OBJECT (store), "show-offline");
}

gboolean
empathy_contact_list_store_get_show_avatars (EmpathyContactListStore *store)
{
	EmpathyContactListStorePriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST_STORE (store), TRUE);

	priv = GET_PRIV (store);

	return priv->show_avatars;
}

void
empathy_contact_list_store_set_show_avatars (EmpathyContactListStore *store,
					    gboolean                show_avatars)
{
	EmpathyContactListStorePriv *priv;
	GtkTreeModel               *model;

	g_return_if_fail (EMPATHY_IS_CONTACT_LIST_STORE (store));

	priv = GET_PRIV (store);

	priv->show_avatars = show_avatars;

	model = GTK_TREE_MODEL (store);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc)
				contact_list_store_update_list_mode_foreach,
				store);

	g_object_notify (G_OBJECT (store), "show-avatars");
}

gboolean
empathy_contact_list_store_get_show_groups (EmpathyContactListStore *store)
{
	EmpathyContactListStorePriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST_STORE (store), TRUE);

	priv = GET_PRIV (store);

	return priv->show_groups;
}

void
empathy_contact_list_store_set_show_groups (EmpathyContactListStore *store,
					    gboolean                 show_groups)
{
	EmpathyContactListStorePriv *priv;
	GList                       *contacts, *l;

	g_return_if_fail (EMPATHY_IS_CONTACT_LIST_STORE (store));

	priv = GET_PRIV (store);

	if (priv->show_groups == show_groups) {
		return;
	}

	priv->show_groups = show_groups;

	/* Remove all contacts and add them back, not optimized but that's the
	 * easy way :) */
	gtk_tree_store_clear (GTK_TREE_STORE (store));
	contacts = empathy_contact_list_get_members (priv->list);
	for (l = contacts; l; l = l->next) {
		contact_list_store_members_changed_cb (priv->list, l->data,
						       NULL, 0, NULL,
						       TRUE,
						       store);

		g_object_unref (l->data);
	}
	g_list_free (contacts);

	g_object_notify (G_OBJECT (store), "show-groups");
}

gboolean
empathy_contact_list_store_get_is_compact (EmpathyContactListStore *store)
{
	EmpathyContactListStorePriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST_STORE (store), TRUE);

	priv = GET_PRIV (store);

	return priv->is_compact;
}

void
empathy_contact_list_store_set_is_compact (EmpathyContactListStore *store,
					  gboolean                is_compact)
{
	EmpathyContactListStorePriv *priv;
	GtkTreeModel               *model;

	g_return_if_fail (EMPATHY_IS_CONTACT_LIST_STORE (store));

	priv = GET_PRIV (store);

	priv->is_compact = is_compact;

	model = GTK_TREE_MODEL (store);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc)
				contact_list_store_update_list_mode_foreach,
				store);

	g_object_notify (G_OBJECT (store), "is-compact");
}

EmpathyContactListStoreSort
empathy_contact_list_store_get_sort_criterium (EmpathyContactListStore *store)
{
	EmpathyContactListStorePriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST_STORE (store), 0);

	priv = GET_PRIV (store);

	return priv->sort_criterium;
}

void
empathy_contact_list_store_set_sort_criterium (EmpathyContactListStore     *store,
					      EmpathyContactListStoreSort  sort_criterium)
{
	EmpathyContactListStorePriv *priv;

	g_return_if_fail (EMPATHY_IS_CONTACT_LIST_STORE (store));

	priv = GET_PRIV (store);

	priv->sort_criterium = sort_criterium;

	switch (sort_criterium) {
	case EMPATHY_CONTACT_LIST_STORE_SORT_STATE:
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
						      EMPATHY_CONTACT_LIST_STORE_COL_STATUS,
						      GTK_SORT_ASCENDING);
		break;
		
	case EMPATHY_CONTACT_LIST_STORE_SORT_NAME:
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
						      EMPATHY_CONTACT_LIST_STORE_COL_NAME,
						      GTK_SORT_ASCENDING);
		break;
	}

	g_object_notify (G_OBJECT (store), "sort-criterium");
}

gboolean
empathy_contact_list_store_row_separator_func (GtkTreeModel *model,
					      GtkTreeIter  *iter,
					      gpointer      data)
{
	gboolean is_separator = FALSE;

	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);

	gtk_tree_model_get (model, iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_SEPARATOR, &is_separator,
			    -1);

	return is_separator;
}

gchar *
empathy_contact_list_store_get_parent_group (GtkTreeModel *model,
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
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP, &is_group,
			    EMPATHY_CONTACT_LIST_STORE_COL_NAME, &name,
			    -1);

	if (!is_group) {
		g_free (name);
		name = NULL;

		if (!gtk_tree_model_iter_parent (model, &parent_iter, &iter)) {
			return NULL;
		}

		iter = parent_iter;

		gtk_tree_model_get (model, &iter,
				    EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP, &is_group,
				    EMPATHY_CONTACT_LIST_STORE_COL_NAME, &name,
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
empathy_contact_list_store_search_equal_func (GtkTreeModel *model,
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
		return TRUE;
	}

	gtk_tree_model_get (model, iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_NAME, &name,
			    -1);

	if (!name) {
		return TRUE;
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

static void
contact_list_store_setup (EmpathyContactListStore *store)
{
	EmpathyContactListStorePriv *priv;
	GType                       types[] = {G_TYPE_STRING,        /* Status icon-name */
					       GDK_TYPE_PIXBUF,      /* Avatar pixbuf */
					       G_TYPE_BOOLEAN,       /* Avatar pixbuf visible */
					       G_TYPE_STRING,        /* Name */
					       G_TYPE_STRING,        /* Status string */
					       G_TYPE_BOOLEAN,       /* Show status */
					       EMPATHY_TYPE_CONTACT, /* Contact type */
					       G_TYPE_BOOLEAN,       /* Is group */
					       G_TYPE_BOOLEAN,       /* Is active */
					       G_TYPE_BOOLEAN,       /* Is online */
					       G_TYPE_BOOLEAN,       /* Is separator */
					       G_TYPE_BOOLEAN};      /* Can VoIP */
	
	priv = GET_PRIV (store);

	gtk_tree_store_set_column_types (GTK_TREE_STORE (store),
					 EMPATHY_CONTACT_LIST_STORE_COL_COUNT,
					 types);

	/* Set up sorting */
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store),
					 EMPATHY_CONTACT_LIST_STORE_COL_NAME,
					 contact_list_store_name_sort_func,
					 store, NULL);
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store),
					 EMPATHY_CONTACT_LIST_STORE_COL_STATUS,
					 contact_list_store_state_sort_func,
					 store, NULL);

	priv->sort_criterium = EMPATHY_CONTACT_LIST_STORE_SORT_NAME;
	empathy_contact_list_store_set_sort_criterium (store, priv->sort_criterium);
}

static gboolean
contact_list_store_inibit_active_cb (EmpathyContactListStore *store)
{
	EmpathyContactListStorePriv *priv;

	priv = GET_PRIV (store);

	priv->show_active = TRUE;
	priv->inhibit_active = 0;

	return FALSE;
}

static void
contact_list_store_members_changed_cb (EmpathyContactList      *list_iface,
				       EmpathyContact          *contact,
				       EmpathyContact          *actor,
				       guint                    reason,
				       gchar                   *message,
				       gboolean                 is_member,
				       EmpathyContactListStore *store)
{
	EmpathyContactListStorePriv *priv;

	priv = GET_PRIV (store);

	DEBUG ("Contact %s (%d) %s",
		empathy_contact_get_id (contact),
		empathy_contact_get_handle (contact),
		is_member ? "added" : "removed");

	if (is_member) {
		g_signal_connect (contact, "notify::presence",
				  G_CALLBACK (contact_list_store_contact_updated_cb),
				  store);
		g_signal_connect (contact, "notify::presence-message",
				  G_CALLBACK (contact_list_store_contact_updated_cb),
				  store);
		g_signal_connect (contact, "notify::name",
				  G_CALLBACK (contact_list_store_contact_updated_cb),
				  store);
		g_signal_connect (contact, "notify::avatar",
				  G_CALLBACK (contact_list_store_contact_updated_cb),
				  store);
		g_signal_connect (contact, "notify::capabilities",
				  G_CALLBACK (contact_list_store_contact_updated_cb),
				  store);

		contact_list_store_add_contact (store, contact);
	} else {
		g_signal_handlers_disconnect_by_func (contact,
						      G_CALLBACK (contact_list_store_contact_updated_cb),
						      store);

		contact_list_store_remove_contact (store, contact);
	}
}

static void
contact_list_store_groups_changed_cb (EmpathyContactList      *list_iface,
				      EmpathyContact          *contact,
				      gchar                   *group,
				      gboolean                 is_member,
				      EmpathyContactListStore *store)
{
	EmpathyContactListStorePriv *priv;
	gboolean                     show_active;

	priv = GET_PRIV (store);

	DEBUG ("Updating groups for contact %s (%d)",
		empathy_contact_get_id (contact),
		empathy_contact_get_handle (contact));

	/* We do this to make sure the groups are correct, if not, we
	 * would have to check the groups already set up for each
	 * contact and then see what has been updated.
	 */
	show_active = priv->show_active;
	priv->show_active = FALSE;
	contact_list_store_remove_contact (store, contact);
	contact_list_store_add_contact (store, contact);
	priv->show_active = show_active;
}

static void
contact_list_store_add_contact (EmpathyContactListStore *store,
				EmpathyContact          *contact)
{
	EmpathyContactListStorePriv *priv;
	GtkTreeIter                 iter;
	GList                      *groups = NULL, *l;

	priv = GET_PRIV (store);
	
	if (EMP_STR_EMPTY (empathy_contact_get_name (contact)) ||
	    (!priv->show_offline && !empathy_contact_is_online (contact))) {
		return;
	}

	if (priv->show_groups) {
		groups = empathy_contact_list_get_groups (priv->list, contact);
	}

	/* If no groups just add it at the top level. */
	if (!groups) {
		gtk_tree_store_append (GTK_TREE_STORE (store), &iter, NULL);
		gtk_tree_store_set (GTK_TREE_STORE (store), &iter,
				    EMPATHY_CONTACT_LIST_STORE_COL_NAME, empathy_contact_get_name (contact),
				    EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, contact,
				    EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP, FALSE,
				    EMPATHY_CONTACT_LIST_STORE_COL_IS_SEPARATOR, FALSE,
				    EMPATHY_CONTACT_LIST_STORE_COL_CAN_VOIP, empathy_contact_can_voip (contact),
				    -1);
	}

	/* Else add to each group. */
	for (l = groups; l; l = l->next) {
		GtkTreeIter iter_group;

		contact_list_store_get_group (store, l->data, &iter_group, NULL, NULL);

		gtk_tree_store_insert_after (GTK_TREE_STORE (store), &iter,
					     &iter_group, NULL);
		gtk_tree_store_set (GTK_TREE_STORE (store), &iter,
				    EMPATHY_CONTACT_LIST_STORE_COL_NAME, empathy_contact_get_name (contact),
				    EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, contact,
				    EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP, FALSE,
				    EMPATHY_CONTACT_LIST_STORE_COL_IS_SEPARATOR, FALSE,
				    EMPATHY_CONTACT_LIST_STORE_COL_CAN_VOIP, empathy_contact_can_voip (contact),
				    -1);
		g_free (l->data);
	}
	g_list_free (groups);

	contact_list_store_contact_update (store, contact);

}

static void
contact_list_store_remove_contact (EmpathyContactListStore *store,
				   EmpathyContact          *contact)
{
	EmpathyContactListStorePriv *priv;
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
contact_list_store_contact_update (EmpathyContactListStore *store,
				   EmpathyContact          *contact)
{
	EmpathyContactListStorePriv *priv;
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
	gboolean                    show_avatar = FALSE;
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
	now_online = empathy_contact_is_online (contact);

	if (priv->show_offline || now_online) {
		should_be_in_list = TRUE;
	} else {
		should_be_in_list = FALSE;
	}

	if (!in_list && !should_be_in_list) {
		/* Nothing to do. */
		DEBUG ("Contact:'%s' in list:NO, should be:NO",
			empathy_contact_get_name (contact));

		g_list_foreach (iters, (GFunc) gtk_tree_iter_free, NULL);
		g_list_free (iters);
		return;
	}
	else if (in_list && !should_be_in_list) {
		DEBUG ("Contact:'%s' in list:YES, should be:NO",
			empathy_contact_get_name (contact));

		if (priv->show_active) {
			do_remove = TRUE;
			do_set_active = TRUE;
			do_set_refresh = TRUE;

			set_model = TRUE;
			DEBUG ("Remove item (after timeout)");
		} else {
			DEBUG ("Remove item (now)!");
			contact_list_store_remove_contact (store, contact);
		}
	}
	else if (!in_list && should_be_in_list) {
		DEBUG ("Contact:'%s' in list:NO, should be:YES",
			empathy_contact_get_name (contact));

		contact_list_store_add_contact (store, contact);

		if (priv->show_active) {
			do_set_active = TRUE;

			DEBUG ("Set active (contact added)");
		}
	} else {
		DEBUG ("Contact:'%s' in list:YES, should be:YES",
			empathy_contact_get_name (contact));

		/* Get online state before. */
		if (iters && g_list_length (iters) > 0) {
			gtk_tree_model_get (model, iters->data,
					    EMPATHY_CONTACT_LIST_STORE_COL_IS_ONLINE, &was_online,
					    -1);
		}

		/* Is this really an update or an online/offline. */
		if (priv->show_active) {
			if (was_online != now_online) {
				do_set_active = TRUE;
				do_set_refresh = TRUE;

				DEBUG ("Set active (contact updated %s)",
					was_online ? "online  -> offline" :
					"offline -> online");
			} else {
				/* Was TRUE for presence updates. */
				/* do_set_active = FALSE;  */
				do_set_refresh = TRUE;

				DEBUG ("Set active (contact updated)");
			}
		}

		set_model = TRUE;
	}

	if (priv->show_avatars && !priv->is_compact) {
		show_avatar = TRUE;
	}
	pixbuf_avatar = empathy_pixbuf_avatar_from_contact_scaled (contact, 32, 32);
	for (l = iters; l && set_model; l = l->next) {
		gtk_tree_store_set (GTK_TREE_STORE (store), l->data,
				    EMPATHY_CONTACT_LIST_STORE_COL_ICON_STATUS, empathy_icon_name_for_contact (contact),
				    EMPATHY_CONTACT_LIST_STORE_COL_PIXBUF_AVATAR, pixbuf_avatar,
				    EMPATHY_CONTACT_LIST_STORE_COL_PIXBUF_AVATAR_VISIBLE, show_avatar,
				    EMPATHY_CONTACT_LIST_STORE_COL_NAME, empathy_contact_get_name (contact),
				    EMPATHY_CONTACT_LIST_STORE_COL_STATUS, empathy_contact_get_status (contact),
				    EMPATHY_CONTACT_LIST_STORE_COL_STATUS_VISIBLE, !priv->is_compact,
				    EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP, FALSE,
				    EMPATHY_CONTACT_LIST_STORE_COL_IS_ONLINE, now_online,
				    EMPATHY_CONTACT_LIST_STORE_COL_IS_SEPARATOR, FALSE,
				    EMPATHY_CONTACT_LIST_STORE_COL_CAN_VOIP, empathy_contact_can_voip (contact),
				    -1);
	}

	if (pixbuf_avatar) {
		g_object_unref (pixbuf_avatar);
	}

	if (priv->show_active && do_set_active) {
		contact_list_store_contact_set_active (store, contact, do_set_active, do_set_refresh);

		if (do_set_active) {
			data = contact_list_store_contact_active_new (store, contact, do_remove);
			g_timeout_add_seconds (ACTIVE_USER_SHOW_TIME,
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
contact_list_store_contact_updated_cb (EmpathyContact          *contact,
				       GParamSpec              *param,
				       EmpathyContactListStore *store)
{
	DEBUG ("Contact:'%s' updated, checking roster is in sync...",
		empathy_contact_get_name (contact));

	contact_list_store_contact_update (store, contact);
}

static void
contact_list_store_contact_set_active (EmpathyContactListStore *store,
				       EmpathyContact          *contact,
				       gboolean                active,
				       gboolean                set_changed)
{
	EmpathyContactListStorePriv *priv;
	GtkTreeModel               *model;
	GList                      *iters, *l;

	priv = GET_PRIV (store);
	model = GTK_TREE_MODEL (store);

	iters = contact_list_store_find_contact (store, contact);
	for (l = iters; l; l = l->next) {
		GtkTreePath *path;

		gtk_tree_store_set (GTK_TREE_STORE (store), l->data,
				    EMPATHY_CONTACT_LIST_STORE_COL_IS_ACTIVE, active,
				    -1);

		DEBUG ("Set item %s", active ? "active" : "inactive");

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
contact_list_store_contact_active_new (EmpathyContactListStore *store,
				       EmpathyContact          *contact,
				       gboolean                remove)
{
	ShowActiveData *data;

	DEBUG ("Contact:'%s' now active, and %s be removed",
		empathy_contact_get_name (contact), 
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
	EmpathyContactListStorePriv *priv;

	priv = GET_PRIV (data->store);

	if (data->remove &&
	    !priv->show_offline &&
	    !empathy_contact_is_online (data->contact)) {
		DEBUG ("Contact:'%s' active timeout, removing item",
			empathy_contact_get_name (data->contact));
		contact_list_store_remove_contact (data->store, data->contact);
	}

	DEBUG ("Contact:'%s' no longer active",
		empathy_contact_get_name (data->contact));

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
			    EMPATHY_CONTACT_LIST_STORE_COL_NAME, &str,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP, &is_group,
			    -1);

	if (is_group && !tp_strdiff (str, fg->name)) {
		fg->found = TRUE;
		fg->iter = *iter;
	}

	g_free (str);

	return fg->found;
}

static void
contact_list_store_get_group (EmpathyContactListStore *store,
			      const gchar            *name,
			      GtkTreeIter            *iter_group_to_set,
			      GtkTreeIter            *iter_separator_to_set,
			      gboolean               *created)
{
	EmpathyContactListStorePriv *priv;
	GtkTreeModel                *model;
	GtkTreeIter                  iter_group;
	GtkTreeIter                  iter_separator;
	FindGroup                    fg;

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
				    EMPATHY_CONTACT_LIST_STORE_COL_ICON_STATUS, NULL,
				    EMPATHY_CONTACT_LIST_STORE_COL_NAME, name,
				    EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP, TRUE,
				    EMPATHY_CONTACT_LIST_STORE_COL_IS_ACTIVE, FALSE,
				    EMPATHY_CONTACT_LIST_STORE_COL_IS_SEPARATOR, FALSE,
				    -1);

		if (iter_group_to_set) {
			*iter_group_to_set = iter_group;
		}

		gtk_tree_store_append (GTK_TREE_STORE (store),
				       &iter_separator, 
				       &iter_group);
		gtk_tree_store_set (GTK_TREE_STORE (store), &iter_separator,
				    EMPATHY_CONTACT_LIST_STORE_COL_IS_SEPARATOR, TRUE,
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
					    EMPATHY_CONTACT_LIST_STORE_COL_IS_SEPARATOR, &is_separator,
					    -1);

			if (is_separator && iter_separator_to_set) {
				*iter_separator_to_set = iter_separator;
			}
		}
	}
}

static guint
contact_list_store_ordered_presence (McPresence state)
{
	switch (state) {
	case MC_PRESENCE_UNSET:
	case MC_PRESENCE_OFFLINE:
		return 5;
	case MC_PRESENCE_AVAILABLE:
		return 0;
	case MC_PRESENCE_AWAY:
		return 2;
	case MC_PRESENCE_EXTENDED_AWAY:
		return 3;
	case MC_PRESENCE_HIDDEN:
		return 4;
	case MC_PRESENCE_DO_NOT_DISTURB:
		return 1;
	default:
		g_return_val_if_reached (6);
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
	EmpathyContact *contact_a, *contact_b;
	guint           presence_a, presence_b;

	gtk_tree_model_get (model, iter_a,
			    EMPATHY_CONTACT_LIST_STORE_COL_NAME, &name_a,
			    EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, &contact_a,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_SEPARATOR, &is_separator_a,
			    -1);
	gtk_tree_model_get (model, iter_b,
			    EMPATHY_CONTACT_LIST_STORE_COL_NAME, &name_b,
			    EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, &contact_b,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_SEPARATOR, &is_separator_b,
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
	presence_a = empathy_contact_get_presence (EMPATHY_CONTACT (contact_a));
	presence_a = contact_list_store_ordered_presence (presence_a);
	presence_b = empathy_contact_get_presence (EMPATHY_CONTACT (contact_b));
	presence_b = contact_list_store_ordered_presence (presence_b);

	if (presence_a < presence_b) {
		ret_val = -1;
	} else if (presence_a > presence_b) {
		ret_val = 1;
	} else {
		/* Fallback: compare by name */
		ret_val = g_utf8_collate (name_a, name_b);
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
	EmpathyContact *contact_a, *contact_b;
	gboolean       is_separator_a, is_separator_b;
	gint           ret_val;

	gtk_tree_model_get (model, iter_a,
			    EMPATHY_CONTACT_LIST_STORE_COL_NAME, &name_a,
			    EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, &contact_a,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_SEPARATOR, &is_separator_a,
			    -1);
	gtk_tree_model_get (model, iter_b,
			    EMPATHY_CONTACT_LIST_STORE_COL_NAME, &name_b,
			    EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, &contact_b,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_SEPARATOR, &is_separator_b,
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
	EmpathyContact *contact;

	gtk_tree_model_get (model, iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, &contact,
			    -1);

	if (contact == fc->contact) {
		fc->found = TRUE;
		fc->iters = g_list_append (fc->iters, gtk_tree_iter_copy (iter));
	}

	if (contact) {
		g_object_unref (contact);
	}

	return FALSE;
}

static GList *
contact_list_store_find_contact (EmpathyContactListStore *store,
				 EmpathyContact          *contact)
{
	EmpathyContactListStorePriv *priv;
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
					     EmpathyContactListStore *store)
{
	EmpathyContactListStorePriv *priv;
	gboolean                    show_avatar = FALSE;

	priv = GET_PRIV (store);

	if (priv->show_avatars && !priv->is_compact) {
		show_avatar = TRUE;
	}

	gtk_tree_store_set (GTK_TREE_STORE (store), iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_PIXBUF_AVATAR_VISIBLE, show_avatar,
			    EMPATHY_CONTACT_LIST_STORE_COL_STATUS_VISIBLE, !priv->is_compact,
			    -1);

	return FALSE;
}

