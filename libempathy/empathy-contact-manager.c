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

#include <libtelepathy/tp-constants.h>

#include "empathy-contact-manager.h"
#include "empathy-contact-list.h"
#include "empathy-utils.h"
#include "empathy-debug.h"

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
	EmpathyContact *contact;
	const gchar   *id;
} ContactManagerFindData;

static void           empathy_contact_manager_class_init   (EmpathyContactManagerClass      *klass);
static void           contact_manager_iface_init           (EmpathyContactListIface         *iface);
static void           empathy_contact_manager_init         (EmpathyContactManager           *manager);
static void           contact_manager_finalize             (GObject                         *object);
static void           contact_manager_setup                (EmpathyContactList              *manager);
static EmpathyContact *contact_manager_find                 (EmpathyContactList              *manager,
							    const gchar                     *id);
static void           contact_manager_add                  (EmpathyContactList              *manager,
							    EmpathyContact                   *contact,
							    const gchar                     *message);
static void           contact_manager_remove               (EmpathyContactList              *manager,
							    EmpathyContact                   *contact,
							    const gchar                     *message);
static GList *        contact_manager_get_members          (EmpathyContactList              *manager);
static GList *        contact_manager_get_local_pending    (EmpathyContactList              *manager);
static void           contact_manager_process_pending      (EmpathyContactList              *manager,
							    EmpathyContact                   *contact,
							    gboolean                         accept);
static void           contact_manager_setup_foreach        (McAccount                       *account,
							    EmpathyTpContactList            *list,
							    EmpathyContactManager           *manager);
static gboolean       contact_manager_find_foreach         (McAccount                       *account,
							    EmpathyTpContactList            *list,
							    ContactManagerFindData          *data);
static void           contact_manager_add_account          (EmpathyContactManager           *manager,
							    McAccount                       *account);
static void           contact_manager_added_cb             (EmpathyTpContactList            *list,
							    EmpathyContact                   *contact,
							    EmpathyContactManager           *manager);
static void           contact_manager_removed_cb           (EmpathyTpContactList            *list,
							    EmpathyContact                   *contact,
							    EmpathyContactManager           *manager);
static void           contact_manager_local_pending_cb     (EmpathyTpContactList            *list,
							    EmpathyContact                   *contact,
							    const gchar                     *message,
							    EmpathyContactManager           *manager);
static void           contact_manager_destroy_cb           (EmpathyTpContactList            *list,
							    EmpathyContactManager           *manager);
static void           contact_manager_rename_group_foreach (McAccount                       *account,
							    EmpathyTpContactList            *list,
							    ContactManagerRenameGroupData   *data);
static void           contact_manager_get_groups_foreach   (McAccount                       *account,
							    EmpathyTpContactList            *list,
							    GList                          **all_groups);
static void           contact_manager_get_members_foreach  (McAccount                       *account,
							    EmpathyTpContactList            *list,
							    GList                          **contacts);
static void           contact_manager_get_local_pending_foreach (McAccount                  *account,
							    EmpathyTpContactList            *list,
							    GList                          **contacts);
static void           contact_manager_status_changed_cb    (MissionControl                  *mc,
							    TelepathyConnectionStatus        status,
							    McPresence                       presence,
							    TelepathyConnectionStatusReason  reason,
							    const gchar                     *unique_name,
							    EmpathyContactManager           *manager);

G_DEFINE_TYPE_WITH_CODE (EmpathyContactManager, empathy_contact_manager, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (EMPATHY_TYPE_CONTACT_LIST,
						contact_manager_iface_init));

static void
empathy_contact_manager_class_init (EmpathyContactManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = contact_manager_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyContactManagerPriv));
}

static void
contact_manager_iface_init (EmpathyContactListIface *iface)
{
	iface->setup             = contact_manager_setup;
	iface->find              = contact_manager_find;
	iface->add               = contact_manager_add;
	iface->remove            = contact_manager_remove;
	iface->get_members       = contact_manager_get_members;
	iface->get_local_pending = contact_manager_get_local_pending;
	iface->process_pending   = contact_manager_process_pending;
}

static void
empathy_contact_manager_init (EmpathyContactManager *manager)
{
	EmpathyContactManagerPriv *priv;
	GSList                    *accounts, *l;

	priv = GET_PRIV (manager);

	priv->lists = g_hash_table_new_full (empathy_account_hash,
					     empathy_account_equal,
					     (GDestroyNotify) g_object_unref,
					     (GDestroyNotify) g_object_unref);

	priv->mc = empathy_mission_control_new ();

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

static void
contact_manager_setup (EmpathyContactList *manager)
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

static EmpathyContact *
contact_manager_find (EmpathyContactList *manager,
		      const gchar        *id)
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

static void
contact_manager_add (EmpathyContactList *manager,
		     EmpathyContact      *contact,
		     const gchar        *message)
{
	EmpathyContactManagerPriv *priv;
	EmpathyContactList        *list;
	McAccount                 *account;

	g_return_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	priv = GET_PRIV (manager);

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
	EmpathyContactManagerPriv *priv;
	EmpathyContactList        *list;
	McAccount                 *account;

	g_return_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	priv = GET_PRIV (manager);

	account = empathy_contact_get_account (contact);
	list = g_hash_table_lookup (priv->lists, account);

	if (list) {
		empathy_contact_list_remove (list, contact, message);
	}
}

static GList *
contact_manager_get_members (EmpathyContactList *manager)
{
	EmpathyContactManagerPriv *priv;
	GList                     *contacts = NULL;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);

	priv = GET_PRIV (manager);

	g_hash_table_foreach (priv->lists,
			      (GHFunc) contact_manager_get_members_foreach,
			      &contacts);

	return contacts;
}

static GList *
contact_manager_get_local_pending (EmpathyContactList *manager)
{
	EmpathyContactManagerPriv *priv;
	GList                     *pending = NULL;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);

	priv = GET_PRIV (manager);

	g_hash_table_foreach (priv->lists,
			      (GHFunc) contact_manager_get_local_pending_foreach,
			      &pending);

	return pending;
}

static void
contact_manager_process_pending (EmpathyContactList *manager,
				 EmpathyContact      *contact,
				 gboolean            accept)
{
	EmpathyContactManagerPriv *priv;
	EmpathyContactList        *list;
	McAccount                 *account;

	g_return_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	priv = GET_PRIV (manager);

	account = empathy_contact_get_account (contact);
	list = g_hash_table_lookup (priv->lists, account);

	if (list) {
		empathy_contact_list_process_pending (list, contact, accept);
	}
}

EmpathyTpContactList *
empathy_contact_manager_get_list (EmpathyContactManager *manager,
				  McAccount             *account)
{
	EmpathyContactManagerPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (manager);

	return g_hash_table_lookup (priv->lists, account);
}

EmpathyContact *
empathy_contact_manager_get_user (EmpathyContactManager *manager,
				  McAccount             *account)
{
	EmpathyContactManagerPriv *priv;
	EmpathyTpContactList        *list;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

	priv = GET_PRIV (manager);

	list = g_hash_table_lookup (priv->lists, account);
	
	if (!list) {
		return NULL;
	}

	return empathy_tp_contact_list_get_user (list);
}

EmpathyContact *
empathy_contact_manager_create (EmpathyContactManager *manager,
				McAccount             *account,
				const gchar           *id)
{
	EmpathyContactManagerPriv *priv;
	EmpathyTpContactList      *list;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_MANAGER (manager), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	priv = GET_PRIV (manager);

	list = g_hash_table_lookup (priv->lists, account);
	
	if (!list) {
		return NULL;
	}

	return empathy_tp_contact_list_get_from_id (list, id);
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

static void
contact_manager_setup_foreach (McAccount             *account,
			       EmpathyTpContactList  *list,
			       EmpathyContactManager *manager)
{
	empathy_contact_list_setup (EMPATHY_CONTACT_LIST (list));
}

static gboolean
contact_manager_find_foreach (McAccount              *account,
			      EmpathyTpContactList   *list,
                              ContactManagerFindData *data)
{
	data->contact = empathy_contact_list_find (EMPATHY_CONTACT_LIST (list),
						   data->id);

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
	EmpathyTpContactList        *list;

	priv = GET_PRIV (manager);

	if (g_hash_table_lookup (priv->lists, account)) {
		return;
	}

	empathy_debug (DEBUG_DOMAIN, "Adding new account: %s",
		      mc_account_get_display_name (account));

	list = empathy_tp_contact_list_new (account);
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
	g_signal_connect (list, "local-pending",
			  G_CALLBACK (contact_manager_local_pending_cb),
			  manager);
	g_signal_connect (list, "destroy",
			  G_CALLBACK (contact_manager_destroy_cb),
			  manager);

	if (priv->setup) {
		empathy_contact_list_setup (EMPATHY_CONTACT_LIST (list));
	}
}

static void
contact_manager_added_cb (EmpathyTpContactList  *list,
			  EmpathyContact         *contact,
			  EmpathyContactManager *manager)
{
	g_signal_emit_by_name (manager, "contact-added", contact);
}

static void
contact_manager_removed_cb (EmpathyTpContactList  *list,
			    EmpathyContact         *contact,
			    EmpathyContactManager *manager)
{
	g_signal_emit_by_name (manager, "contact-removed", contact);
}

static void
contact_manager_local_pending_cb (EmpathyTpContactList  *list,
				  EmpathyContact         *contact,
				  const gchar           *message,
				  EmpathyContactManager *manager)
{
	g_signal_emit_by_name (manager, "local-pending", contact, message);
}

static void
contact_manager_destroy_cb (EmpathyTpContactList  *list,
			    EmpathyContactManager *manager)
{
	EmpathyContactManagerPriv *priv;
	McAccount                 *account;

	priv = GET_PRIV (manager);

	account = empathy_tp_contact_list_get_account (list);

	empathy_debug (DEBUG_DOMAIN, "Removing account: %s",
		      mc_account_get_display_name (account));

	/* Disconnect signals from the list */
	g_signal_handlers_disconnect_by_func (list,
					      contact_manager_added_cb,
					      manager);
	g_signal_handlers_disconnect_by_func (list,
					      contact_manager_removed_cb,
					      manager);
	g_signal_handlers_disconnect_by_func (list,
					      contact_manager_local_pending_cb,
					      manager);
	g_signal_handlers_disconnect_by_func (list,
					      contact_manager_destroy_cb,
					      manager);

	g_hash_table_remove (priv->lists, account);
}

static void
contact_manager_rename_group_foreach (McAccount                     *account,
				      EmpathyTpContactList          *list,
				      ContactManagerRenameGroupData *data)
{
	empathy_tp_contact_list_rename_group (list,
					      data->old_group,
					      data->new_group);
}

static void
contact_manager_get_groups_foreach (McAccount             *account,
				    EmpathyTpContactList  *list,
				    GList                **all_groups)
{
	GList *groups, *l;

	groups = empathy_tp_contact_list_get_groups (list);
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
contact_manager_get_members_foreach (McAccount             *account,
				     EmpathyTpContactList  *list,
				     GList                **contacts)
{
	GList *l;

	l = empathy_contact_list_get_members (EMPATHY_CONTACT_LIST (list));
	*contacts = g_list_concat (*contacts, l);
}

static void
contact_manager_get_local_pending_foreach (McAccount             *account,
					   EmpathyTpContactList  *list,
					   GList                **contacts)
{
	GList *l;

	l = empathy_contact_list_get_local_pending (EMPATHY_CONTACT_LIST (list));
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

