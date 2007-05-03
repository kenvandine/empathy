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

#include <config.h>

#include <string.h>

#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-constants.h>

#include "empathy-contact-manager.h"
#include "gossip-utils.h"
#include "gossip-debug.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_CONTACT_MANAGER, EmpathyContactManagerPriv))

#define DEBUG_DOMAIN "ContactManager"

struct _EmpathyContactManagerPriv {
	GHashTable     *lists;
	MissionControl *mc;
	gboolean        setup;
};

typedef struct {
	const gchar *old_group;
	const gchar *new_group;
} ContactManagerRenameGroupData;

typedef struct {
	GossipContact *contact;
	const gchar   *id;
} ContactManagerFindData;

static void     empathy_contact_manager_class_init   (EmpathyContactManagerClass      *klass);
static void     empathy_contact_manager_init         (EmpathyContactManager           *manager);
static void     contact_manager_finalize             (GObject                         *object);
static void     contact_manager_setup_foreach        (McAccount                       *account,
						      EmpathyContactList              *list,
						      EmpathyContactManager           *manager);
static gboolean contact_manager_find_foreach         (McAccount                       *account,
						      EmpathyContactList              *list,
						      ContactManagerFindData          *data);
static void     contact_manager_add_account          (EmpathyContactManager           *manager,
						      McAccount                       *account);
static void     contact_manager_added_cb             (EmpathyContactList              *list,
						      GossipContact                   *contact,
						      EmpathyContactManager           *manager);
static void     contact_manager_removed_cb           (EmpathyContactList              *list,
						      GossipContact                   *contact,
						      EmpathyContactManager           *manager);
static void     contact_manager_destroy_cb           (EmpathyContactList              *list,
						      EmpathyContactManager           *manager);
static void     contact_manager_rename_group_foreach (McAccount                       *account,
						      EmpathyContactList              *list,
						      ContactManagerRenameGroupData   *data);
static void     contact_manager_get_groups_foreach   (McAccount                       *account,
						      EmpathyContactList              *list,
						      GList                          **all_groups);
static void     contact_manager_get_contacts_foreach (McAccount                       *account,
						      EmpathyContactList              *list,
						      GList                          **contacts);
static void     contact_manager_status_changed_cb    (MissionControl                  *mc,
						      TelepathyConnectionStatus        status,
						      McPresence                       presence,
						      TelepathyConnectionStatusReason  reason,
						      const gchar                     *unique_name,
						      EmpathyContactManager           *manager);

enum {
	CONTACT_ADDED,
	CONTACT_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyContactManager, empathy_contact_manager, G_TYPE_OBJECT);

static void
empathy_contact_manager_class_init (EmpathyContactManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = contact_manager_finalize;

	signals[CONTACT_ADDED] =
		g_signal_new ("contact-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CONTACT);

	signals[CONTACT_REMOVED] =
		g_signal_new ("contact-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, GOSSIP_TYPE_CONTACT);

	g_type_class_add_private (object_class, sizeof (EmpathyContactManagerPriv));
}

static void
empathy_contact_manager_init (EmpathyContactManager *manager)
{
	EmpathyContactManagerPriv *priv;
	GSList                    *accounts, *l;

	priv = GET_PRIV (manager);

	priv->lists = g_hash_table_new_full (gossip_account_hash,
					     gossip_account_equal,
					     (GDestroyNotify) g_object_unref,
					     (GDestroyNotify) g_object_unref);

	priv->mc = mission_control_new (tp_get_bus ());

	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->mc),
				     "AccountStatusChanged",
				     G_CALLBACK (contact_manager_status_changed_cb),
				     manager, NULL);

	/* Get ContactList for existing connections */
	accounts = mission_control_get_online_connections (priv->mc, NULL);
	for (l = accounts; l; l = l->next) {
		McAccount *account;

		account = l->data;
		contact_manager_add_account (manager, account);
		
		g_object_unref (account);
	}
	g_slist_free (accounts);
}

static void
contact_manager_finalize (GObject *object)
{
	EmpathyContactManagerPriv *priv;

	priv = GET_PRIV (object);

	g_hash_table_destroy (priv->lists);
	g_object_unref (priv->mc);
}

EmpathyContactManager *
empathy_contact_manager_new (void)
{
	static EmpathyContactManager *manager = NULL;

	if (!manager) {
		manager = g_object_new (EMPATHY_TYPE_CONTACT_MANAGER, NULL);
		g_object_add_weak_pointer (G_OBJECT (manager), (gpointer) &manager);
	} else {
		g_object_ref (manager);
	}

	return manager;
}

void
empathy_contact_manager_setup (EmpathyContactManager *manager)
{
	EmpathyContactManagerPriv *priv;

	g_return_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager));

	priv = GET_PRIV (manager);

	if (priv->setup) {
		/* Already done */
		return;
	}

	g_hash_table_foreach (priv->lists,
			      (GHFunc) contact_manager_setup_foreach,
			      manager);

	priv->setup = TRUE;
}

EmpathyContactList *
empathy_contact_manager_get_list (EmpathyContactManager *manager,
				  McAccount             *account)
{
	EmpathyContactManagerPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (manager);

	return g_hash_table_lookup (priv->lists, account);
}

GossipContact *
empathy_contact_manager_get_own (EmpathyContactManager *manager,
				 McAccount             *account)
{
	EmpathyContactManagerPriv *priv;
	EmpathyContactList        *list;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (manager);

	list = g_hash_table_lookup (priv->lists, account);
	
	if (!list) {
		return NULL;
	}

	return empathy_contact_list_get_own (list);
}

GossipContact *
empathy_contact_manager_create (EmpathyContactManager *manager,
				McAccount             *account,
				const gchar           *id)
{
	EmpathyContactManagerPriv *priv;
	EmpathyContactList        *list;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	priv = GET_PRIV (manager);

	list = g_hash_table_lookup (priv->lists, account);
	
	if (!list) {
		return NULL;
	}

	return empathy_contact_list_get_from_id (list, id);
}

GossipContact *
empathy_contact_manager_find (EmpathyContactManager *manager,
			      const gchar           *id)
{
	EmpathyContactManagerPriv *priv;
	ContactManagerFindData     data;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	priv = GET_PRIV (manager);

	data.contact = NULL;
	data.id = id;

	g_hash_table_find (priv->lists,
			   (GHRFunc) contact_manager_find_foreach,
			   &data);

	return data.contact;
}

void
empathy_contact_manager_add (EmpathyContactManager *manager,
			     GossipContact         *contact,
			     const gchar           *message)
{
	EmpathyContactManagerPriv *priv;
	EmpathyContactList        *list;
	McAccount                 *account;
	guint                      handle;

	g_return_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (manager);

	account = gossip_contact_get_account (contact);
	handle = gossip_contact_get_handle (contact);
	list = g_hash_table_lookup (priv->lists, account);

	if (list) {
		empathy_contact_list_add (list, handle, message);
	}
}

void
empathy_contact_manager_remove (EmpathyContactManager *manager,
				GossipContact         *contact)
{
	EmpathyContactManagerPriv *priv;
	EmpathyContactList        *list;
	McAccount                 *account;
	guint                      handle;

	g_return_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (manager);

	account = gossip_contact_get_account (contact);
	handle = gossip_contact_get_handle (contact);
	list = g_hash_table_lookup (priv->lists, account);

	if (list) {
		empathy_contact_list_remove (list, handle);
	}
}

void
empathy_contact_manager_rename_group (EmpathyContactManager *manager,
				      const gchar           *old_group,
				      const gchar           *new_group)
{
	EmpathyContactManagerPriv   *priv;
	ContactManagerRenameGroupData  data;

	g_return_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager));
	g_return_if_fail (old_group != NULL);
	g_return_if_fail (new_group != NULL);

	priv = GET_PRIV (manager);

	data.old_group = old_group;
	data.new_group = new_group;

	g_hash_table_foreach (priv->lists,
			      (GHFunc) contact_manager_rename_group_foreach,
			      &data);
}

GList *
empathy_contact_manager_get_groups (EmpathyContactManager *manager)
{
	EmpathyContactManagerPriv *priv;
	GList                     *groups = NULL;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);

	priv = GET_PRIV (manager);

	g_hash_table_foreach (priv->lists,
			      (GHFunc) contact_manager_get_groups_foreach,
			      &groups);

	return groups;
}

GList *
empathy_contact_manager_get_contacts (EmpathyContactManager *manager)
{
	EmpathyContactManagerPriv *priv;
	GList                     *contacts = NULL;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);

	priv = GET_PRIV (manager);

	g_hash_table_foreach (priv->lists,
			      (GHFunc) contact_manager_get_contacts_foreach,
			      &contacts);

	return contacts;
}

static void
contact_manager_setup_foreach (McAccount             *account,
			       EmpathyContactList    *list,
			       EmpathyContactManager *manager)
{
	empathy_contact_list_setup (list);
}

static gboolean
contact_manager_find_foreach (McAccount              *account,
			      EmpathyContactList     *list,
                              ContactManagerFindData *data)
{
	data->contact = empathy_contact_list_find (list, data->id);
	
	if (data->contact) {
		return TRUE;
	}

	return FALSE;
}

static void
contact_manager_add_account (EmpathyContactManager *manager,
			     McAccount             *account)
{
	EmpathyContactManagerPriv *priv;
	EmpathyContactList        *list;

	priv = GET_PRIV (manager);

	if (g_hash_table_lookup (priv->lists, account)) {
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Adding new account: %s",
		      mc_account_get_display_name (account));

	list = empathy_contact_list_new (account);
	if (!list) {
		return;
	}

	g_hash_table_insert (priv->lists, g_object_ref (account), list);

	/* Connect signals */
	g_signal_connect (list, "contact-added",
			  G_CALLBACK (contact_manager_added_cb),
			  manager);
	g_signal_connect (list, "contact-removed",
			  G_CALLBACK (contact_manager_removed_cb),
			  manager);
	g_signal_connect (list, "destroy",
			  G_CALLBACK (contact_manager_destroy_cb),
			  manager);

	if (priv->setup) {
		empathy_contact_list_setup (list);
	}
}

static void
contact_manager_added_cb (EmpathyContactList    *list,
			  GossipContact         *contact,
			  EmpathyContactManager *manager)
{
	g_signal_emit (manager, signals[CONTACT_ADDED], 0, contact);
}

static void
contact_manager_removed_cb (EmpathyContactList    *list,
			    GossipContact         *contact,
			    EmpathyContactManager *manager)
{
	g_signal_emit (manager, signals[CONTACT_REMOVED], 0, contact);
}

static void
contact_manager_destroy_cb (EmpathyContactList    *list,
			    EmpathyContactManager *manager)
{
	EmpathyContactManagerPriv *priv;
	McAccount                 *account;

	priv = GET_PRIV (manager);

	account = empathy_contact_list_get_account (list);

	gossip_debug (DEBUG_DOMAIN, "Removing account: %s",
		      mc_account_get_display_name (account));

	/* Disconnect signals from the list */
	g_signal_handlers_disconnect_by_func (list,
					      contact_manager_added_cb,
					      manager);
	g_signal_handlers_disconnect_by_func (list,
					      contact_manager_removed_cb,
					      manager);
	g_signal_handlers_disconnect_by_func (list,
					      contact_manager_destroy_cb,
					      manager);

	g_hash_table_remove (priv->lists, account);
}

static void
contact_manager_rename_group_foreach (McAccount                     *account,
				      EmpathyContactList            *list,
				      ContactManagerRenameGroupData *data)
{
	empathy_contact_list_rename_group (list,
					   data->old_group,
					   data->new_group);
}

static void
contact_manager_get_groups_foreach (McAccount           *account,
				    EmpathyContactList  *list,
				    GList              **all_groups)
{
	GList *groups, *l;

	groups = empathy_contact_list_get_groups (list);
	for (l = groups; l; l = l->next) {
		if (!g_list_find_custom (*all_groups,
					 l->data,
					 (GCompareFunc) strcmp)) {
			*all_groups = g_list_append (*all_groups,
						     g_strdup (l->data));
		}
		g_free (l->data);
	}

	g_list_free (groups);
}

static void
contact_manager_get_contacts_foreach (McAccount           *account,
				      EmpathyContactList  *list,
				      GList              **contacts)
{
	GList *l;

	l = empathy_contact_list_get_contacts (list);
	*contacts = g_list_concat (*contacts, l);
}

static void
contact_manager_status_changed_cb (MissionControl                  *mc,
				   TelepathyConnectionStatus        status,
				   McPresence                       presence,
				   TelepathyConnectionStatusReason  reason,
				   const gchar                     *unique_name,
				   EmpathyContactManager           *manager)
{
	EmpathyContactManagerPriv *priv;
	McAccount                 *account;

	priv = GET_PRIV (manager);

	if (status != TP_CONN_STATUS_CONNECTED) {
		/* We only care about newly connected accounts */
		return;
	}

	account = mc_account_lookup (unique_name);
	contact_manager_add_account (manager, account);

	g_object_unref (account);
}

