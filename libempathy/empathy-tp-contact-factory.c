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

#include <telepathy-glib/util.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/gtypes.h>
#include <libmissioncontrol/mission-control.h>

#include <extensions/extensions.h>

#include "empathy-tp-contact-factory.h"
#include "empathy-utils.h"
#include "empathy-account-manager.h"

#define DEBUG_FLAG EMPATHY_DEBUG_TP | EMPATHY_DEBUG_CONTACT
#include "empathy-debug.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyTpContactFactory)
typedef struct {
	EmpathyAccountManager *account_manager;
	McAccount      *account;
	TpConnection   *connection;
	gboolean        ready;

	GList          *contacts;
	EmpathyContact *user;

	gchar         **avatar_mime_types;
	guint           avatar_min_width;
	guint           avatar_min_height;
	guint           avatar_max_width;
	guint           avatar_max_height;
	guint           avatar_max_size;
  gboolean        can_request_ft;
} EmpathyTpContactFactoryPriv;

G_DEFINE_TYPE (EmpathyTpContactFactory, empathy_tp_contact_factory, G_TYPE_OBJECT);

enum {
	PROP_0,
	PROP_ACCOUNT,
	PROP_READY,

	PROP_MIME_TYPES,
	PROP_MIN_WIDTH,
	PROP_MIN_HEIGHT,
	PROP_MAX_WIDTH,
	PROP_MAX_HEIGHT,
	PROP_MAX_SIZE
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

	DEBUG ("Remove finalized contact %p", where_the_object_was);

	priv->contacts = g_list_remove (priv->contacts, where_the_object_was);
}

static void
tp_contact_factory_presences_table_foreach (const gchar    *state_str,
					    GHashTable     *presences_table,
					    EmpathyContact *contact)
{
	const GValue *message;
	const gchar  *message_str = NULL;

	empathy_contact_set_presence (contact,
				      empathy_presence_from_str (state_str));
	
	message = g_hash_table_lookup (presences_table, "message");
	if (message) {
		message_str = g_value_get_string (message);
	}

	if (!EMP_STR_EMPTY (message_str)) {
		empathy_contact_set_presence_message (contact, message_str);
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

	DEBUG ("Changing presence for contact %s (%d) to '%s' (%d)",
		empathy_contact_get_id (contact),
		handle,
		empathy_contact_get_presence_message (contact),
		empathy_contact_get_presence (contact));
}

static void
tp_contact_factory_get_presence_cb (TpConnection *connection,
				    GHashTable   *handle_table,
				    const GError *error,
				    gpointer      user_data,
				    GObject      *tp_factory)
{
	if (error) {
		DEBUG ("Error getting presence: %s", error->message);
		if (error->domain == TP_DBUS_ERRORS &&
		    error->code == TP_DBUS_ERROR_NO_INTERFACE) {
			guint *handles = user_data;

			/* We have no presence iface, set default presence
			 * to available */
			while (*handles != 0) {
				EmpathyContact *contact;

				contact = tp_contact_factory_find_by_handle (
					(EmpathyTpContactFactory*) tp_factory,
					*handles);
				if (contact) {
					empathy_contact_set_presence (contact,
								      MC_PRESENCE_AVAILABLE);
				}

				handles++;
			}
		}

		return;
	}

	g_hash_table_foreach (handle_table,
			      (GHFunc) tp_contact_factory_parse_presence_foreach,
			      EMPATHY_TP_CONTACT_FACTORY (tp_factory));
}

static void
tp_contact_factory_presence_update_cb (TpConnection *connection,
				       GHashTable   *handle_table,
				       gpointer      user_data,
				       GObject      *tp_factory)
{
	g_hash_table_foreach (handle_table,
			      (GHFunc) tp_contact_factory_parse_presence_foreach,
			      EMPATHY_TP_CONTACT_FACTORY (tp_factory));
}

static void
tp_contact_factory_set_aliases_cb (TpConnection *connection,
				   const GError *error,
				   gpointer      user_data,
				   GObject      *tp_factory)
{
	if (error) {
		DEBUG ("Error setting alias: %s", error->message);
	}
}

static void
tp_contact_factory_request_aliases_cb (TpConnection *connection,
				       const gchar  **contact_names,
				       const GError  *error,
				       gpointer       user_data,
				       GObject       *tp_factory)
{
	guint        *handles = user_data;
	guint         i = 0;
	const gchar **name;

	if (error) {
		DEBUG ("Error requesting aliases: %s", error->message);

		/* If we failed to get alias set it to NULL, like that if
		 * someone is waiting for the name to be ready it won't wait
		 * infinitely */
		while (*handles != 0) {
			EmpathyContact *contact;

			contact = tp_contact_factory_find_by_handle (
				(EmpathyTpContactFactory*) tp_factory,
				*handles);
			if (contact) {
				empathy_contact_set_name (contact, NULL);
			}

			handles++;
		}
		return;
	}

	for (name = contact_names; *name; name++) {
		EmpathyContact *contact;

		contact = tp_contact_factory_find_by_handle (EMPATHY_TP_CONTACT_FACTORY (tp_factory),
							     handles[i]);
		if (!contact) {
			continue;
		}

		DEBUG ("Renaming contact %s (%d) to %s (request cb)",
			empathy_contact_get_id (contact),
			empathy_contact_get_handle (contact),
			*name);

		empathy_contact_set_name (contact, *name);

		i++;
	}
}

static void
tp_contact_factory_aliases_changed_cb (TpConnection    *connection,
				       const GPtrArray *renamed_handlers,
				       gpointer         user_data,
				       GObject         *weak_object)
{
	EmpathyTpContactFactory *tp_factory = EMPATHY_TP_CONTACT_FACTORY (weak_object);
	guint                    i;

	for (i = 0; renamed_handlers->len > i; i++) {
		guint           handle;
		const gchar    *alias;
		GValueArray    *renamed_struct;
		EmpathyContact *contact;

		renamed_struct = g_ptr_array_index (renamed_handlers, i);
		handle = g_value_get_uint (g_value_array_get_nth (renamed_struct, 0));
		alias = g_value_get_string (g_value_array_get_nth (renamed_struct, 1));
		contact = tp_contact_factory_find_by_handle (tp_factory, handle);

		if (!contact) {
			/* We don't know this contact, skip */
			continue;
		}

		DEBUG ("Renaming contact %s (%d) to %s (changed cb)",
			empathy_contact_get_id (contact),
			handle, alias);

		empathy_contact_set_name (contact, alias);
	}
}

static void
tp_contact_factory_set_avatar_cb (TpConnection *connection,
				  const gchar  *token,
				  const GError *error,
				  gpointer      user_data,
				  GObject      *tp_factory)
{
	if (error) {
		DEBUG ("Error setting avatar: %s", error->message);
	}
}

static void
tp_contact_factory_clear_avatar_cb (TpConnection *connection,
				    const GError *error,
				    gpointer      user_data,
				    GObject      *tp_factory)
{
	if (error) {
		DEBUG ("Error clearing avatar: %s", error->message);
	}
}

static void
tp_contact_factory_avatar_retrieved_cb (TpConnection *connection,
					guint         handle,
					const gchar  *token,
					const GArray *avatar_data,
					const gchar  *mime_type,
					gpointer      user_data,
					GObject      *tp_factory)
{
	EmpathyContact *contact;

	contact = tp_contact_factory_find_by_handle (EMPATHY_TP_CONTACT_FACTORY (tp_factory),
						     handle);
	if (!contact) {
		return;
	}

	DEBUG ("Avatar retrieved for contact %s (%d)",
		empathy_contact_get_id (contact),
		handle);

	empathy_contact_load_avatar_data (contact,
					  avatar_data->data,
					  avatar_data->len,
					  mime_type,
					  token);
}

static void
tp_contact_factory_request_avatars_cb (TpConnection *connection,
				       const GError *error,
				       gpointer      user_data,
				       GObject      *tp_factory)
{
	if (error) {
		DEBUG ("Error requesting avatars: %s", error->message);
	}
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
	if (EMP_STR_EMPTY (token)) {
		empathy_contact_set_avatar (contact, NULL);
		return TRUE;
	}

	/* Check if the avatar changed */
	avatar = empathy_contact_get_avatar (contact);
	if (avatar && !tp_strdiff (avatar->token, token)) {
		return TRUE;
	}

	/* The avatar changed, search the new one in the cache */
	if (empathy_contact_load_avatar_cache (contact, token)) {
		/* Got from cache, use it */
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
tp_contact_factory_get_known_avatar_tokens_cb (TpConnection *connection,
					       GHashTable   *tokens,
					       const GError *error,
					       gpointer      user_data,
					       GObject      *tp_factory)
{
	TokensData data;

	if (error) {
		DEBUG ("Error getting known avatars tokens: %s", error->message);
		return;
	}

	data.tp_factory = EMPATHY_TP_CONTACT_FACTORY (tp_factory);
	data.handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_hash_table_foreach (tokens,
			      tp_contact_factory_avatar_tokens_foreach,
			      &data);

	DEBUG ("Got %d tokens, need to request %d avatars",
		g_hash_table_size (tokens), data.handles->len);

	/* Request needed avatars */
	if (data.handles->len > 0) {
		tp_cli_connection_interface_avatars_call_request_avatars (connection,
									  -1,
									  data.handles,
									  tp_contact_factory_request_avatars_cb,
									  NULL, NULL,
									  tp_factory);
	}

	g_array_free (data.handles, TRUE);
}

static void
tp_contact_factory_avatar_updated_cb (TpConnection *connection,
				      guint         handle,
				      const gchar  *new_token,
				      gpointer      user_data,
				      GObject      *tp_factory)
{
	GArray *handles;

	if (tp_contact_factory_avatar_maybe_update (EMPATHY_TP_CONTACT_FACTORY (tp_factory),
						    handle, new_token)) {
		/* Avatar was cached, nothing to do */
		return;
	}

	DEBUG ("Need to request avatar for token %s", new_token);

	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);

	tp_cli_connection_interface_avatars_call_request_avatars (connection,
								  -1,
								  handles,
								  tp_contact_factory_request_avatars_cb,
								  NULL, NULL,
								  tp_factory);
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
	capabilities &= ~EMPATHY_CAPABILITIES_UNKNOWN;

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

	DEBUG ("Changing capabilities for contact %s (%d) to %d",
		empathy_contact_get_id (contact),
		empathy_contact_get_handle (contact),
		capabilities);

	empathy_contact_set_capabilities (contact, capabilities);
}

static void
tp_contact_factory_get_capabilities_cb (TpConnection    *connection,
					const GPtrArray *capabilities,
					const GError    *error,
					gpointer         user_data,
					GObject         *weak_object)
{
	EmpathyTpContactFactory *tp_factory = EMPATHY_TP_CONTACT_FACTORY (weak_object);
	guint                    i;

	if (error) {
		DEBUG ("Error getting capabilities: %s", error->message);
		/* FIXME Should set the capabilities of the contacts for which this request
		 * originated to NONE */
		return;
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
	}
}

static void
tp_contact_factory_capabilities_changed_cb (TpConnection    *connection,
					    const GPtrArray *capabilities,
					    gpointer         user_data,
					    GObject         *weak_object)
{
	EmpathyTpContactFactory *tp_factory = EMPATHY_TP_CONTACT_FACTORY (weak_object);
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
				       const GArray            *handles)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	guint                       *dup_handles;

	g_return_if_fail (priv->ready);

	dup_handles = g_malloc0 ((handles->len + 1) * sizeof (guint));
	g_memmove (dup_handles, handles->data, handles->len * sizeof (guint));
	tp_cli_connection_interface_presence_call_get_presence (priv->connection,
								-1,
								handles,
								tp_contact_factory_get_presence_cb,
								dup_handles, g_free,
								G_OBJECT (tp_factory));

	/* FIXME: Sometimes the dbus call timesout because CM takes
	 * too much time to request all aliases from the server,
	 * that's why we increase the timeout here. See fd.o bug #14795 */
	dup_handles = g_malloc0 ((handles->len + 1) * sizeof (guint));
	g_memmove (dup_handles, handles->data, handles->len * sizeof (guint));
	tp_cli_connection_interface_aliasing_call_request_aliases (priv->connection,
								   5*60*1000,
								   handles,
								   tp_contact_factory_request_aliases_cb,
								   dup_handles, g_free,
								   G_OBJECT (tp_factory));

	tp_cli_connection_interface_avatars_call_get_known_avatar_tokens (priv->connection,
									  -1,
									  handles,
									  tp_contact_factory_get_known_avatar_tokens_cb,
									  NULL, NULL,
									  G_OBJECT (tp_factory));

	tp_cli_connection_interface_capabilities_call_get_capabilities (priv->connection,
									-1,
									handles,
									tp_contact_factory_get_capabilities_cb,
									NULL, NULL,
									G_OBJECT (tp_factory));
}

static void
tp_contact_factory_list_free (gpointer data)
{
	GList *l = data;

	g_list_foreach (l, (GFunc) g_object_unref, NULL);
	g_list_free (l);
}

static void
tp_contact_factory_request_handles_cb (TpConnection *connection,
				       const GArray *handles,
				       const GError *error,
				       gpointer      user_data,
				       GObject      *tp_factory)
{
	GList *contacts = user_data;
	GList *l;
	guint  i = 0;

	if (error) {
		DEBUG ("Failed to request handles: %s", error->message);
		return;
	}

	for (l = contacts; l; l = l->next) {
		guint handle;

		handle = g_array_index (handles, guint, i);
		empathy_contact_set_handle (l->data, handle);

		i++;
	}

	tp_contact_factory_request_everything (EMPATHY_TP_CONTACT_FACTORY (tp_factory),
					       handles);
}

static void
tp_contact_factory_inspect_handles_cb (TpConnection  *connection,
				       const gchar  **ids,
				       const GError  *error,
				       gpointer       user_data,
				       GObject       *tp_factory)
{
	const gchar **id;
	GList        *contacts = user_data;
	GList        *l;

	if (error) {
		DEBUG ("Failed to inspect handles: %s", error->message);
		return;
	}

	id = ids;
	for (l = contacts; l; l = l->next) {
		empathy_contact_set_id (l->data, *id);
		id++;
	}
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
tp_contact_factory_connection_invalidated_cb (EmpathyTpContactFactory *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	DEBUG ("Connection invalidated");

	g_object_unref (priv->connection);
	priv->connection = NULL;
	priv->ready = FALSE;
	g_object_notify (G_OBJECT (tp_factory), "ready");


	g_list_foreach (priv->contacts,
			tp_contact_factory_disconnect_contact_foreach,
			tp_factory);
}

static void
tp_contact_factory_ready (EmpathyTpContactFactory *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	GList                       *l;
	GArray                      *handle_needed;
	GArray                      *id_needed;
	GList                       *handle_needed_contacts = NULL;
	GList                       *id_needed_contacts = NULL;

	DEBUG ("Connection ready");

	priv->ready = TRUE;
	g_object_notify (G_OBJECT (tp_factory), "ready");

	/* Connect signals */
	tp_cli_connection_interface_aliasing_connect_to_aliases_changed (priv->connection,
									 tp_contact_factory_aliases_changed_cb,
									 NULL, NULL,
									 G_OBJECT (tp_factory),
									 NULL);
	tp_cli_connection_interface_avatars_connect_to_avatar_updated (priv->connection,
								       tp_contact_factory_avatar_updated_cb,
								       NULL, NULL,
								       G_OBJECT (tp_factory),
								       NULL);
	tp_cli_connection_interface_avatars_connect_to_avatar_retrieved (priv->connection,
									 tp_contact_factory_avatar_retrieved_cb,
									 NULL, NULL,
									 G_OBJECT (tp_factory),
									 NULL);
	tp_cli_connection_interface_presence_connect_to_presence_update (priv->connection,
									 tp_contact_factory_presence_update_cb,
									 NULL, NULL,
									 G_OBJECT (tp_factory),
									 NULL);
	tp_cli_connection_interface_capabilities_connect_to_capabilities_changed (priv->connection,
										  tp_contact_factory_capabilities_changed_cb,
										  NULL, NULL,
										  G_OBJECT (tp_factory),
										  NULL);

	/* Request needed info for all existing contacts */
	handle_needed = g_array_new (TRUE, FALSE, sizeof (gchar*));
	id_needed = g_array_new (FALSE, FALSE, sizeof (guint));
	for (l = priv->contacts; l; l = l->next) {
		EmpathyContact *contact;
		guint           handle;
		const gchar    *id;

		contact = l->data;
		handle = empathy_contact_get_handle (contact);
		id = empathy_contact_get_id (contact);
		if (handle == 0) {
			g_assert (!EMP_STR_EMPTY (id));
			g_array_append_val (handle_needed, id);
			handle_needed_contacts = g_list_prepend (handle_needed_contacts,
								 g_object_ref (contact));
		}
		if (EMP_STR_EMPTY (id)) {
			g_array_append_val (id_needed, handle);
			id_needed_contacts = g_list_prepend (id_needed_contacts,
							     g_object_ref (contact));
		}
	}
	handle_needed_contacts = g_list_reverse (handle_needed_contacts);
	id_needed_contacts = g_list_reverse (id_needed_contacts);

	tp_cli_connection_call_request_handles (priv->connection,
						-1,
						TP_HANDLE_TYPE_CONTACT,
						(const gchar**) handle_needed->data,
						tp_contact_factory_request_handles_cb,
						handle_needed_contacts, tp_contact_factory_list_free,
						G_OBJECT (tp_factory));

	tp_cli_connection_call_inspect_handles (priv->connection,
						-1,
						TP_HANDLE_TYPE_CONTACT,
						id_needed,
						tp_contact_factory_inspect_handles_cb,
						id_needed_contacts, tp_contact_factory_list_free,
						G_OBJECT (tp_factory));

	tp_contact_factory_request_everything ((EmpathyTpContactFactory*) tp_factory,
					       id_needed);

	g_array_free (handle_needed, TRUE);
	g_array_free (id_needed, TRUE);
}

static void
get_requestable_channel_classes_cb (TpProxy *connection,
				    const GValue *value,
				    const GError *error,
				    gpointer user_data,
				    GObject *weak_object)
{
	EmpathyTpContactFactory     *self = EMPATHY_TP_CONTACT_FACTORY (weak_object);
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (self);
	GPtrArray                   *classes;
	guint                        i;

	if (error != NULL) {
		DEBUG ("Error: %s", error->message);
		tp_contact_factory_ready (self);
		return;
	}

	classes = g_value_get_boxed (value);
	for (i = 0; i < classes->len; i++) {
		GValue class = {0,};
		GValue *chan_type, *handle_type;
		GHashTable *fixed_prop;
		GList *l;

		g_value_init (&class, TP_STRUCT_TYPE_REQUESTABLE_CHANNEL_CLASS);
		g_value_set_static_boxed (&class, g_ptr_array_index (classes, i));

		dbus_g_type_struct_get (&class,
					0, &fixed_prop,
					G_MAXUINT);

		chan_type = g_hash_table_lookup (fixed_prop,
			TP_IFACE_CHANNEL ".ChannelType");
		if (chan_type == NULL ||
		    tp_strdiff (g_value_get_string (chan_type),
		    		TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER)) {
			continue;
		}

		handle_type = g_hash_table_lookup (fixed_prop,
			TP_IFACE_CHANNEL ".TargetHandleType");
		if (handle_type == NULL ||
		    g_value_get_uint (handle_type) != TP_HANDLE_TYPE_CONTACT) {
			continue;
		}

		/* We can request file transfer channel to contacts. */
		priv->can_request_ft = TRUE;

		/* Update the capabilities of all contacts */
		for (l = priv->contacts; l != NULL; l = g_list_next (l)) {
			EmpathyContact *contact = l->data;
			EmpathyCapabilities caps;

			caps = empathy_contact_get_capabilities (contact);
			empathy_contact_set_capabilities (contact, caps |
				EMPATHY_CAPABILITIES_FT);
		}
		break;
	}

	tp_contact_factory_ready (self);
}

static void
tp_contact_factory_got_avatar_requirements_cb (TpConnection *proxy,
					       const gchar **mime_types,
					       guint         min_width,
					       guint         min_height,
					       guint         max_width,
					       guint         max_height,
					       guint         max_size,
					       const GError *error,
					       gpointer      user_data,
					       GObject      *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	if (error) {
		DEBUG ("Failed to get avatar requirements: %s", error->message);
		/* We'll just leave avatar_mime_types as NULL; the
		 * avatar-setting code can use this as a signal that you can't
		 * set avatars.
		 */
	} else {
		priv->avatar_mime_types = g_strdupv ((gchar **) mime_types);
		priv->avatar_min_width = min_width;
		priv->avatar_min_height = min_height;
		priv->avatar_max_width = max_width;
		priv->avatar_max_height = max_height;
		priv->avatar_max_size = max_size;
	}

	/* Can we request file transfer channels? */
	tp_cli_dbus_properties_call_get (priv->connection, -1,
		TP_IFACE_CONNECTION_INTERFACE_REQUESTS,
		"RequestableChannelClasses",
		get_requestable_channel_classes_cb, NULL, NULL,
		G_OBJECT (tp_factory));
}

static void
tp_contact_factory_got_self_handle_cb (TpConnection *proxy,
				       guint         handle,
				       const GError *error,
				       gpointer      user_data,
				       GObject      *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	if (error) {
		DEBUG ("Failed to get self handles: %s", error->message);
		return;
	}

	empathy_contact_set_handle (priv->user, handle);

	/* Get avatar requirements for this connection */
	tp_cli_connection_interface_avatars_call_get_avatar_requirements (
		priv->connection,
		-1,
		tp_contact_factory_got_avatar_requirements_cb,
		NULL, NULL,
		tp_factory);
}

static void
tp_contact_factory_connection_ready_cb (EmpathyTpContactFactory *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	/* Get our own handle */
	tp_cli_connection_call_get_self_handle (priv->connection,
						-1,
						tp_contact_factory_got_self_handle_cb,
						NULL, NULL,
						G_OBJECT (tp_factory));
}

static void
tp_contact_factory_status_updated (EmpathyTpContactFactory *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	gboolean                     connection_ready;
	MissionControl              *mc;

	if (priv->connection) {
		/* We already have our connection object */
		return;
	}

	mc = empathy_mission_control_dup_singleton ();
	priv->connection = mission_control_get_tpconnection (mc, priv->account, NULL);
	if (!priv->connection) {
		return;
	}

	/* We got a new connection, wait for it to be ready */
	g_signal_connect_swapped (priv->connection, "invalidated",
				  G_CALLBACK (tp_contact_factory_connection_invalidated_cb),
				  tp_factory);

	g_object_get (priv->connection, "connection-ready", &connection_ready, NULL);
	if (connection_ready) {
		tp_contact_factory_connection_ready_cb (tp_factory);
	} else {
		g_signal_connect_swapped (priv->connection, "notify::connection-ready",
					  G_CALLBACK (tp_contact_factory_connection_ready_cb),
					  tp_factory);
	}

	g_object_unref (mc);
}

static void
tp_contact_factory_account_connection_cb (EmpathyAccountManager *account_manager,
					  McAccount *account,
					  TpConnectionStatusReason reason,
					  TpConnectionStatus current,
					  TpConnectionStatus previous,
					  EmpathyTpContactFactory  *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	if (account && empathy_account_equal (account, priv->account)) {
		tp_contact_factory_status_updated (tp_factory);
	}
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

	DEBUG ("Contact added: %s (%d)",
		empathy_contact_get_id (contact),
		empathy_contact_get_handle (contact));
}

static void
tp_contact_factory_hold_handles_cb (TpConnection *connection,
				    const GError *error,
				    gpointer      userdata,
				    GObject      *tp_factory)
{
	if (error) {
		DEBUG ("Failed to hold handles: %s", error->message);
	}
}

EmpathyContact *
empathy_tp_contact_factory_get_user (EmpathyTpContactFactory *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_FACTORY (tp_factory), NULL);

	return g_object_ref (priv->user);
}

static void
contact_created (EmpathyTpContactFactory *self,
                 EmpathyContact *contact)
{
  EmpathyTpContactFactoryPriv *priv = GET_PRIV (self);

  if (priv->can_request_ft)
    {
      /* Set the FT capability */
      /* FIXME: We should use the futur ContactCapabilities interface */
      EmpathyCapabilities caps;

      caps = empathy_contact_get_capabilities (contact);
      caps |= EMPATHY_CAPABILITIES_FT;

      empathy_contact_set_capabilities (contact, caps);
    }

  tp_contact_factory_add_contact (self, contact);
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
	contact_created (tp_factory, contact);

	if (priv->ready) {
		const gchar *contact_ids[] = {id, NULL};
		GList       *contacts;
		
		contacts = g_list_prepend (NULL, g_object_ref (contact));
		tp_cli_connection_call_request_handles (priv->connection,
							-1,
							TP_HANDLE_TYPE_CONTACT,
							contact_ids,
							tp_contact_factory_request_handles_cb,
							contacts, tp_contact_factory_list_free,
							G_OBJECT (tp_factory));
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
					     const GArray            *handles)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	GList                       *contacts = NULL;
	GArray                      *new_handles;
	GList                       *new_contacts = NULL;
	guint                        i;

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

	/* Create new contacts */
	for (i = 0; i < new_handles->len; i++) {
		EmpathyContact *contact;
		guint           handle;

		handle = g_array_index (new_handles, guint, i);

		contact = g_object_new (EMPATHY_TYPE_CONTACT,
					"account", priv->account,
					"handle", handle,
					NULL);
		contact_created (tp_factory, contact);
		contacts = g_list_prepend (contacts, contact);
		new_contacts = g_list_prepend (new_contacts, g_object_ref (contact));
	}
	new_contacts = g_list_reverse (new_contacts);

	if (priv->ready) {
		/* Get the IDs of all new handles */
		tp_cli_connection_call_inspect_handles (priv->connection,
							-1,
							TP_HANDLE_TYPE_CONTACT,
							new_handles,
							tp_contact_factory_inspect_handles_cb,
							new_contacts, tp_contact_factory_list_free,
							G_OBJECT (tp_factory));

		/* Hold all new handles. */
		/* FIXME: Should be unholded when removed from the factory */
		tp_cli_connection_call_hold_handles (priv->connection,
						     -1,
						     TP_HANDLE_TYPE_CONTACT,
						     new_handles,
						     tp_contact_factory_hold_handles_cb,
						     NULL, NULL,
						     G_OBJECT (tp_factory));

		tp_contact_factory_request_everything (tp_factory, new_handles);
	}

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
	g_return_if_fail (priv->ready);
	g_return_if_fail (empathy_account_equal (empathy_contact_get_account (contact),
						 priv->account));

	handle = empathy_contact_get_handle (contact);

	DEBUG ("Setting alias for contact %s (%d) to %s",
		empathy_contact_get_id (contact),
		handle, alias);

	new_alias = g_hash_table_new_full (g_direct_hash,
					   g_direct_equal,
					   NULL,
					   g_free);

	g_hash_table_insert (new_alias,
			     GUINT_TO_POINTER (handle),
			     g_strdup (alias));

	tp_cli_connection_interface_aliasing_call_set_aliases (priv->connection,
							       -1,
							       new_alias,
							       tp_contact_factory_set_aliases_cb,
							       NULL, NULL,
							       G_OBJECT (tp_factory));

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
	g_return_if_fail (priv->ready);

	if (data && size > 0 && size < G_MAXUINT) {
		GArray avatar;

		avatar.data = (gchar*) data;
		avatar.len = size;

		DEBUG ("Setting avatar on account %s",
			mc_account_get_unique_name (priv->account));

		tp_cli_connection_interface_avatars_call_set_avatar (priv->connection,
								     -1,
								     &avatar,
								     mime_type,
								     tp_contact_factory_set_avatar_cb,
								     NULL, NULL,
								     G_OBJECT (tp_factory));
	} else {
		DEBUG ("Clearing avatar on account %s",
			mc_account_get_unique_name (priv->account));

		tp_cli_connection_interface_avatars_call_clear_avatar (priv->connection,
								       -1,
								       tp_contact_factory_clear_avatar_cb,
								       NULL, NULL,
								       G_OBJECT (tp_factory));
	}
}

gboolean
empathy_tp_contact_factory_is_ready (EmpathyTpContactFactory *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_FACTORY (tp_factory), FALSE);

	return priv->ready;
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
	case PROP_READY:
		g_value_set_boolean (value, priv->ready);
		break;
	case PROP_MIME_TYPES:
		g_value_set_boxed (value, priv->avatar_mime_types);
		break;
	case PROP_MIN_WIDTH:
		g_value_set_uint (value, priv->avatar_min_width);
		break;
	case PROP_MIN_HEIGHT:
		g_value_set_uint (value, priv->avatar_min_height);
		break;
	case PROP_MAX_WIDTH:
		g_value_set_uint (value, priv->avatar_max_width);
		break;
	case PROP_MAX_HEIGHT:
		g_value_set_uint (value, priv->avatar_max_height);
		break;
	case PROP_MAX_SIZE:
		g_value_set_uint (value, priv->avatar_max_size);
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

	DEBUG ("Finalized: %p (%s)", object,
		mc_account_get_normalized_name (priv->account));

	g_signal_handlers_disconnect_by_func (priv->account_manager,
					      tp_contact_factory_account_connection_cb,
					      object);

	for (l = priv->contacts; l; l = l->next) {
		g_object_weak_unref (G_OBJECT (l->data),
				     tp_contact_factory_weak_notify,
				     object);
	}

	g_list_free (priv->contacts);
	g_object_unref (priv->account_manager);
	g_object_unref (priv->account);
	g_object_unref (priv->user);

	if (priv->connection) {
		g_signal_handlers_disconnect_by_func (priv->connection,
						      tp_contact_factory_connection_invalidated_cb,
						      object);
		g_object_unref (priv->connection);
	}

	g_strfreev (priv->avatar_mime_types);

	G_OBJECT_CLASS (empathy_tp_contact_factory_parent_class)->finalize (object);
}

static GObject *
tp_contact_factory_constructor (GType                  type,
				guint                  n_props,
				GObjectConstructParam *props)
{
	GObject *tp_factory;
	EmpathyTpContactFactoryPriv *priv;

	tp_factory = G_OBJECT_CLASS (empathy_tp_contact_factory_parent_class)->constructor (type, n_props, props);
	priv = GET_PRIV (tp_factory);

	priv->ready = FALSE;
	priv->user = empathy_contact_new (priv->account);
	empathy_contact_set_is_user (priv->user, TRUE);
	tp_contact_factory_add_contact ((EmpathyTpContactFactory*) tp_factory, priv->user);
	tp_contact_factory_status_updated (EMPATHY_TP_CONTACT_FACTORY (tp_factory));

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

	g_object_class_install_property (object_class,
					 PROP_ACCOUNT,
					 g_param_spec_object ("account",
							      "Factory's Account",
							      "The account associated with the factory",
							      MC_TYPE_ACCOUNT,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
					 PROP_READY,
					 g_param_spec_boolean ("ready",
							       "Whether the factory is ready",
							       "TRUE once the factory is ready to be used",
							       FALSE,
							       G_PARAM_READABLE |
							       G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
					 PROP_MIME_TYPES,
					 g_param_spec_boxed ("avatar-mime-types",
							     "Supported MIME types for avatars",
							     "Types of images that may be set as "
							     "avatars on this connection.  Only valid "
							     "once 'ready' becomes TRUE.",
							     G_TYPE_STRV,
							     G_PARAM_READABLE |
							     G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
					 PROP_MIN_WIDTH,
					 g_param_spec_uint ("avatar-min-width",
							    "Minimum width for avatars",
							    "Minimum width of avatar that may be set. "
							    "Only valid once 'ready' becomes TRUE.",
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READABLE |
							    G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
					 PROP_MIN_HEIGHT,
					 g_param_spec_uint ("avatar-min-height",
							    "Minimum height for avatars",
							    "Minimum height of avatar that may be set. "
							    "Only valid once 'ready' becomes TRUE.",
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READABLE |
							    G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
					 PROP_MAX_WIDTH,
					 g_param_spec_uint ("avatar-max-width",
							    "Maximum width for avatars",
							    "Maximum width of avatar that may be set "
							    "or 0 if there is no maximum. "
							    "Only valid once 'ready' becomes TRUE.",
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READABLE |
							    G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
					 PROP_MAX_HEIGHT,
					 g_param_spec_uint ("avatar-max-height",
							    "Maximum height for avatars",
							    "Maximum height of avatar that may be set "
							    "or 0 if there is no maximum. "
							    "Only valid once 'ready' becomes TRUE.",
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READABLE |
							    G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
					 PROP_MAX_SIZE,
					 g_param_spec_uint ("avatar-max-size",
							    "Maximum size for avatars in bytes",
							    "Maximum file size of avatar that may be "
							    "set or 0 if there is no maximum. "
							    "Only valid once 'ready' becomes TRUE.",
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READABLE |
							    G_PARAM_STATIC_STRINGS));


	g_type_class_add_private (object_class, sizeof (EmpathyTpContactFactoryPriv));
}

static void
empathy_tp_contact_factory_init (EmpathyTpContactFactory *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (tp_factory,
		EMPATHY_TYPE_TP_CONTACT_FACTORY, EmpathyTpContactFactoryPriv);

	tp_factory->priv = priv;
	priv->account_manager = empathy_account_manager_dup_singleton ();

	g_signal_connect (priv->account_manager, "account-connection-changed",
			  G_CALLBACK (tp_contact_factory_account_connection_cb),
			  tp_factory);

	priv->can_request_ft = FALSE;
}

EmpathyTpContactFactory *
empathy_tp_contact_factory_new (McAccount *account)
{
	return g_object_new (EMPATHY_TYPE_TP_CONTACT_FACTORY,
			     "account", account,
			     NULL);
}

