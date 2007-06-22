/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Xavier Claessens <xclaesse@gmail.com>
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
#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-chan-type-contact-list-gen.h>
#include <libtelepathy/tp-conn-iface-aliasing-gen.h>
#include <libtelepathy/tp-conn-iface-presence-gen.h>
#include <libtelepathy/tp-conn-iface-avatars-gen.h>

#include "empathy-tp-contact-list.h"
#include "empathy-contact-list.h"
#include "empathy-tp-group.h"
#include "empathy-debug.h"
#include "empathy-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_TP_CONTACT_LIST, EmpathyTpContactListPriv))

#define DEBUG_DOMAIN "TpContactList"
#define MAX_AVATAR_REQUESTS 10

struct _EmpathyTpContactListPriv {
	TpConn         *tp_conn;
	McAccount      *account;
	MissionControl *mc;
	EmpathyContact *user_contact;
	gboolean        setup;

	EmpathyTpGroup *publish;
	EmpathyTpGroup *subscribe;

	GHashTable     *groups;
	GHashTable     *contacts;
	GList          *members;
	GList          *local_pending;

	DBusGProxy     *aliasing_iface;
	DBusGProxy     *avatars_iface;
	DBusGProxy     *presence_iface;

	GList          *avatar_requests_queue;
};

typedef enum {
	TP_CONTACT_LIST_TYPE_PUBLISH,
	TP_CONTACT_LIST_TYPE_SUBSCRIBE,
	TP_CONTACT_LIST_TYPE_UNKNOWN,
	TP_CONTACT_LIST_TYPE_COUNT
} TpContactListType;

typedef struct {
	guint  handle;
	GList *new_groups;
} TpContactListData;

typedef struct {
	EmpathyTpContactList *list;
	guint                 handle;
} TpContactListAvatarRequestData;

typedef struct {
	EmpathyTpContactList *list;
	guint                *handles;
} TpContactListAliasesRequestData;

static void               empathy_tp_contact_list_class_init       (EmpathyTpContactListClass       *klass);
static void               tp_contact_list_iface_init               (EmpathyContactListIface         *iface);
static void               empathy_tp_contact_list_init             (EmpathyTpContactList            *list);
static void               tp_contact_list_finalize                 (GObject                         *object);
static void               tp_contact_list_finalize_proxies         (EmpathyTpContactList            *list);
static void               tp_contact_list_setup                    (EmpathyContactList              *list);
static EmpathyContact *   tp_contact_list_find                     (EmpathyContactList              *list,
								    const gchar                     *id);
static void               tp_contact_list_add                      (EmpathyContactList              *list,
								    EmpathyContact                  *contact,
								    const gchar                     *message);
static void               tp_contact_list_remove                   (EmpathyContactList              *list,
								    EmpathyContact                  *contact,
								    const gchar                     *message);
static GList *            tp_contact_list_get_members              (EmpathyContactList              *list);
static GList *            tp_contact_list_get_local_pending        (EmpathyContactList              *list);
static void               tp_contact_list_process_pending          (EmpathyContactList              *list,
								    EmpathyContact                  *contact,
								    gboolean                         accept);
static void               tp_contact_list_remove_local_pending     (EmpathyTpContactList            *list,
								    EmpathyContact                  *contact);
static void               tp_contact_list_contact_removed_foreach  (guint                            handle,
								    EmpathyContact                  *contact,
								    EmpathyTpContactList            *list);
static void               tp_contact_list_destroy_cb               (DBusGProxy                      *proxy,
								    EmpathyTpContactList            *list);
static gboolean           tp_contact_list_find_foreach             (guint                            handle,
								    EmpathyContact                  *contact,
								    gchar                           *id);
static void               tp_contact_list_newchannel_cb            (DBusGProxy                      *proxy,
								    const gchar                     *object_path,
								    const gchar                     *channel_type,
								    TelepathyHandleType              handle_type,
								    guint                            channel_handle,
								    gboolean                         suppress_handle,
								    EmpathyTpContactList            *list);
static TpContactListType  tp_contact_list_get_type                 (EmpathyTpContactList            *list,
								    EmpathyTpGroup                  *group);
static void               tp_contact_list_added_cb                 (EmpathyTpGroup                  *group,
								    GArray                          *handles,
								    guint                            actor_handle,
								    guint                            reason,
								    const gchar                     *message,
								    EmpathyTpContactList            *list);
static void               tp_contact_list_removed_cb               (EmpathyTpGroup                  *group,
								    GArray                          *handles,
								    guint                            actor_handle,
								    guint                            reason,
								    const gchar                     *message,
								    EmpathyTpContactList            *list);
static void               tp_contact_list_pending_cb               (EmpathyTpGroup                  *group,
								    GArray                          *handles,
								    guint                            actor_handle,
								    guint                            reason,
								    const gchar                     *message,
								    EmpathyTpContactList            *list);
static void               tp_contact_list_groups_updated_cb        (EmpathyContact                  *contact,
								    GParamSpec                      *param,
								    EmpathyTpContactList            *list);
static void               tp_contact_list_name_updated_cb          (EmpathyContact                  *contact,
								    GParamSpec                      *param,
								    EmpathyTpContactList            *list);
static void               tp_contact_list_update_groups_foreach    (gchar                           *object_path,
								    EmpathyTpGroup                  *group,
								    TpContactListData               *data);
static EmpathyTpGroup *   tp_contact_list_get_group                (EmpathyTpContactList            *list,
								    const gchar                     *name);
static gboolean           tp_contact_list_find_group               (gchar                           *key,
								    EmpathyTpGroup                  *group,
								    gchar                           *group_name);
static void               tp_contact_list_get_groups_foreach       (gchar                           *key,
								    EmpathyTpGroup                  *group,
								    GList                          **groups);
static void               tp_contact_list_group_channel_closed_cb  (TpChan                          *channel,
								    EmpathyTpContactList            *list);
static void               tp_contact_list_group_members_added_cb   (EmpathyTpGroup                  *group,
								    GArray                          *members,
								    guint                            actor_handle,
								    guint                            reason,
								    const gchar                     *message,
								    EmpathyTpContactList            *list);
static void               tp_contact_list_group_members_removed_cb (EmpathyTpGroup                  *group,
								    GArray                          *members,
								    guint                            actor_handle,
								    guint                            reason,
								    const gchar                     *message,
								    EmpathyTpContactList            *list);
static void               tp_contact_list_get_info                 (EmpathyTpContactList            *list,
								    GArray                          *handles);
static void               tp_contact_list_request_avatar           (EmpathyTpContactList            *list,
								    guint                            handle);
static void               tp_contact_list_start_avatar_requests    (EmpathyTpContactList            *list);
static void               tp_contact_list_avatar_update_cb         (DBusGProxy                      *proxy,
								    guint                            handle,
								    gchar                           *new_token,
								    EmpathyTpContactList            *list);
static void               tp_contact_list_request_avatar_cb        (DBusGProxy                      *proxy,
								    GArray                          *avatar_data,
								    gchar                           *mime_type,
								    GError                          *error,
								    TpContactListAvatarRequestData  *data);
static void               tp_contact_list_aliases_update_cb        (DBusGProxy                      *proxy,
								    GPtrArray                       *handlers,
								    EmpathyTpContactList            *list);
static void               tp_contact_list_request_aliases_cb       (DBusGProxy                      *proxy,
								    gchar                          **contact_names,
								    GError                          *error,
								    TpContactListAliasesRequestData *data);
static void               tp_contact_list_presence_update_cb       (DBusGProxy                      *proxy,
								    GHashTable                      *handle_table,
								    EmpathyTpContactList            *list);
static void               tp_contact_list_parse_presence_foreach   (guint                            handle,
								    GValueArray                     *presence_struct,
								    EmpathyTpContactList            *list);
static void               tp_contact_list_presences_table_foreach  (const gchar                     *state_str,
								    GHashTable                      *presences_table,
								    EmpathyPresence                 **presence);
static void               tp_contact_list_status_changed_cb        (MissionControl                  *mc,
								    TelepathyConnectionStatus        status,
								    McPresence                       presence,
								    TelepathyConnectionStatusReason  reason,
								    const gchar                     *unique_name,
								    EmpathyTpContactList            *list);

enum {
	DESTROY,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static guint n_avatar_requests = 0;

G_DEFINE_TYPE_WITH_CODE (EmpathyTpContactList, empathy_tp_contact_list, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (EMPATHY_TYPE_CONTACT_LIST,
						tp_contact_list_iface_init));

static void
empathy_tp_contact_list_class_init (EmpathyTpContactListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tp_contact_list_finalize;

	signals[DESTROY] =
		g_signal_new ("destroy",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_type_class_add_private (object_class, sizeof (EmpathyTpContactListPriv));
}

static void
tp_contact_list_iface_init (EmpathyContactListIface *iface)
{
	iface->setup             = tp_contact_list_setup;
	iface->find              = tp_contact_list_find;
	iface->add               = tp_contact_list_add;
	iface->remove            = tp_contact_list_remove;
	iface->get_members       = tp_contact_list_get_members;
	iface->get_local_pending = tp_contact_list_get_local_pending;
	iface->process_pending   = tp_contact_list_process_pending;
}

static void
empathy_tp_contact_list_init (EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;

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
tp_contact_list_finalize (GObject *object)
{
	EmpathyTpContactListPriv *priv;
	EmpathyTpContactList     *list;

	list = EMPATHY_TP_CONTACT_LIST (object);
	priv = GET_PRIV (list);

	empathy_debug (DEBUG_DOMAIN, "finalize: %p", object);

	dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (priv->mc),
					"AccountStatusChanged",
					G_CALLBACK (tp_contact_list_status_changed_cb),
					list);

	tp_contact_list_finalize_proxies (list);

	if (priv->tp_conn) {
		g_object_unref (priv->tp_conn);
	}
	if (priv->subscribe) {
		g_object_unref (priv->subscribe);
	}
	if (priv->publish) {
		g_object_unref (priv->publish);
	}

	g_object_unref (priv->account);
	g_object_unref (priv->user_contact);
	g_object_unref (priv->mc);
	g_hash_table_destroy (priv->groups);
	g_hash_table_destroy (priv->contacts);

	g_list_foreach (priv->local_pending, (GFunc) empathy_contact_list_info_free, NULL);
	g_list_free (priv->local_pending);

	g_list_foreach (priv->members, (GFunc) g_object_unref, NULL);
	g_list_free (priv->members);

	G_OBJECT_CLASS (empathy_tp_contact_list_parent_class)->finalize (object);
}

EmpathyTpContactList *
empathy_tp_contact_list_new (McAccount *account)
{
	EmpathyTpContactListPriv *priv;
	EmpathyTpContactList     *list;
	MissionControl           *mc;
	guint                     handle;
	GError                   *error = NULL;

	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

	mc = empathy_mission_control_new ();

	if (mission_control_get_connection_status (mc, account, NULL) != 0) {
		/* The account is not connected, nothing to do. */
		return NULL;
	}

	list = g_object_new (EMPATHY_TYPE_TP_CONTACT_LIST, NULL);
	priv = GET_PRIV (list);

	priv->tp_conn = mission_control_get_connection (mc, account, NULL);
	priv->account = g_object_ref (account);
	priv->mc = mc;

	g_signal_connect (priv->tp_conn, "destroy",
			  G_CALLBACK (tp_contact_list_destroy_cb),
			  list);
	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->mc),
				     "AccountStatusChanged",
				     G_CALLBACK (tp_contact_list_status_changed_cb),
				     list, NULL);

	priv->aliasing_iface = tp_conn_get_interface (priv->tp_conn,
						      TELEPATHY_CONN_IFACE_ALIASING_QUARK);
	priv->avatars_iface = tp_conn_get_interface (priv->tp_conn,
						     TELEPATHY_CONN_IFACE_AVATARS_QUARK);
	priv->presence_iface = tp_conn_get_interface (priv->tp_conn,
						      TELEPATHY_CONN_IFACE_PRESENCE_QUARK);

	if (priv->aliasing_iface) {
		dbus_g_proxy_connect_signal (priv->aliasing_iface,
					     "AliasesChanged",
					     G_CALLBACK (tp_contact_list_aliases_update_cb),
					     list, NULL);
	}

	if (priv->avatars_iface) {
		dbus_g_proxy_connect_signal (priv->avatars_iface,
					     "AvatarUpdated",
					     G_CALLBACK (tp_contact_list_avatar_update_cb),
					     list, NULL);
	}

	if (priv->presence_iface) {
		dbus_g_proxy_connect_signal (priv->presence_iface,
					     "PresenceUpdate",
					     G_CALLBACK (tp_contact_list_presence_update_cb),
					     list, NULL);
	}

	/* Get our own handle and contact */
	if (!tp_conn_get_self_handle (DBUS_G_PROXY (priv->tp_conn),
				      &handle, &error)) {
		empathy_debug (DEBUG_DOMAIN, "GetSelfHandle Error: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	} else {
		priv->user_contact = empathy_tp_contact_list_get_from_handle (list, handle);
		empathy_contact_set_is_user (priv->user_contact, TRUE);
	}

	return list;
}

static void
tp_contact_list_setup (EmpathyContactList *list)
{
	EmpathyTpContactListPriv *priv;
	GPtrArray                *channels;
	GError                   *error = NULL;
	guint                     i;

	g_return_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list));

	priv = GET_PRIV (list);

	empathy_debug (DEBUG_DOMAIN, "setup contact list: %p", list);

	priv->setup = TRUE;
	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->tp_conn), "NewChannel",
				     G_CALLBACK (tp_contact_list_newchannel_cb),
				     list, NULL);

	/* Get existing channels */
	if (!tp_conn_list_channels (DBUS_G_PROXY (priv->tp_conn),
				    &channels,
				    &error)) {
		empathy_debug (DEBUG_DOMAIN,
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

		tp_contact_list_newchannel_cb (DBUS_G_PROXY (priv->tp_conn),
					       object_path, chan_iface,
					       handle_type, handle,
					       FALSE,
					       EMPATHY_TP_CONTACT_LIST (list));

		g_value_array_free (chan_struct);
	}

	g_ptr_array_free (channels, TRUE);
}

static EmpathyContact *
tp_contact_list_find (EmpathyContactList *list,
		      const gchar        *id)
{
	EmpathyTpContactListPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);

	return g_hash_table_find (priv->contacts,
				  (GHRFunc) tp_contact_list_find_foreach,
				  (gchar*) id);
}

static void
tp_contact_list_add (EmpathyContactList *list,
		     EmpathyContact      *contact,
		     const gchar        *message)
{
	EmpathyTpContactListPriv *priv;
	guint                     handle;

	g_return_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list));

	priv = GET_PRIV (list);

	handle = empathy_contact_get_handle (contact);
	empathy_tp_group_add_member (priv->subscribe, handle, message);
}

static void
tp_contact_list_remove (EmpathyContactList *list,
			EmpathyContact      *contact,
			const gchar        *message)
{
	EmpathyTpContactListPriv *priv;
	guint                     handle;

	g_return_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list));

	priv = GET_PRIV (list);

	handle = empathy_contact_get_handle (contact);
	empathy_tp_group_remove_member (priv->subscribe, handle, message);
}

static GList *
tp_contact_list_get_members (EmpathyContactList *list)
{
	EmpathyTpContactListPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);

	g_list_foreach (priv->members, (GFunc) g_object_ref, NULL);
	return g_list_copy (priv->members);
}

static GList *
tp_contact_list_get_local_pending (EmpathyContactList *list)
{
	EmpathyTpContactListPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);

	return g_list_copy (priv->local_pending);
}

static void
tp_contact_list_process_pending (EmpathyContactList *list,
				 EmpathyContact      *contact,
				 gboolean            accept)
{
	EmpathyTpContactListPriv *priv;
	guint                     handle;

	g_return_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	priv = GET_PRIV (list);

	handle = empathy_contact_get_handle (contact);
	if (accept) {
		empathy_tp_group_add_member (priv->publish, handle, NULL);
	} else {
		empathy_tp_group_remove_member (priv->publish, handle, NULL);
	}
}

McAccount *
empathy_tp_contact_list_get_account (EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);

	return priv->account;
}

EmpathyContact *
empathy_tp_contact_list_get_user (EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);
	
	return priv->user_contact;
}

EmpathyContact *
empathy_tp_contact_list_get_from_id (EmpathyTpContactList *list,
				     const gchar          *id)
{
	EmpathyTpContactListPriv *priv;
	EmpathyContact            *contact;
	const gchar              *contact_ids[] = {id, NULL};
	GArray                   *handles;
	guint                     handle;
	GError                   *error = NULL;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);
	g_return_val_if_fail (id != NULL, NULL);
	
	priv = GET_PRIV (list);

	contact = tp_contact_list_find (EMPATHY_CONTACT_LIST (list), id);
	if (contact) {
		return g_object_ref (contact);
	}

	/* The id is unknown, requests a new handle */
	if (!tp_conn_request_handles (DBUS_G_PROXY (priv->tp_conn),
				      TP_HANDLE_TYPE_CONTACT,
				      contact_ids,
				      &handles, &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "RequestHandle for %s failed: %s", id,
			      error ? error->message : "No error given");
		g_clear_error (&error);
		return NULL;
	}

	handle = g_array_index(handles, guint, 0);
	g_array_free (handles, TRUE);

	return empathy_tp_contact_list_get_from_handle (list, handle);
}

EmpathyContact *
empathy_tp_contact_list_get_from_handle (EmpathyTpContactList *list,
					 guint                 handle)
{
	EmpathyContact *contact;
	GArray        *handles;
	GList         *contacts;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);

	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);

	contacts = empathy_tp_contact_list_get_from_handles (list, handles);
	g_array_free (handles, TRUE);

	if (!contacts) {
		return NULL;
	}

	contact = contacts->data;
	g_list_free (contacts);

	return contact;
}

GList *
empathy_tp_contact_list_get_from_handles (EmpathyTpContactList *list,
					  GArray               *handles)
{
	EmpathyTpContactListPriv  *priv;
	gchar                    **handles_names;
	gchar                    **id;
	GArray                    *new_handles;
	GList                     *contacts = NULL;
	guint                      i;
	GError                    *error = NULL;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);
	g_return_val_if_fail (handles != NULL, NULL);

	priv = GET_PRIV (list);

	/* Search all handles we already have */
	new_handles = g_array_new (FALSE, FALSE, sizeof (guint));
	for (i = 0; i < handles->len; i++) {
		EmpathyContact *contact;
		guint          handle;

		handle = g_array_index (handles, guint, i);

		if (handle == 0) {
			continue;
		}

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
		g_array_free (new_handles, TRUE);
		return contacts;
	}

	/* Holds all handles we don't have yet.
	 * FIXME: We should release them at some point. */
	if (!tp_conn_hold_handles (DBUS_G_PROXY (priv->tp_conn),
				   TP_HANDLE_TYPE_CONTACT,
				   new_handles, &error)) {
		empathy_debug (DEBUG_DOMAIN, 
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
		empathy_debug (DEBUG_DOMAIN, 
			      "InspectHandle Error: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		g_array_free (new_handles, TRUE);
		return contacts;
	}

	/* Create contact objects */
	for (i = 0, id = handles_names; *id && i < new_handles->len; id++, i++) {
		EmpathyContact *contact;
		guint          handle;

		handle = g_array_index (new_handles, guint, i);
		contact = g_object_new (EMPATHY_TYPE_CONTACT,
					"account", priv->account,
					"id", *id,
					"handle", handle,
					NULL);

		if (!priv->presence_iface) {
			EmpathyPresence *presence;

			/* We have no presence iface, set default presence
			 * to available */
			presence = empathy_presence_new_full (MC_PRESENCE_AVAILABLE,
							     NULL);

			empathy_contact_set_presence (contact, presence);
			g_object_unref (presence);
		}

		g_signal_connect (contact, "notify::groups",
				  G_CALLBACK (tp_contact_list_groups_updated_cb),
				  list);
		g_signal_connect (contact, "notify::name",
				  G_CALLBACK (tp_contact_list_name_updated_cb),
				  list);

		empathy_debug (DEBUG_DOMAIN, "new contact created: %s (%d)",
			      *id, handle);

		g_hash_table_insert (priv->contacts,
				     GUINT_TO_POINTER (handle),
				     contact);

		contacts = g_list_prepend (contacts, g_object_ref (contact));
	}

	tp_contact_list_get_info (list, new_handles);

	g_array_free (new_handles, TRUE);
	g_strfreev (handles_names);

	return contacts;
}

void
empathy_tp_contact_list_rename_group (EmpathyTpContactList *list,
				      const gchar          *old_group,
				      const gchar          *new_group)
{
	EmpathyTpContactListPriv *priv;
	EmpathyTpGroup           *group;
	GArray                   *members;

	g_return_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list));
	g_return_if_fail (old_group != NULL);
	g_return_if_fail (new_group != NULL);

	priv = GET_PRIV (list);

	group = g_hash_table_find (priv->groups,
				   (GHRFunc) tp_contact_list_find_group,
				   (gchar*) old_group);
	if (!group) {
		/* The group doesn't exists on this account */
		return;
	}

	empathy_debug (DEBUG_DOMAIN, "rename group %s to %s", group, new_group);

	/* Remove all members from the old group */
	members = empathy_tp_group_get_members (group);
	empathy_tp_group_remove_members (group, members, "");
	tp_contact_list_group_members_removed_cb (group, members, 
						  0, 
						  TP_CHANNEL_GROUP_CHANGE_REASON_NONE, 
						  NULL, list);
	g_hash_table_remove (priv->groups,
			     empathy_tp_group_get_object_path (group));

	/* Add all members to the new group */
	group = tp_contact_list_get_group (list, new_group);
	if (group) {
		empathy_tp_group_add_members (group, members, "");
	}
}

GList *
empathy_tp_contact_list_get_groups (EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;
	GList                    *groups = NULL;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);

	g_hash_table_foreach (priv->groups,
			      (GHFunc) tp_contact_list_get_groups_foreach,
			      &groups);

	groups = g_list_sort (groups, (GCompareFunc) strcmp);

	return groups;
}

static void
tp_contact_list_finalize_proxies (EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;

	priv = GET_PRIV (list);

	if (priv->tp_conn) {
		g_signal_handlers_disconnect_by_func (priv->tp_conn,
						      tp_contact_list_destroy_cb,
						      list);
		dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (priv->tp_conn), "NewChannel",
						G_CALLBACK (tp_contact_list_newchannel_cb),
						list);
	}

	if (priv->aliasing_iface) {
		dbus_g_proxy_disconnect_signal (priv->aliasing_iface,
						"AliasesChanged",
						G_CALLBACK (tp_contact_list_aliases_update_cb),
						list);
	}

	if (priv->avatars_iface) {
		dbus_g_proxy_disconnect_signal (priv->avatars_iface,
						"AvatarUpdated",
						G_CALLBACK (tp_contact_list_avatar_update_cb),
						list);
	}

	if (priv->presence_iface) {
		dbus_g_proxy_disconnect_signal (priv->presence_iface,
						"PresenceUpdate",
						G_CALLBACK (tp_contact_list_presence_update_cb),
						list);
	}
}

static void
tp_contact_list_destroy_cb (DBusGProxy           *proxy,
			    EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;

	priv = GET_PRIV (list);

	empathy_debug (DEBUG_DOMAIN, "Connection destroyed... "
		      "Account disconnected or CM crashed");

	/* DBus proxies should NOT be used anymore */
	g_object_unref (priv->tp_conn);
	priv->tp_conn = NULL;
	priv->aliasing_iface = NULL;
	priv->avatars_iface = NULL;
	priv->presence_iface = NULL;

	/* Remove all contacts */
	g_hash_table_foreach (priv->contacts,
			      (GHFunc) tp_contact_list_contact_removed_foreach,
			      list);
	g_hash_table_remove_all (priv->contacts);

	/* Tell the world to not use us anymore */
	g_signal_emit (list, signals[DESTROY], 0);
}

static void
tp_contact_list_contact_removed_foreach (guint                 handle,
					 EmpathyContact        *contact,
					 EmpathyTpContactList *list)
{
	g_signal_handlers_disconnect_by_func (contact,
					      tp_contact_list_groups_updated_cb,
					      list);
	g_signal_handlers_disconnect_by_func (contact,
					      tp_contact_list_name_updated_cb,
					      list);

	g_signal_emit_by_name (list, "contact-removed", contact);
}

static void
tp_contact_list_block_contact (EmpathyTpContactList *list,
			       EmpathyContact        *contact)
{
	g_signal_handlers_block_by_func (contact,
					 tp_contact_list_groups_updated_cb,
					 list);
	g_signal_handlers_block_by_func (contact,
					 tp_contact_list_name_updated_cb,
					 list);
}

static void
tp_contact_list_unblock_contact (EmpathyTpContactList *list,
				 EmpathyContact        *contact)
{
	g_signal_handlers_unblock_by_func (contact,
					   tp_contact_list_groups_updated_cb,
					   list);
	g_signal_handlers_unblock_by_func (contact,
					   tp_contact_list_name_updated_cb,
					   list);
}

static gboolean
tp_contact_list_find_foreach (guint          handle,
			      EmpathyContact *contact,
			      gchar         *id)
{
	if (strcmp (empathy_contact_get_id (contact), id) == 0) {
		return TRUE;
	}

	return FALSE;
}

static void
tp_contact_list_newchannel_cb (DBusGProxy           *proxy,
			       const gchar          *object_path,
			       const gchar          *channel_type,
			       TelepathyHandleType   handle_type,
			       guint                 channel_handle,
			       gboolean              suppress_handle,
			       EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;
	EmpathyTpGroup           *group;
	TpChan                   *new_chan;
	const gchar              *bus_name;
	GArray                   *members;

	priv = GET_PRIV (list);

	if (strcmp (channel_type, TP_IFACE_CHANNEL_TYPE_CONTACT_LIST) != 0 ||
	    suppress_handle ||
	    !priv->setup) {
		return;
	}

	bus_name = dbus_g_proxy_get_bus_name (DBUS_G_PROXY (priv->tp_conn));
	new_chan = tp_chan_new (tp_get_bus (),
				bus_name,
				object_path,
				channel_type, handle_type, channel_handle);

	if (handle_type == TP_HANDLE_TYPE_LIST) {
		TpContactListType list_type;

		group = empathy_tp_group_new (new_chan, priv->tp_conn);

		list_type = tp_contact_list_get_type (list, group);
		if (list_type == TP_CONTACT_LIST_TYPE_UNKNOWN) {
			empathy_debug (DEBUG_DOMAIN,
				      "Type of contact list channel unknown: %s",
				      empathy_tp_group_get_name (group));

			g_object_unref (new_chan);
			g_object_unref (group);
			return;
		} else {
			empathy_debug (DEBUG_DOMAIN,
				      "New contact list channel of type: %d",
				      list_type);
		}

		g_signal_connect (group, "members-added",
				  G_CALLBACK (tp_contact_list_added_cb),
				  list);
		g_signal_connect (group, "members-removed",
				  G_CALLBACK (tp_contact_list_removed_cb),
				  list);

		if (list_type == TP_CONTACT_LIST_TYPE_PUBLISH) {
			GList *pendings, *l;

			if (priv->publish) {
				g_object_unref (priv->publish);
			}
			priv->publish = group;

			/* Makes no sense to be in remote-pending */
			g_signal_connect (group, "local-pending",
					  G_CALLBACK (tp_contact_list_pending_cb),
					  list);

			pendings = empathy_tp_group_get_local_pending_members_with_info (group);
			if (pendings) {
				GArray *pending;

				pending = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
				for (l = pendings; l; l = l->next) {
					EmpathyTpGroupInfo *info;

					info = l->data;

					g_array_insert_val (pending, 0, info->member);
					tp_contact_list_pending_cb (group, pending,
								    info->actor,
								    info->reason,
								    info->message,
								    list);
				}
				g_array_free (pending, TRUE);
				empathy_tp_group_info_list_free (pendings);
			}
		}
		if (list_type == TP_CONTACT_LIST_TYPE_SUBSCRIBE) {
			GArray *remote_pendings = NULL;

			if (priv->subscribe) {
				g_object_unref (priv->subscribe);
			}
			priv->subscribe = group;

			/* Makes no sense to be in local-pending */
			g_signal_connect (group, "remote-pending",
					  G_CALLBACK (tp_contact_list_pending_cb),
					  list);
			empathy_tp_group_get_all_members (group,
							  &members,
							  NULL,
							  &remote_pendings);

			tp_contact_list_pending_cb (group, remote_pendings, 0,
						    TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
						    NULL,
						    list);
			g_array_free (remote_pendings, TRUE);
		} else {
			members = empathy_tp_group_get_members (group);
		}

		tp_contact_list_added_cb (group, members, 0,
					  TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
					  NULL, list);
		g_array_free (members, TRUE);
	}
	else if (handle_type == TP_HANDLE_TYPE_GROUP) {
		const gchar *object_path;

		object_path = dbus_g_proxy_get_path (DBUS_G_PROXY (new_chan));
		if (g_hash_table_lookup (priv->groups, object_path)) {
			g_object_unref (new_chan);
			return;
		}

		group = empathy_tp_group_new (new_chan, priv->tp_conn);

		empathy_debug (DEBUG_DOMAIN, "New server-side group channel: %s",
			      empathy_tp_group_get_name (group));

		dbus_g_proxy_connect_signal (DBUS_G_PROXY (new_chan), "Closed",
					     G_CALLBACK
					     (tp_contact_list_group_channel_closed_cb),
					     list, NULL);

		g_hash_table_insert (priv->groups, g_strdup (object_path), group);
		g_signal_connect (group, "members-added",
				  G_CALLBACK (tp_contact_list_group_members_added_cb),
				  list);
		g_signal_connect (group, "members-removed",
				  G_CALLBACK (tp_contact_list_group_members_removed_cb),
				  list);

		members = empathy_tp_group_get_members (group);
		tp_contact_list_group_members_added_cb (group, members, 0,
							TP_CHANNEL_GROUP_CHANGE_REASON_NONE,
							NULL, list);
		g_array_free (members, TRUE);
	}

	g_object_unref (new_chan);
}

static TpContactListType
tp_contact_list_get_type (EmpathyTpContactList *list,
			  EmpathyTpGroup       *group)
{
	EmpathyTpContactListPriv *priv;
	TpContactListType         list_type;
	const gchar              *name;

	priv = GET_PRIV (list);

	name = empathy_tp_group_get_name (group);
	if (strcmp (name, "subscribe") == 0) {
		list_type = TP_CONTACT_LIST_TYPE_SUBSCRIBE;
	} else if (strcmp (name, "publish") == 0) {
		list_type = TP_CONTACT_LIST_TYPE_PUBLISH;
	} else {
		list_type = TP_CONTACT_LIST_TYPE_UNKNOWN;
	}

	return list_type;
}

static void
tp_contact_list_added_cb (EmpathyTpGroup       *group,
			  GArray               *handles,
			  guint                 actor_handle,
			  guint                 reason,
			  const gchar          *message,
			  EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;
	GList                    *added_list, *l;
	TpContactListType         list_type;

	priv = GET_PRIV (list);

	list_type = tp_contact_list_get_type (list, group);

	added_list = empathy_tp_contact_list_get_from_handles (list, handles);
	for (l = added_list; l; l = l->next) {
		EmpathyContact      *contact;
		EmpathySubscription  subscription;

		contact = EMPATHY_CONTACT (l->data);

		empathy_debug (DEBUG_DOMAIN, "Contact '%s' added to list type %d",
			      empathy_contact_get_name (contact),
			      list_type);

		subscription = empathy_contact_get_subscription (contact);
		if (list_type == TP_CONTACT_LIST_TYPE_SUBSCRIBE) {
			subscription |= EMPATHY_SUBSCRIPTION_FROM;
		}
		else if (list_type == TP_CONTACT_LIST_TYPE_PUBLISH) {
			subscription |= EMPATHY_SUBSCRIPTION_TO;
			tp_contact_list_remove_local_pending (list, contact);
		}

		tp_contact_list_block_contact (list, contact);
		empathy_contact_set_subscription (contact, subscription);
		tp_contact_list_unblock_contact (list, contact);

		if (list_type == TP_CONTACT_LIST_TYPE_SUBSCRIBE) {
			if (!g_list_find (priv->members, contact)) {
				priv->members = g_list_prepend (priv->members,
								g_object_ref (contact));
				g_signal_emit_by_name (list, "contact-added", contact);
			}
		}

		g_object_unref (contact);
	}

	g_list_free (added_list);
}

static void
tp_contact_list_removed_cb (EmpathyTpGroup       *group,
			    GArray               *handles,
			    guint                 actor_handle,
			    guint                 reason,
			    const gchar          *message,
			    EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;
	GList                    *removed_list, *l;
	TpContactListType         list_type;

	priv = GET_PRIV (list);

	list_type = tp_contact_list_get_type (list, group);

	removed_list = empathy_tp_contact_list_get_from_handles (list, handles);
	for (l = removed_list; l; l = l->next) {
		EmpathyContact      *contact;
		EmpathySubscription  subscription;

		contact = EMPATHY_CONTACT (l->data);

		empathy_debug (DEBUG_DOMAIN, "Contact '%s' removed from list type %d",
			      empathy_contact_get_name (contact),
			      list_type);

		subscription = empathy_contact_get_subscription (contact);
		if (list_type == TP_CONTACT_LIST_TYPE_SUBSCRIBE) {
			subscription &= !EMPATHY_SUBSCRIPTION_FROM;
		}
		else if (list_type == TP_CONTACT_LIST_TYPE_PUBLISH) {
			subscription &= !EMPATHY_SUBSCRIPTION_TO;
			tp_contact_list_remove_local_pending (list, contact);
		}

		tp_contact_list_block_contact (list, contact);
		empathy_contact_set_subscription (contact, subscription);
		tp_contact_list_unblock_contact (list, contact);

		if (list_type == TP_CONTACT_LIST_TYPE_SUBSCRIBE) {
			GList *l;

			if ((l = g_list_find (priv->members, contact))) {
				g_signal_emit_by_name (list, "contact-removed", contact);
				priv->members = g_list_delete_link (priv->members, l);
				g_object_unref (contact);
			}
		}
		g_object_unref (contact);
	}

	g_list_free (removed_list);
}

static void
tp_contact_list_pending_cb (EmpathyTpGroup       *group,
			    GArray               *handles,
			    guint                 actor_handle,
			    guint                 reason,
			    const gchar          *message,
			    EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;
	GList                    *pending_list, *l;
	TpContactListType         list_type;

	priv = GET_PRIV (list);

	list_type = tp_contact_list_get_type (list, group);

	pending_list = empathy_tp_contact_list_get_from_handles (list, handles);
	for (l = pending_list; l; l = l->next) {
		EmpathyContact *contact;

		contact = EMPATHY_CONTACT (l->data);

		empathy_debug (DEBUG_DOMAIN, "Contact '%s' pending in list type %d",
			      empathy_contact_get_name (contact),
			      list_type);

		if (list_type == TP_CONTACT_LIST_TYPE_PUBLISH) {
			EmpathyContactListInfo *info;

			info = empathy_contact_list_info_new (contact, message);
			priv->local_pending = g_list_prepend (priv->local_pending,
							      info);

			g_signal_emit_by_name (list, "local-pending",
					       contact, message);
		}
		else if (list_type == TP_CONTACT_LIST_TYPE_SUBSCRIBE) {
			if (!g_list_find (priv->members, contact)) {
				priv->members = g_list_prepend (priv->members,
								g_object_ref (contact));
				g_signal_emit_by_name (list, "contact-added", contact);
			}
		}

		g_object_unref (contact);
	}

	g_list_free (pending_list);
}

static void
tp_contact_list_remove_local_pending (EmpathyTpContactList *list,
				      EmpathyContact        *contact)
{
	EmpathyTpContactListPriv *priv;
	GList                    *l;

	priv = GET_PRIV (list);

	for (l = priv->local_pending; l; l = l->next) {
		EmpathyContactListInfo *info;

		info = l->data;
		if (empathy_contact_equal (contact, info->contact)) {
			empathy_debug (DEBUG_DOMAIN, "Contact no more local-pending: %s",
				      empathy_contact_get_name (contact));

			priv->local_pending = g_list_delete_link (priv->local_pending, l);
			empathy_contact_list_info_free (info);
			break;
		}
	}
}

static void
tp_contact_list_groups_updated_cb (EmpathyContact       *contact,
				   GParamSpec           *param,
				   EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;
	TpContactListData         data;
	GList                    *groups, *l;

	priv = GET_PRIV (list);

	/* Make sure all groups are created */
	groups = empathy_contact_get_groups (contact);
	for (l = groups; l; l = l->next) {
		tp_contact_list_get_group (list, l->data);
	}

	data.handle = empathy_contact_get_handle (contact);
	data.new_groups = groups;

	g_hash_table_foreach (priv->groups,
			      (GHFunc) tp_contact_list_update_groups_foreach,
			      &data);
}

static void
tp_contact_list_name_updated_cb (EmpathyContact        *contact,
				 GParamSpec           *param,
				 EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;
	GHashTable               *new_alias;
	const gchar              *new_name;
	guint                     handle;
	GError                   *error = NULL;

	priv = GET_PRIV (list);
	
	if (!priv->aliasing_iface) {
		return;
	}

	handle = empathy_contact_get_handle (contact);
	new_name = empathy_contact_get_name (contact);

	empathy_debug (DEBUG_DOMAIN, "renaming handle %d to %s",
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
		empathy_debug (DEBUG_DOMAIN, 
			      "Couldn't rename contact: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}

	g_hash_table_destroy (new_alias);
}

static void
tp_contact_list_update_groups_foreach (gchar             *object_path,
				       EmpathyTpGroup    *group,
				       TpContactListData *data)
{
	gboolean     is_member;
	gboolean     found = FALSE;
	const gchar *group_name;
	GList       *l;

	is_member = empathy_tp_group_is_member (group, data->handle);
	group_name = empathy_tp_group_get_name (group);

	for (l = data->new_groups; l; l = l->next) {
		if (strcmp (group_name, l->data) == 0) {
			found = TRUE;
			break;
		}
	}

	if (is_member && !found) {
		/* We are no longer member of this group */
		empathy_debug (DEBUG_DOMAIN, "Contact %d removed from group '%s'",
			      data->handle, group_name);
		empathy_tp_group_remove_member (group, data->handle, "");
	}

	if (!is_member && found) {
		/* We are now member of this group */
		empathy_debug (DEBUG_DOMAIN, "Contact %d added to group '%s'",
			      data->handle, group_name);
		empathy_tp_group_add_member (group, data->handle, "");
	}
}

static EmpathyTpGroup *
tp_contact_list_get_group (EmpathyTpContactList *list,
			   const gchar          *name)
{
	EmpathyTpContactListPriv *priv;
	EmpathyTpGroup           *group;
	TpChan                   *group_channel;
	GArray                   *handles;
	guint                     group_handle;
	char                     *group_object_path;
	const char               *names[2] = {name, NULL};
	GError                   *error = NULL;

	priv = GET_PRIV (list);

	group = g_hash_table_find (priv->groups,
				   (GHRFunc) tp_contact_list_find_group,
				   (gchar*) name);
	if (group) {
		return group;
	}

	empathy_debug (DEBUG_DOMAIN, "creating new group: %s", name);

	if (!tp_conn_request_handles (DBUS_G_PROXY (priv->tp_conn),
				      TP_HANDLE_TYPE_GROUP,
				      names,
				      &handles,
				      &error)) {
		empathy_debug (DEBUG_DOMAIN,
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
		empathy_debug (DEBUG_DOMAIN,
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
				     (tp_contact_list_group_channel_closed_cb),
				     list,
				     NULL);

	group = empathy_tp_group_new (group_channel, priv->tp_conn);
	g_hash_table_insert (priv->groups, group_object_path, group);
	g_signal_connect (group, "members-added",
			  G_CALLBACK (tp_contact_list_group_members_added_cb),
			  list);
	g_signal_connect (group, "members-removed",
			  G_CALLBACK (tp_contact_list_group_members_removed_cb),
			  list);

	return group;
}

static gboolean
tp_contact_list_find_group (gchar          *key,
			    EmpathyTpGroup *group,
			    gchar          *group_name)
{
	if (strcmp (group_name, empathy_tp_group_get_name (group)) == 0) {
		return TRUE;
	}

	return FALSE;
}

static void
tp_contact_list_get_groups_foreach (gchar          *key,
				    EmpathyTpGroup *group,
				    GList          **groups)
{
	const gchar *name;

	name = empathy_tp_group_get_name (group);
	*groups = g_list_append (*groups, g_strdup (name));
}

static void
tp_contact_list_group_channel_closed_cb (TpChan             *channel,
					 EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;

	priv = GET_PRIV (list);

	g_hash_table_remove (priv->groups,
			     dbus_g_proxy_get_path (DBUS_G_PROXY (channel)));
}

static void
tp_contact_list_group_members_added_cb (EmpathyTpGroup       *group,
					GArray               *members,
					guint                 actor_handle,
					guint                 reason,
					const gchar          *message,
					EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;
	GList                    *added_list, *l;
	const gchar              *group_name;

	priv = GET_PRIV (list);

	group_name = empathy_tp_group_get_name (group);
	added_list = empathy_tp_contact_list_get_from_handles (list, members);

	for (l = added_list; l; l = l->next) {
		EmpathyContact *contact;

		contact = EMPATHY_CONTACT (l->data);

		tp_contact_list_block_contact (list, contact);
		empathy_contact_add_group (contact, group_name);
		tp_contact_list_unblock_contact (list, contact);

		g_object_unref (contact);
	}

	g_list_free (added_list);
}

static void
tp_contact_list_group_members_removed_cb (EmpathyTpGroup       *group,
					  GArray               *members,
					  guint                 actor_handle,
					  guint                 reason,
					  const gchar          *message,
					  EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;
	GList                    *removed_list, *l;
	const gchar              *group_name;

	priv = GET_PRIV (list);

	group_name = empathy_tp_group_get_name (group);
	removed_list = empathy_tp_contact_list_get_from_handles (list, members);

	for (l = removed_list; l; l = l->next) {
		EmpathyContact *contact;

		contact = l->data;

		tp_contact_list_block_contact (list, contact);
		empathy_contact_remove_group (contact, group_name);
		tp_contact_list_unblock_contact (list, contact);

		g_object_unref (contact);
	}

	g_list_free (removed_list);
}

static void
tp_contact_list_get_info (EmpathyTpContactList *list,
			  GArray               *handles)
{
	EmpathyTpContactListPriv *priv;
	GError                   *error = NULL;

	priv = GET_PRIV (list);

	if (priv->presence_iface) {
		/* FIXME: We should use GetPresence instead */
		if (!tp_conn_iface_presence_request_presence (priv->presence_iface,
							      handles, &error)) {
			empathy_debug (DEBUG_DOMAIN, 
				      "Could not request presences: %s",
				      error ? error->message : "No error given");
			g_clear_error (&error);
		}
	}

	if (priv->aliasing_iface) {
		TpContactListAliasesRequestData *data;

		data = g_slice_new (TpContactListAliasesRequestData);
		data->list = list;
		data->handles = g_memdup (handles->data, handles->len * sizeof (guint));

		tp_conn_iface_aliasing_request_aliases_async (priv->aliasing_iface,
							      handles,
							      (tp_conn_iface_aliasing_request_aliases_reply)
							      tp_contact_list_request_aliases_cb,
							      data);
	}

	if (priv->avatars_iface) {
		guint i;

		for (i = 0; i < handles->len; i++) {
			guint handle;

			handle = g_array_index (handles, gint, i);
			tp_contact_list_request_avatar (list, handle);
		}
	}
}

static void
tp_contact_list_request_avatar (EmpathyTpContactList *list,
				guint                 handle)
{
	EmpathyTpContactListPriv *priv;

	priv = GET_PRIV (list);
	
	/* We queue avatar requests to not send too many dbus async
	 * calls at once. If we don't we reach the dbus's limit of
	 * pending calls */
	priv->avatar_requests_queue = g_list_append (priv->avatar_requests_queue,
						     GUINT_TO_POINTER (handle));
	tp_contact_list_start_avatar_requests (list);
}

static void
tp_contact_list_start_avatar_requests (EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv       *priv;
	TpContactListAvatarRequestData *data;

	priv = GET_PRIV (list);

	while (n_avatar_requests <  MAX_AVATAR_REQUESTS &&
	       priv->avatar_requests_queue) {
		data = g_slice_new (TpContactListAvatarRequestData);
		data->list = list;
		data->handle = GPOINTER_TO_UINT (priv->avatar_requests_queue->data);

		n_avatar_requests++;
		priv->avatar_requests_queue = g_list_remove (priv->avatar_requests_queue,
							     priv->avatar_requests_queue->data);

		tp_conn_iface_avatars_request_avatar_async (priv->avatars_iface,
							    data->handle,
							    (tp_conn_iface_avatars_request_avatar_reply)
							    tp_contact_list_request_avatar_cb,
							    data);
	}
}

static void
tp_contact_list_avatar_update_cb (DBusGProxy           *proxy,
				  guint                 handle,
				  gchar                *new_token,
				  EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;

	priv = GET_PRIV (list);

	if (!g_hash_table_lookup (priv->contacts, GUINT_TO_POINTER (handle))) {
		/* We don't know this contact, skip */
		return;
	}

	empathy_debug (DEBUG_DOMAIN, "Changing avatar for %d to %s",
		      handle, new_token);

	tp_contact_list_request_avatar (list, handle);
}

static void
tp_contact_list_request_avatar_cb (DBusGProxy                     *proxy,
				   GArray                         *avatar_data,
				   gchar                          *mime_type,
				   GError                         *error,
				   TpContactListAvatarRequestData *data)
{
	EmpathyContact *contact;

	contact = empathy_tp_contact_list_get_from_handle (data->list, data->handle);

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error requesting avatar for %s: %s",
			      empathy_contact_get_name (contact),
			      error ? error->message : "No error given");
	} else {
		EmpathyAvatar *avatar;

		avatar = empathy_avatar_new (avatar_data->data,
					    avatar_data->len,
					    mime_type);
		tp_contact_list_block_contact (data->list, contact);
		empathy_contact_set_avatar (contact, avatar);
		tp_contact_list_unblock_contact (data->list, contact);
		empathy_avatar_unref (avatar);
	}

	n_avatar_requests--;
	tp_contact_list_start_avatar_requests (data->list);

	g_object_unref (contact);
	g_slice_free (TpContactListAvatarRequestData, data);
}

static void
tp_contact_list_aliases_update_cb (DBusGProxy           *proxy,
				   GPtrArray            *renamed_handlers,
				   EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;
	guint                     i;

	priv = GET_PRIV (list);

	for (i = 0; renamed_handlers->len > i; i++) {
		guint          handle;
		const gchar   *alias;
		GValueArray   *renamed_struct;
		EmpathyContact *contact;

		renamed_struct = g_ptr_array_index (renamed_handlers, i);
		handle = g_value_get_uint(g_value_array_get_nth (renamed_struct, 0));
		alias = g_value_get_string(g_value_array_get_nth (renamed_struct, 1));

		if (!g_hash_table_lookup (priv->contacts, GUINT_TO_POINTER (handle))) {
			/* We don't know this contact, skip */
			continue;
		}

		if (G_STR_EMPTY (alias)) {
			alias = NULL;
		}

		contact = empathy_tp_contact_list_get_from_handle (list, handle);
		tp_contact_list_block_contact (list, contact);
		empathy_contact_set_name (contact, alias);
		tp_contact_list_unblock_contact (list, contact);
		g_object_unref (contact);

		empathy_debug (DEBUG_DOMAIN, "contact %d renamed to %s (update cb)",
			      handle, alias);
	}
}

static void
tp_contact_list_request_aliases_cb (DBusGProxy                       *proxy,
				    gchar                           **contact_names,
				    GError                           *error,
				    TpContactListAliasesRequestData  *data)
{
	guint   i = 0;
	gchar **name;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error requesting aliases: %s",
			      error->message);
	}

	for (name = contact_names; *name && !error; name++) {
		EmpathyContact *contact;

		contact = empathy_tp_contact_list_get_from_handle (data->list,
								   data->handles[i]);
		tp_contact_list_block_contact (data->list, contact);
		empathy_contact_set_name (contact, *name);
		tp_contact_list_unblock_contact (data->list, contact);
		g_object_unref (contact);

		empathy_debug (DEBUG_DOMAIN, "contact %d renamed to %s (request cb)",
			      data->handles[i], *name);

		i++;
	}

	g_free (data->handles);
	g_slice_free (TpContactListAliasesRequestData, data);
}

static void
tp_contact_list_presence_update_cb (DBusGProxy           *proxy,
				    GHashTable           *handle_table,
				    EmpathyTpContactList *list)
{
	g_hash_table_foreach (handle_table,
			      (GHFunc) tp_contact_list_parse_presence_foreach,
			      list);
}

static void
tp_contact_list_parse_presence_foreach (guint                 handle,
					GValueArray          *presence_struct,
					EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;
	GHashTable     *presences_table;
	EmpathyContact  *contact;
	EmpathyPresence *presence = NULL;

	priv = GET_PRIV (list);

	if (!g_hash_table_lookup (priv->contacts, GUINT_TO_POINTER (handle))) {
		/* We don't know this contact, skip */
		return;
	}

	contact = empathy_tp_contact_list_get_from_handle (list, handle);
	presences_table = g_value_get_boxed (g_value_array_get_nth (presence_struct, 1));

	g_hash_table_foreach (presences_table,
			      (GHFunc) tp_contact_list_presences_table_foreach,
			      &presence);

	empathy_debug (DEBUG_DOMAIN, "Presence changed for %s (%d) to %s (%d)",
		      empathy_contact_get_name (contact),
		      handle,
		      presence ? empathy_presence_get_status (presence) : "unset",
		      presence ? empathy_presence_get_state (presence) : MC_PRESENCE_UNSET);

	tp_contact_list_block_contact (list, contact);
	empathy_contact_set_presence (contact, presence);
	tp_contact_list_unblock_contact (list, contact);

	g_object_unref (contact);
}

static void
tp_contact_list_presences_table_foreach (const gchar     *state_str,
					 GHashTable      *presences_table,
					 EmpathyPresence **presence)
{
	McPresence    state;
	const GValue *message;

	state = empathy_presence_state_from_str (state_str);
	if ((state == MC_PRESENCE_UNSET) || (state == MC_PRESENCE_OFFLINE)) {
		return;
	}

	if (*presence) {
		g_object_unref (*presence);
		*presence = NULL;
	}

	*presence = empathy_presence_new ();
	empathy_presence_set_state (*presence, state);

	message = g_hash_table_lookup (presences_table, "message");
	if (message != NULL) {
		empathy_presence_set_status (*presence,
					    g_value_get_string (message));
	}
}

static void
tp_contact_list_status_changed_cb (MissionControl                  *mc,
				   TelepathyConnectionStatus        status,
				   McPresence                       presence,
				   TelepathyConnectionStatusReason  reason,
				   const gchar                     *unique_name,
				   EmpathyTpContactList            *list)
{
	EmpathyTpContactListPriv *priv;
	McAccount                *account;

	priv = GET_PRIV (list);

	account = mc_account_lookup (unique_name);
	if (status != TP_CONN_STATUS_DISCONNECTED ||
	    !empathy_account_equal (account, priv->account) ||
	    !priv->tp_conn) {
		g_object_unref (account);
		return;
	}

	/* We are disconnected, do just like if the connection was destroyed */
	g_signal_handlers_disconnect_by_func (priv->tp_conn,
					      tp_contact_list_destroy_cb,
					      list);
	tp_contact_list_destroy_cb (DBUS_G_PROXY (priv->tp_conn), list);

	g_object_unref (account);
}

