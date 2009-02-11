/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#include <config.h>

#include <string.h>

#include <telepathy-glib/enums.h>

#include "empathy-contact-manager.h"
#include "empathy-account-manager.h"
#include "empathy-contact-monitor.h"
#include "empathy-contact-list.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_CONTACT
#include "empathy-debug.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyContactManager)
typedef struct {
	GHashTable     *lists;
	EmpathyAccountManager *account_manager;
	EmpathyContactMonitor *contact_monitor;
} EmpathyContactManagerPriv;

static void contact_manager_iface_init         (EmpathyContactListIface    *iface);

G_DEFINE_TYPE_WITH_CODE (EmpathyContactManager, empathy_contact_manager, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (EMPATHY_TYPE_CONTACT_LIST,
						contact_manager_iface_init));

static EmpathyContactManager *manager_singleton = NULL;

static void
contact_manager_members_changed_cb (EmpathyTpContactList  *list,
				    EmpathyContact        *contact,
				    EmpathyContact        *actor,
				    guint                  reason,
				    gchar                 *message,
				    gboolean               is_member,
				    EmpathyContactManager *manager)
{
	g_signal_emit_by_name (manager, "members-changed",
			       contact, actor, reason, message, is_member);
}

static void
contact_manager_pendings_changed_cb (EmpathyTpContactList  *list,
				     EmpathyContact        *contact,
				     EmpathyContact        *actor,
				     guint                  reason,
				     gchar                 *message,
				     gboolean               is_pending,
				     EmpathyContactManager *manager)
{
	g_signal_emit_by_name (manager, "pendings-changed",
			       contact, actor, reason, message, is_pending);
}

static void
contact_manager_groups_changed_cb (EmpathyTpContactList  *list,
				   EmpathyContact        *contact,
				   gchar                 *group,
				   gboolean               is_member,
				   EmpathyContactManager *manager)
{
	g_signal_emit_by_name (manager, "groups-changed",
			       contact, group, is_member);
}

static void contact_manager_destroy_cb (EmpathyTpContactList  *list,
					EmpathyContactManager *manager);

static void
contact_manager_disconnect_foreach (gpointer key,
				    gpointer value,
				    gpointer user_data)
{
	EmpathyTpContactList  *list = value;
	EmpathyContactManager *manager = user_data;

	/* Disconnect signals from the list */
	g_signal_handlers_disconnect_by_func (list,
					      contact_manager_members_changed_cb,
					      manager);
	g_signal_handlers_disconnect_by_func (list,
					      contact_manager_pendings_changed_cb,
					      manager);
	g_signal_handlers_disconnect_by_func (list,
					      contact_manager_groups_changed_cb,
					      manager);
	g_signal_handlers_disconnect_by_func (list,
					      contact_manager_destroy_cb,
					      manager);
}

static void
contact_manager_destroy_cb (EmpathyTpContactList  *list,
			    EmpathyContactManager *manager)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);
	McAccount                 *account;

	account = empathy_tp_contact_list_get_account (list);

	DEBUG ("Removing account: %s", mc_account_get_display_name (account));

	contact_manager_disconnect_foreach (account, list, manager);
	g_hash_table_remove (priv->lists, account);
}

static void
contact_manager_add_account (EmpathyContactManager *manager,
			     McAccount             *account)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);
	EmpathyTpContactList      *list;

	if (g_hash_table_lookup (priv->lists, account)) {
		return;
	}

	DEBUG ("Adding new account: %s", mc_account_get_display_name (account));

	list = empathy_tp_contact_list_new (account);
	if (!list) {
		return;
	}

	g_hash_table_insert (priv->lists, g_object_ref (account), list);

	/* Connect signals */
	g_signal_connect (list, "members-changed",
			  G_CALLBACK (contact_manager_members_changed_cb),
			  manager);
	g_signal_connect (list, "pendings-changed",
			  G_CALLBACK (contact_manager_pendings_changed_cb),
			  manager);
	g_signal_connect (list, "groups-changed",
			  G_CALLBACK (contact_manager_groups_changed_cb),
			  manager);
	g_signal_connect (list, "destroy",
			  G_CALLBACK (contact_manager_destroy_cb),
			  manager);
}

static void
contact_manager_connection_changed_cb (EmpathyAccountManager *account_manager,
				       McAccount *account,
				       TpConnectionStatusReason  reason,
				       TpConnectionStatus current,
				       TpConnectionStatus previous,
				       EmpathyContactManager *manager)
{
	if (current != TP_CONNECTION_STATUS_CONNECTED) {
		/* We only care about newly connected accounts */
		return;
	}

	contact_manager_add_account (manager, account);
}

static void
contact_manager_finalize (GObject *object)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (object);

	g_hash_table_foreach (priv->lists,
			      contact_manager_disconnect_foreach,
			      object);
	g_hash_table_destroy (priv->lists);

	g_signal_handlers_disconnect_by_func (priv->account_manager,
					      contact_manager_connection_changed_cb,
					      object);
	g_object_unref (priv->account_manager);

	if (priv->contact_monitor) {
		g_object_unref (priv->contact_monitor);
	}
}

static GObject *
contact_manager_constructor (GType type,
			     guint n_props,
			     GObjectConstructParam *props)
{
	GObject *retval;

	if (manager_singleton) {
		retval = g_object_ref (manager_singleton);
	} else {
		retval = G_OBJECT_CLASS (empathy_contact_manager_parent_class)->constructor
			(type, n_props, props);

		manager_singleton = EMPATHY_CONTACT_MANAGER (retval);
		g_object_add_weak_pointer (retval, (gpointer) &manager_singleton);
	}

	return retval;
}

static void
empathy_contact_manager_class_init (EmpathyContactManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = contact_manager_finalize;
	object_class->constructor = contact_manager_constructor;

	g_type_class_add_private (object_class, sizeof (EmpathyContactManagerPriv));
}

static void
empathy_contact_manager_init (EmpathyContactManager *manager)
{
	GSList                    *accounts, *l;
	MissionControl            *mc;
	EmpathyContactManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
		EMPATHY_TYPE_CONTACT_MANAGER, EmpathyContactManagerPriv);

	manager->priv = priv;
	priv->lists = g_hash_table_new_full (empathy_account_hash,
					     empathy_account_equal,
					     (GDestroyNotify) g_object_unref,
					     (GDestroyNotify) g_object_unref);
	priv->account_manager = empathy_account_manager_dup_singleton ();
	priv->contact_monitor = NULL;

	g_signal_connect (priv->account_manager,
			  "account-connection-changed",
			  G_CALLBACK (contact_manager_connection_changed_cb), manager);

	mc = empathy_mission_control_dup_singleton ();

	/* Get ContactList for existing connections */
	accounts = mission_control_get_online_connections (mc, NULL);
	for (l = accounts; l; l = l->next) {
		contact_manager_add_account (manager, l->data);
		g_object_unref (l->data);
	}

	g_slist_free (accounts);
	g_object_unref (mc);
}

EmpathyContactManager *
empathy_contact_manager_dup_singleton (void)
{
	return g_object_new (EMPATHY_TYPE_CONTACT_MANAGER, NULL);
}

EmpathyTpContactList *
empathy_contact_manager_get_list (EmpathyContactManager *manager,
				  McAccount             *account)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

	return g_hash_table_lookup (priv->lists, account);
}

static void
contact_manager_add (EmpathyContactList *manager,
		     EmpathyContact     *contact,
		     const gchar        *message)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);
	EmpathyContactList        *list;
	McAccount                 *account;

	g_return_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager));

	account = empathy_contact_get_account (contact);
	list = g_hash_table_lookup (priv->lists, account);

	if (list) {
		empathy_contact_list_add (list, contact, message);
	}
}

static void
contact_manager_remove (EmpathyContactList *manager,
			EmpathyContact      *contact,
			const gchar        *message)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);
	EmpathyContactList        *list;
	McAccount                 *account;

	g_return_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager));

	account = empathy_contact_get_account (contact);
	list = g_hash_table_lookup (priv->lists, account);

	if (list) {
		empathy_contact_list_remove (list, contact, message);
	}
}

static void
contact_manager_get_members_foreach (McAccount             *account,
				     EmpathyTpContactList  *list,
				     GList                **contacts)
{
	GList *l;

	l = empathy_contact_list_get_members (EMPATHY_CONTACT_LIST (list));
	*contacts = g_list_concat (*contacts, l);
}

static GList *
contact_manager_get_members (EmpathyContactList *manager)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);
	GList                     *contacts = NULL;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);

	g_hash_table_foreach (priv->lists,
			      (GHFunc) contact_manager_get_members_foreach,
			      &contacts);

	return contacts;
}

static EmpathyContactMonitor *
contact_manager_get_monitor (EmpathyContactList *manager)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);

	if (priv->contact_monitor == NULL) {
		priv->contact_monitor = empathy_contact_monitor_new_for_iface (manager);
	}

	return priv->contact_monitor;
}

static void
contact_manager_get_pendings_foreach (McAccount             *account,
				      EmpathyTpContactList  *list,
				      GList                **contacts)
{
	GList *l;

	l = empathy_contact_list_get_pendings (EMPATHY_CONTACT_LIST (list));
	*contacts = g_list_concat (*contacts, l);
}

static GList *
contact_manager_get_pendings (EmpathyContactList *manager)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);
	GList                     *contacts = NULL;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);

	g_hash_table_foreach (priv->lists,
			      (GHFunc) contact_manager_get_pendings_foreach,
			      &contacts);

	return contacts;
}

static void
contact_manager_get_all_groups_foreach (McAccount             *account,
					EmpathyTpContactList  *list,
					GList                **all_groups)
{
	GList *groups, *l;

	groups = empathy_contact_list_get_all_groups (EMPATHY_CONTACT_LIST (list));
	for (l = groups; l; l = l->next) {
		if (!g_list_find_custom (*all_groups,
					 l->data,
					 (GCompareFunc) strcmp)) {
			*all_groups = g_list_prepend (*all_groups, l->data);
		} else {
			g_free (l->data);
		}
	}

	g_list_free (groups);
}

static GList *
contact_manager_get_all_groups (EmpathyContactList *manager)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);
	GList                     *groups = NULL;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);

	g_hash_table_foreach (priv->lists,
			      (GHFunc) contact_manager_get_all_groups_foreach,
			      &groups);

	return groups;
}

static GList *
contact_manager_get_groups (EmpathyContactList *manager,
			    EmpathyContact     *contact)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);
	EmpathyContactList        *list;
	McAccount                 *account;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);

	account = empathy_contact_get_account (contact);
	list = g_hash_table_lookup (priv->lists, account);

	if (list) {
		return empathy_contact_list_get_groups (list, contact);
	}

	return NULL;
}

static void
contact_manager_add_to_group (EmpathyContactList *manager,
			      EmpathyContact     *contact,
			      const gchar        *group)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);
	EmpathyContactList        *list;
	McAccount                 *account;

	g_return_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager));

	account = empathy_contact_get_account (contact);
	list = g_hash_table_lookup (priv->lists, account);

	if (list) {
		empathy_contact_list_add_to_group (list, contact, group);
	}
}

static void
contact_manager_remove_from_group (EmpathyContactList *manager,
				   EmpathyContact     *contact,
				   const gchar        *group)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);
	EmpathyContactList        *list;
	McAccount                 *account;

	g_return_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager));

	account = empathy_contact_get_account (contact);
	list = g_hash_table_lookup (priv->lists, account);

	if (list) {
		empathy_contact_list_remove_from_group (list, contact, group);
	}
}

typedef struct {
	const gchar *old_group;
	const gchar *new_group;
} RenameGroupData;

static void
contact_manager_rename_group_foreach (McAccount            *account,
				      EmpathyTpContactList *list,
				      RenameGroupData      *data)
{
	empathy_contact_list_rename_group (EMPATHY_CONTACT_LIST (list),
					   data->old_group,
					   data->new_group);
}

static void
contact_manager_rename_group (EmpathyContactList *manager,
			      const gchar        *old_group,
			      const gchar        *new_group)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);
	RenameGroupData            data;

	g_return_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager));

	data.old_group = old_group;
	data.new_group = new_group;
	g_hash_table_foreach (priv->lists,
			      (GHFunc) contact_manager_rename_group_foreach,
			      &data);
}

static void contact_manager_remove_group_foreach (McAccount	*account,
						  EmpathyTpContactList *list,
						  const gchar *group)
{
	empathy_contact_list_remove_group (EMPATHY_CONTACT_LIST (list),
					   group);
}

static void
contact_manager_remove_group (EmpathyContactList *manager,
			      const gchar *group)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);
	
	g_return_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager));

	g_hash_table_foreach (priv->lists,
			      (GHFunc) contact_manager_remove_group_foreach,
			      (gpointer) group);
}

static void
contact_manager_iface_init (EmpathyContactListIface *iface)
{
	iface->add               = contact_manager_add;
	iface->remove            = contact_manager_remove;
	iface->get_members       = contact_manager_get_members;
	iface->get_monitor       = contact_manager_get_monitor;
	iface->get_pendings      = contact_manager_get_pendings;
	iface->get_all_groups    = contact_manager_get_all_groups;
	iface->get_groups        = contact_manager_get_groups;
	iface->add_to_group      = contact_manager_add_to_group;
	iface->remove_from_group = contact_manager_remove_from_group;
	iface->rename_group      = contact_manager_rename_group;
	iface->remove_group	 = contact_manager_remove_group;
}

gboolean
empathy_contact_manager_can_add (EmpathyContactManager *manager,
				 McAccount             *account)
{
	EmpathyContactManagerPriv *priv = GET_PRIV (manager);
	EmpathyTpContactList      *list;
	
	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), FALSE);

	list = g_hash_table_lookup (priv->lists, account);
	if (list == NULL)
		return FALSE;

	return empathy_tp_contact_list_can_add (list);
}

