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

#include <telepathy-glib/util.h>
#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-conn-iface-aliasing-gen.h>
#include <libtelepathy/tp-conn-iface-presence-gen.h>
#include <libtelepathy/tp-conn-iface-avatars-gen.h>
#include <libtelepathy/tp-conn-iface-capabilities-gen.h>
#include <libmissioncontrol/mission-control.h>

#include "empathy-tp-contact-factory.h"
#include "empathy-utils.h"
#include "empathy-debug.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_TP_CONTACT_FACTORY, EmpathyTpContactFactoryPriv))

#define DEBUG_DOMAIN "TpContactFactory"

struct _EmpathyTpContactFactoryPriv {
	MissionControl *mc;
	McAccount      *account;

	TpConn         *tp_conn;
	DBusGProxy     *aliasing_iface;
	DBusGProxy     *avatars_iface;
	DBusGProxy     *presence_iface;
	DBusGProxy     *capabilities_iface;

	GList          *contacts;
	guint           self_handle;
};

static void empathy_tp_contact_factory_class_init (EmpathyTpContactFactoryClass *klass);
static void empathy_tp_contact_factory_init       (EmpathyTpContactFactory      *factory);

G_DEFINE_TYPE (EmpathyTpContactFactory, empathy_tp_contact_factory, G_TYPE_OBJECT);

enum {
	PROP_0,
	PROP_ACCOUNT,
};

static EmpathyContact *
tp_contact_factory_find_by_handle (EmpathyTpContactFactory *tp_factory,
				   guint                    handle)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	GList                       *l;

	for (l = priv->contacts; l; l = l->next) {
		if (empathy_contact_get_handle (l->data) == handle) {
			return l->data;
		}
	}

	return NULL;
}

static EmpathyContact *
tp_contact_factory_find_by_id (EmpathyTpContactFactory *tp_factory,
			       const gchar             *id)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	GList                       *l;

	for (l = priv->contacts; l; l = l->next) {
		if (!tp_strdiff (empathy_contact_get_id (l->data), id)) {
			return l->data;
		}
	}

	return NULL;
}

static void
tp_contact_factory_weak_notify (gpointer data,
				GObject *where_the_object_was)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (data);

	empathy_debug (DEBUG_DOMAIN, "Remove finalized contact %p",
		       where_the_object_was);

	priv->contacts = g_list_remove (priv->contacts, where_the_object_was);
}

static void
tp_contact_factory_presences_table_foreach (const gchar    *state_str,
					    GHashTable     *presences_table,
					    EmpathyContact *contact)
{
	const GValue *message;

	empathy_contact_set_presence (contact,
				      empathy_presence_from_str (state_str));
	
	message = g_hash_table_lookup (presences_table, "message");
	if (message != NULL) {
		empathy_contact_set_presence_message (contact,
						      g_value_get_string (message));
	} else {
		empathy_contact_set_presence_message (contact, NULL);
	}
}

static void
tp_contact_factory_parse_presence_foreach (guint                    handle,
					   GValueArray             *presence_struct,
					   EmpathyTpContactFactory *tp_factory)
{
	GHashTable      *presences_table;
	EmpathyContact  *contact;

	contact = tp_contact_factory_find_by_handle (tp_factory, handle);
	if (!contact) {
		return;
	}

	presences_table = g_value_get_boxed (g_value_array_get_nth (presence_struct, 1));

	g_hash_table_foreach (presences_table,
			      (GHFunc) tp_contact_factory_presences_table_foreach,
			      contact);

	empathy_debug (DEBUG_DOMAIN, "Changing presence for contact %s (%d) to %s (%d)",
		      empathy_contact_get_id (contact),
		      handle,
		      empathy_contact_get_presence_message (contact),
		      empathy_contact_get_presence (contact));
}

static void
tp_contact_factory_get_presence_cb (DBusGProxy *proxy,
				    GHashTable *handle_table,
				    GError     *error,
				    gpointer    user_data)
{
	EmpathyTpContactFactory *tp_factory = user_data;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error getting presence: %s",
			      error->message);
		goto OUT;
	}

	g_hash_table_foreach (handle_table,
			      (GHFunc) tp_contact_factory_parse_presence_foreach,
			      tp_factory);

	g_hash_table_destroy (handle_table);
OUT:
	g_object_unref (tp_factory);
}

static void
tp_contact_factory_presence_update_cb (DBusGProxy              *proxy,
				       GHashTable              *handle_table,
				       EmpathyTpContactFactory *tp_factory)
{
	g_hash_table_foreach (handle_table,
			      (GHFunc) tp_contact_factory_parse_presence_foreach,
			      tp_factory);
}

static void
tp_contact_factory_set_aliases_cb (DBusGProxy *proxy,
				   GError     *error,
				   gpointer    user_data)
{
	EmpathyTpContactFactory *tp_factory = user_data;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error setting alias: %s",
			       error->message);
	}

	g_object_unref (tp_factory);
}

typedef struct {
	EmpathyTpContactFactory *tp_factory;
	guint                   *handles;
} RequestAliasesData;

static void
tp_contact_factory_request_aliases_cb (DBusGProxy  *proxy,
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

		contact = tp_contact_factory_find_by_handle (data->tp_factory,
							     data->handles[i]);
		if (!contact) {
			continue;
		}

		empathy_debug (DEBUG_DOMAIN, "Renaming contact %s (%d) to %s (request cb)",
			       empathy_contact_get_id (contact),
			       data->handles[i], *name);

		empathy_contact_set_name (contact, *name);

		i++;
	}

	g_strfreev (contact_names);
OUT:
	g_object_unref (data->tp_factory);
	g_free (data->handles);
	g_slice_free (RequestAliasesData, data);
}

static void
tp_contact_factory_aliases_changed_cb (DBusGProxy *proxy,
				       GPtrArray  *renamed_handlers,
				       gpointer    user_data)
{
	EmpathyTpContactFactory *tp_factory = user_data;
	guint                    i;

	for (i = 0; renamed_handlers->len > i; i++) {
		guint           handle;
		const gchar    *alias;
		GValueArray    *renamed_struct;
		EmpathyContact *contact;

		renamed_struct = g_ptr_array_index (renamed_handlers, i);
		handle = g_value_get_uint(g_value_array_get_nth (renamed_struct, 0));
		alias = g_value_get_string(g_value_array_get_nth (renamed_struct, 1));
		contact = tp_contact_factory_find_by_handle (tp_factory, handle);

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
tp_contact_factory_set_avatar_cb (DBusGProxy *proxy,
				  gchar      *token,
				  GError     *error,
				  gpointer    user_data)
{
	EmpathyTpContactFactory *tp_factory = user_data;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error setting avatar: %s",
			       error->message);
	}

	g_object_unref (tp_factory);
	g_free (token);
}

static void
tp_contact_factory_clear_avatar_cb (DBusGProxy *proxy,
				    GError     *error,
				    gpointer    user_data)
{
	EmpathyTpContactFactory *tp_factory = user_data;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error clearing avatar: %s",
			       error->message);
	}

	g_object_unref (tp_factory);
}

static void
tp_contact_factory_avatar_retrieved_cb (DBusGProxy *proxy,
					guint       handle,
					gchar      *token,
					GArray     *avatar_data,
					gchar      *mime_type,
					gpointer    user_data)
{
	EmpathyTpContactFactory *tp_factory = user_data;
	EmpathyContact          *contact;
	EmpathyAvatar           *avatar;

	contact = tp_contact_factory_find_by_handle (tp_factory, handle);
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
tp_contact_factory_request_avatars_cb (DBusGProxy *proxy,
				       GError     *error,
				       gpointer    user_data)
{
	EmpathyTpContactFactory *tp_factory = user_data;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error requesting avatars: %s",
			       error->message);
	}

	g_object_unref (tp_factory);
}

static gboolean
tp_contact_factory_avatar_maybe_update (EmpathyTpContactFactory *tp_factory,
					guint                    handle,
					const gchar             *token)
{
	EmpathyContact *contact;
	EmpathyAvatar  *avatar;

	contact = tp_contact_factory_find_by_handle (tp_factory, handle);
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
	if (avatar && !tp_strdiff (avatar->token, token)) {
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

typedef struct {
	EmpathyTpContactFactory *tp_factory;
	GArray                  *handles;
} TokensData;

static void
tp_contact_factory_avatar_tokens_foreach (gpointer key,
					  gpointer value,
					  gpointer user_data)
{
	TokensData  *data = user_data;
	const gchar *token = value;
	guint        handle = GPOINTER_TO_UINT (key);

	if (!tp_contact_factory_avatar_maybe_update (data->tp_factory,
						     handle, token)) {
		g_array_append_val (data->handles, handle);
	}
}

static void
tp_contact_factory_get_known_avatar_tokens_cb (DBusGProxy *proxy,
					       GHashTable *tokens,
					       GError     *error,
					       gpointer    user_data)
{
	EmpathyTpContactFactory     *tp_factory = user_data;
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	TokensData                   data;

	if (error) {
		empathy_debug (DEBUG_DOMAIN,
			       "Error getting known avatars tokens: %s",
			       error->message);
		goto OUT;
	}

	data.tp_factory = tp_factory;
	data.handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_hash_table_foreach (tokens,
			      tp_contact_factory_avatar_tokens_foreach,
			      &data);

	empathy_debug (DEBUG_DOMAIN, "Got %d tokens, need to request %d avatars",
		       g_hash_table_size (tokens),
		       data.handles->len);

	/* Request needed avatars */
	if (data.handles->len > 0) {
		tp_conn_iface_avatars_request_avatars_async (priv->avatars_iface,
							     data.handles,
							     tp_contact_factory_request_avatars_cb,
							     g_object_ref (tp_factory));
	}

	g_hash_table_destroy (tokens);
	g_array_free (data.handles, TRUE);
OUT:
	g_object_unref (tp_factory);
}

static void
tp_contact_factory_avatar_updated_cb (DBusGProxy *proxy,
				      guint       handle,
				      gchar      *new_token,
				      gpointer    user_data)
{
	EmpathyTpContactFactory     *tp_factory = user_data;
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	GArray                      *handles;

	if (tp_contact_factory_avatar_maybe_update (tp_factory, handle, new_token)) {
		/* Avatar was cached, nothing to do */
		return;
	}

	empathy_debug (DEBUG_DOMAIN, "Need to request avatar for token %s",
		       new_token);

	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);

	tp_conn_iface_avatars_request_avatars_async (priv->avatars_iface,
						     handles,
						     tp_contact_factory_request_avatars_cb,
						     g_object_ref (tp_factory));
	g_array_free (handles, TRUE);
}

static void
tp_contact_factory_update_capabilities (EmpathyTpContactFactory *tp_factory,
					guint                    handle,
					const gchar             *channel_type,
					guint                    generic,
					guint                    specific)
{
	EmpathyContact      *contact;
	EmpathyCapabilities  capabilities;

	contact = tp_contact_factory_find_by_handle (tp_factory, handle);
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
tp_contact_factory_get_capabilities_cb (DBusGProxy *proxy,
					GPtrArray  *capabilities,
					GError     *error,
					gpointer    user_data)
{
	EmpathyTpContactFactory *tp_factory = user_data;
	guint                    i;

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

		tp_contact_factory_update_capabilities (tp_factory,
							handle,
							channel_type,
							generic,
							specific);

		g_value_array_free (values);
	}

	g_ptr_array_free (capabilities, TRUE);
OUT:
	g_object_unref (tp_factory);
}

static void
tp_contact_factory_capabilities_changed_cb (DBusGProxy *proxy,
					    GPtrArray  *capabilities,
					    gpointer    user_data)
{
	EmpathyTpContactFactory *tp_factory = user_data;
	guint                    i;

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

		tp_contact_factory_update_capabilities (tp_factory,
							handle,
							channel_type,
							generic,
							specific);
	}
}

static void
tp_contact_factory_request_everything (EmpathyTpContactFactory *tp_factory,
				       GArray                  *handles)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	if (priv->presence_iface) {
		tp_conn_iface_presence_get_presence_async (priv->presence_iface,
							   handles,
							   tp_contact_factory_get_presence_cb,
							   g_object_ref (tp_factory));
	}

	if (priv->aliasing_iface) {
		RequestAliasesData *data;

		data = g_slice_new (RequestAliasesData);
		data->tp_factory = g_object_ref (tp_factory);
		data->handles = g_memdup (handles->data, handles->len * sizeof (guint));

		tp_conn_iface_aliasing_request_aliases_async (priv->aliasing_iface,
							      handles,
							      tp_contact_factory_request_aliases_cb,
							      data);
	}

	if (priv->avatars_iface) {
		tp_conn_iface_avatars_get_known_avatar_tokens_async (priv->avatars_iface,
								     handles,
								     tp_contact_factory_get_known_avatar_tokens_cb,
								     g_object_ref (tp_factory));
	}

	if (priv->capabilities_iface) {
		tp_conn_iface_capabilities_get_capabilities_async (priv->capabilities_iface,
								   handles,
								   tp_contact_factory_get_capabilities_cb,
								   g_object_ref (tp_factory));
	}
}

typedef struct {
	EmpathyTpContactFactory *tp_factory;
	GList                   *contacts;
} RequestHandlesData;

static void
tp_contact_factory_request_handles_cb (DBusGProxy *proxy,
				       GArray     *handles,
				       GError     *error,
				       gpointer    user_data)
{
	RequestHandlesData          *data = user_data;
	EmpathyTpContactFactory     *tp_factory = data->tp_factory;
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	GList                       *l;
	guint                        i = 0;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Failed to request handles: %s",
			       error->message);
		goto OUT;
	}

	for (l = data->contacts; l; l = l->next) {
		guint handle;

		handle = g_array_index (handles, guint, i);
		empathy_contact_set_handle (l->data, handle);
		if (handle == priv->self_handle) {
			empathy_contact_set_is_user (l->data, TRUE);
		}

		i++;
	}

	tp_contact_factory_request_everything (tp_factory, handles);
	g_array_free (handles, TRUE);

OUT:
	g_list_foreach (data->contacts, (GFunc) g_object_unref, NULL);
	g_list_free (data->contacts);
	g_object_unref (tp_factory);
	g_slice_free (RequestHandlesData, data);
}

static void
tp_contact_factory_disconnect_contact_foreach (gpointer data,
					       gpointer user_data)
{
	EmpathyContact *contact = data;
	
	empathy_contact_set_presence (contact, MC_PRESENCE_UNSET);
	empathy_contact_set_handle (contact, 0);
}

static void
tp_contact_factory_destroy_cb (TpConn                  *tp_conn,
			       EmpathyTpContactFactory *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	empathy_debug (DEBUG_DOMAIN, "Account disconnected or CM crashed");

	g_object_unref (priv->tp_conn);
	priv->tp_conn = NULL;
	priv->aliasing_iface = NULL;
	priv->avatars_iface = NULL;
	priv->presence_iface = NULL;
	priv->capabilities_iface = NULL;

	g_list_foreach (priv->contacts,
			tp_contact_factory_disconnect_contact_foreach,
			tp_factory);
}

static void
tp_contact_factory_disconnect (EmpathyTpContactFactory *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	if (priv->aliasing_iface) {
		dbus_g_proxy_disconnect_signal (priv->aliasing_iface,
						"AliasesChanged",
						G_CALLBACK (tp_contact_factory_aliases_changed_cb),
						tp_factory);
	}
	if (priv->avatars_iface) {
		dbus_g_proxy_disconnect_signal (priv->avatars_iface,
						"AvatarUpdated",
						G_CALLBACK (tp_contact_factory_avatar_updated_cb),
						tp_factory);
		dbus_g_proxy_disconnect_signal (priv->avatars_iface,
						"AvatarRetrieved",
						G_CALLBACK (tp_contact_factory_avatar_retrieved_cb),
						tp_factory);
	}
	if (priv->presence_iface) {
		dbus_g_proxy_disconnect_signal (priv->presence_iface,
						"PresenceUpdate",
						G_CALLBACK (tp_contact_factory_presence_update_cb),
						tp_factory);
	}
	if (priv->capabilities_iface) {
		dbus_g_proxy_disconnect_signal (priv->capabilities_iface,
						"CapabilitiesChanged",
						G_CALLBACK (tp_contact_factory_capabilities_changed_cb),
						tp_factory);
	}
	if (priv->tp_conn) {
		g_signal_handlers_disconnect_by_func (priv->tp_conn,
						      tp_contact_factory_destroy_cb,
						      tp_factory);
	}
}

static void
tp_contact_factory_update (EmpathyTpContactFactory *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	TpConn                      *tp_conn = NULL;
	RequestHandlesData          *data;
	const gchar                **contact_ids;
	guint                        i;
	GList                       *l;
	GError                      *error = NULL;

	if (priv->account) {
		guint status;

		/* status == 0 means the status is CONNECTED */
		status = mission_control_get_connection_status (priv->mc,
								priv->account,
								NULL);
		if (status == 0) {
			tp_conn = mission_control_get_connection (priv->mc,
								  priv->account,
								  NULL);
		}
	}

	if (!tp_conn) {
		/* We are not connected anymore, remove the old connection */
		tp_contact_factory_disconnect (tp_factory);
		if (priv->tp_conn) {
			tp_contact_factory_destroy_cb (priv->tp_conn, tp_factory);
		}
		return;
	}
	else if (priv->tp_conn) {
		/* We were connected and we still are connected, nothing
		 * changed so nothing to do. */
		g_object_unref (tp_conn);
		return;
	}

	/* We got a new connection */
	priv->tp_conn = tp_conn;
	priv->aliasing_iface = tp_conn_get_interface (priv->tp_conn,
						      TP_IFACE_QUARK_CONNECTION_INTERFACE_ALIASING);
	priv->avatars_iface = tp_conn_get_interface (priv->tp_conn,
						     TP_IFACE_QUARK_CONNECTION_INTERFACE_AVATARS);
	priv->presence_iface = tp_conn_get_interface (priv->tp_conn,
						      TP_IFACE_QUARK_CONNECTION_INTERFACE_PRESENCE);
	priv->capabilities_iface = tp_conn_get_interface (priv->tp_conn,
							  TP_IFACE_QUARK_CONNECTION_INTERFACE_CAPABILITIES);

	/* Connect signals */
	if (priv->aliasing_iface) {
		dbus_g_proxy_connect_signal (priv->aliasing_iface,
					     "AliasesChanged",
					     G_CALLBACK (tp_contact_factory_aliases_changed_cb),
					     tp_factory, NULL);
	}
	if (priv->avatars_iface) {
		dbus_g_proxy_connect_signal (priv->avatars_iface,
					     "AvatarUpdated",
					     G_CALLBACK (tp_contact_factory_avatar_updated_cb),
					     tp_factory, NULL);
		dbus_g_proxy_connect_signal (priv->avatars_iface,
					     "AvatarRetrieved",
					     G_CALLBACK (tp_contact_factory_avatar_retrieved_cb),
					     tp_factory, NULL);
	}
	if (priv->presence_iface) {
		dbus_g_proxy_connect_signal (priv->presence_iface,
					     "PresenceUpdate",
					     G_CALLBACK (tp_contact_factory_presence_update_cb),
					     tp_factory, NULL);
	}
	if (priv->capabilities_iface) {
		dbus_g_proxy_connect_signal (priv->capabilities_iface,
					     "CapabilitiesChanged",
					     G_CALLBACK (tp_contact_factory_capabilities_changed_cb),
					     tp_factory, NULL);
	}
	g_signal_connect (priv->tp_conn, "destroy",
			  G_CALLBACK (tp_contact_factory_destroy_cb),
			  tp_factory);

	/* Get our own handle */
	if (!tp_conn_get_self_handle (DBUS_G_PROXY (priv->tp_conn),
				      &priv->self_handle,
				      &error)) {
		empathy_debug (DEBUG_DOMAIN, "GetSelfHandle Error: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}

	/* Request new handles for all contacts */
	if (priv->contacts) {
		data = g_slice_new (RequestHandlesData);
		data->tp_factory = g_object_ref (tp_factory);
		data->contacts = g_list_copy (priv->contacts);
		g_list_foreach (data->contacts, (GFunc) g_object_ref, NULL);

		i = g_list_length (data->contacts);
		contact_ids = g_new0 (const gchar*, i + 1);
		i = 0;
		for (l = data->contacts; l; l = l->next) {
			contact_ids[i] = empathy_contact_get_id (l->data);
			i++;
		}

		tp_conn_request_handles_async (DBUS_G_PROXY (priv->tp_conn),
					       TP_HANDLE_TYPE_CONTACT,
					       contact_ids,
					       tp_contact_factory_request_handles_cb,
					       data);
		g_free (contact_ids);
	}
}

static void
tp_contact_factory_status_changed_cb (MissionControl           *mc,
				      TpConnectionStatus        status,
				      McPresence                presence,
				      TpConnectionStatusReason  reason,
				      const gchar              *unique_name,
				      EmpathyTpContactFactory  *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	McAccount                   *account;

	account = mc_account_lookup (unique_name);
	if (account && empathy_account_equal (account, priv->account)) {
		tp_contact_factory_update (tp_factory);
	}
	g_object_unref (account);
}

static void
tp_contact_factory_add_contact (EmpathyTpContactFactory *tp_factory,
				EmpathyContact          *contact)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	g_object_weak_ref (G_OBJECT (contact),
			   tp_contact_factory_weak_notify,
			   tp_factory);
	priv->contacts = g_list_prepend (priv->contacts, contact);

	if (!priv->presence_iface) {
		/* We have no presence iface, set default presence
		 * to available */
		empathy_contact_set_presence (contact, MC_PRESENCE_AVAILABLE);
	}

	empathy_debug (DEBUG_DOMAIN, "Contact added: %s (%d)",
		       empathy_contact_get_id (contact),
		       empathy_contact_get_handle (contact));
}

static void
tp_contact_factory_hold_handles_cb (DBusGProxy *proxy,
				    GError     *error,
				    gpointer    userdata)
{
	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Failed to hold handles: %s",
			       error->message);
	}
}

EmpathyContact *
empathy_tp_contact_factory_get_user (EmpathyTpContactFactory *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_FACTORY (tp_factory), NULL);

	return empathy_tp_contact_factory_get_from_handle (tp_factory,
							   priv->self_handle);
}

EmpathyContact *
empathy_tp_contact_factory_get_from_id (EmpathyTpContactFactory *tp_factory,
					const gchar             *id)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	EmpathyContact              *contact;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_FACTORY (tp_factory), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	/* Check if the contact already exists */
	contact = tp_contact_factory_find_by_id (tp_factory, id);
	if (contact) {
		return g_object_ref (contact);
	}

	/* Create new contact */
	contact = g_object_new (EMPATHY_TYPE_CONTACT,
				"account", priv->account,
				"id", id,
				NULL);
	tp_contact_factory_add_contact (tp_factory, contact);

	/* If the account is connected, request contact's handle */
	if (priv->tp_conn) {
		RequestHandlesData *data;
		const gchar        *contact_ids[] = {id, NULL};
		
		data = g_slice_new (RequestHandlesData);
		data->tp_factory = g_object_ref (tp_factory);
		data->contacts = g_list_prepend (NULL, g_object_ref (contact));
		tp_conn_request_handles_async (DBUS_G_PROXY (priv->tp_conn),
					       TP_HANDLE_TYPE_CONTACT,
					       contact_ids,
					       tp_contact_factory_request_handles_cb,
					       data);
	}

	return contact;
}

EmpathyContact *
empathy_tp_contact_factory_get_from_handle (EmpathyTpContactFactory *tp_factory,
					    guint                    handle)
{
	EmpathyContact *contact;
	GArray         *handles;
	GList          *contacts;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_FACTORY (tp_factory), NULL);

	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);

	contacts = empathy_tp_contact_factory_get_from_handles (tp_factory, handles);
	g_array_free (handles, TRUE);

	contact = contacts ? contacts->data : NULL;
	g_list_free (contacts);

	return contact;
}

GList *
empathy_tp_contact_factory_get_from_handles (EmpathyTpContactFactory *tp_factory,
					     GArray                  *handles)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	GList                       *contacts = NULL;
	GArray                      *new_handles;
	gchar                      **handles_names;
	guint                        i;
	GError                      *error = NULL;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_FACTORY (tp_factory), NULL);
	g_return_val_if_fail (handles != NULL, NULL);

	/* Search all contacts we already have */
	new_handles = g_array_new (FALSE, FALSE, sizeof (guint));
	for (i = 0; i < handles->len; i++) {
		EmpathyContact *contact;
		guint           handle;

		handle = g_array_index (handles, guint, i);
		if (handle == 0) {
			continue;
		}

		contact = tp_contact_factory_find_by_handle (tp_factory, handle);
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
	if (!tp_conn_inspect_handles (DBUS_G_PROXY (priv->tp_conn),
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

		is_user = (handle == priv->self_handle);
		contact = g_object_new (EMPATHY_TYPE_CONTACT,
					"account", priv->account,
					"handle", handle,
					"id", id,
					"is-user", is_user,
					NULL);
		tp_contact_factory_add_contact (tp_factory, contact);
		contacts = g_list_prepend (contacts, contact);
		g_free (id);
	}
	g_free (handles_names);

	/* Hold all new handles. */
	tp_conn_hold_handles_async (DBUS_G_PROXY (priv->tp_conn),
				    TP_HANDLE_TYPE_CONTACT,
				    new_handles,
				    tp_contact_factory_hold_handles_cb,
				    NULL);

	tp_contact_factory_request_everything (tp_factory, new_handles);

	g_array_free (new_handles, TRUE);

	return contacts;
}

void
empathy_tp_contact_factory_set_alias (EmpathyTpContactFactory *tp_factory,
				      EmpathyContact          *contact,
				      const gchar             *alias)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	GHashTable                  *new_alias;
	guint                        handle;

	g_return_if_fail (EMPATHY_IS_TP_CONTACT_FACTORY (tp_factory));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));
	g_return_if_fail (empathy_account_equal (empathy_contact_get_account (contact),
						 priv->account));

	if (!priv->aliasing_iface) {
		return;
	}

	handle = empathy_contact_get_handle (contact);

	empathy_debug (DEBUG_DOMAIN, "Setting alias for contact %s (%d) to %s",
		       empathy_contact_get_id (contact),
		       handle, alias);

	new_alias = g_hash_table_new_full (g_direct_hash,
					   g_direct_equal,
					   NULL,
					   g_free);

	g_hash_table_insert (new_alias,
			     GUINT_TO_POINTER (handle),
			     g_strdup (alias));

	tp_conn_iface_aliasing_set_aliases_async (priv->aliasing_iface,
						  new_alias,
						  tp_contact_factory_set_aliases_cb,
						  g_object_ref (tp_factory));

	g_hash_table_destroy (new_alias);
}

void
empathy_tp_contact_factory_set_avatar (EmpathyTpContactFactory *tp_factory,
				       const gchar             *data,
				       gsize                    size,
				       const gchar             *mime_type)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	g_return_if_fail (EMPATHY_IS_TP_CONTACT_FACTORY (tp_factory));

	if (!priv->avatars_iface) {
		return;
	}

	if (data && size > 0 && size < G_MAXUINT) {
		GArray avatar;

		avatar.data = (gchar*) data;
		avatar.len = size;

		empathy_debug (DEBUG_DOMAIN, "Setting avatar on account %s",
			       mc_account_get_unique_name (priv->account));

		tp_conn_iface_avatars_set_avatar_async (priv->avatars_iface,
							&avatar,
							mime_type,
							tp_contact_factory_set_avatar_cb,
							g_object_ref (tp_factory));
	} else {
		empathy_debug (DEBUG_DOMAIN, "Clearing avatar on account %s",
			       mc_account_get_unique_name (priv->account));
		tp_conn_iface_avatars_clear_avatar_async (priv->avatars_iface,
							  tp_contact_factory_clear_avatar_cb,
							  g_object_ref (tp_factory));
	}
}

static void
tp_contact_factory_get_property (GObject    *object,
				 guint       param_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ACCOUNT:
		g_value_set_object (value, priv->account);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
tp_contact_factory_set_property (GObject      *object,
				 guint         param_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_ACCOUNT:
		priv->account = g_object_ref (g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
tp_contact_factory_finalize (GObject *object)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (object);
	GList                       *l;

	empathy_debug (DEBUG_DOMAIN, "Finalized: %p (%s)",
		       object,
		       mc_account_get_normalized_name (priv->account));

	tp_contact_factory_disconnect (EMPATHY_TP_CONTACT_FACTORY (object));
	dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (priv->mc),
					"AccountStatusChanged",
					G_CALLBACK (tp_contact_factory_status_changed_cb),
					object);

	for (l = priv->contacts; l; l = l->next) {
		g_object_weak_unref (G_OBJECT (l->data),
				     tp_contact_factory_weak_notify,
				     object);
	}

	g_list_free (priv->contacts);
	g_object_unref (priv->mc);
	g_object_unref (priv->account);

	if (priv->tp_conn) {
		g_object_unref (priv->tp_conn);
	}

	G_OBJECT_CLASS (empathy_tp_contact_factory_parent_class)->finalize (object);
}

static GObject *
tp_contact_factory_constructor (GType                  type,
				guint                  n_props,
				GObjectConstructParam *props)
{
	GObject *tp_factory;

	tp_factory = G_OBJECT_CLASS (empathy_tp_contact_factory_parent_class)->constructor (type, n_props, props);

	tp_contact_factory_update (EMPATHY_TP_CONTACT_FACTORY (tp_factory));

	return tp_factory;
}


static void
empathy_tp_contact_factory_class_init (EmpathyTpContactFactoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tp_contact_factory_finalize;
	object_class->constructor = tp_contact_factory_constructor;
	object_class->get_property = tp_contact_factory_get_property;
	object_class->set_property = tp_contact_factory_set_property;

	/* Construct-only properties */
	g_object_class_install_property (object_class,
					 PROP_ACCOUNT,
					 g_param_spec_object ("account",
							      "Factory's Account",
							      "The account associated with the factory",
							      MC_TYPE_ACCOUNT,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (EmpathyTpContactFactoryPriv));
}

static void
empathy_tp_contact_factory_init (EmpathyTpContactFactory *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	priv->mc = empathy_mission_control_new ();
	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->mc),
				     "AccountStatusChanged",
				     G_CALLBACK (tp_contact_factory_status_changed_cb),
				     tp_factory, NULL);
}

EmpathyTpContactFactory *
empathy_tp_contact_factory_new (McAccount *account)
{
	return g_object_new (EMPATHY_TYPE_TP_CONTACT_FACTORY,
			     "account", account,
			     NULL);
}

