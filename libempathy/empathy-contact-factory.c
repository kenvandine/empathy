/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
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

#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-conn-iface-aliasing-gen.h>
#include <libtelepathy/tp-conn-iface-presence-gen.h>
#include <libtelepathy/tp-conn-iface-avatars-gen.h>
#include <libtelepathy/tp-conn-iface-capabilities-gen.h>
#include <libmissioncontrol/mission-control.h>

#include "empathy-contact-factory.h"
#include "empathy-utils.h"
#include "empathy-debug.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_CONTACT_FACTORY, EmpathyContactFactoryPriv))

#define DEBUG_DOMAIN "ContactFactory"

struct _EmpathyContactFactoryPriv {
	MissionControl *mc;
	GHashTable     *accounts;
};

typedef struct {
	EmpathyContactFactory *factory;
	McAccount             *account;
	guint                  refcount;

	TpConn                *tp_conn;
	DBusGProxy            *aliasing_iface;
	DBusGProxy            *avatars_iface;
	DBusGProxy            *presence_iface;
	DBusGProxy            *capabilities_iface;

	GList                 *contacts;
	guint                  self_handle;
} ContactFactoryAccountData;

typedef struct {
	ContactFactoryAccountData *account_data;
	GList                     *contacts;
} RequestHandlesData;

typedef struct {
	ContactFactoryAccountData *account_data;
	guint                     *handles;
} RequestAliasesData;

static void empathy_contact_factory_class_init (EmpathyContactFactoryClass *klass);
static void empathy_contact_factory_init       (EmpathyContactFactory      *factory);

G_DEFINE_TYPE (EmpathyContactFactory, empathy_contact_factory, G_TYPE_OBJECT);

static gint
contact_factory_find_by_handle (gconstpointer a,
				gconstpointer b)
{
	EmpathyContact *contact;
	guint           handle; 

	contact = EMPATHY_CONTACT (a);
	handle = GPOINTER_TO_UINT (b);

	return handle - empathy_contact_get_handle (contact);
}

static EmpathyContact *
contact_factory_account_data_find_by_handle (ContactFactoryAccountData *account_data,
					     guint                      handle)
{
	GList *l;

	l = g_list_find_custom (account_data->contacts,
				GUINT_TO_POINTER (handle),
				contact_factory_find_by_handle);

	return l ? l->data : NULL;
}

static gint
contact_factory_find_by_id (gconstpointer a,
			    gconstpointer b)
{
	EmpathyContact *contact;
	const gchar    *id = b;

	contact = EMPATHY_CONTACT (a);

	return strcmp (id, empathy_contact_get_id (contact));
}

static EmpathyContact *
contact_factory_account_data_find_by_id (ContactFactoryAccountData *account_data,
					 const gchar               *id)
{
	GList *l;

	l = g_list_find_custom (account_data->contacts,
				id,
				contact_factory_find_by_id);

	return l ? l->data : NULL;
}

static void contact_factory_account_data_disconnect (ContactFactoryAccountData *account_data);

static void
contact_factory_weak_notify (gpointer data,
			     GObject *where_the_object_was)
{
	ContactFactoryAccountData *account_data = data;

	empathy_debug (DEBUG_DOMAIN, "Remove finalized contact %p",
		       where_the_object_was);

	account_data->contacts = g_list_remove (account_data->contacts,
						where_the_object_was);
	if (!account_data->contacts) {
		EmpathyContactFactoryPriv *priv = GET_PRIV (account_data->factory);

		g_hash_table_remove (priv->accounts, account_data->account);
	}
}

static void
contact_factory_remove_foreach (gpointer data,
				gpointer user_data)
{
	ContactFactoryAccountData *account_data = user_data;
	EmpathyContact            *contact = data;

	g_object_weak_unref (G_OBJECT (contact),
			     contact_factory_weak_notify,
			     account_data);
}

static ContactFactoryAccountData *
contact_factory_account_data_ref (ContactFactoryAccountData *account_data)
{
	account_data->refcount++;

	return account_data;
}

static void
contact_factory_account_data_unref (ContactFactoryAccountData *account_data)
{
	account_data->refcount--;
	if (account_data->refcount > 0) {
		return;
	}

	empathy_debug (DEBUG_DOMAIN, "Account data finalized: %p (%s)",
		       account_data,
		       mc_account_get_normalized_name (account_data->account));

	contact_factory_account_data_disconnect (account_data);

	if (account_data->contacts) {
		g_list_foreach (account_data->contacts,
				contact_factory_remove_foreach,
				account_data);
		g_list_free (account_data->contacts);
	}

	if (account_data->account) {
		g_object_unref (account_data->account);
	}

	if (account_data->tp_conn) {
		g_object_unref (account_data->tp_conn);
	}

	g_slice_free (ContactFactoryAccountData, account_data);
}

static void
contact_factory_presences_table_foreach (const gchar      *state_str,
					 GHashTable       *presences_table,
					 EmpathyPresence **presence)
{
	McPresence    state;
	const GValue *message;

	state = empathy_presence_state_from_str (state_str);
	if (state == MC_PRESENCE_UNSET) {
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
contact_factory_parse_presence_foreach (guint                      handle,
					GValueArray               *presence_struct,
					ContactFactoryAccountData *account_data)
{
	GHashTable      *presences_table;
	EmpathyContact  *contact;
	EmpathyPresence *presence = NULL;

	contact = contact_factory_account_data_find_by_handle (account_data,
							       handle);
	if (!contact) {
		return;
	}

	presences_table = g_value_get_boxed (g_value_array_get_nth (presence_struct, 1));

	g_hash_table_foreach (presences_table,
			      (GHFunc) contact_factory_presences_table_foreach,
			      &presence);

	empathy_debug (DEBUG_DOMAIN, "Changing presence for contact %s (%d) to %s (%d)",
		      empathy_contact_get_id (contact),
		      handle,
		      presence ? empathy_presence_get_status (presence) : "unset",
		      presence ? empathy_presence_get_state (presence) : MC_PRESENCE_UNSET);

	empathy_contact_set_presence (contact, presence);
	g_object_unref (presence);
}

static void
contact_factory_get_presence_cb (DBusGProxy *proxy,
				 GHashTable *handle_table,
				 GError     *error,
				 gpointer    user_data)
{
	ContactFactoryAccountData *account_data = user_data;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error getting presence: %s",
			      error->message);
		goto OUT;
	}

	g_hash_table_foreach (handle_table,
			      (GHFunc) contact_factory_parse_presence_foreach,
			      account_data);

	g_hash_table_destroy (handle_table);
OUT:
	contact_factory_account_data_unref (account_data);
}

static void
contact_factory_presence_update_cb (DBusGProxy                *proxy,
				    GHashTable                *handle_table,
				    ContactFactoryAccountData *account_data)
{
	g_hash_table_foreach (handle_table,
			      (GHFunc) contact_factory_parse_presence_foreach,
			      account_data);
}

static void
contact_factory_set_aliases_cb (DBusGProxy *proxy,
				GError     *error,
				gpointer    user_data)
{
	ContactFactoryAccountData *account_data = user_data;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error setting alias: %s",
			       error->message);
	}

	contact_factory_account_data_unref (account_data);
}

static void
contact_factory_request_aliases_cb (DBusGProxy  *proxy,
				    gchar      **contact_names,
				    GError      *error,
				    gpointer     user_data)
{
	RequestAliasesData  *data = user_data;
	guint                i = 0;
	gchar              **name;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error requesting aliases: %s",
			      error->message);
		goto OUT;
	}

	for (name = contact_names; *name; name++) {
		EmpathyContact *contact;

		contact = contact_factory_account_data_find_by_handle (data->account_data,
								       data->handles[i]);
		if (!contact) {
			continue;
		}

		empathy_debug (DEBUG_DOMAIN, "Renaming contact %s (%d) to %s (request cb)",
			       empathy_contact_get_id (contact),
			       data->handles[i], *name);

		empathy_contact_set_name  (contact, *name);

		i++;
	}

	g_strfreev (contact_names);
OUT:
	contact_factory_account_data_unref (data->account_data);
	g_free (data->handles);
	g_slice_free (RequestAliasesData, data);
}

static void
contact_factory_aliases_changed_cb (DBusGProxy *proxy,
				    GPtrArray  *renamed_handlers,
				    gpointer    user_data)
{
	ContactFactoryAccountData *account_data = user_data;
	guint                     i;

	for (i = 0; renamed_handlers->len > i; i++) {
		guint           handle;
		const gchar    *alias;
		GValueArray    *renamed_struct;
		EmpathyContact *contact;

		renamed_struct = g_ptr_array_index (renamed_handlers, i);
		handle = g_value_get_uint(g_value_array_get_nth (renamed_struct, 0));
		alias = g_value_get_string(g_value_array_get_nth (renamed_struct, 1));
		contact = contact_factory_account_data_find_by_handle (account_data, handle);

		if (!contact) {
			/* We don't know this contact, skip */
			continue;
		}

		if (G_STR_EMPTY (alias)) {
			alias = NULL;
		}

		empathy_debug (DEBUG_DOMAIN, "Renaming contact %s (%d) to %s (changed cb)",
			       empathy_contact_get_id (contact),
			       handle, alias);

		empathy_contact_set_name (contact, alias);
	}
}

static void
contact_factory_avatar_retrieved_cb (DBusGProxy *proxy,
				     guint       handle,
				     gchar      *token,
				     GArray     *avatar_data,
				     gchar      *mime_type,
				     gpointer    user_data)
{
	ContactFactoryAccountData *account_data = user_data;
	EmpathyContact            *contact;
	EmpathyAvatar             *avatar;

	contact = contact_factory_account_data_find_by_handle (account_data,
							       handle);
	if (!contact) {
		return;
	}

	empathy_debug (DEBUG_DOMAIN, "Avatar retrieved for contact %s (%d)",
		       empathy_contact_get_id (contact),
		       handle);

	avatar = empathy_avatar_new (avatar_data->data,
				     avatar_data->len,
				     mime_type,
				     token);

	empathy_contact_set_avatar (contact, avatar);
	empathy_avatar_unref (avatar);
}

static void
contact_factory_request_avatars_cb (DBusGProxy *proxy,
				    GError     *error,
				    gpointer    user_data)
{
	ContactFactoryAccountData *account_data = user_data;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error requesting avatars: %s",
			       error->message);
	}

	contact_factory_account_data_unref (account_data);
}

typedef struct {
	ContactFactoryAccountData *account_data;
	GArray                    *handles;
} TokensData;

static gboolean
contact_factory_avatar_maybe_update (ContactFactoryAccountData *account_data,
				     guint                      handle,
				     const gchar               *token)
{
	EmpathyContact *contact;
	EmpathyAvatar  *avatar;

	contact = contact_factory_account_data_find_by_handle (account_data,
							       handle);
	if (!contact) {
		return TRUE;
	}

	/* Check if we have an avatar */
	if (G_STR_EMPTY (token)) {
		empathy_contact_set_avatar (contact, NULL);
		return TRUE;
	}

	/* Check if the avatar changed */
	avatar = empathy_contact_get_avatar (contact);
	if (avatar && !empathy_strdiff (avatar->token, token)) {
		return TRUE;
	}

	/* The avatar changed, search the new one in the cache */
	avatar = empathy_avatar_new_from_cache (token);
	if (avatar) {
		/* Got from cache, use it */
		empathy_contact_set_avatar (contact, avatar);
		empathy_avatar_unref (avatar);
		return TRUE;
	}

	/* Avatar is not up-to-date, we have to request it. */
	return FALSE;
}

static void
contact_factory_avatar_tokens_foreach (gpointer key,
				       gpointer value,
				       gpointer user_data)
{
	TokensData  *data = user_data;
	const gchar *token = value;
	guint        handle = GPOINTER_TO_UINT (key);

	if (!contact_factory_avatar_maybe_update (data->account_data,
						  handle, token)) {
		g_array_append_val (data->handles, handle);
	}
}

static void
contact_factory_get_known_avatar_tokens_cb (DBusGProxy *proxy,
					    GHashTable *tokens,
					    GError     *error,
					    gpointer    user_data)
{
	ContactFactoryAccountData *account_data = user_data;
	TokensData                 data;

	if (error) {
		empathy_debug (DEBUG_DOMAIN,
			       "Error getting known avatars tokens: %s",
			       error->message);
		goto OUT;
	}

	data.account_data = account_data;
	data.handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_hash_table_foreach (tokens,
			      contact_factory_avatar_tokens_foreach,
			      &data);

	empathy_debug (DEBUG_DOMAIN, "Got %d tokens, need to request %d avatars",
		       g_hash_table_size (tokens),
		       data.handles->len);

	/* Request needed avatars */
	if (data.handles->len > 0) {
		tp_conn_iface_avatars_request_avatars_async (account_data->avatars_iface,
							     data.handles,
							     contact_factory_request_avatars_cb,
							     contact_factory_account_data_ref (account_data));
	}

	g_hash_table_destroy (tokens);
	g_array_free (data.handles, TRUE);
OUT:
	contact_factory_account_data_unref (account_data);
}

static void
contact_factory_avatar_updated_cb (DBusGProxy *proxy,
				   guint       handle,
				   gchar      *new_token,
				   gpointer    user_data)
{
	ContactFactoryAccountData *account_data = user_data;
	GArray                    *handles;

	if (contact_factory_avatar_maybe_update (account_data, handle, new_token)) {
		/* Avatar was cached, nothing to do */
		return;
	}

	empathy_debug (DEBUG_DOMAIN, "Need to request one avatar");

	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);

	tp_conn_iface_avatars_request_avatars_async (account_data->avatars_iface,
						     handles,
						     contact_factory_request_avatars_cb,
						     contact_factory_account_data_ref (account_data));
	g_array_free (handles, TRUE);
}

static void
contact_factory_update_capabilities (ContactFactoryAccountData *account_data,
				     guint                      handle,
				     const gchar               *channel_type,
				     guint                      generic,
				     guint                      specific)
{
	EmpathyContact      *contact;
	EmpathyCapabilities  capabilities;

	contact = contact_factory_account_data_find_by_handle (account_data,
							       handle);
	if (!contact) {
		return;
	}

	capabilities = empathy_contact_get_capabilities (contact);

	if (strcmp (channel_type, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA) == 0) {
		capabilities &= ~EMPATHY_CAPABILITIES_AUDIO;
		capabilities &= ~EMPATHY_CAPABILITIES_VIDEO;
		if (specific & TP_CHANNEL_MEDIA_CAPABILITY_AUDIO) {
			capabilities |= EMPATHY_CAPABILITIES_AUDIO;
		}
		if (specific & TP_CHANNEL_MEDIA_CAPABILITY_VIDEO) {
			capabilities |= EMPATHY_CAPABILITIES_VIDEO;
		}
	}

	empathy_debug (DEBUG_DOMAIN, "Changing capabilities for contact %s (%d) to %d",
		       empathy_contact_get_id (contact),
		       empathy_contact_get_handle (contact),
		       capabilities);

	empathy_contact_set_capabilities (contact, capabilities);
}

static void
contact_factory_get_capabilities_cb (DBusGProxy *proxy,
				     GPtrArray  *capabilities,
				     GError     *error,
				     gpointer    user_data)
{
	ContactFactoryAccountData *account_data = user_data;
	guint                      i;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error getting capabilities: %s",
			       error->message);
		goto OUT;
	}

	for (i = 0; i < capabilities->len; i++)	{
		GValueArray *values;
		guint        handle;
		const gchar *channel_type;
		guint        generic;
		guint        specific;

		values = g_ptr_array_index (capabilities, i);
		handle = g_value_get_uint (g_value_array_get_nth (values, 0));
		channel_type = g_value_get_string (g_value_array_get_nth (values, 1));
		generic = g_value_get_uint (g_value_array_get_nth (values, 2));
		specific = g_value_get_uint (g_value_array_get_nth (values, 3));

		contact_factory_update_capabilities (account_data,
						     handle,
						     channel_type,
						     generic,
						     specific);

		g_value_array_free (values);
	}

	g_ptr_array_free (capabilities, TRUE);
OUT:
	contact_factory_account_data_unref (account_data);
}

static void
contact_factory_capabilities_changed_cb (DBusGProxy *proxy,
					 GPtrArray  *capabilities,
					 gpointer    user_data)
{
	ContactFactoryAccountData *account_data = user_data;
	guint                      i;

	for (i = 0; i < capabilities->len; i++)	{
		GValueArray *values;
		guint        handle;
		const gchar *channel_type;
		guint        generic;
		guint        specific;

		values = g_ptr_array_index (capabilities, i);
		handle = g_value_get_uint (g_value_array_get_nth (values, 0));
		channel_type = g_value_get_string (g_value_array_get_nth (values, 1));
		generic = g_value_get_uint (g_value_array_get_nth (values, 3));
		specific = g_value_get_uint (g_value_array_get_nth (values, 5));

		contact_factory_update_capabilities (account_data,
						     handle,
						     channel_type,
						     generic,
						     specific);
	}
}

static void
contact_factory_request_everything (ContactFactoryAccountData *account_data,
				    GArray                    *handles)
{
	if (account_data->presence_iface) {
		tp_conn_iface_presence_get_presence_async (account_data->presence_iface,
							   handles,
							   contact_factory_get_presence_cb,
							   contact_factory_account_data_ref (account_data));
	}

	if (account_data->aliasing_iface) {
		RequestAliasesData *data;

		data = g_slice_new (RequestAliasesData);
		data->account_data = contact_factory_account_data_ref (account_data);
		data->handles = g_memdup (handles->data, handles->len * sizeof (guint));

		tp_conn_iface_aliasing_request_aliases_async (account_data->aliasing_iface,
							      handles,
							      contact_factory_request_aliases_cb,
							      data);
	}

	if (account_data->avatars_iface) {
		tp_conn_iface_avatars_get_known_avatar_tokens_async (account_data->avatars_iface,
								     handles,
								     contact_factory_get_known_avatar_tokens_cb,
								     contact_factory_account_data_ref (account_data));
	}

	if (account_data->capabilities_iface) {
		tp_conn_iface_capabilities_get_capabilities_async (account_data->capabilities_iface,
								   handles,
								   contact_factory_get_capabilities_cb,
								   contact_factory_account_data_ref (account_data));
	}
}

static void
contact_factory_request_handles_cb (DBusGProxy *proxy,
				    GArray     *handles,
				    GError     *error,
				    gpointer    user_data)
{
	RequestHandlesData *data = user_data;
	GList              *l;
	guint               i = 0;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Failed to request handles: %s",
			       error->message);
		goto OUT;
	}

	for (l = data->contacts; l; l = l->next) {
		guint handle;

		handle = g_array_index (handles, guint, i);
		empathy_contact_set_handle (l->data, handle);
		if (handle == data->account_data->self_handle) {
			empathy_contact_set_is_user (l->data, TRUE);
		}

		i++;
	}

	contact_factory_request_everything (data->account_data, handles);
	g_array_free (handles, TRUE);

OUT:
	g_list_foreach (data->contacts, (GFunc) g_object_unref, NULL);
	g_list_free (data->contacts);
	contact_factory_account_data_unref (data->account_data);
	g_slice_free (RequestHandlesData, data);
}

static void
contact_factory_disconnect_contact_foreach (gpointer data,
					    gpointer user_data)
{
	EmpathyContact *contact = data;
	
	empathy_contact_set_presence (contact, NULL);
	empathy_contact_set_handle (contact, 0);
}

static void
contact_factory_destroy_cb (TpConn                    *tp_conn,
			    ContactFactoryAccountData *account_data)
{
	empathy_debug (DEBUG_DOMAIN, "Account disconnected or CM crashed");

	g_object_unref (account_data->tp_conn);
	account_data->tp_conn = NULL;
	account_data->aliasing_iface = NULL;
	account_data->avatars_iface = NULL;
	account_data->presence_iface = NULL;
	account_data->capabilities_iface = NULL;

	g_list_foreach (account_data->contacts,
			contact_factory_disconnect_contact_foreach,
			account_data);
}

static void
contact_factory_account_data_disconnect (ContactFactoryAccountData *account_data)
{
	if (account_data->aliasing_iface) {
		dbus_g_proxy_disconnect_signal (account_data->aliasing_iface,
						"AliasesChanged",
						G_CALLBACK (contact_factory_aliases_changed_cb),
						account_data);
	}
	if (account_data->avatars_iface) {
		dbus_g_proxy_disconnect_signal (account_data->avatars_iface,
						"AvatarUpdated",
						G_CALLBACK (contact_factory_avatar_updated_cb),
						account_data);
		dbus_g_proxy_disconnect_signal (account_data->avatars_iface,
						"AvatarRetrieved",
						G_CALLBACK (contact_factory_avatar_retrieved_cb),
						account_data);
	}
	if (account_data->presence_iface) {
		dbus_g_proxy_disconnect_signal (account_data->presence_iface,
						"PresenceUpdate",
						G_CALLBACK (contact_factory_presence_update_cb),
						account_data);
	}
	if (account_data->capabilities_iface) {
		dbus_g_proxy_disconnect_signal (account_data->capabilities_iface,
						"CapabilitiesChanged",
						G_CALLBACK (contact_factory_capabilities_changed_cb),
						account_data);
	}
	if (account_data->tp_conn) {
		g_signal_handlers_disconnect_by_func (account_data->tp_conn,
						      contact_factory_destroy_cb,
						      account_data);
	}
}

static void
contact_factory_account_data_update (ContactFactoryAccountData *account_data)
{
	EmpathyContactFactory     *factory = account_data->factory;
	EmpathyContactFactoryPriv *priv = GET_PRIV (factory);
	McAccount                 *account = account_data->account;
	TpConn                    *tp_conn = NULL;
	RequestHandlesData        *data;
	const gchar              **contact_ids;
	guint                      i;
	GList                     *l;
	GError                    *error = NULL;

	if (account_data->account) {
		guint status;

		/* status == 0 means the status is CONNECTED */
		status = mission_control_get_connection_status (priv->mc,
								account, NULL);
		if (status == 0) {
			tp_conn = mission_control_get_connection (priv->mc,
								  account, NULL);
		}
	}

	if (!tp_conn) {
		/* We are not connected anymore, remove the old connection */
		contact_factory_account_data_disconnect (account_data);
		if (account_data->tp_conn) {
			contact_factory_destroy_cb (account_data->tp_conn,
						    account_data);
		}
		return;
	}
	else if (account_data->tp_conn) {
		/* We were connected and we still are connected, nothing
		 * changed so nothing to do. */
		g_object_unref (tp_conn);
		return;
	}

	/* We got a new connection */
	account_data->tp_conn = tp_conn;
	account_data->aliasing_iface = tp_conn_get_interface (tp_conn,
							      TELEPATHY_CONN_IFACE_ALIASING_QUARK);
	account_data->avatars_iface = tp_conn_get_interface (tp_conn,
							     TELEPATHY_CONN_IFACE_AVATARS_QUARK);
	account_data->presence_iface = tp_conn_get_interface (tp_conn,
							      TELEPATHY_CONN_IFACE_PRESENCE_QUARK);
	account_data->capabilities_iface = tp_conn_get_interface (tp_conn,
							          TELEPATHY_CONN_IFACE_CAPABILITIES_QUARK);

	/* Connect signals */
	if (account_data->aliasing_iface) {
		dbus_g_proxy_connect_signal (account_data->aliasing_iface,
					     "AliasesChanged",
					     G_CALLBACK (contact_factory_aliases_changed_cb),
					     account_data, NULL);
	}
	if (account_data->avatars_iface) {
		dbus_g_proxy_connect_signal (account_data->avatars_iface,
					     "AvatarUpdated",
					     G_CALLBACK (contact_factory_avatar_updated_cb),
					     account_data, NULL);
		dbus_g_proxy_connect_signal (account_data->avatars_iface,
					     "AvatarRetrieved",
					     G_CALLBACK (contact_factory_avatar_retrieved_cb),
					     account_data, NULL);
	}
	if (account_data->presence_iface) {
		dbus_g_proxy_connect_signal (account_data->presence_iface,
					     "PresenceUpdate",
					     G_CALLBACK (contact_factory_presence_update_cb),
					     account_data, NULL);
	}
	if (account_data->capabilities_iface) {
		dbus_g_proxy_connect_signal (account_data->capabilities_iface,
					     "CapabilitiesChanged",
					     G_CALLBACK (contact_factory_capabilities_changed_cb),
					     account_data, NULL);
	}
	g_signal_connect (tp_conn, "destroy",
			  G_CALLBACK (contact_factory_destroy_cb),
			  account_data);

	/* Get our own handle */
	if (!tp_conn_get_self_handle (DBUS_G_PROXY (account_data->tp_conn),
				      &account_data->self_handle,
				      &error)) {
		empathy_debug (DEBUG_DOMAIN, "GetSelfHandle Error: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}

	/* Request new handles for all contacts */
	if (account_data->contacts) {
		data = g_slice_new (RequestHandlesData);
		data->account_data = contact_factory_account_data_ref (account_data);
		data->contacts = g_list_copy (account_data->contacts);
		g_list_foreach (data->contacts, (GFunc) g_object_ref, NULL);

		i = g_list_length (data->contacts);
		contact_ids = g_new0 (const gchar*, i + 1);
		i = 0;
		for (l = data->contacts; l; l = l->next) {
			contact_ids[i] = empathy_contact_get_id (l->data);
			i++;
		}

		tp_conn_request_handles_async (DBUS_G_PROXY (account_data->tp_conn),
					       TP_HANDLE_TYPE_CONTACT,
					       contact_ids,
					       contact_factory_request_handles_cb,
					       data);
		g_free (contact_ids);
	}
}

static ContactFactoryAccountData *
contact_factory_account_data_new (EmpathyContactFactory *factory,
				  McAccount             *account)
{
	ContactFactoryAccountData *account_data;

	account_data = g_slice_new0 (ContactFactoryAccountData);
	account_data->factory = factory;
	account_data->account = g_object_ref (account);
	account_data->refcount = 1;

	contact_factory_account_data_update (account_data);

	return account_data;
}

static void
contact_factory_status_changed_cb (MissionControl                  *mc,
				   TelepathyConnectionStatus        status,
				   McPresence                       presence,
				   TelepathyConnectionStatusReason  reason,
				   const gchar                     *unique_name,
				   EmpathyContactFactory           *factory)
{
	EmpathyContactFactoryPriv *priv = GET_PRIV (factory);
	ContactFactoryAccountData *account_data;
	McAccount                 *account;

	account = mc_account_lookup (unique_name);
	account_data = g_hash_table_lookup (priv->accounts, account);
	if (account_data) {
		contact_factory_account_data_update (account_data);
	}
	g_object_unref (account);
}

static ContactFactoryAccountData *
contact_factory_account_data_get (EmpathyContactFactory *factory,
				  McAccount             *account)
{
	EmpathyContactFactoryPriv *priv = GET_PRIV (factory);
	ContactFactoryAccountData *account_data;

	account_data = g_hash_table_lookup (priv->accounts, account);
	if (!account_data) {
		account_data = contact_factory_account_data_new (factory, account);
		g_hash_table_insert (priv->accounts,
				     g_object_ref (account),
				     account_data);
	}

	return account_data;
}

static void
contact_factory_account_data_add_contact (ContactFactoryAccountData *account_data,
					  EmpathyContact            *contact)
{
	g_object_weak_ref (G_OBJECT (contact),
			   contact_factory_weak_notify,
			   account_data);
	account_data->contacts = g_list_prepend (account_data->contacts, contact);

	if (!account_data->presence_iface) {
		EmpathyPresence *presence;

		/* We have no presence iface, set default presence
		 * to available */
		presence = empathy_presence_new_full (MC_PRESENCE_AVAILABLE,
						     NULL);

		empathy_contact_set_presence (contact, presence);
		g_object_unref (presence);
	}

	empathy_debug (DEBUG_DOMAIN, "Contact added: %s (%d)",
		       empathy_contact_get_id (contact),
		       empathy_contact_get_handle (contact));
}

static void
contact_factory_hold_handles_cb (DBusGProxy *proxy,
				 GError     *error,
				 gpointer    userdata)
{
	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Failed to hold handles: %s",
			       error->message);
	}
}

EmpathyContact *
empathy_contact_factory_get_user (EmpathyContactFactory *factory,
				  McAccount             *account)
{
	ContactFactoryAccountData *account_data;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_FACTORY (factory), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

	account_data = contact_factory_account_data_get (factory, account);

	return empathy_contact_factory_get_from_handle (factory, account,
							account_data->self_handle);
}

EmpathyContact *
empathy_contact_factory_get_from_id (EmpathyContactFactory *factory,
				     McAccount             *account,
				     const gchar           *id)
{
	ContactFactoryAccountData *account_data;
	EmpathyContact            *contact;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_FACTORY (factory), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	/* Check if the contact already exists */
	account_data = contact_factory_account_data_get (factory, account);
	contact = contact_factory_account_data_find_by_id (account_data, id);
	if (contact) {
		return g_object_ref (contact);
	}

	/* Create new contact */
	contact = g_object_new (EMPATHY_TYPE_CONTACT,
				"account", account,
				"id", id,
				NULL);
	contact_factory_account_data_add_contact (account_data, contact);

	/* If the account is connected, request contact's handle */
	if (account_data->tp_conn) {
		RequestHandlesData *data;
		const gchar        *contact_ids[] = {id, NULL};
		
		data = g_slice_new (RequestHandlesData);
		data->account_data = contact_factory_account_data_ref (account_data);
		data->contacts = g_list_prepend (NULL, g_object_ref (contact));
		tp_conn_request_handles_async (DBUS_G_PROXY (account_data->tp_conn),
					       TP_HANDLE_TYPE_CONTACT,
					       contact_ids,
					       contact_factory_request_handles_cb,
					       data);
	}

	return contact;
}

EmpathyContact *
empathy_contact_factory_get_from_handle (EmpathyContactFactory *factory,
					 McAccount             *account,
					 guint                  handle)
{
	EmpathyContact *contact;
	GArray         *handles;
	GList          *contacts;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_FACTORY (factory), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);

	contacts = empathy_contact_factory_get_from_handles (factory, account, handles);
	g_array_free (handles, TRUE);

	contact = contacts ? contacts->data : NULL;
	g_list_free (contacts);

	return contact;
}

GList *
empathy_contact_factory_get_from_handles (EmpathyContactFactory *factory,
					  McAccount             *account,
					  GArray                *handles)
{
	ContactFactoryAccountData *account_data;
	GList                     *contacts = NULL;
	GArray                    *new_handles;
	gchar                    **handles_names;
	guint                      i;
	GError                    *error = NULL;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_FACTORY (factory), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (handles != NULL, NULL);

	/* Search all contacts we already have */
	account_data = contact_factory_account_data_get (factory, account);
	new_handles = g_array_new (FALSE, FALSE, sizeof (guint));
	for (i = 0; i < handles->len; i++) {
		EmpathyContact *contact;
		guint           handle;

		handle = g_array_index (handles, guint, i);
		if (handle == 0) {
			continue;
		}

		contact = contact_factory_account_data_find_by_handle (account_data, handle);
		if (contact) {
			contacts = g_list_prepend (contacts, g_object_ref (contact));
		} else {
			g_array_append_val (new_handles, handle);
		}
	}

	if (new_handles->len == 0) {
		g_array_free (new_handles, TRUE);
		return contacts;
	}

	/* Get the IDs of all new handles */
	if (!tp_conn_inspect_handles (DBUS_G_PROXY (account_data->tp_conn),
				      TP_HANDLE_TYPE_CONTACT,
				      new_handles,
				      &handles_names,
				      &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "Couldn't inspect contact: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		g_array_free (new_handles, TRUE);
		return contacts;
	}

	/* Create new contacts */
	for (i = 0; i < new_handles->len; i++) {
		EmpathyContact *contact;
		gchar          *id;
		guint           handle;
		gboolean        is_user;

		id = handles_names[i];
		handle = g_array_index (new_handles, guint, i);

		is_user = (handle == account_data->self_handle);
		contact = g_object_new (EMPATHY_TYPE_CONTACT,
					"account", account,
					"handle", handle,
					"id", id,
					"is-user", is_user,
					NULL);
		contact_factory_account_data_add_contact (account_data,
							  contact);
		contacts = g_list_prepend (contacts, contact);
		g_free (id);
	}
	g_free (handles_names);

	/* Hold all new handles. */
	tp_conn_hold_handles_async (DBUS_G_PROXY (account_data->tp_conn),
				    TP_HANDLE_TYPE_CONTACT,
				    new_handles,
				    contact_factory_hold_handles_cb,
				    NULL);

	contact_factory_request_everything (account_data, new_handles);

	g_array_free (new_handles, TRUE);

	return contacts;
}

void
empathy_contact_factory_set_name (EmpathyContactFactory *factory,
				  EmpathyContact        *contact,
				  const gchar           *name)
{
	ContactFactoryAccountData *account_data;
	McAccount                 *account;
	GHashTable                *new_alias;
	guint                      handle;

	g_return_if_fail (EMPATHY_IS_CONTACT_FACTORY (factory));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	account = empathy_contact_get_account (contact);
	account_data = contact_factory_account_data_get (factory, account);

	if (!account_data->aliasing_iface) {
		return;
	}

	handle = empathy_contact_get_handle (contact);

	empathy_debug (DEBUG_DOMAIN, "Setting alias for contact %s (%d) to %s",
		       empathy_contact_get_id (contact),
		       handle, name);

	new_alias = g_hash_table_new_full (g_direct_hash,
					   g_direct_equal,
					   NULL,
					   g_free);

	g_hash_table_insert (new_alias,
			     GUINT_TO_POINTER (handle),
			     g_strdup (name));

	tp_conn_iface_aliasing_set_aliases_async (account_data->aliasing_iface,
						  new_alias,
						  contact_factory_set_aliases_cb,
						  contact_factory_account_data_ref (account_data));

	g_hash_table_destroy (new_alias);
}

static void
contact_factory_finalize (GObject *object)
{
	EmpathyContactFactoryPriv *priv;

	priv = GET_PRIV (object);

	dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (priv->mc),
					"AccountStatusChanged",
					G_CALLBACK (contact_factory_status_changed_cb),
					object);

	g_hash_table_destroy (priv->accounts);
	g_object_unref (priv->mc);

	G_OBJECT_CLASS (empathy_contact_factory_parent_class)->finalize (object);
}

static void
empathy_contact_factory_class_init (EmpathyContactFactoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = contact_factory_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyContactFactoryPriv));
}

static void
empathy_contact_factory_init (EmpathyContactFactory *factory)
{
	EmpathyContactFactoryPriv *priv;

	priv = GET_PRIV (factory);

	priv->mc = empathy_mission_control_new ();
	priv->accounts = g_hash_table_new_full (empathy_account_hash,
						empathy_account_equal,
						g_object_unref,
						(GDestroyNotify) contact_factory_account_data_unref);

	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->mc),
				     "AccountStatusChanged",
				     G_CALLBACK (contact_factory_status_changed_cb),
				     factory, NULL);
}

EmpathyContactFactory *
empathy_contact_factory_new (void)
{
	static EmpathyContactFactory *factory = NULL;

	if (!factory) {
		factory = g_object_new (EMPATHY_TYPE_CONTACT_FACTORY, NULL);
		g_object_add_weak_pointer (G_OBJECT (factory), (gpointer) &factory);
	} else {
		g_object_ref (factory);
	}

	return factory;
}

