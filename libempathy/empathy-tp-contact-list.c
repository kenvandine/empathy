/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Xavier Claessens <xclaesse@gmail.com>
 * Copyright (C) 2007-2009 Collabora Ltd.
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
#include <glib/gi18n-lib.h>

#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/dbus.h>

#include "empathy-tp-contact-list.h"
#include "empathy-tp-contact-factory.h"
#include "empathy-contact-list.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_TP | EMPATHY_DEBUG_CONTACT
#include "empathy-debug.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyTpContactList)
typedef struct {
	EmpathyTpContactFactory *factory;
	TpConnection   *connection;
	const gchar    *protocol_group;

	TpChannel      *publish;
	TpChannel      *subscribe;
	GHashTable     *members;
	GHashTable     *pendings;
	GHashTable     *groups;
} EmpathyTpContactListPriv;

typedef enum {
	TP_CONTACT_LIST_TYPE_PUBLISH,
	TP_CONTACT_LIST_TYPE_SUBSCRIBE,
	TP_CONTACT_LIST_TYPE_UNKNOWN
} TpContactListType;

static void tp_contact_list_iface_init         (EmpathyContactListIface   *iface);

enum {
	PROP_0,
	PROP_CONNECTION,
};

G_DEFINE_TYPE_WITH_CODE (EmpathyTpContactList, empathy_tp_contact_list, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (EMPATHY_TYPE_CONTACT_LIST,
						tp_contact_list_iface_init));

static void
tp_contact_list_group_invalidated_cb (TpChannel *channel,
				      guint      domain,
				      gint       code,
				      gchar     *message,
				      EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	const TpIntSet *members;
	TpIntSetIter iter;
	const gchar *group_name;

	group_name = tp_channel_get_identifier (channel);
	DEBUG ("Group %s invalidated. Message: %s", group_name, message);

	/* Signal that all members are not in that group anymore */
	members = tp_channel_group_get_members (channel);
	tp_intset_iter_init (&iter, members);
	while (tp_intset_iter_next (&iter)) {
		EmpathyContact *contact;

		contact = g_hash_table_lookup (priv->members,
					       GUINT_TO_POINTER (iter.element));
		if (contact == NULL) {
			continue;
		}

		DEBUG ("Contact %s (%d) removed from group %s",
			empathy_contact_get_id (contact), iter.element,
			group_name);
		g_signal_emit_by_name (list, "groups-changed", contact,
				       group_name,
				       FALSE);
	}

	g_hash_table_remove (priv->groups, group_name);
}

static void
tp_contact_list_group_ready_cb (TpChannel *channel,
				const GError *error,
				gpointer list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);

	if (error) {
		DEBUG ("Error: %s", error->message);
		g_object_unref (channel);
		return;
	}
	
	DEBUG ("Add group %s", tp_channel_get_identifier (channel));
	g_hash_table_insert (priv->groups,
			     (gpointer) tp_channel_get_identifier (channel),
			     channel);

	g_signal_connect (channel, "invalidated",
			  G_CALLBACK (tp_contact_list_group_invalidated_cb),
			  list);
}

static void
tp_contact_list_group_members_changed_cb (TpChannel     *channel,
					  gchar         *message,
					  GArray        *added,
					  GArray        *removed,
					  GArray        *local_pending,
					  GArray        *remote_pending,
					  guint          actor,
					  guint          reason,
					  EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv  *priv = GET_PRIV (list);
	const gchar *group_name;
	gint i;

	group_name = tp_channel_get_identifier (channel);

	for (i = 0; i < added->len; i++) {
		EmpathyContact *contact;
		TpHandle handle;

		handle = g_array_index (added, TpHandle, i);
		contact = g_hash_table_lookup (priv->members,
					       GUINT_TO_POINTER (handle));
		if (contact == NULL) {
			continue;
		}

		DEBUG ("Contact %s (%d) added to group %s",
			empathy_contact_get_id (contact), handle, group_name);
		g_signal_emit_by_name (list, "groups-changed", contact,
				       group_name,
				       TRUE);
	}	

	for (i = 0; i < removed->len; i++) {
		EmpathyContact *contact;
		TpHandle handle;

		handle = g_array_index (removed, TpHandle, i);
		contact = g_hash_table_lookup (priv->members,
					       GUINT_TO_POINTER (handle));
		if (contact == NULL) {
			continue;
		}

		DEBUG ("Contact %s (%d) removed from group %s",
			empathy_contact_get_id (contact), handle, group_name);

		g_signal_emit_by_name (list, "groups-changed", contact,
				       group_name,
				       FALSE);
	}	
}

static TpChannel *
tp_contact_list_group_add_channel (EmpathyTpContactList *list,
				   const gchar          *object_path,
				   const gchar          *channel_type,
				   TpHandleType          handle_type,
				   guint                 handle)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	TpChannel                *channel;

	/* Only accept server-side contact groups */
	if (tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_CONTACT_LIST) ||
	    handle_type != TP_HANDLE_TYPE_GROUP) {
		return NULL;
	}

	channel = tp_channel_new (priv->connection,
				  object_path, channel_type,
				  handle_type, handle, NULL);

	/* TpChannel emits initial set of members just before being ready */
	g_signal_connect (channel, "group-members-changed",
			  G_CALLBACK (tp_contact_list_group_members_changed_cb),
			  list);

	/* Give the ref to the callback */
	tp_channel_call_when_ready (channel,
				    tp_contact_list_group_ready_cb,
				    list);

	return channel;
}

typedef struct {
	GArray *handles;
	TpHandle channel_handle;
	guint ref_count;
} GroupAddData;

static void
tp_contact_list_group_add_data_unref (gpointer user_data)
{
	GroupAddData *data = user_data;

	if (--data->ref_count == 0) {
		g_array_free (data->handles, TRUE);
		g_slice_free (GroupAddData, data);
	}
}

static void
tp_contact_list_group_add_ready_cb (TpChannel    *channel,
				    const GError *error,
				    gpointer      user_data)
{
	GroupAddData *data = user_data;

	if (error) {
		tp_contact_list_group_add_data_unref (data);
		return;
	}

	tp_cli_channel_interface_group_call_add_members (channel, -1,
		data->handles, NULL, NULL, NULL, NULL, NULL);
	tp_contact_list_group_add_data_unref (data);
}

static void
tp_contact_list_group_request_channel_cb (TpConnection *connection,
					  const gchar  *object_path,
					  const GError *error,
					  gpointer      user_data,
					  GObject      *list)
{
	GroupAddData *data = user_data;
	TpChannel *channel;

	if (error) {
		DEBUG ("Error: %s", error->message);
		return;
	}

	channel = tp_contact_list_group_add_channel (EMPATHY_TP_CONTACT_LIST (list),
						     object_path,
						     TP_IFACE_CHANNEL_TYPE_CONTACT_LIST,
						     TP_HANDLE_TYPE_GROUP,
						     data->channel_handle);

	data->ref_count++;
	tp_channel_call_when_ready (channel,
				    tp_contact_list_group_add_ready_cb,
				    data);
}

static void
tp_contact_list_group_request_handles_cb (TpConnection *connection,
					  const GArray *handles,
					  const GError *error,
					  gpointer      user_data,
					  GObject      *list)
{
	GroupAddData *data = user_data;

	if (error) {
		DEBUG ("Error: %s", error->message);
		return;
	}

	data->channel_handle = g_array_index (handles, TpHandle, 1);
	data->ref_count++;
	tp_cli_connection_call_request_channel (connection, -1,
						TP_IFACE_CHANNEL_TYPE_CONTACT_LIST,
						TP_HANDLE_TYPE_GROUP,
						data->channel_handle,
						TRUE,
						tp_contact_list_group_request_channel_cb,
						data, tp_contact_list_group_add_data_unref,
						list);
}

static void
tp_contact_list_group_add (EmpathyTpContactList *list,
			   const gchar          *group_name,
			   GArray               *handles)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	TpChannel                *channel;
	const gchar              *names[] = {group_name, NULL};
	GroupAddData             *data;

	channel = g_hash_table_lookup (priv->groups, group_name);
	if (channel) {
		tp_cli_channel_interface_group_call_add_members (channel, -1,
			handles, NULL, NULL, NULL, NULL, NULL);
		g_array_free (handles, TRUE);
		return;
	}

	data = g_slice_new0 (GroupAddData);
	data->handles = handles;
	data->ref_count = 1;
	tp_cli_connection_call_request_handles (priv->connection, -1,
						TP_HANDLE_TYPE_GROUP, names,
						tp_contact_list_group_request_handles_cb,
						data, tp_contact_list_group_add_data_unref,
						G_OBJECT (list));
}

static void
tp_contact_list_got_added_members_cb (EmpathyTpContactFactory *factory,
				      GList                   *contacts,
				      gpointer                 user_data,
				      GObject                 *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	GList *l;

	for (l = contacts; l; l = l->next) {
		EmpathyContact *contact = l->data;
		TpHandle handle;

		handle = empathy_contact_get_handle (contact);
		if (g_hash_table_lookup (priv->members, GUINT_TO_POINTER (handle)))
			continue;

		/* Add to the list and emit signal */
		g_hash_table_insert (priv->members, GUINT_TO_POINTER (handle),
				     g_object_ref (contact));
		g_signal_emit_by_name (list, "members-changed", contact,
				       0, 0, NULL, TRUE);

		/* This contact is now member, implicitly accept pending. */
		if (g_hash_table_lookup (priv->pendings, GUINT_TO_POINTER (handle))) {
			GArray handles = {(gchar*) &handle, 1};

			tp_cli_channel_interface_group_call_add_members (priv->publish,
				-1, &handles, NULL, NULL, NULL, NULL, NULL);
		}
	}
}

static void
tp_contact_list_got_local_pending_cb (EmpathyTpContactFactory *factory,
				      GList                   *contacts,
				      gpointer                 info,
				      GObject                 *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	GList *l;

	for (l = contacts; l; l = l->next) {
		EmpathyContact *contact = l->data;
		TpHandle handle;
		const gchar *message;
		TpChannelGroupChangeReason reason;

		handle = empathy_contact_get_handle (contact);
		if (g_hash_table_lookup (priv->members, GUINT_TO_POINTER (handle))) {
			GArray handles = {(gchar*) &handle, 1};

			/* This contact is already member, auto accept. */
			tp_cli_channel_interface_group_call_add_members (priv->publish,
				-1, &handles, NULL, NULL, NULL, NULL, NULL);
		}
		else if (tp_channel_group_get_local_pending_info (priv->publish,
								  handle,
								  NULL,
								  &reason,
								  &message)) {
			/* Add contact to pendings */
			g_hash_table_insert (priv->pendings, GUINT_TO_POINTER (handle),
					     g_object_ref (contact));
			g_signal_emit_by_name (list, "pendings-changed", contact,
					       contact, reason, message, TRUE);
		}
	}
}

static void
tp_contact_list_remove_handle (EmpathyTpContactList *list,
			       GHashTable *table,
			       TpHandle handle)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	EmpathyContact *contact;
	const gchar *signal;

	if (table == priv->pendings)
		signal = "pendings-changed";
	else if (table == priv->members)
		signal = "members-changed";
	else
		return;

	contact = g_hash_table_lookup (table, GUINT_TO_POINTER (handle));
	if (contact) {
		g_object_ref (contact);
		g_hash_table_remove (table, GUINT_TO_POINTER (handle));
		g_signal_emit_by_name (list, signal, contact, 0, 0, NULL,
				       FALSE);
		g_object_unref (contact);
	}
}

static void
tp_contact_list_publish_group_members_changed_cb (TpChannel     *channel,
						  gchar         *message,
						  GArray        *added,
						  GArray        *removed,
						  GArray        *local_pending,
						  GArray        *remote_pending,
						  TpHandle       actor,
						  TpChannelGroupChangeReason reason,
						  EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	guint i;

	/* We now send our presence to those contacts, remove them from pendings */
	for (i = 0; i < added->len; i++) {
		tp_contact_list_remove_handle (list, priv->pendings,
			g_array_index (added, TpHandle, i));
	}

	/* We refuse to send our presence to those contacts, remove from pendings */
	for (i = 0; i < removed->len; i++) {
		tp_contact_list_remove_handle (list, priv->pendings,
			g_array_index (added, TpHandle, i));
	}

	/* Those contacts want our presence, auto accept those that are already
	 * member, otherwise add in pendings. */
	if (local_pending->len > 0) {
		empathy_tp_contact_factory_get_from_handles (priv->factory,
			local_pending->len, (TpHandle*) local_pending->data,
			tp_contact_list_got_local_pending_cb, NULL, NULL,
			G_OBJECT (list));
	}
}

static void
tp_contact_list_publish_request_channel_cb (TpConnection *connection,
					    const gchar  *object_path,
					    const GError *error,
					    gpointer      user_data,
					    GObject      *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);

	if (error) {
		DEBUG ("Error: %s", error->message);
		return;
	}

	priv->publish = tp_channel_new (connection, object_path,
					TP_IFACE_CHANNEL_TYPE_CONTACT_LIST,
					TP_HANDLE_TYPE_LIST,
					GPOINTER_TO_UINT (user_data),
					NULL);

	/* TpChannel emits initial set of members just before being ready */
	g_signal_connect (priv->publish, "group-members-changed",
			  G_CALLBACK (tp_contact_list_publish_group_members_changed_cb),
			  list);
}

static void
tp_contact_list_publish_request_handle_cb (TpConnection *connection,
					   const GArray *handles,
					   const GError *error,
					   gpointer      user_data,
					   GObject      *list)
{
	TpHandle handle;

	if (error) {
		DEBUG ("Error: %s", error->message);
		return;
	}

	handle = g_array_index (handles, TpHandle, 0);
	tp_cli_connection_call_request_channel (connection, -1,
						TP_IFACE_CHANNEL_TYPE_CONTACT_LIST,
						TP_HANDLE_TYPE_LIST,
						handle,
						TRUE,
						tp_contact_list_publish_request_channel_cb,
						GUINT_TO_POINTER (handle), NULL,
						list);
}

static void
tp_contact_list_subscribe_group_members_changed_cb (TpChannel     *channel,
						    gchar         *message,
						    GArray        *added,
						    GArray        *removed,
						    GArray        *local_pending,
						    GArray        *remote_pending,
						    guint          actor,
						    guint          reason,
						    EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	guint i;

	/* We now get the presence of those contacts, add them to members */
	if (added->len > 0) {
		empathy_tp_contact_factory_get_from_handles (priv->factory,
			added->len, (TpHandle*) added->data,
			tp_contact_list_got_added_members_cb, NULL, NULL,
			G_OBJECT (list));
	}

	/* Those contacts refuse to send us their presence, remove from members. */
	for (i = 0; i < removed->len; i++) {
		tp_contact_list_remove_handle (list, priv->members,
			g_array_index (added, TpHandle, i));
	}

	/* We want those contacts in our contact list but we don't get their 
	 * presence yet. Add to members anyway. */
	if (remote_pending->len > 0) {
		empathy_tp_contact_factory_get_from_handles (priv->factory,
			remote_pending->len, (TpHandle*) remote_pending->data,
			tp_contact_list_got_added_members_cb, NULL, NULL,
			G_OBJECT (list));
	}
}

static void
tp_contact_list_subscribe_request_channel_cb (TpConnection *connection,
					      const gchar  *object_path,
					      const GError *error,
					      gpointer      user_data,
					      GObject      *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);

	if (error) {
		DEBUG ("Error: %s", error->message);
		return;
	}

	priv->subscribe = tp_channel_new (connection, object_path,
					  TP_IFACE_CHANNEL_TYPE_CONTACT_LIST,
					  TP_HANDLE_TYPE_LIST,
					  GPOINTER_TO_UINT (user_data),
					  NULL);

	/* TpChannel emits initial set of members just before being ready */
	g_signal_connect (priv->subscribe, "group-members-changed",
			  G_CALLBACK (tp_contact_list_subscribe_group_members_changed_cb),
			  list);
}

static void
tp_contact_list_subscribe_request_handle_cb (TpConnection *connection,
					     const GArray *handles,
					     const GError *error,
					     gpointer      user_data,
					     GObject      *list)
{
	TpHandle handle;

	if (error) {
		DEBUG ("Error: %s", error->message);
		return;
	}

	handle = g_array_index (handles, TpHandle, 0);
	tp_cli_connection_call_request_channel (connection, -1,
						TP_IFACE_CHANNEL_TYPE_CONTACT_LIST,
						TP_HANDLE_TYPE_LIST,
						handle,
						TRUE,
						tp_contact_list_subscribe_request_channel_cb,
						GUINT_TO_POINTER (handle), NULL,
						list);
}

static void
tp_contact_list_new_channel_cb (TpConnection *proxy,
				const gchar  *object_path,
				const gchar  *channel_type,
				guint         handle_type,
				guint         handle,
				gboolean      suppress_handler,
				gpointer      user_data,
				GObject      *list)
{
	tp_contact_list_group_add_channel (EMPATHY_TP_CONTACT_LIST (list),
					   object_path, channel_type,
					   handle_type, handle);
}

static void
tp_contact_list_list_channels_cb (TpConnection    *connection,
				  const GPtrArray *channels,
				  const GError    *error,
				  gpointer         user_data,
				  GObject         *list)
{
	guint i;

	if (error) {
		DEBUG ("Error: %s", error->message);
		return;
	}

	for (i = 0; i < channels->len; i++) {
		GValueArray  *chan_struct;
		const gchar  *object_path;
		const gchar  *channel_type;
		TpHandleType  handle_type;
		guint         handle;

		chan_struct = g_ptr_array_index (channels, i);
		object_path = g_value_get_boxed (g_value_array_get_nth (chan_struct, 0));
		channel_type = g_value_get_string (g_value_array_get_nth (chan_struct, 1));
		handle_type = g_value_get_uint (g_value_array_get_nth (chan_struct, 2));
		handle = g_value_get_uint (g_value_array_get_nth (chan_struct, 3));

		tp_contact_list_group_add_channel (EMPATHY_TP_CONTACT_LIST (list),
						   object_path, channel_type,
						   handle_type, handle);
	}
}

static void
tp_contact_list_finalize (GObject *object)
{
	EmpathyTpContactListPriv *priv;
	EmpathyTpContactList     *list;
	GHashTableIter            iter;
	gpointer                  channel;

	list = EMPATHY_TP_CONTACT_LIST (object);
	priv = GET_PRIV (list);

	DEBUG ("finalize: %p", object);

	if (priv->subscribe) {
		g_object_unref (priv->subscribe);
	}
	if (priv->publish) {
		g_object_unref (priv->publish);
	}

	if (priv->connection) {
		g_object_unref (priv->connection);
	}

	if (priv->factory) {
		g_object_unref (priv->factory);
	}

	g_hash_table_iter_init (&iter, priv->groups);
	while (g_hash_table_iter_next (&iter, NULL, &channel)) {
		g_signal_handlers_disconnect_by_func (channel,
			tp_contact_list_group_invalidated_cb, list);
	}

	g_hash_table_destroy (priv->groups);
	g_hash_table_destroy (priv->members);
	g_hash_table_destroy (priv->pendings);

	G_OBJECT_CLASS (empathy_tp_contact_list_parent_class)->finalize (object);
}

static void
tp_contact_list_constructed (GObject *list)
{

	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	const gchar              *protocol_name = NULL;
	const gchar              *names[] = {NULL, NULL};

	priv->factory = empathy_tp_contact_factory_dup_singleton (priv->connection);

	names[0] = "publish";
	tp_cli_connection_call_request_handles (priv->connection,
						-1,
						TP_HANDLE_TYPE_LIST,
						names,
						tp_contact_list_publish_request_handle_cb,
						NULL, NULL,
						G_OBJECT (list));
	names[0] = "subscribe";
	tp_cli_connection_call_request_handles (priv->connection,
						-1,
						TP_HANDLE_TYPE_LIST,
						names,
						tp_contact_list_subscribe_request_handle_cb,
						NULL, NULL,
						G_OBJECT (list));

	tp_cli_connection_call_list_channels (priv->connection, -1,
					      tp_contact_list_list_channels_cb,
					      NULL, NULL,
					      list);

	tp_cli_connection_connect_to_new_channel (priv->connection,
						  tp_contact_list_new_channel_cb,
						  NULL, NULL,
						  list, NULL);

	/* Check for protocols that does not support contact groups. We can
	 * put all contacts into a special group in that case.
	 * FIXME: Default group should be an information in the profile */
	//protocol_name = tp_connection_get_protocol (priv->connection);
	if (!tp_strdiff (protocol_name, "local-xmpp")) {
		priv->protocol_group = _("People nearby");
	}
}

static void
tp_contact_list_get_property (GObject    *object,
			      guint       param_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_CONNECTION:
		g_value_set_object (value, priv->connection);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
tp_contact_list_set_property (GObject      *object,
			      guint         param_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_CONNECTION:
		priv->connection = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
empathy_tp_contact_list_class_init (EmpathyTpContactListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tp_contact_list_finalize;
	object_class->constructed = tp_contact_list_constructed;
	object_class->get_property = tp_contact_list_get_property;
	object_class->set_property = tp_contact_list_set_property;

	g_object_class_install_property (object_class,
					 PROP_CONNECTION,
					 g_param_spec_object ("connection",
							      "The Connection",
							      "The connection associated with the contact list",
							      TP_TYPE_CONNECTION,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (EmpathyTpContactListPriv));
}

static void
empathy_tp_contact_list_init (EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (list,
		EMPATHY_TYPE_TP_CONTACT_LIST, EmpathyTpContactListPriv);

	list->priv = priv;

	/* Map group's name to group's channel */
	priv->groups = g_hash_table_new_full (g_str_hash, g_str_equal,
					      NULL,
					      (GDestroyNotify) g_object_unref);

	/* Map contact's handle to EmpathyContact object */
	priv->members = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					       NULL,
					       (GDestroyNotify) g_object_unref);

	/* Map contact's handle to EmpathyContact object */
	priv->pendings = g_hash_table_new_full (g_direct_hash, g_direct_equal,
						NULL,
						(GDestroyNotify) g_object_unref);
}

EmpathyTpContactList *
empathy_tp_contact_list_new (TpConnection *connection)
{
	return g_object_new (EMPATHY_TYPE_TP_CONTACT_LIST,
			     "connection", connection,
			     NULL);
}

TpConnection *
empathy_tp_contact_list_get_connection (EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);

	return priv->connection;
}

static void
tp_contact_list_add (EmpathyContactList *list,
		     EmpathyContact     *contact,
		     const gchar        *message)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	TpHandle handle;
	GArray handles = {(gchar *) &handle, 1};

	handle = empathy_contact_get_handle (contact);
	if (priv->subscribe) {
		tp_cli_channel_interface_group_call_add_members (priv->subscribe,
			-1, &handles, message, NULL, NULL, NULL, NULL);
	}
}

static void
tp_contact_list_remove (EmpathyContactList *list,
			EmpathyContact     *contact,
			const gchar        *message)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	TpHandle handle;
	GArray handles = {(gchar *) &handle, 1};

	handle = empathy_contact_get_handle (contact);
	if (priv->subscribe) {
		tp_cli_channel_interface_group_call_remove_members (priv->subscribe,
			-1, &handles, message, NULL, NULL, NULL, NULL);
	}
	if (priv->publish) {
		tp_cli_channel_interface_group_call_remove_members (priv->publish,
			-1, &handles, message, NULL, NULL, NULL, NULL);
	}
}

static GList *
tp_contact_list_get_members (EmpathyContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	GList *ret;

	ret = g_hash_table_get_values (priv->members);
	g_list_foreach (ret, (GFunc) g_object_ref, NULL);
	return ret;
}

static GList *
tp_contact_list_get_pendings (EmpathyContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	GList *ret;

	ret = g_hash_table_get_values (priv->pendings);
	g_list_foreach (ret, (GFunc) g_object_ref, NULL);
	return ret;
}

static GList *
tp_contact_list_get_all_groups (EmpathyContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	GList                    *ret, *l;

	ret = g_hash_table_get_keys (priv->groups);
	for (l = ret; l; l = l->next) {
		l->data = g_strdup (l->data);
	}

	if (priv->protocol_group) {
		ret = g_list_prepend (ret, g_strdup (priv->protocol_group));
	}

	return ret;
}

static GList *
tp_contact_list_get_groups (EmpathyContactList *list,
			    EmpathyContact     *contact)
{
	EmpathyTpContactListPriv  *priv = GET_PRIV (list);
	GList                     *ret = NULL;
	GHashTableIter             iter;
	gpointer                   group_name;
	gpointer                   channel;
	TpHandle                   handle;

	handle = empathy_contact_get_handle (contact);
	g_hash_table_iter_init (&iter, priv->groups);
	while (g_hash_table_iter_next (&iter, &group_name, &channel)) {
		const TpIntSet *members;

		members = tp_channel_group_get_members (channel);
		if (tp_intset_is_member (members, handle)) {
			ret = g_list_prepend (ret, g_strdup (group_name));
		}
	}

	if (priv->protocol_group) {
		ret = g_list_prepend (ret, g_strdup (priv->protocol_group));
	}

	return ret;
}

static void
tp_contact_list_add_to_group (EmpathyContactList *list,
			      EmpathyContact     *contact,
			      const gchar        *group_name)
{
	TpHandle handle;
	GArray *handles;

	handle = empathy_contact_get_handle (contact);
	handles = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), 1);
	g_array_append_val (handles, handle);
	tp_contact_list_group_add (EMPATHY_TP_CONTACT_LIST (list),
				   group_name, handles);
}

static void
tp_contact_list_remove_from_group (EmpathyContactList *list,
				   EmpathyContact     *contact,
				   const gchar        *group_name)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	TpChannel                *channel;
	TpHandle                  handle;
	GArray                    handles = {(gchar *) &handle, 1};

	channel = g_hash_table_lookup (priv->groups, group_name);
	if (channel == NULL) {
		return;
	}

	handle = empathy_contact_get_handle (contact);
	DEBUG ("remove contact %s (%d) from group %s",
		empathy_contact_get_id (contact), handle, group_name);

	tp_cli_channel_interface_group_call_remove_members (channel, -1,
		&handles, NULL, NULL, NULL, NULL, NULL);
}

static void
tp_contact_list_rename_group (EmpathyContactList *list,
			      const gchar        *old_group_name,
			      const gchar        *new_group_name)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	TpChannel                *channel;
	const TpIntSet           *members;
	GArray                   *handles;

	channel = g_hash_table_lookup (priv->groups, old_group_name);
	if (channel == NULL) {
		return;
	}

	DEBUG ("rename group %s to %s", old_group_name, new_group_name);

	/* Remove all members and close the old channel */
	members = tp_channel_group_get_members (channel);
	handles = tp_intset_to_array (members);
	tp_cli_channel_interface_group_call_remove_members (channel, -1,
		handles, NULL, NULL, NULL, NULL, NULL);
	tp_cli_channel_call_close (channel, -1, NULL, NULL, NULL, NULL);

	tp_contact_list_group_add (EMPATHY_TP_CONTACT_LIST (list),
				   new_group_name, handles);
}

static void
tp_contact_list_remove_group (EmpathyContactList *list,
			      const gchar *group_name)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	TpChannel                *channel;
	const TpIntSet           *members;
	GArray                   *handles;

	channel = g_hash_table_lookup (priv->groups, group_name);
	if (channel == NULL) {
		return;
	}

	DEBUG ("remove group %s", group_name);

	/* Remove all members and close the channel */
	members = tp_channel_group_get_members (channel);
	handles = tp_intset_to_array (members);
	tp_cli_channel_interface_group_call_remove_members (channel, -1,
		handles, NULL, NULL, NULL, NULL, NULL);
	tp_cli_channel_call_close (channel, -1, NULL, NULL, NULL, NULL);
	g_array_free (handles, TRUE);
}

static void
tp_contact_list_iface_init (EmpathyContactListIface *iface)
{
	iface->add               = tp_contact_list_add;
	iface->remove            = tp_contact_list_remove;
	iface->get_members       = tp_contact_list_get_members;
	iface->get_pendings      = tp_contact_list_get_pendings;
	iface->get_all_groups    = tp_contact_list_get_all_groups;
	iface->get_groups        = tp_contact_list_get_groups;
	iface->add_to_group      = tp_contact_list_add_to_group;
	iface->remove_from_group = tp_contact_list_remove_from_group;
	iface->rename_group      = tp_contact_list_rename_group;
	iface->remove_group	 = tp_contact_list_remove_group;
}

gboolean
empathy_tp_contact_list_can_add (EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;
	TpChannelGroupFlags       flags;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), FALSE);

	priv = GET_PRIV (list);

	if (priv->subscribe == NULL)
		return FALSE;

	flags = tp_channel_group_get_flags (priv->subscribe);
	return (flags & TP_CHANNEL_GROUP_FLAG_CAN_ADD) != 0;
}

void
empathy_tp_contact_list_remove_all (EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	GHashTableIter            iter;
	gpointer                  contact;

	g_return_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list));

	/* Remove all contacts */
	g_hash_table_iter_init (&iter, priv->members);
	while (g_hash_table_iter_next (&iter, NULL, &contact)) {
		g_signal_emit_by_name (list, "members-changed", contact,
				       NULL, 0, NULL,
				       FALSE);
	}
	g_hash_table_remove_all (priv->members);

	g_hash_table_iter_init (&iter, priv->pendings);
	while (g_hash_table_iter_next (&iter, NULL, &contact)) {
		g_signal_emit_by_name (list, "pendings-changed", contact,
				       NULL, 0, NULL,
				       FALSE);
	}
	g_hash_table_remove_all (priv->pendings);
}

