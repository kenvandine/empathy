/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Xavier Claessens <xclaesse@gmail.com>
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

#include <libmissioncontrol/mc-account.h>

#include <telepathy-glib/util.h>
#include <telepathy-glib/interfaces.h>

#include "empathy-tp-group.h"
#include "empathy-contact-factory.h"
#include "empathy-utils.h"
#include "empathy-marshal.h"

#define DEBUG_FLAG EMPATHY_DEBUG_TP
#include "empathy-debug.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyTpGroup)
typedef struct {
	TpChannel             *channel;
	gboolean               ready;

	EmpathyContactFactory *factory;
	McAccount             *account;
	gchar                 *group_name;
	guint                  self_handle;
	GList                 *members;
	GList                 *local_pendings;
	GList                 *remote_pendings;
} EmpathyTpGroupPriv;

enum {
	MEMBER_ADDED,
	MEMBER_REMOVED,
	LOCAL_PENDING,
	REMOTE_PENDING,
	DESTROY,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_CHANNEL,
	PROP_READY
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyTpGroup, empathy_tp_group, G_TYPE_OBJECT);

static EmpathyContact *
tp_group_get_contact (EmpathyTpGroup *group,
		      guint           handle)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	EmpathyContact     *contact = NULL;
	
	if (handle != 0) {
		contact = empathy_contact_factory_get_from_handle (priv->factory,
								   priv->account,
								   handle);
	}

	if (contact && handle == priv->self_handle) {
		empathy_contact_set_is_user (contact, TRUE);
	}

	return contact;
}

static GList *
tp_group_get_contacts (EmpathyTpGroup *group,
		       const GArray   *handles)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	GList              *contacts,  *l;

	if (!handles) {
		return NULL;
	}

	contacts = empathy_contact_factory_get_from_handles (priv->factory,
							     priv->account,
							     handles);

	/* FIXME: Only useful if the group has a different self handle than
	 * the connection, otherwise the contact factory already set that
	 * property. That can be known using group flags. */
	for (l = contacts; l; l = l->next) {
		if (empathy_contact_get_handle (l->data) == priv->self_handle) {
			empathy_contact_set_is_user (l->data, TRUE);
		}
	}

	return contacts;
}

EmpathyPendingInfo *
empathy_pending_info_new (EmpathyContact *member,
			  EmpathyContact *actor,
			  const gchar    *message)
{
	EmpathyPendingInfo *info;

	info = g_slice_new0 (EmpathyPendingInfo);

	if (member) {
		info->member = g_object_ref (member);
	}
	if (actor) {
		info->actor = g_object_ref (actor);
	}
	if (message) {
		info->message = g_strdup (message);
	}

	return info;
}

void
empathy_pending_info_free (EmpathyPendingInfo *info)
{
	if (!info) {
		return;
	}

	if (info->member) {
		g_object_unref (info->member);
	}
	if (info->actor) {
		g_object_unref (info->actor);
	}
	g_free (info->message);

	g_slice_free (EmpathyPendingInfo, info);
}

static gint
tp_group_local_pending_find (gconstpointer a,
			     gconstpointer b)
{
	const EmpathyPendingInfo *info = a;

	return (info->member != b);
}

static void
tp_group_remove_from_pendings (EmpathyTpGroup *group,
			       EmpathyContact *contact)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	GList              *l;

	/* local pending */
	l = g_list_find_custom (priv->local_pendings,
				contact,
				tp_group_local_pending_find);
	if (l) {
		empathy_pending_info_free (l->data);
		priv->local_pendings = g_list_delete_link (priv->local_pendings, l);
	}

	/* remote pending */
	l = g_list_find (priv->remote_pendings, contact);
	if (l) {
		g_object_unref (l->data);
		priv->remote_pendings = g_list_delete_link (priv->remote_pendings, l);
	}
}

static void
tp_group_update_members (EmpathyTpGroup *group,
			 const gchar    *message,
			 const GArray   *added,
			 const GArray   *removed,
			 const GArray   *local_pending,
			 const GArray   *remote_pending,
			 guint           actor,
			 guint           reason)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	EmpathyContact     *actor_contact = NULL;
	GList              *contacts, *l, *ll;

	actor_contact = tp_group_get_contact (group, actor);

	DEBUG ("Members changed for list %s:\n"
		"  added-len=%d, current-len=%d\n"
		"  removed-len=%d\n"
		"  local-pending-len=%d, current-len=%d\n"
		"  remote-pending-len=%d, current-len=%d",
		priv->group_name, added ? added->len : 0,
		g_list_length (priv->members), removed ? removed->len : 0,
		local_pending ? local_pending->len : 0,
		g_list_length (priv->local_pendings),
		remote_pending ? remote_pending->len : 0,
		g_list_length (priv->remote_pendings));

	/* Contacts added */
	contacts = tp_group_get_contacts (group, added);
	for (l = contacts; l; l = l->next) {
		tp_group_remove_from_pendings (group, l->data);

		/* If the contact is not yet member, add it and emit signal */
		if (!g_list_find (priv->members, l->data)) {
			priv->members = g_list_prepend (priv->members,
							g_object_ref (l->data));
			g_signal_emit (group, signals[MEMBER_ADDED], 0,
				       l->data, actor_contact, reason, message);
		}
		g_object_unref (l->data);
	}
	g_list_free (contacts);

	/* Contacts removed */
	contacts = tp_group_get_contacts (group, removed);
	for (l = contacts; l; l = l->next) {
		tp_group_remove_from_pendings (group, l->data);

		/* If the contact is member, remove it and emit signal */
		if ((ll = g_list_find (priv->members, l->data))) {
			g_object_unref (ll->data);
			priv->members = g_list_delete_link (priv->members, ll);
			g_signal_emit (group, signals[MEMBER_REMOVED], 0,
				       l->data, actor_contact, reason, message);
		}
		g_object_unref (l->data);
	}
	g_list_free (contacts);

	/* Contacts local pending */
	contacts = tp_group_get_contacts (group, local_pending);
	for (l = contacts; l; l = l->next) {
		/* If the contact is not yet local-pending, add it and emit signal */
		if (!g_list_find_custom (priv->local_pendings, l->data,
					 tp_group_local_pending_find)) {
			EmpathyPendingInfo *info;

			info = empathy_pending_info_new (l->data,
							 actor_contact,
							 message);

			priv->local_pendings = g_list_prepend (priv->local_pendings, info);
			g_signal_emit (group, signals[LOCAL_PENDING], 0,
				       l->data, actor_contact, reason, message);
		}
		g_object_unref (l->data);
	}
	g_list_free (contacts);

	/* Contacts remote pending */
	contacts = tp_group_get_contacts (group, remote_pending);
	for (l = contacts; l; l = l->next) {
		/* If the contact is not yet remote-pending, add it and emit signal */
		if (!g_list_find (priv->remote_pendings, l->data)) {
			priv->remote_pendings = g_list_prepend (priv->remote_pendings,
								g_object_ref (l->data));
			g_signal_emit (group, signals[REMOTE_PENDING], 0,
				       l->data, actor_contact, reason, message);
		}
		g_object_unref (l->data);
	}
	g_list_free (contacts);

	if (actor_contact) {
		g_object_unref (actor_contact);
	}

	DEBUG ("Members changed done for list %s:\n"
		"  members-len=%d\n"
		"  local-pendings-len=%d\n"
		"  remote-pendings-len=%d",
		priv->group_name, g_list_length (priv->members),
		g_list_length (priv->local_pendings),
		g_list_length (priv->remote_pendings));
}

static void
tp_group_members_changed_cb (TpChannel    *channel,
			     const gchar  *message,
			     const GArray *added,
			     const GArray *removed,
			     const GArray *local_pending,
			     const GArray *remote_pending,
			     guint         actor,
			     guint         reason,
			     gpointer      user_data,
			     GObject      *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	if (priv->ready) {
		tp_group_update_members (EMPATHY_TP_GROUP (group), message,
					 added, removed,
					 local_pending, remote_pending,
					 actor, reason);
	}
}

static void
tp_group_get_members_cb (TpChannel    *channel,
			 const GArray *handles,
			 const GError *error,
			 gpointer      user_data,
			 GObject      *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	if (error) {
		DEBUG ("Failed to get members: %s", error->message);
		return;
	}

	tp_group_update_members (EMPATHY_TP_GROUP (group),
				 NULL,    /* message */
				 handles, /* added */
				 NULL,    /* removed */
				 NULL,    /* local_pending */
				 NULL,    /* remote_pending */
				 0,       /* actor */
				 0);      /* reason */

	DEBUG ("Ready");
	priv->ready = TRUE;
	g_object_notify (group, "ready");
}

static void
tp_group_get_local_pending_cb (TpChannel        *channel,
			       const GPtrArray  *array,
			       const GError     *error,
			       gpointer          user_data,
			       GObject          *group)
{
	GArray *handles;
	guint   i = 0;
	
	if (error) {
		DEBUG ("Failed to get local pendings: %s", error->message);
		return;
	}

	handles = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
	g_array_append_val (handles, i);
	for (i = 0; array->len > i; i++) {
		GValueArray *pending_struct;
		const gchar *message;
		guint        member_handle;
		guint        actor_handle;
		guint        reason;

		pending_struct = g_ptr_array_index (array, i);
		member_handle = g_value_get_uint (g_value_array_get_nth (pending_struct, 0));
		actor_handle = g_value_get_uint (g_value_array_get_nth (pending_struct, 1));
		reason = g_value_get_uint (g_value_array_get_nth (pending_struct, 2));
		message = g_value_get_string (g_value_array_get_nth (pending_struct, 3));

		g_array_index (handles, guint, 0) = member_handle;

		tp_group_update_members (EMPATHY_TP_GROUP (group),
					 message,      /* message */
					 NULL,         /* added */
					 NULL,         /* removed */
					 handles,      /* local_pending */
					 NULL,         /* remote_pending */
					 actor_handle, /* actor */
					 reason);      /* reason */
	}
	g_array_free (handles, TRUE);
}

static void
tp_group_get_remote_pending_cb (TpChannel    *channel,
				const GArray *handles,
				const GError *error,
				gpointer      user_data,
				GObject      *group)
{
	if (error) {
		DEBUG ("Failed to get remote pendings: %s", error->message);
		return;
	}

	tp_group_update_members (EMPATHY_TP_GROUP (group),
				 NULL,    /* message */
				 NULL,    /* added */
				 NULL,    /* removed */
				 NULL,    /* local_pending */
				 handles, /* remote_pending */
				 0,       /* actor */
				 0);      /* reason */
}

static void
tp_group_inspect_handles_cb (TpConnection  *connection,
			     const gchar  **names,
			     const GError  *error,
			     gpointer       user_data,
			     GObject       *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	if (error) {
		DEBUG ("Failed to inspect channel handle: %s", error->message);
		return;
	}

	priv->group_name = g_strdup (*names);
}

static void
tp_group_invalidated_cb (TpProxy        *proxy,
			 guint           domain,
			 gint            code,
			 gchar          *message,
			 EmpathyTpGroup *group)
{
	DEBUG ("Channel invalidated: %s", message);
	g_signal_emit (group, signals[DESTROY], 0);
}

static void
tp_group_get_self_handle_cb (TpChannel    *proxy,
			     guint         handle,
			     const GError *error,
			     gpointer      user_data,
			     GObject      *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	TpConnection       *connection;
	guint               channel_handle;
	guint               channel_handle_type;
	GArray             *handles;

	if (error) {
		DEBUG ("Failed to get self handle: %s", error->message);
		return;
	}

	priv->self_handle = handle;
	tp_cli_channel_interface_group_connect_to_members_changed (priv->channel,
								   tp_group_members_changed_cb,
								   NULL, NULL,
								   group, NULL);

	/* GetMembers is called last, so it will be the last to get the reply,
	 * so we'll be ready once that call return. */
	g_object_get (priv->channel,
		      "connection", &connection,
		      "handle-type", &channel_handle_type,
		      "handle", &channel_handle,
		      NULL);
	handles = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
	g_array_prepend_val (handles, channel_handle);
	tp_cli_connection_call_inspect_handles (connection, -1,
						channel_handle_type,
						handles,
						tp_group_inspect_handles_cb,
						NULL, NULL,
						group);
	g_array_free (handles, TRUE);

	tp_cli_channel_interface_group_call_get_local_pending_members_with_info
							(priv->channel, -1,
							 tp_group_get_local_pending_cb,
							 NULL, NULL, 
							 group);
	tp_cli_channel_interface_group_call_get_remote_pending_members
							(priv->channel, -1,
							 tp_group_get_remote_pending_cb,
							 NULL, NULL, 
							 group);
	tp_cli_channel_interface_group_call_get_members (priv->channel, -1,
							 tp_group_get_members_cb,
							 NULL, NULL, 
							 group);
}

static void
tp_group_factory_ready_cb (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv      *priv = GET_PRIV (group);
	EmpathyTpContactFactory *tp_factory;

	tp_factory = empathy_contact_factory_get_tp_factory (priv->factory, priv->account);
	g_signal_handlers_disconnect_by_func (tp_factory, tp_group_factory_ready_cb, group);
	tp_cli_channel_interface_group_call_get_self_handle (priv->channel, -1,
							     tp_group_get_self_handle_cb,
							     NULL, NULL,
							     G_OBJECT (group));
}

static void
tp_group_channel_ready_cb (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv      *priv = GET_PRIV (group);
	EmpathyTpContactFactory *tp_factory;

	tp_factory = empathy_contact_factory_get_tp_factory (priv->factory,
							     priv->account);
	if (empathy_tp_contact_factory_is_ready (tp_factory)) {
		tp_group_factory_ready_cb (group);
	} else {
		g_signal_connect_swapped (tp_factory, "notify::ready",
					  G_CALLBACK (tp_group_factory_ready_cb),
					  group);
	}
}

static void
tp_group_finalize (GObject *object)
{
	EmpathyTpGroupPriv      *priv = GET_PRIV (object);
	EmpathyTpContactFactory *tp_factory;

	DEBUG ("finalize: %p", object);

	tp_factory = empathy_contact_factory_get_tp_factory (priv->factory, priv->account);
	g_signal_handlers_disconnect_by_func (tp_factory, tp_group_factory_ready_cb, object);

	if (priv->channel) {
		g_signal_handlers_disconnect_by_func (priv->channel,
						      tp_group_invalidated_cb,
						      object);
		g_object_unref (priv->channel);
	}
	if (priv->account) {
		g_object_unref (priv->account);
	}
	if (priv->factory) {
		g_object_unref (priv->factory);
	}
	g_free (priv->group_name);

	g_list_foreach (priv->members, (GFunc) g_object_unref, NULL);
	g_list_free (priv->members);

	g_list_foreach (priv->local_pendings, (GFunc) empathy_pending_info_free, NULL);
	g_list_free (priv->local_pendings);

	g_list_foreach (priv->remote_pendings, (GFunc) g_object_unref, NULL);
	g_list_free (priv->remote_pendings);

	G_OBJECT_CLASS (empathy_tp_group_parent_class)->finalize (object);
}

static void
tp_group_constructed (GObject *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	gboolean            channel_ready;

	priv->factory = empathy_contact_factory_dup_singleton ();
	priv->account = empathy_channel_get_account (priv->channel);

	g_signal_connect (priv->channel, "invalidated",
			  G_CALLBACK (tp_group_invalidated_cb),
			  group);

	g_object_get (priv->channel, "channel-ready", &channel_ready, NULL);
	if (channel_ready) {
		tp_group_channel_ready_cb (EMPATHY_TP_GROUP (group));
	} else {
		g_signal_connect_swapped (priv->channel, "notify::channel-ready",
					  G_CALLBACK (tp_group_channel_ready_cb),
					  group);
	}
}

static void
tp_group_get_property (GObject    *object,
		       guint       param_id,
		       GValue     *value,
		       GParamSpec *pspec)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_CHANNEL:
		g_value_set_object (value, priv->channel);
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
tp_group_set_property (GObject      *object,
		       guint         param_id,
		       const GValue *value,
		       GParamSpec   *pspec)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_CHANNEL:
		priv->channel = g_object_ref (g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
empathy_tp_group_class_init (EmpathyTpGroupClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tp_group_finalize;
	object_class->constructed = tp_group_constructed;
	object_class->get_property = tp_group_get_property;
	object_class->set_property = tp_group_set_property;

	g_object_class_install_property (object_class,
					 PROP_CHANNEL,
					 g_param_spec_object ("channel",
							      "telepathy channel",
							      "The channel for the group",
							      TP_TYPE_CHANNEL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_READY,
					 g_param_spec_boolean ("ready",
							       "Is the object ready",
							       "This object can't be used until this becomes true",
							       FALSE,
							       G_PARAM_READABLE));

	signals[MEMBER_ADDED] =
		g_signal_new ("member-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      _empathy_marshal_VOID__OBJECT_OBJECT_UINT_STRING,
			      G_TYPE_NONE,
			      4, EMPATHY_TYPE_CONTACT, EMPATHY_TYPE_CONTACT, G_TYPE_UINT, G_TYPE_STRING);

	signals[MEMBER_REMOVED] =
		g_signal_new ("member-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      _empathy_marshal_VOID__OBJECT_OBJECT_UINT_STRING,
			      G_TYPE_NONE,
			      4, EMPATHY_TYPE_CONTACT, EMPATHY_TYPE_CONTACT, G_TYPE_UINT, G_TYPE_STRING);

	signals[LOCAL_PENDING] =
		g_signal_new ("local-pending",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      _empathy_marshal_VOID__OBJECT_OBJECT_UINT_STRING,
			      G_TYPE_NONE,
			      4, EMPATHY_TYPE_CONTACT, EMPATHY_TYPE_CONTACT, G_TYPE_UINT, G_TYPE_STRING);

	signals[REMOTE_PENDING] =
		g_signal_new ("remote-pending",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      _empathy_marshal_VOID__OBJECT_OBJECT_UINT_STRING,
			      G_TYPE_NONE,
			      4, EMPATHY_TYPE_CONTACT, EMPATHY_TYPE_CONTACT, G_TYPE_UINT, G_TYPE_STRING);

	signals[DESTROY] =
		g_signal_new ("destroy",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_type_class_add_private (object_class, sizeof (EmpathyTpGroupPriv));
}

static void
empathy_tp_group_init (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (group,
		EMPATHY_TYPE_TP_GROUP, EmpathyTpGroupPriv);

	group->priv = priv;
}

EmpathyTpGroup *
empathy_tp_group_new (TpChannel *channel)
{
	g_return_val_if_fail (TP_IS_CHANNEL (channel), NULL);

	return g_object_new (EMPATHY_TYPE_TP_GROUP, 
			     "channel", channel,
			     NULL);
}

static void
tp_group_async_cb (TpChannel    *channel,
		   const GError *error,
		   gpointer      user_data,
		   GObject      *weak_object)
{
	if (error) {
		DEBUG ("%s: %s", (gchar*) user_data, error->message);
	}
}

void
empathy_tp_group_close (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	g_return_if_fail (EMPATHY_IS_TP_GROUP (group));
	g_return_if_fail (priv->ready);

	tp_cli_channel_call_close (priv->channel, -1,
				   tp_group_async_cb,
				   "Failed to close", NULL,
				   G_OBJECT (group));
}

static GArray *
tp_group_get_handles (GList *contacts)
{
	GArray *handles;
	guint   length;
	GList  *l;

	length = g_list_length (contacts);
	handles = g_array_sized_new (FALSE, FALSE, sizeof (guint), length);

	for (l = contacts; l; l = l->next) {
		guint handle;

		handle = empathy_contact_get_handle (l->data);
		g_array_append_val (handles, handle);
	}

	return handles;
}

void
empathy_tp_group_add_members (EmpathyTpGroup *group,
			      GList          *contacts,
			      const gchar    *message)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	GArray             *handles;

	g_return_if_fail (EMPATHY_IS_TP_GROUP (group));
	g_return_if_fail (contacts != NULL);
	g_return_if_fail (priv->ready);

	handles = tp_group_get_handles (contacts);
	tp_cli_channel_interface_group_call_add_members (priv->channel, -1,
							 handles,
							 message,
							 tp_group_async_cb,
							 "Failed to add members", NULL,
							 G_OBJECT (group));
	g_array_free (handles, TRUE);
}

void
empathy_tp_group_remove_members (EmpathyTpGroup *group,
				 GList          *contacts,
				 const gchar    *message)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	GArray             *handles;

	g_return_if_fail (EMPATHY_IS_TP_GROUP (group));
	g_return_if_fail (contacts != NULL);
	g_return_if_fail (priv->ready);

	handles = tp_group_get_handles (contacts);
	tp_cli_channel_interface_group_call_remove_members (priv->channel, -1,
							    handles,
							    message,
							    tp_group_async_cb,
							    "Failed to remove members", NULL,
							    G_OBJECT (group));
	g_array_free (handles, TRUE);
}

void
empathy_tp_group_add_member (EmpathyTpGroup *group,
			     EmpathyContact *contact,
			     const gchar    *message)
{
	GList *contacts;

	contacts = g_list_prepend (NULL, contact);
	empathy_tp_group_add_members (group, contacts, message);
	g_list_free (contacts);
}

void
empathy_tp_group_remove_member (EmpathyTpGroup *group,
				EmpathyContact *contact,
				const gchar    *message)
{
	GList *contacts;

	contacts = g_list_prepend (NULL, contact);
	empathy_tp_group_remove_members (group, contacts, message);
	g_list_free (contacts);
}

GList *
empathy_tp_group_get_members (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), NULL);

	g_list_foreach (priv->members, (GFunc) g_object_ref, NULL);

	return g_list_copy (priv->members);
}

GList *
empathy_tp_group_get_local_pendings (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	GList              *pendings = NULL, *l;

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), NULL);

	for (l = priv->local_pendings; l; l = l->next) {
		EmpathyPendingInfo *info;
		EmpathyPendingInfo *new_info;

		info = l->data;
		new_info = empathy_pending_info_new (info->member,
						     info->actor,
						     info->message);
		pendings = g_list_prepend (pendings, new_info);
	}

	return pendings;
}

GList *
empathy_tp_group_get_remote_pendings (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), NULL);

	g_list_foreach (priv->remote_pendings, (GFunc) g_object_ref, NULL);

	return g_list_copy (priv->remote_pendings);
}

const gchar *
empathy_tp_group_get_name (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), NULL);
	g_return_val_if_fail (priv->ready, NULL);

	return priv->group_name;
}

EmpathyContact *
empathy_tp_group_get_self_contact (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), NULL);
	g_return_val_if_fail (priv->ready, NULL);

	return tp_group_get_contact (group, priv->self_handle);
}

gboolean
empathy_tp_group_is_member (EmpathyTpGroup *group,
			    EmpathyContact *contact)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), FALSE);
	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), FALSE);

	return g_list_find (priv->members, contact) != NULL;
}

gboolean
empathy_tp_group_is_ready (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), FALSE);

	return priv->ready;
}

EmpathyPendingInfo *
empathy_tp_group_get_invitation (EmpathyTpGroup  *group,
				 EmpathyContact **remote_contact)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	EmpathyContact     *contact = NULL;
	EmpathyPendingInfo *invitation = NULL;
	GList              *l;

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), FALSE);
	g_return_val_if_fail (priv->ready, NULL);

	for (l = priv->local_pendings; l; l = l->next) {
		EmpathyPendingInfo *info = l->data;

		if (empathy_contact_is_user (info->member)) {
			invitation = info;
			break;
		}
	}

	if (invitation) {
		contact = invitation->actor;
	}
	if (!invitation) {
		if (priv->remote_pendings) {
			contact = priv->remote_pendings->data;
		}
		else if (priv->members) {
			contact = priv->members->data;
		}
	}

	if (remote_contact && contact) {
		*remote_contact = g_object_ref (contact);
	}

	return invitation;
}

TpChannelGroupFlags
empathy_tp_group_get_flags (EmpathyTpGroup *self)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (self);

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (self), 0);

	if (priv->channel == NULL)
		return 0;

	return tp_channel_group_get_flags (priv->channel);
}
