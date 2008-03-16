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
	TpConnection   *connection;
	gboolean        ready;

	GList          *contacts;
	EmpathyContact *user;
};

static void empathy_tp_contact_factory_class_init (EmpathyTpContactFactoryClass *klass);
static void empathy_tp_contact_factory_init       (EmpathyTpContactFactory      *factory);

G_DEFINE_TYPE (EmpathyTpContactFactory, empathy_tp_contact_factory, G_TYPE_OBJECT);

enum {
	PROP_0,
	PROP_ACCOUNT,
	PROP_READY
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
tp_contact_factory_get_presence_cb (TpConnection *connection,
				    GHashTable   *handle_table,
				    const GError *error,
				    gpointer      user_data,
				    GObject      *tp_factory)
{
	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error getting presence: %s",
			      error->message);
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
		empathy_debug (DEBUG_DOMAIN, "Error setting alias: %s",
			       error->message);
	}
}

static void
tp_contact_factory_request_aliases_cb (TpConnection *connection,
				       const gchar  **contact_names,
				       const GError  *error,
				       gpointer       user_data,
				       GObject       *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	guint                       *handles = user_data;
	guint                        i = 0;
	const gchar                **name;

	if (error) {
		GArray handles_array;
		guint  size = 0;

		empathy_debug (DEBUG_DOMAIN, "Error requesting aliases: %s",
			      error->message);

		/* FIXME: Sometimes the dbus call timesout because CM takes
		 * too much time to request all aliases from the server,
		 * that's why we retry. */
		while (handles[size] != 0) {
			size++;
		}
		handles = g_memdup (handles, size * sizeof (guint));
		handles_array.len = size;
		handles_array.data = (gchar*) handles;
		
		tp_cli_connection_interface_aliasing_call_request_aliases (priv->connection,
									   -1,
									   &handles_array,
									   tp_contact_factory_request_aliases_cb,
									   handles, g_free,
									   G_OBJECT (tp_factory));

		return;
	}

	for (name = contact_names; *name; name++) {
		EmpathyContact *contact;

		contact = tp_contact_factory_find_by_handle (EMPATHY_TP_CONTACT_FACTORY (tp_factory),
							     handles[i]);
		if (!contact) {
			continue;
		}

		empathy_debug (DEBUG_DOMAIN, "Renaming contact %s (%d) to %s (request cb)",
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

		empathy_debug (DEBUG_DOMAIN, "Renaming contact %s (%d) to %s (changed cb)",
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
		empathy_debug (DEBUG_DOMAIN, "Error setting avatar: %s",
			       error->message);
	}
}

static void
tp_contact_factory_clear_avatar_cb (TpConnection *connection,
				    const GError *error,
				    gpointer      user_data,
				    GObject      *tp_factory)
{
	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error clearing avatar: %s",
			       error->message);
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
	EmpathyAvatar  *avatar;

	contact = tp_contact_factory_find_by_handle (EMPATHY_TP_CONTACT_FACTORY (tp_factory),
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
tp_contact_factory_request_avatars_cb (TpConnection *connection,
				       const GError *error,
				       gpointer      user_data,
				       GObject      *tp_factory)
{
	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error requesting avatars: %s",
			       error->message);
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
tp_contact_factory_get_known_avatar_tokens_cb (TpConnection *connection,
					       GHashTable   *tokens,
					       const GError *error,
					       gpointer      user_data,
					       GObject      *tp_factory)
{
	TokensData data;

	if (error) {
		empathy_debug (DEBUG_DOMAIN,
			       "Error getting known avatars tokens: %s",
			       error->message);
		return;
	}

	data.tp_factory = EMPATHY_TP_CONTACT_FACTORY (tp_factory);
	data.handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_hash_table_foreach (tokens,
			      tp_contact_factory_avatar_tokens_foreach,
			      &data);

	empathy_debug (DEBUG_DOMAIN, "Got %d tokens, need to request %d avatars",
		       g_hash_table_size (tokens),
		       data.handles->len);

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

	empathy_debug (DEBUG_DOMAIN, "Need to request avatar for token %s",
		       new_token);

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

	empathy_debug (DEBUG_DOMAIN, "Changing capabilities for contact %s (%d) to %d",
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
		empathy_debug (DEBUG_DOMAIN, "Error getting capabilities: %s",
			       error->message);
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

	if (!priv->ready) {
		return;
	}

	dup_handles = g_malloc0 ((handles->len + 1) * sizeof (guint));
	g_memmove (dup_handles, handles->data, handles->len * sizeof (guint));
	tp_cli_connection_interface_presence_call_get_presence (priv->connection,
								-1,
								handles,
								tp_contact_factory_get_presence_cb,
								dup_handles, g_free,
								G_OBJECT (tp_factory));

	dup_handles = g_new (guint, handles->len + 1);
	g_memmove (dup_handles, handles->data, handles->len * sizeof (guint));
	dup_handles[handles->len] = 0;
	tp_cli_connection_interface_aliasing_call_request_aliases (priv->connection,
								   -1,
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
		empathy_debug (DEBUG_DOMAIN, "Failed to request handles: %s",
			       error->message);
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
		empathy_debug (DEBUG_DOMAIN, "Failed to inspect handles: %s",
			       error->message);
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

	empathy_debug (DEBUG_DOMAIN, "Connection invalidated");

	g_object_unref (priv->connection);
	priv->connection = NULL;
	priv->ready = FALSE;
	g_object_notify (G_OBJECT (tp_factory), "ready");


	g_list_foreach (priv->contacts,
			tp_contact_factory_disconnect_contact_foreach,
			tp_factory);
}


static void
tp_contact_factory_got_self_handle_cb (TpConnection *proxy,
				       guint         handle,
				       const GError *error,
				       gpointer      user_data,
				       GObject      *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);
	GList                       *l;
	GArray                      *handle_needed;
	GArray                      *id_needed;
	GList                       *handle_needed_contacts = NULL;
	GList                       *id_needed_contacts = NULL;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Failed to get self handles: %s",
			       error->message);
		return;
	}

	empathy_debug (DEBUG_DOMAIN, "Connection ready");

	empathy_contact_set_handle (priv->user, handle);
	priv->ready = TRUE;
	g_object_notify (tp_factory, "ready");

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
			g_assert (!G_STR_EMPTY (id));
			g_array_append_val (handle_needed, id);
			handle_needed_contacts = g_list_prepend (handle_needed_contacts,
								 g_object_ref (contact));
		}
		if (G_STR_EMPTY (id)) {
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

	tp_contact_factory_request_everything ((EmpathyTpContactFactory*) tp_factory,
					       id_needed);
	tp_cli_connection_call_inspect_handles (priv->connection,
						-1,
						TP_HANDLE_TYPE_CONTACT,
						id_needed,
						tp_contact_factory_inspect_handles_cb,
						id_needed_contacts, tp_contact_factory_list_free,
						G_OBJECT (tp_factory));

	g_array_free (handle_needed, TRUE);
	g_array_free (id_needed, TRUE);
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
	TpConn                      *tp_conn;
	gboolean                     connection_ready;

	if (priv->connection) {
		/* We already have our connection object */
		return;
	}

	tp_conn = mission_control_get_connection (priv->mc, priv->account, NULL);
	if (!tp_conn) {
		return;
	}

	/* We got a new connection, wait for it to be ready */
	priv->connection = tp_conn_dup_connection (tp_conn);
	g_object_unref (tp_conn);

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
		tp_contact_factory_status_updated (tp_factory);
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

	empathy_debug (DEBUG_DOMAIN, "Contact added: %s (%d)",
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
		empathy_debug (DEBUG_DOMAIN, "Failed to hold handles: %s",
			       error->message);
	}
}

EmpathyContact *
empathy_tp_contact_factory_get_user (EmpathyTpContactFactory *tp_factory)
{
	EmpathyTpContactFactoryPriv *priv = GET_PRIV (tp_factory);

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_FACTORY (tp_factory), NULL);

	return g_object_ref (priv->user);
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
		tp_contact_factory_add_contact (tp_factory, contact);
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

	if (!priv->ready) {
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

	if (!priv->ready) {
		return;
	}

	if (data && size > 0 && size < G_MAXUINT) {
		GArray avatar;

		avatar.data = (gchar*) data;
		avatar.len = size;

		empathy_debug (DEBUG_DOMAIN, "Setting avatar on account %s",
			       mc_account_get_unique_name (priv->account));

		tp_cli_connection_interface_avatars_call_set_avatar (priv->connection,
								     -1,
								     &avatar,
								     mime_type,
								     tp_contact_factory_set_avatar_cb,
								     NULL, NULL,
								     G_OBJECT (tp_factory));
	} else {
		empathy_debug (DEBUG_DOMAIN, "Clearing avatar on account %s",
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
	g_object_unref (priv->user);

	if (priv->connection) {
		g_signal_handlers_disconnect_by_func (priv->connection,
						      tp_contact_factory_connection_invalidated_cb,
						      object);
		g_object_unref (priv->connection);
	}

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
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_READY,
					 g_param_spec_boolean ("ready",
							       "Wheter the factor is ready",
							       "Is the factory ready",
							       FALSE,
							       G_PARAM_READABLE));

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

