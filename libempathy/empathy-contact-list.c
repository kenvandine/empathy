/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Xavier Claessens <xclaesse@gmail.com>
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
 */

#include <config.h>

#include <string.h>

#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-chan-type-contact-list-gen.h>
#include <libtelepathy/tp-conn-iface-aliasing-gen.h>
#include <libtelepathy/tp-conn-iface-presence-gen.h>
#include <libtelepathy/tp-conn-iface-avatars-gen.h>

#include "empathy-contact-list.h"
#include "empathy-session.h"
#include "gossip-debug.h"
#include "gossip-telepathy-group.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_CONTACT_LIST, EmpathyContactListPriv))

#define DEBUG_DOMAIN "ContactList"
#define MAX_AVATAR_REQUESTS 10

struct _EmpathyContactListPriv {
	TpConn               *tp_conn;
	McAccount            *account;
	GossipContact        *own_contact;

	GossipTelepathyGroup *known;
	GossipTelepathyGroup *publish;
	GossipTelepathyGroup *subscribe;

	GHashTable           *groups;
	GHashTable           *contacts;

	DBusGProxy           *aliasing_iface;
	DBusGProxy           *avatars_iface;
	DBusGProxy           *presence_iface;

	GList                *avatar_requests_queue;
};

typedef enum {
	CONTACT_LIST_TYPE_KNOWN,
	CONTACT_LIST_TYPE_PUBLISH,
	CONTACT_LIST_TYPE_SUBSCRIBE,
	CONTACT_LIST_TYPE_UNKNOWN,
	CONTACT_LIST_TYPE_COUNT
} ContactListType;

typedef struct {
	guint  handle;
	GList *new_groups;
} ContactListData;

typedef struct {
	EmpathyContactList *list;
	guint                 handle;
} ContactListAvatarRequestData;

typedef struct {
	EmpathyContactList *list;
	guint                *handles;
} ContactListAliasesRequestData;

static void                   empathy_contact_list_class_init       (EmpathyContactListClass       *klass);
static void                   empathy_contact_list_init             (EmpathyContactList            *list);
static void                   contact_list_finalize                 (GObject                       *object);
static void                   contact_list_finalize_proxies         (EmpathyContactList            *list);
static void                   contact_list_contact_removed_foreach  (guint                          handle,
								     GossipContact                 *contact,
								     EmpathyContactList            *list);
static void                   contact_list_destroy_cb               (DBusGProxy                    *proxy,
								     EmpathyContactList            *list);
static gboolean               contact_list_find_foreach             (guint                          handle,
								     GossipContact                 *contact,
								     gchar                         *id);
static void                   contact_list_newchannel_cb            (DBusGProxy                    *proxy,
								     const gchar                   *object_path,
								     const gchar                   *channel_type,
								     TelepathyHandleType            handle_type,
								     guint                          channel_handle,
								     gboolean                       suppress_handle,
								     EmpathyContactList            *list);
static ContactListType        contact_list_get_type                 (EmpathyContactList            *list,
								     TpChan                        *list_chan);
static void                   contact_list_contact_added_cb         (GossipTelepathyGroup          *group,
								     GArray                        *handles,
								     guint                          actor_handle,
								     guint                          reason,
								     const gchar                   *message,
								     EmpathyContactList            *list);
static void                   contact_list_contact_removed_cb       (GossipTelepathyGroup          *group,
								     GArray                        *handles,
								     guint                          actor_handle,
								     guint                          reason,
								     const gchar                   *message,
								     EmpathyContactList            *list);
static void                   contact_list_local_pending_cb         (GossipTelepathyGroup          *group,
								     GArray                        *handles,
								     guint                          actor_handle,
								     guint                          reason,
								     const gchar                   *message,
								     EmpathyContactList            *list);
static void                   contact_list_groups_updated_cb        (GossipContact                 *contact,
								     GParamSpec                    *param,
								     EmpathyContactList            *list);
static void                   contact_list_subscription_updated_cb  (GossipContact                 *contact,
								     GParamSpec                    *param,
								     EmpathyContactList            *list);
static void                   contact_list_name_updated_cb          (GossipContact                 *contact,
								     GParamSpec                    *param,
								     EmpathyContactList            *list);
static void                   contact_list_update_groups_foreach    (gchar                         *object_path,
								     GossipTelepathyGroup          *group,
								     ContactListData               *data);
static GossipTelepathyGroup * contact_list_get_group                (EmpathyContactList            *list,
								     const gchar                   *name);
static gboolean               contact_list_find_group               (gchar                         *key,
								     GossipTelepathyGroup          *group,
								     gchar                         *group_name);
static void                   contact_list_get_groups_foreach       (gchar                         *key,
								     GossipTelepathyGroup          *group,
								     GList                        **groups);
static void                   contact_list_group_channel_closed_cb  (TpChan                        *channel,
								     EmpathyContactList            *list);
static void                   contact_list_group_members_added_cb   (GossipTelepathyGroup          *group,
								     GArray                        *members,
								     guint                          actor_handle,
								     guint                          reason,
								     const gchar                   *message,
								     EmpathyContactList            *list);
static void                   contact_list_group_members_removed_cb (GossipTelepathyGroup          *group,
								     GArray                        *members,
								     guint                          actor_handle,
								     guint                          reason,
								     const gchar                   *message,
								     EmpathyContactList            *list);
static void                   contact_list_get_contacts_foreach     (guint                          handle,
								     GossipContact                 *contact,
								     GList                        **contacts);
static void                   contact_list_get_info                 (EmpathyContactList            *list,
								     GArray                        *handles);
static void                   contact_list_request_avatar           (EmpathyContactList            *list,
								     guint                          handle);
static void                   contact_list_start_avatar_requests    (EmpathyContactList            *list);
static void                   contact_list_avatar_update_cb         (DBusGProxy                    *proxy,
								     guint                          handle,
								     gchar                         *new_token,
								     EmpathyContactList            *list);
static void                   contact_list_request_avatar_cb        (DBusGProxy                    *proxy,
								     GArray                        *avatar_data,
								     gchar                         *mime_type,
								     GError                        *error,
								     ContactListAvatarRequestData  *data);
static void                   contact_list_aliases_update_cb        (DBusGProxy                    *proxy,
								     GPtrArray                     *handlers,
								     EmpathyContactList            *list);
static void                   contact_list_request_aliases_cb       (DBusGProxy                    *proxy,
								     gchar                        **contact_names,
								     GError                        *error,
								     ContactListAliasesRequestData *data);
static void                   contact_list_presence_update_cb       (DBusGProxy                    *proxy,
								     GHashTable                    *handle_table,
								     EmpathyContactList            *list);
static void                   contact_list_parse_presence_foreach   (guint                          handle,
								     GValueArray                   *presence_struct,
								     EmpathyContactList            *list);
static void                   contact_list_presences_table_foreach  (const gchar                   *state_str,
								     GHashTable                    *presences_table,
								     GossipPresence               **presence);
static GossipPresenceState    contact_list_presence_state_from_str  (const gchar                   *str);

enum {
	CONTACT_ADDED,
	CONTACT_REMOVED,
	DESTROY,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static guint n_avatar_requests = 0;

G_DEFINE_TYPE (EmpathyContactList, empathy_contact_list, G_TYPE_OBJECT);

static void
empathy_contact_list_class_init (EmpathyContactListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = contact_list_finalize;

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

	signals[DESTROY] =
		g_signal_new ("destroy",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_type_class_add_private (object_class, sizeof (EmpathyContactListPriv));
}

static void
empathy_contact_list_init (EmpathyContactList *list)
{
	EmpathyContactListPriv *priv;

	priv = GET_PRIV (list);

	priv->groups = g_hash_table_new_full (g_str_hash,
					      g_str_equal,
					      (GDestroyNotify) g_free,
					      (GDestroyNotify) g_object_unref);
	priv->contacts = g_hash_table_new_full (g_direct_hash,
						g_direct_equal,
						NULL,
						(GDestroyNotify) g_object_unref);
}


static void
contact_list_finalize (GObject *object)
{
	EmpathyContactListPriv *priv;
	EmpathyContactList     *list;

	list = EMPATHY_CONTACT_LIST (object);
	priv = GET_PRIV (list);

	gossip_debug (DEBUG_DOMAIN, "finalize: %p", object);

	if (priv->tp_conn) {
		contact_list_finalize_proxies (list);
		g_object_unref (priv->tp_conn);
	}

	if (priv->known) {
		g_object_unref (priv->known);
	}

	if (priv->subscribe) {
		g_object_unref (priv->subscribe);
	}

	if (priv->publish) {
		g_object_unref (priv->publish);
	}

	g_object_unref (priv->account);
	g_object_unref (priv->own_contact);
	g_hash_table_destroy (priv->groups);
	g_hash_table_destroy (priv->contacts);

	G_OBJECT_CLASS (empathy_contact_list_parent_class)->finalize (object);
}

EmpathyContactList *
empathy_contact_list_new (McAccount *account)
{
	EmpathyContactListPriv *priv;
	EmpathyContactList     *list;
	MissionControl         *mc;
	TpConn                 *tp_conn;
	guint                   handle;
	GError                 *error = NULL;

	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

	mc = empathy_session_get_mission_control ();

	if (mission_control_get_connection_status (mc, account, NULL) != 0) {
		/* The account is not connected, nothing to do. */
		return NULL;
	}

	tp_conn = mission_control_get_connection (mc, account, NULL);
	g_return_val_if_fail (tp_conn != NULL, NULL);

	list = g_object_new (EMPATHY_TYPE_CONTACT_LIST, NULL);
	priv = GET_PRIV (list);

	priv->tp_conn = tp_conn;
	priv->account = g_object_ref (account);

	g_signal_connect (priv->tp_conn, "destroy",
			  G_CALLBACK (contact_list_destroy_cb),
			  list);

	priv->aliasing_iface = tp_conn_get_interface (priv->tp_conn,
						      TELEPATHY_CONN_IFACE_ALIASING_QUARK);
	priv->avatars_iface = tp_conn_get_interface (priv->tp_conn,
						     TELEPATHY_CONN_IFACE_AVATARS_QUARK);
	priv->presence_iface = tp_conn_get_interface (priv->tp_conn,
						      TELEPATHY_CONN_IFACE_PRESENCE_QUARK);

	if (priv->aliasing_iface) {
		dbus_g_proxy_connect_signal (priv->aliasing_iface,
					     "AliasesChanged",
					     G_CALLBACK (contact_list_aliases_update_cb),
					     list, NULL);
	}

	if (priv->avatars_iface) {
		dbus_g_proxy_connect_signal (priv->avatars_iface,
					     "AvatarUpdated",
					     G_CALLBACK (contact_list_avatar_update_cb),
					     list, NULL);
	}

	if (priv->presence_iface) {
		dbus_g_proxy_connect_signal (priv->presence_iface,
					     "PresenceUpdate",
					     G_CALLBACK (contact_list_presence_update_cb),
					     list, NULL);
	}

	/* Get our own handle and contact */
	if (!tp_conn_get_self_handle (DBUS_G_PROXY (priv->tp_conn),
				      &handle, &error)) {
		gossip_debug (DEBUG_DOMAIN, "GetSelfHandle Error: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	} else {
		priv->own_contact = empathy_contact_list_get_from_handle (list, handle);
	}

	return list;
}

void
empathy_contact_list_setup (EmpathyContactList *list)
{
	EmpathyContactListPriv *priv;
	GPtrArray                *channels;
	GError                   *error = NULL;
	guint                     i;

	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));

	priv = GET_PRIV (list);

	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->tp_conn), "NewChannel",
				     G_CALLBACK (contact_list_newchannel_cb),
				     list, NULL);

	/* Get existing channels */
	if (!tp_conn_list_channels (DBUS_G_PROXY (priv->tp_conn),
				    &channels,
				    &error)) {
		gossip_debug (DEBUG_DOMAIN,
			      "Failed to get list of open channels: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		return;
	}

	for (i = 0; channels->len > i; i++) {
		GValueArray         *chan_struct;
		const gchar         *object_path;
		const gchar         *chan_iface;
		TelepathyHandleType  handle_type;
		guint                handle;

		chan_struct = g_ptr_array_index (channels, i);
		object_path = g_value_get_boxed (g_value_array_get_nth (chan_struct, 0));
		chan_iface = g_value_get_string (g_value_array_get_nth (chan_struct, 1));
		handle_type = g_value_get_uint (g_value_array_get_nth (chan_struct, 2));
		handle = g_value_get_uint (g_value_array_get_nth (chan_struct, 3));

		contact_list_newchannel_cb (DBUS_G_PROXY (priv->tp_conn),
					    object_path, chan_iface,
					    handle_type, handle,
					    FALSE, list);

		g_value_array_free (chan_struct);
	}

	g_ptr_array_free (channels, TRUE);
}

McAccount *
empathy_contact_list_get_account (EmpathyContactList *list)
{
	EmpathyContactListPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);

	return priv->account;
}

GossipContact *
empathy_contact_list_get_own (EmpathyContactList *list)
{
	EmpathyContactListPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);
	
	return priv->own_contact;
}

GossipContact *
empathy_contact_list_find (EmpathyContactList *list,
			   const gchar        *id)
{
	EmpathyContactListPriv *priv;
	GossipContact            *contact;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);

	contact = g_hash_table_find (priv->contacts,
				     (GHRFunc) contact_list_find_foreach,
				     (gchar*) id);

	return NULL;
}

void
empathy_contact_list_add (EmpathyContactList *list,
			  guint               handle,
			  const gchar        *message)
{
	EmpathyContactListPriv *priv;

	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));

	priv = GET_PRIV (list);

	gossip_telepathy_group_add_member (priv->subscribe, handle, message);
}

void
empathy_contact_list_remove (EmpathyContactList *list,
			     guint               handle)
{
	EmpathyContactListPriv *priv;

	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));

	priv = GET_PRIV (list);

	gossip_telepathy_group_remove_member (priv->subscribe, handle, "");
	gossip_telepathy_group_remove_member (priv->publish, handle, "");
	gossip_telepathy_group_remove_member (priv->known, handle, "");
}

GossipContact *
empathy_contact_list_get_from_id (EmpathyContactList *list,
				  const gchar        *id)
{
	EmpathyContactListPriv *priv;
	GossipContact          *contact;
	const gchar            *contact_ids[] = {id, NULL};
	GArray                 *handles;
	guint                   handle;
	GError                 *error = NULL;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);
	g_return_val_if_fail (id != NULL, NULL);
	
	priv = GET_PRIV (list);

	contact = empathy_contact_list_find (list, id);
	if (contact) {
		return contact;
	}

	/* The id is unknown, requests a new handle */
	if (!tp_conn_request_handles (DBUS_G_PROXY (priv->tp_conn),
				      TP_HANDLE_TYPE_CONTACT,
				      contact_ids,
				      &handles, &error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "RequestHandle for %s failed: %s", id,
			      error ? error->message : "No error given");
		g_clear_error (&error);
		return 0;
	}

	handle = g_array_index(handles, guint, 0);
	g_array_free (handles, TRUE);

	return empathy_contact_list_get_from_handle (list, handle);
}

GossipContact *
empathy_contact_list_get_from_handle (EmpathyContactList *list,
				      guint               handle)
{
	GossipContact *contact;
	GArray        *handles;
	GList         *contacts;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);

	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);

	contacts = empathy_contact_list_get_from_handles (list, handles);
	g_array_free (handles, TRUE);

	if (!contacts) {
		return NULL;
	}

	contact = contacts->data;
	g_list_free (contacts);

	return contact;
}

GList *
empathy_contact_list_get_from_handles (EmpathyContactList *list,
				       GArray             *handles)
{
	EmpathyContactListPriv  *priv;
	gchar                  **handles_names;
	gchar                  **id;
	GArray                  *new_handles;
	GList                   *contacts = NULL;
	guint                    i;
	GError                  *error = NULL;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);
	g_return_val_if_fail (handles != NULL, NULL);

	priv = GET_PRIV (list);

	/* Search all handles we already have */
	new_handles = g_array_new (FALSE, FALSE, sizeof (guint));
	for (i = 0; i < handles->len; i++) {
		GossipContact *contact;
		guint          handle;

		handle = g_array_index (handles, guint, i);
		contact = g_hash_table_lookup (priv->contacts,
					       GUINT_TO_POINTER (handle));

		if (contact) {
			contacts = g_list_prepend (contacts,
						   g_object_ref (contact));
		} else {
			g_array_append_val (new_handles, handle);
		}
	}

	if (new_handles->len == 0) {
		return contacts;
	}

	/* Holds all handles we don't have yet.
	 * FIXME: We should release them at some point. */
	if (!tp_conn_hold_handles (DBUS_G_PROXY (priv->tp_conn),
				   TP_HANDLE_TYPE_CONTACT,
				   new_handles, &error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "HoldHandles Error: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		g_array_free (new_handles, TRUE);
		return contacts;
	}

	/* Get the IDs of all new handles */
	if (!tp_conn_inspect_handles (DBUS_G_PROXY (priv->tp_conn),
				      TP_HANDLE_TYPE_CONTACT,
				      new_handles,
				      &handles_names,
				      &error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "InspectHandle Error: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		g_array_free (new_handles, TRUE);
		return contacts;
	}

	/* Create contact objects */
	for (i = 0, id = handles_names; *id && i < new_handles->len; id++, i++) {
		GossipContact *contact;
		guint          handle;

		handle = g_array_index (new_handles, guint, i);
		contact = g_object_new (GOSSIP_TYPE_CONTACT,
					"account", priv->account,
					"id", *id,
					"handle", handle,
					NULL);

		g_signal_connect (contact, "notify::groups",
				  G_CALLBACK (contact_list_groups_updated_cb),
				  list);
		g_signal_connect (contact, "notify::subscription",
				  G_CALLBACK (contact_list_subscription_updated_cb),
				  list);
		g_signal_connect (contact, "notify::name",
				  G_CALLBACK (contact_list_name_updated_cb),
				  list);

		gossip_debug (DEBUG_DOMAIN, "new contact created: %s (%d)",
			      *id, handle);

		g_hash_table_insert (priv->contacts,
				     GUINT_TO_POINTER (handle),
				     contact);

		contacts = g_list_prepend (contacts, g_object_ref (contact));
	}

	contact_list_get_info (list, new_handles);

	g_array_free (new_handles, TRUE);
	g_strfreev (handles_names);

	return contacts;
}

void
empathy_contact_list_rename_group (EmpathyContactList *list,
				   const gchar        *old_group,
				   const gchar        *new_group)
{
	EmpathyContactListPriv *priv;
	GossipTelepathyGroup   *group;
	GArray                 *members;

	g_return_if_fail (EMPATHY_IS_CONTACT_LIST (list));
	g_return_if_fail (old_group != NULL);
	g_return_if_fail (new_group != NULL);

	priv = GET_PRIV (list);

	group = g_hash_table_find (priv->groups,
				   (GHRFunc) contact_list_find_group,
				   (gchar*) old_group);
	if (!group) {
		/* The group doesn't exists on this account */
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "rename group %s to %s", group, new_group);

	/* Remove all members from the old group */
	members = gossip_telepathy_group_get_members (group);
	gossip_telepathy_group_remove_members (group, members, "");
	contact_list_group_members_removed_cb (group, members, 
					       0, 
					       TP_CHANNEL_GROUP_CHANGE_REASON_NONE, 
					       NULL, list);
	g_hash_table_remove (priv->groups,
			     gossip_telepathy_group_get_object_path (group));

	/* Add all members to the new group */
	group = contact_list_get_group (list, new_group);
	if (group) {
		gossip_telepathy_group_add_members (group, members, "");
	}
}

GList *
empathy_contact_list_get_groups (EmpathyContactList *list)
{
	EmpathyContactListPriv *priv;
	GList                    *groups = NULL;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);

	g_hash_table_foreach (priv->groups,
			      (GHFunc) contact_list_get_groups_foreach,
			      &groups);

	groups = g_list_sort (groups, (GCompareFunc) strcmp);

	return groups;
}

GList *
empathy_contact_list_get_contacts (EmpathyContactList *list)
{
	EmpathyContactListPriv *priv;
	GList                    *contacts = NULL;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);

	/* FIXME: we should only return contacts that are in the contact list */
	g_hash_table_foreach (priv->contacts,
			      (GHFunc) contact_list_get_contacts_foreach,
			      &contacts);

	return contacts;
}

static void
contact_list_finalize_proxies (EmpathyContactList *list)
{
	EmpathyContactListPriv *priv;

	priv = GET_PRIV (list);

	g_signal_handlers_disconnect_by_func (priv->tp_conn,
					      contact_list_destroy_cb,
					      list);
	dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (priv->tp_conn), "NewChannel",
					G_CALLBACK (contact_list_newchannel_cb),
					list);

	if (priv->aliasing_iface) {
		dbus_g_proxy_disconnect_signal (priv->aliasing_iface,
						"AliasesChanged",
						G_CALLBACK (contact_list_aliases_update_cb),
						list);
	}

	if (priv->avatars_iface) {
		dbus_g_proxy_disconnect_signal (priv->avatars_iface,
						"AvatarUpdated",
						G_CALLBACK (contact_list_avatar_update_cb),
						list);
	}

	if (priv->presence_iface) {
		dbus_g_proxy_disconnect_signal (priv->presence_iface,
						"PresenceUpdate",
						G_CALLBACK (contact_list_presence_update_cb),
						list);
	}
}

static void
contact_list_destroy_cb (DBusGProxy           *proxy,
			 EmpathyContactList *list)
{
	EmpathyContactListPriv *priv;

	priv = GET_PRIV (list);

	gossip_debug (DEBUG_DOMAIN, "Connection destroyed. "
				    "Account disconnected or CM crashed.");

	/* DBus proxies should NOT be used anymore */
	g_object_unref (priv->tp_conn);
	priv->tp_conn = NULL;
	priv->aliasing_iface = NULL;
	priv->avatars_iface = NULL;
	priv->presence_iface = NULL;

	/* Remove all contacts */
	g_hash_table_foreach (priv->contacts,
			      (GHFunc) contact_list_contact_removed_foreach,
			      list);
	g_hash_table_remove_all (priv->contacts);

	/* Tell the world to not use us anymore */
	g_signal_emit (list, signals[DESTROY], 0);
}

static void
contact_list_contact_removed_foreach (guint                 handle,
				      GossipContact        *contact,
				      EmpathyContactList *list)
{
	g_signal_handlers_disconnect_by_func (contact,
					      contact_list_groups_updated_cb,
					      list);
	g_signal_handlers_disconnect_by_func (contact,
					      contact_list_subscription_updated_cb,
					      list);
	g_signal_handlers_disconnect_by_func (contact,
					      contact_list_name_updated_cb,
					      list);

	g_signal_emit (list, signals[CONTACT_REMOVED], 0, contact);
}

static void
contact_list_block_contact (EmpathyContactList *list,
			    GossipContact        *contact)
{
	g_signal_handlers_block_by_func (contact,
					 contact_list_groups_updated_cb,
					 list);
	g_signal_handlers_block_by_func (contact,
					 contact_list_subscription_updated_cb,
					 list);
	g_signal_handlers_block_by_func (contact,
					 contact_list_name_updated_cb,
					 list);
}

static void
contact_list_unblock_contact (EmpathyContactList *list,
			      GossipContact        *contact)
{
	g_signal_handlers_unblock_by_func (contact,
					   contact_list_groups_updated_cb,
					   list);
	g_signal_handlers_unblock_by_func (contact,
					   contact_list_subscription_updated_cb,
					   list);
	g_signal_handlers_unblock_by_func (contact,
					   contact_list_name_updated_cb,
					   list);
}

static gboolean
contact_list_find_foreach (guint          handle,
			   GossipContact *contact,
			   gchar         *id)
{
	if (strcmp (gossip_contact_get_id (contact), id) == 0) {
		return TRUE;
	}

	return FALSE;
}

static void
contact_list_newchannel_cb (DBusGProxy          *proxy,
			    const gchar         *object_path,
			    const gchar         *channel_type,
			    TelepathyHandleType  handle_type,
			    guint                channel_handle,
			    gboolean             suppress_handle,
			    EmpathyContactList  *list)
{
	EmpathyContactListPriv *priv;
	GossipTelepathyGroup   *group;
	TpChan                 *new_chan;
	const gchar            *bus_name;
	GArray                 *members;

	priv = GET_PRIV (list);

	if (strcmp (channel_type, TP_IFACE_CHANNEL_TYPE_CONTACT_LIST) != 0 ||
	    suppress_handle) {
		return;
	}

	bus_name = dbus_g_proxy_get_bus_name (DBUS_G_PROXY (priv->tp_conn));
	new_chan = tp_chan_new (tp_get_bus (),
				bus_name,
				object_path,
				channel_type, handle_type, channel_handle);

	if (handle_type == TP_HANDLE_TYPE_LIST) {
		ContactListType list_type;

		list_type = contact_list_get_type (list, new_chan);
		if (list_type == CONTACT_LIST_TYPE_UNKNOWN) {
			gossip_debug (DEBUG_DOMAIN, "Unknown contact list channel");
			g_object_unref (new_chan);
			return;
		}

		gossip_debug (DEBUG_DOMAIN, "New contact list channel of type: %d",
			      list_type);

		group = gossip_telepathy_group_new (new_chan, priv->tp_conn);

		switch (list_type) {
		case CONTACT_LIST_TYPE_KNOWN:
			if (priv->known) {
				g_object_unref (priv->known);
			}
			priv->known = group;
			break;
		case CONTACT_LIST_TYPE_PUBLISH:
			if (priv->publish) {
				g_object_unref (priv->publish);
			}
			priv->publish = group;
			break;
		case CONTACT_LIST_TYPE_SUBSCRIBE:
			if (priv->subscribe) {
				g_object_unref (priv->subscribe);
			}
			priv->subscribe = group;
			break;
		default:
			g_assert_not_reached ();
		}

		/* Connect and setup the new contact-list group */
		if (list_type == CONTACT_LIST_TYPE_KNOWN ||
		    list_type == CONTACT_LIST_TYPE_SUBSCRIBE) {
			g_signal_connect (group, "members-added",
					  G_CALLBACK (contact_list_contact_added_cb),
					  list);
			g_signal_connect (group, "members-removed",
					  G_CALLBACK (contact_list_contact_removed_cb),
					  list);

			members = gossip_telepathy_group_get_members (group);
			contact_list_contact_added_cb (group, members, 0,
						       TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
						       NULL, list);
			g_array_free (members, TRUE);
		}
		if (list_type == CONTACT_LIST_TYPE_PUBLISH) {
			GPtrArray *info;
			GArray    *pending; 
			guint      i;

			g_signal_connect (group, "local-pending",
					  G_CALLBACK (contact_list_local_pending_cb),
					  list);

			info = gossip_telepathy_group_get_local_pending_members_with_info (group);

			if (!info) {
				/* This happens with butterfly because
				 * GetLocalPendingMembersWithInfo is not 
				 * implemented */
				g_object_unref (new_chan);
				return;
			}

			pending = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
			for (i = 0; info->len > i; i++) {
				GValueArray *pending_struct;
				guint        member;
				guint        invitor;
				guint        reason;
				const gchar *message;

				pending_struct = g_ptr_array_index (info, i);
				member = g_value_get_uint (g_value_array_get_nth (pending_struct, 0));
				invitor = g_value_get_uint (g_value_array_get_nth (pending_struct, 1));
				reason = g_value_get_uint (g_value_array_get_nth (pending_struct, 2));
				message = g_value_get_string (g_value_array_get_nth (pending_struct, 3));

				g_array_insert_val (pending, 0, member);

				contact_list_local_pending_cb (group, pending,
							       invitor,
							       reason,
							       message, list);

				g_value_array_free (pending_struct);
			}

			g_ptr_array_free (info, TRUE);
			g_array_free (pending, TRUE);
		}
	}
	else if (handle_type == TP_HANDLE_TYPE_GROUP) {
		const gchar *object_path;

		object_path = dbus_g_proxy_get_path (DBUS_G_PROXY (new_chan));
		if (g_hash_table_lookup (priv->groups, object_path)) {
			g_object_unref (new_chan);
			return;
		}

		group = gossip_telepathy_group_new (new_chan, priv->tp_conn);

		gossip_debug (DEBUG_DOMAIN, "New server-side group channel: %s",
			      gossip_telepathy_group_get_name (group));

		dbus_g_proxy_connect_signal (DBUS_G_PROXY (new_chan), "Closed",
					     G_CALLBACK
					     (contact_list_group_channel_closed_cb),
					     list, NULL);

		g_hash_table_insert (priv->groups, g_strdup (object_path), group);
		g_signal_connect (group, "members-added",
				  G_CALLBACK (contact_list_group_members_added_cb),
				  list);
		g_signal_connect (group, "members-removed",
				  G_CALLBACK (contact_list_group_members_removed_cb),
				  list);

		members = gossip_telepathy_group_get_members (group);
		contact_list_group_members_added_cb (group, members, 0,
						     TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
						     NULL, list);
		g_array_free (members, TRUE);
	}

	g_object_unref (new_chan);
}

static ContactListType
contact_list_get_type (EmpathyContactList *list,
		       TpChan             *list_chan)
{
	EmpathyContactListPriv  *priv;
	GArray                  *handles;
	gchar                  **handle_name;
	ContactListType          list_type;
	GError                  *error = NULL;

	priv = GET_PRIV (list);

	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, list_chan->handle);

	if (!tp_conn_inspect_handles (DBUS_G_PROXY (priv->tp_conn),
				      TP_HANDLE_TYPE_LIST,
				      handles,
				      &handle_name,
				      &error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "InspectHandle Error: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		g_array_free (handles, TRUE);
		return CONTACT_LIST_TYPE_UNKNOWN;
	}

	if (strcmp (*handle_name, "subscribe") == 0) {
		list_type = CONTACT_LIST_TYPE_SUBSCRIBE;
	} else if (strcmp (*handle_name, "publish") == 0) {
		list_type = CONTACT_LIST_TYPE_PUBLISH;
	} else if (strcmp (*handle_name, "known") == 0) {
		list_type = CONTACT_LIST_TYPE_KNOWN;
	} else {
		list_type = CONTACT_LIST_TYPE_UNKNOWN;
	}

	g_strfreev (handle_name);
	g_array_free (handles, TRUE);

	return list_type;
}

static void
contact_list_contact_added_cb (GossipTelepathyGroup *group,
			       GArray               *handles,
			       guint                 actor_handle,
			       guint                 reason,
			       const gchar          *message,
			       EmpathyContactList   *list)
{
	EmpathyContactListPriv *priv;
	GList                  *added_list, *l;

	priv = GET_PRIV (list);

	added_list = empathy_contact_list_get_from_handles (list, handles);

	for (l = added_list; l; l = l->next) {
		GossipContact *contact;

		contact = GOSSIP_CONTACT (l->data);
		contact_list_block_contact (list, contact);
		gossip_contact_set_subscription (contact, GOSSIP_SUBSCRIPTION_BOTH);
		contact_list_unblock_contact (list, contact);

		g_signal_emit (list, signals[CONTACT_ADDED], 0, contact);

		g_object_unref (contact);
	}

	g_list_free (added_list);
}

static void
contact_list_contact_removed_cb (GossipTelepathyGroup *group,
				 GArray               *handles,
				 guint                 actor_handle,
				 guint                 reason,
				 const gchar          *message,
				 EmpathyContactList   *list)
{
	EmpathyContactListPriv *priv;
	GList                  *removed_list, *l;

	priv = GET_PRIV (list);

	removed_list = empathy_contact_list_get_from_handles (list, handles);

	for (l = removed_list; l; l = l->next) {
		GossipContact *contact;
		guint          handle;

		contact = GOSSIP_CONTACT (l->data);

		handle = gossip_contact_get_handle (contact);
		g_hash_table_remove (priv->contacts, GUINT_TO_POINTER (handle));

		g_signal_emit (list, signals[CONTACT_REMOVED], 0, contact);

		g_object_unref (contact);
	}

	g_list_free (removed_list);
}

static void
contact_list_local_pending_cb (GossipTelepathyGroup *group,
			       GArray               *handles,
			       guint                 actor_handle,
			       guint                 reason,
			       const gchar          *message,
			       EmpathyContactList   *list)
{
	EmpathyContactListPriv *priv;
	GList                  *pending_list, *l;

	priv = GET_PRIV (list);

	pending_list = empathy_contact_list_get_from_handles (list, handles);

	for (l = pending_list; l; l = l->next) {
		GossipContact *contact;

		contact = GOSSIP_CONTACT (l->data);

		/* FIXME: Is that the correct way ? */
		contact_list_block_contact (list, contact);
		gossip_contact_set_subscription (contact, GOSSIP_SUBSCRIPTION_FROM);
		contact_list_unblock_contact (list, contact);
		g_signal_emit (list, signals[CONTACT_ADDED], 0, contact);

		g_object_unref (contact);
	}

	g_list_free (pending_list);
}

static void
contact_list_groups_updated_cb (GossipContact      *contact,
				GParamSpec         *param,
				EmpathyContactList *list)
{
	EmpathyContactListPriv *priv;
	ContactListData         data;
	GList                  *groups, *l;

	priv = GET_PRIV (list);

	/* Make sure all groups are created */
	groups = gossip_contact_get_groups (contact);
	for (l = groups; l; l = l->next) {
		contact_list_get_group (list, l->data);
	}

	data.handle = gossip_contact_get_handle (contact);
	data.new_groups = groups;

	g_hash_table_foreach (priv->groups,
			      (GHFunc) contact_list_update_groups_foreach,
			      &data);
}

static void
contact_list_subscription_updated_cb (GossipContact      *contact,
				      GParamSpec         *param,
				      EmpathyContactList *list)
{
	EmpathyContactListPriv *priv;
	GossipSubscription      subscription;
	guint                   handle;

	priv = GET_PRIV (list);

	subscription = gossip_contact_get_subscription (contact);
	handle = gossip_contact_get_handle (contact);

	/* FIXME: what to do here, I'm a bit lost... */
	if (subscription) {
		gossip_telepathy_group_add_member (priv->publish, handle, "");
	} else {
		gossip_telepathy_group_remove_member (priv->publish, handle, "");
	}
}

static void
contact_list_name_updated_cb (GossipContact      *contact,
			      GParamSpec         *param,
			      EmpathyContactList *list)
{
	EmpathyContactListPriv *priv;
	GHashTable             *new_alias;
	const gchar            *new_name;
	guint                   handle;
	GError                 *error = NULL;

	priv = GET_PRIV (list);
	
	handle = gossip_contact_get_handle (contact);
	new_name = gossip_contact_get_name (contact);

	gossip_debug (DEBUG_DOMAIN, "renaming handle %d to %s",
		      handle, new_name);

	new_alias = g_hash_table_new_full (g_direct_hash,
					   g_direct_equal,
					   NULL,
					   g_free);

	g_hash_table_insert (new_alias,
			     GUINT_TO_POINTER (handle),
			     g_strdup (new_name));

	if (!tp_conn_iface_aliasing_set_aliases (priv->aliasing_iface,
						 new_alias,
						 &error)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Couldn't rename contact: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}

	g_hash_table_destroy (new_alias);
}

static void
contact_list_update_groups_foreach (gchar                *object_path,
				    GossipTelepathyGroup *group,
				    ContactListData      *data)
{
	gboolean     is_member;
	gboolean     found = FALSE;
	const gchar *group_name;
	GList       *l;

	is_member = gossip_telepathy_group_is_member (group, data->handle);
	group_name = gossip_telepathy_group_get_name (group);

	for (l = data->new_groups; l; l = l->next) {
		if (strcmp (group_name, l->data) == 0) {
			found = TRUE;
			break;
		}
	}

	if (is_member && !found) {
		/* We are no longer member of this group */
		gossip_debug (DEBUG_DOMAIN, "Contact %d removed from group '%s'",
			      data->handle, group_name);
		gossip_telepathy_group_remove_member (group, data->handle, "");
	}

	if (!is_member && found) {
		/* We are now member of this group */
		gossip_debug (DEBUG_DOMAIN, "Contact %d added to group '%s'",
			      data->handle, group_name);
		gossip_telepathy_group_add_member (group, data->handle, "");
	}
}

static GossipTelepathyGroup *
contact_list_get_group (EmpathyContactList *list,
			const gchar        *name)
{
	EmpathyContactListPriv *priv;
	GossipTelepathyGroup   *group;
	TpChan                 *group_channel;
	GArray                 *handles;
	guint                   group_handle;
	char                   *group_object_path;
	const char             *names[2] = {name, NULL};
	GError                 *error = NULL;

	priv = GET_PRIV (list);

	group = g_hash_table_find (priv->groups,
				   (GHRFunc) contact_list_find_group,
				   (gchar*) name);
	if (group) {
		return group;
	}

	gossip_debug (DEBUG_DOMAIN, "creating new group: %s", name);

	if (!tp_conn_request_handles (DBUS_G_PROXY (priv->tp_conn),
				      TP_HANDLE_TYPE_GROUP,
				      names,
				      &handles,
				      &error)) {
		gossip_debug (DEBUG_DOMAIN,
			      "Couldn't request the creation of a new handle for group: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		return NULL;
	}
	group_handle = g_array_index (handles, guint, 0);
	g_array_free (handles, TRUE);

	if (!tp_conn_request_channel (DBUS_G_PROXY (priv->tp_conn),
				      TP_IFACE_CHANNEL_TYPE_CONTACT_LIST,
				      TP_HANDLE_TYPE_GROUP,
				      group_handle,
				      FALSE,
				      &group_object_path,
				      &error)) {
		gossip_debug (DEBUG_DOMAIN,
			      "Couldn't request the creation of a new group channel: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		return NULL;
	}

	group_channel = tp_chan_new (tp_get_bus (),
				     dbus_g_proxy_get_bus_name (DBUS_G_PROXY (priv->tp_conn)),
				     group_object_path,
				     TP_IFACE_CHANNEL_TYPE_CONTACT_LIST,
				     TP_HANDLE_TYPE_GROUP,
				     group_handle);

	dbus_g_proxy_connect_signal (DBUS_G_PROXY (group_channel),
				     "Closed",
				     G_CALLBACK
				     (contact_list_group_channel_closed_cb),
				     list,
				     NULL);

	group = gossip_telepathy_group_new (group_channel, priv->tp_conn);
	g_hash_table_insert (priv->groups, group_object_path, group);
	g_signal_connect (group, "members-added",
			  G_CALLBACK (contact_list_group_members_added_cb),
			  list);
	g_signal_connect (group, "members-removed",
			  G_CALLBACK (contact_list_group_members_removed_cb),
			  list);

	return group;
}

static gboolean
contact_list_find_group (gchar                 *key,
			 GossipTelepathyGroup  *group,
			 gchar                 *group_name)
{
	if (strcmp (group_name, gossip_telepathy_group_get_name (group)) == 0) {
		return TRUE;
	}

	return FALSE;
}

static void
contact_list_get_groups_foreach (gchar                 *key,
				 GossipTelepathyGroup  *group,
				 GList                **groups)
{
	const gchar *name;

	name = gossip_telepathy_group_get_name (group);
	*groups = g_list_append (*groups, g_strdup (name));
}

static void
contact_list_group_channel_closed_cb (TpChan             *channel,
				      EmpathyContactList *list)
{
	EmpathyContactListPriv *priv;

	priv = GET_PRIV (list);

	g_hash_table_remove (priv->groups,
			     dbus_g_proxy_get_path (DBUS_G_PROXY (channel)));
}

static void
contact_list_group_members_added_cb (GossipTelepathyGroup *group,
				     GArray               *members,
				     guint                 actor_handle,
				     guint                 reason,
				     const gchar          *message,
				     EmpathyContactList   *list)
{
	EmpathyContactListPriv *priv;
	GList                  *added_list, *l;
	const gchar            *group_name;

	priv = GET_PRIV (list);

	group_name = gossip_telepathy_group_get_name (group);
	added_list = empathy_contact_list_get_from_handles (list, members);

	for (l = added_list; l; l = l->next) {
		GossipContact *contact;
		GList         *contact_groups;

		contact = GOSSIP_CONTACT (l->data);
		contact_groups = gossip_contact_get_groups (contact);

		if (!g_list_find_custom (contact_groups,
					 group_name,
					 (GCompareFunc) strcmp)) {
			gossip_debug (DEBUG_DOMAIN, "Contact %s added to group '%s'",
				      gossip_contact_get_name (contact),
				      group_name);
			contact_groups = g_list_append (contact_groups,
							g_strdup (group_name));
			contact_list_block_contact (list, contact);
			gossip_contact_set_groups (contact, contact_groups);
			contact_list_unblock_contact (list, contact);
		}

		g_object_unref (contact);
	}

	g_list_free (added_list);
}

static void
contact_list_group_members_removed_cb (GossipTelepathyGroup *group,
				       GArray               *members,
				       guint                 actor_handle,
				       guint                 reason,
				       const gchar          *message,
				       EmpathyContactList   *list)
{
	EmpathyContactListPriv *priv;
	GList                  *removed_list, *l;
	const gchar            *group_name;

	priv = GET_PRIV (list);

	group_name = gossip_telepathy_group_get_name (group);
	removed_list = empathy_contact_list_get_from_handles (list, members);

	for (l = removed_list; l; l = l->next) {
		GossipContact *contact;
		GList         *contact_groups;
		GList         *to_remove;

		/* FIXME: Does it leak ? */
		contact = GOSSIP_CONTACT (l->data);
		contact_groups = gossip_contact_get_groups (contact);
		contact_groups = g_list_copy (contact_groups);

		to_remove = g_list_find_custom (contact_groups,
						group_name,
						(GCompareFunc) strcmp);
		if (to_remove) {
			gossip_debug (DEBUG_DOMAIN, "Contact %d removed from group '%s'",
				      gossip_contact_get_handle (contact),
				      group_name);
			contact_groups = g_list_remove_link (contact_groups,
							     to_remove);
			contact_list_block_contact (list, contact);
			gossip_contact_set_groups (contact, contact_groups);
			contact_list_unblock_contact (list, contact);
		}

		g_list_free (contact_groups);

		g_object_unref (contact);
	}

	g_list_free (removed_list);
}

static void
contact_list_get_contacts_foreach (guint           handle,
				   GossipContact  *contact,
				   GList         **contacts)
{
	*contacts = g_list_append (*contacts, g_object_ref (contact));
}

static void
contact_list_get_info (EmpathyContactList *list,
		       GArray             *handles)
{
	EmpathyContactListPriv *priv;
	GError                 *error = NULL;

	priv = GET_PRIV (list);

	if (priv->presence_iface) {
		/* FIXME: We should use GetPresence instead */
		if (!tp_conn_iface_presence_request_presence (priv->presence_iface,
							      handles, &error)) {
			gossip_debug (DEBUG_DOMAIN, 
				      "Could not request presences: %s",
				      error ? error->message : "No error given");
			g_clear_error (&error);
		}
	}

	if (priv->aliasing_iface) {
		ContactListAliasesRequestData *data;

		data = g_slice_new (ContactListAliasesRequestData);
		data->list = list;
		data->handles = g_memdup (handles->data, handles->len * sizeof (guint));

		tp_conn_iface_aliasing_request_aliases_async (priv->aliasing_iface,
							      handles,
							      (tp_conn_iface_aliasing_request_aliases_reply)
							      contact_list_request_aliases_cb,
							      data);
	}

	if (priv->avatars_iface) {
		guint i;

		for (i = 0; i < handles->len; i++) {
			guint handle;

			handle = g_array_index (handles, gint, i);
			contact_list_request_avatar (list, handle);
		}
	}
}

static void
contact_list_request_avatar (EmpathyContactList *list,
			     guint               handle)
{
	EmpathyContactListPriv *priv;

	priv = GET_PRIV (list);
	
	/* We queue avatar requests to not send too many dbus async
	 * calls at once. If we don't we reach the dbus's limit of
	 * pending calls */
	priv->avatar_requests_queue = g_list_append (priv->avatar_requests_queue,
						     GUINT_TO_POINTER (handle));
	contact_list_start_avatar_requests (list);
}

static void
contact_list_start_avatar_requests (EmpathyContactList *list)
{
	EmpathyContactListPriv       *priv;
	ContactListAvatarRequestData *data;

	priv = GET_PRIV (list);

	while (n_avatar_requests <  MAX_AVATAR_REQUESTS &&
	       priv->avatar_requests_queue) {
		data = g_slice_new (ContactListAvatarRequestData);
		data->list = list;
		data->handle = GPOINTER_TO_UINT (priv->avatar_requests_queue->data);

		n_avatar_requests++;
		priv->avatar_requests_queue = g_list_remove (priv->avatar_requests_queue,
							     priv->avatar_requests_queue->data);

		tp_conn_iface_avatars_request_avatar_async (priv->avatars_iface,
							    data->handle,
							    (tp_conn_iface_avatars_request_avatar_reply)
							    contact_list_request_avatar_cb,
							    data);
	}
}

static void
contact_list_avatar_update_cb (DBusGProxy         *proxy,
			       guint               handle,
			       gchar              *new_token,
			       EmpathyContactList *list)
{
	gossip_debug (DEBUG_DOMAIN, "Changing avatar for %d to %s",
		      handle, new_token);

	contact_list_request_avatar (list, handle);
}

static void
contact_list_request_avatar_cb (DBusGProxy                   *proxy,
				GArray                       *avatar_data,
				gchar                        *mime_type,
				GError                       *error,
				ContactListAvatarRequestData *data)
{
	GossipContact *contact;

	contact = empathy_contact_list_get_from_handle (data->list, data->handle);

	if (error) {
		gossip_debug (DEBUG_DOMAIN, "Error requesting avatar for %s: %s",
			      gossip_contact_get_name (contact),
			      error ? error->message : "No error given");
	} else {
		GossipAvatar *avatar;

		avatar = gossip_avatar_new (avatar_data->data,
					    avatar_data->len,
					    mime_type);
		contact_list_block_contact (data->list, contact);
		gossip_contact_set_avatar (contact, avatar);
		contact_list_unblock_contact (data->list, contact);
		gossip_avatar_unref (avatar);
	}

	n_avatar_requests--;
	contact_list_start_avatar_requests (data->list);

	g_slice_free (ContactListAvatarRequestData, data);
}

static void
contact_list_aliases_update_cb (DBusGProxy         *proxy,
				GPtrArray          *renamed_handlers,
				EmpathyContactList *list)
{
	gint i;

	for (i = 0; renamed_handlers->len > i; i++) {
		guint          handle;
		const gchar   *alias;
		GValueArray   *renamed_struct;
		GossipContact *contact;

		renamed_struct = g_ptr_array_index (renamed_handlers, i);
		handle = g_value_get_uint(g_value_array_get_nth (renamed_struct, 0));
		alias = g_value_get_string(g_value_array_get_nth (renamed_struct, 1));

		if (alias && *alias == '\0') {
			alias = NULL;
		}

		contact = empathy_contact_list_get_from_handle (list, handle);
		contact_list_block_contact (list, contact);
		gossip_contact_set_name (contact, alias);
		contact_list_unblock_contact (list, contact);

		gossip_debug (DEBUG_DOMAIN, "contact %d renamed to %s (update cb)",
			      handle, alias);
	}
}

static void
contact_list_request_aliases_cb (DBusGProxy                     *proxy,
				 gchar                         **contact_names,
				 GError                         *error,
				 ContactListAliasesRequestData  *data)
{
	guint   i = 0;
	gchar **name;

	for (name = contact_names; *name && !error; name++) {
		GossipContact *contact;

		contact = empathy_contact_list_get_from_handle (data->list,
								data->handles[i]);
		contact_list_block_contact (data->list, contact);
		gossip_contact_set_name (contact, *name);
		contact_list_unblock_contact (data->list, contact);

		gossip_debug (DEBUG_DOMAIN, "contact %d renamed to %s (request cb)",
			      data->handles[i], *name);

		i++;
	}

	g_free (data->handles);
	g_slice_free (ContactListAliasesRequestData, data);
}

static void
contact_list_presence_update_cb (DBusGProxy         *proxy,
				 GHashTable         *handle_table,
				 EmpathyContactList *list)
{
	g_hash_table_foreach (handle_table,
			      (GHFunc) contact_list_parse_presence_foreach,
			      list);
}

static void
contact_list_parse_presence_foreach (guint               handle,
				     GValueArray        *presence_struct,
				     EmpathyContactList *list)
{
	GHashTable     *presences_table;
	GossipContact  *contact;
	GossipPresence *presence = NULL;

	contact = empathy_contact_list_get_from_handle (list, handle);
	presences_table = g_value_get_boxed (g_value_array_get_nth (presence_struct, 1));

	g_hash_table_foreach (presences_table,
			      (GHFunc) contact_list_presences_table_foreach,
			      &presence);

	gossip_debug (DEBUG_DOMAIN, "Presence changed for %s (%d)",
		      gossip_contact_get_name (contact),
		      handle);

	contact_list_block_contact (list, contact);
	if (presence) {
		gossip_contact_add_presence (contact, presence);
		g_object_unref (presence);
	} else {
		g_object_set (contact, "presences", NULL, NULL);
	}
	contact_list_unblock_contact (list, contact);
}

static void
contact_list_presences_table_foreach (const gchar     *state_str,
				      GHashTable      *presences_table,
				      GossipPresence **presence)
{
	GossipPresenceState  state;
	const GValue        *message;

	if (*presence) {
		g_object_unref (*presence);
		*presence = NULL;
	}

	state = contact_list_presence_state_from_str (state_str);
	if (state == GOSSIP_PRESENCE_STATE_UNAVAILABLE) {
		return;
	}

	*presence = gossip_presence_new ();
	gossip_presence_set_state (*presence, state);

	message = g_hash_table_lookup (presences_table, "message");
	if (message != NULL) {
		gossip_presence_set_status (*presence,
					    g_value_get_string (message));
	}

	gossip_presence_set_resource (*presence, "");
}

static GossipPresenceState
contact_list_presence_state_from_str (const gchar *str)
{
	if (strcmp (str, "available") == 0) {
		return GOSSIP_PRESENCE_STATE_AVAILABLE;
	} else if ((strcmp (str, "dnd") == 0) || (strcmp (str, "busy") == 0)) {
		return GOSSIP_PRESENCE_STATE_BUSY;
	} else if ((strcmp (str, "away") == 0) || (strcmp (str, "brb") == 0)) {
		return GOSSIP_PRESENCE_STATE_AWAY;
	} else if (strcmp (str, "xa") == 0) {
		return GOSSIP_PRESENCE_STATE_EXT_AWAY;
	} else if (strcmp (str, "hidden") == 0) {
		return GOSSIP_PRESENCE_STATE_HIDDEN;
	} else if (strcmp (str, "offline") == 0) {
		return GOSSIP_PRESENCE_STATE_UNAVAILABLE;
	} else if (strcmp (str, "chat") == 0) {
		/* We don't support chat, so treat it like available. */
		return GOSSIP_PRESENCE_STATE_AVAILABLE;
	}

	return GOSSIP_PRESENCE_STATE_AVAILABLE;
}

