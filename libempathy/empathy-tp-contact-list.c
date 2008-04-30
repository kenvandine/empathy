/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Xavier Claessens <xclaesse@gmail.com>
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
#include <glib/gi18n.h>

#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/dbus.h>

#include "empathy-tp-contact-list.h"
#include "empathy-contact-list.h"
#include "empathy-tp-group.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_TP | EMPATHY_DEBUG_CONTACT
#include "empathy-debug.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_TP_CONTACT_LIST, EmpathyTpContactListPriv))

struct _EmpathyTpContactListPriv {
	McAccount      *account;
	TpConnection   *connection;
	const gchar    *protocol_group;
	gboolean        ready;

	EmpathyTpGroup *publish;
	EmpathyTpGroup *subscribe;
	GList          *members;
	GList          *pendings;

	GList          *groups;
	GHashTable     *contacts_groups;
};

typedef enum {
	TP_CONTACT_LIST_TYPE_PUBLISH,
	TP_CONTACT_LIST_TYPE_SUBSCRIBE,
	TP_CONTACT_LIST_TYPE_UNKNOWN
} TpContactListType;

static void empathy_tp_contact_list_class_init (EmpathyTpContactListClass *klass);
static void empathy_tp_contact_list_init       (EmpathyTpContactList      *list);
static void tp_contact_list_iface_init         (EmpathyContactListIface   *iface);

enum {
	DESTROY,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_ACCOUNT,
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (EmpathyTpContactList, empathy_tp_contact_list, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (EMPATHY_TYPE_CONTACT_LIST,
						tp_contact_list_iface_init));

static void
tp_contact_list_group_destroy_cb (EmpathyTpGroup       *group,
				  EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);

	DEBUG ("Group destroyed: %s", empathy_tp_group_get_name (group));

	priv->groups = g_list_remove (priv->groups, group);
	g_object_unref (group);
}

static void
tp_contact_list_group_member_added_cb (EmpathyTpGroup       *group,
				       EmpathyContact       *contact,
				       EmpathyContact       *actor,
				       guint                 reason,
				       const gchar          *message,
				       EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv  *priv = GET_PRIV (list);
	const gchar               *group_name;
	GList                    **groups;

	if (!g_list_find (priv->members, contact)) {
		return;
	}

	groups = g_hash_table_lookup (priv->contacts_groups, contact);
	if (!groups) {
		groups = g_slice_new0 (GList*);
		g_hash_table_insert (priv->contacts_groups,
				     g_object_ref (contact),
				     groups);
	}

	group_name = empathy_tp_group_get_name (group);
	if (!g_list_find_custom (*groups, group_name, (GCompareFunc) strcmp)) {
		DEBUG ("Contact %s (%d) added to group %s",
			empathy_contact_get_id (contact),
			empathy_contact_get_handle (contact),
			group_name);
		*groups = g_list_prepend (*groups, g_strdup (group_name));
		g_signal_emit_by_name (list, "groups-changed", contact,
				       group_name,
				       TRUE);
	}
}

static void
tp_contact_list_group_member_removed_cb (EmpathyTpGroup       *group,
					 EmpathyContact       *contact,
					 EmpathyContact       *actor,
					 guint                 reason,
					 const gchar          *message,
					 EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv  *priv = GET_PRIV (list);
	const gchar               *group_name;
	GList                    **groups, *l;

	if (!g_list_find (priv->members, contact)) {
		return;
	}

	groups = g_hash_table_lookup (priv->contacts_groups, contact);
	if (!groups) {
		return;
	}

	group_name = empathy_tp_group_get_name (group);
	if ((l = g_list_find_custom (*groups, group_name, (GCompareFunc) strcmp))) {
		DEBUG ("Contact %s (%d) removed from group %s",
			empathy_contact_get_id (contact),
			empathy_contact_get_handle (contact),
			group_name);
		*groups = g_list_delete_link (*groups, l);
		g_signal_emit_by_name (list, "groups-changed", contact,
				       group_name,
				       FALSE);
	}
}

static EmpathyTpGroup *
tp_contact_list_find_group (EmpathyTpContactList *list,
			    const gchar          *group)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	GList                    *l;

	for (l = priv->groups; l; l = l->next) {
		if (!tp_strdiff (group, empathy_tp_group_get_name (l->data))) {
			return l->data;
		}
	}
	return NULL;
}

static TpContactListType
tp_contact_list_get_type (EmpathyTpContactList *list,
			  EmpathyTpGroup       *group)
{
	const gchar *name;

	name = empathy_tp_group_get_name (group);
	if (!tp_strdiff (name, "subscribe")) {
		return TP_CONTACT_LIST_TYPE_SUBSCRIBE;
	} else if (!tp_strdiff (name, "publish")) {
		return TP_CONTACT_LIST_TYPE_PUBLISH;
	}

	return TP_CONTACT_LIST_TYPE_UNKNOWN;
}

static void
tp_contact_list_add_member (EmpathyTpContactList *list,
			    EmpathyContact       *contact,
			    EmpathyContact       *actor,
			    guint                 reason,
			    const gchar          *message)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	GList                    *l;

	/* Add to the list and emit signal */
	priv->members = g_list_prepend (priv->members, g_object_ref (contact));
	g_signal_emit_by_name (list, "members-changed",
			       contact, actor, reason, message,
			       TRUE);

	/* This contact is now member, implicitly accept pending. */
	if (g_list_find (priv->pendings, contact)) {
		empathy_tp_group_add_member (priv->publish, contact, "");
	}

	/* Update groups of the contact */
	for (l = priv->groups; l; l = l->next) {
		if (empathy_tp_group_is_member (l->data, contact)) {
			tp_contact_list_group_member_added_cb (l->data, contact,
							       NULL, 0, NULL, 
							       list);
		}
	}
}

static void
tp_contact_list_added_cb (EmpathyTpGroup       *group,
			  EmpathyContact       *contact,
			  EmpathyContact       *actor,
			  guint                 reason,
			  const gchar          *message,
			  EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	TpContactListType         list_type;

	list_type = tp_contact_list_get_type (list, group);
	DEBUG ("Contact %s (%d) added to list type %d",
		empathy_contact_get_id (contact),
		empathy_contact_get_handle (contact),
		list_type);

	/* We now get the presence of that contact, add it to members */
	if (list_type == TP_CONTACT_LIST_TYPE_SUBSCRIBE &&
	    !g_list_find (priv->members, contact)) {
		tp_contact_list_add_member (list, contact, actor, reason, message);
	}

	/* We now send our presence to that contact, remove it from pendings */
	if (list_type == TP_CONTACT_LIST_TYPE_PUBLISH &&
	    g_list_find (priv->pendings, contact)) {
		g_signal_emit_by_name (list, "pendings-changed",
				       contact, actor, reason, message,
				       FALSE);
		priv->pendings = g_list_remove (priv->pendings, contact);
		g_object_unref (contact);
	}
}

static void
tp_contact_list_removed_cb (EmpathyTpGroup       *group,
			    EmpathyContact       *contact,
			    EmpathyContact       *actor,
			    guint                 reason,
			    const gchar          *message,
			    EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	TpContactListType         list_type;

	list_type = tp_contact_list_get_type (list, group);
	DEBUG ("Contact %s (%d) removed from list type %d",
		empathy_contact_get_id (contact),
		empathy_contact_get_handle (contact),
		list_type);

	/* This contact refuses to send us his presence, remove from members. */
	if (list_type == TP_CONTACT_LIST_TYPE_SUBSCRIBE &&
	    g_list_find (priv->members, contact)) {
		g_signal_emit_by_name (list, "members-changed",
				       contact, actor, reason, message,
				       FALSE);
		priv->members = g_list_remove (priv->members, contact);
		g_object_unref (contact);
	}

	/* We refuse to send our presence to that contact, remove from pendings */
	if (list_type == TP_CONTACT_LIST_TYPE_PUBLISH &&
	    g_list_find (priv->pendings, contact)) {
		g_signal_emit_by_name (list, "pendings-changed",
				       contact, actor, reason, message,
				       FALSE);
		priv->pendings = g_list_remove (priv->pendings, contact);
		g_object_unref (contact);
	}
}

static void
tp_contact_list_pending_cb (EmpathyTpGroup       *group,
			    EmpathyContact       *contact,
			    EmpathyContact       *actor,
			    guint                 reason,
			    const gchar          *message,
			    EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	TpContactListType         list_type;

	list_type = tp_contact_list_get_type (list, group);
	DEBUG ("Contact %s (%d) pending in list type %d",
		empathy_contact_get_id (contact),
		empathy_contact_get_handle (contact),
		list_type);

	/* We want this contact in our contact list but we don't get its 
	 * presence yet. Add to members anyway. */
	if (list_type == TP_CONTACT_LIST_TYPE_SUBSCRIBE &&
	    !g_list_find (priv->members, contact)) {
		tp_contact_list_add_member (list, contact, actor, reason, message);
	}

	/* This contact wants our presence, auto accept if he is member,
	 * otherwise he is pending. */
	if (list_type == TP_CONTACT_LIST_TYPE_PUBLISH &&
	    !g_list_find (priv->pendings, contact)) {
		if (g_list_find (priv->members, contact)) {
			empathy_tp_group_add_member (priv->publish, contact, "");
		} else {
			priv->pendings = g_list_prepend (priv->pendings,
							 g_object_ref (contact));
			g_signal_emit_by_name (list, "pendings-changed",
					       contact, actor, reason, message,
					       TRUE);
		}
	}
}

static void
tp_contact_list_invalidated_cb (TpConnection         *connection,
				guint                 domain,
				gint                  code,
				gchar                *message,
				EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	GList                    *l;

	DEBUG ("Connection invalidated");

	/* Remove all contacts */
	for (l = priv->members; l; l = l->next) {
		g_signal_emit_by_name (list, "members-changed", l->data,
				       NULL, 0, NULL,
				       FALSE);
		g_object_unref (l->data);
	}
	for (l = priv->pendings; l; l = l->next) {
		g_signal_emit_by_name (list, "pendings-changed", l->data,
				       NULL, 0, NULL,
				       FALSE);
		g_object_unref (l->data);
	}
	g_list_free (priv->members);
	g_list_free (priv->pendings);
	priv->members = NULL;
	priv->pendings = NULL;

	/* Tell the world to not use us anymore */
	g_signal_emit (list, signals[DESTROY], 0);
}

static void
tp_contact_list_group_list_free (GList **groups)
{
	g_list_foreach (*groups, (GFunc) g_free, NULL);
	g_list_free (*groups);
	g_slice_free (GList*, groups);
}

static void
tp_contact_list_add_channel (EmpathyTpContactList *list,
			     const gchar          *object_path,
			     const gchar          *channel_type,
			     TpHandleType          handle_type,
			     guint                 handle)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	EmpathyTpGroup           *group;
	TpChannel                *channel;

	if (strcmp (channel_type, TP_IFACE_CHANNEL_TYPE_CONTACT_LIST) != 0 ||
	    (handle_type != TP_HANDLE_TYPE_LIST &&
	     handle_type != TP_HANDLE_TYPE_GROUP)) {
		return;
	}

	channel = tp_channel_new (priv->connection,
				  object_path, channel_type,
				  handle_type, handle, NULL);

	group = empathy_tp_group_new (channel);
	empathy_run_until_ready (group);
	g_object_unref (channel);

	if (handle_type == TP_HANDLE_TYPE_LIST) {
		TpContactListType  list_type;
		GList             *contacts, *l;

		list_type = tp_contact_list_get_type (list, group);
		if (list_type == TP_CONTACT_LIST_TYPE_PUBLISH && !priv->publish) {
			priv->publish = g_object_ref (group);

			/* Publish is the list of contacts to who we send our
			 * presence. Makes no sense to be in remote-pending */
			g_signal_connect (group, "local-pending",
					  G_CALLBACK (tp_contact_list_pending_cb),
					  list);

			contacts = empathy_tp_group_get_local_pendings (group);
			for (l = contacts; l; l = l->next) {
				EmpathyPendingInfo *info = l->data;

				tp_contact_list_pending_cb (group,
							    info->member,
							    info->actor,
							    0,
							    info->message,
							    list);
				empathy_pending_info_free (info);
			}
			g_list_free (contacts);
		}
		else if (list_type == TP_CONTACT_LIST_TYPE_SUBSCRIBE && !priv->subscribe) {
			priv->subscribe = g_object_ref (group);

			/* Subscribe is the list of contacts from who we
			 * receive presence. Makes no sense to be in
			 * local-pending */
			g_signal_connect (group, "remote-pending",
					  G_CALLBACK (tp_contact_list_pending_cb),
					  list);

			contacts = empathy_tp_group_get_remote_pendings (group);
			for (l = contacts; l; l = l->next) {
				tp_contact_list_pending_cb (group,
							    l->data,
							    NULL, 0,
							    NULL, list);
				g_object_unref (l->data);
			}
			g_list_free (contacts);
		} else {
			DEBUG ("Type of contact list channel unknown or aleady "
				"have that list: %s",
				empathy_tp_group_get_name (group));
			goto OUT;
		}
		DEBUG ("New contact list channel of type: %d", list_type);

		g_signal_connect (group, "member-added",
				  G_CALLBACK (tp_contact_list_added_cb),
				  list);
		g_signal_connect (group, "member-removed",
				  G_CALLBACK (tp_contact_list_removed_cb),
				  list);

		contacts = empathy_tp_group_get_members (group);
		for (l = contacts; l; l = l->next) {
			tp_contact_list_added_cb (group,
						  l->data,
						  NULL, 0, NULL,
						  list);
			g_object_unref (l->data);
		}
		g_list_free (contacts);
	}
	else if (handle_type == TP_HANDLE_TYPE_GROUP) {
		const gchar *group_name;
		GList       *contacts, *l;

		/* Check if already exists */
		group_name = empathy_tp_group_get_name (group);
		if (tp_contact_list_find_group (list, group_name)) {
			goto OUT;
		}

		DEBUG ("New server-side group channel: %s", group_name);

		priv->groups = g_list_prepend (priv->groups, g_object_ref (group));

		g_signal_connect (group, "member-added",
				  G_CALLBACK (tp_contact_list_group_member_added_cb),
				  list);
		g_signal_connect (group, "member-removed",
				  G_CALLBACK (tp_contact_list_group_member_removed_cb),
				  list);
		g_signal_connect (group, "destroy",
				  G_CALLBACK (tp_contact_list_group_destroy_cb),
				  list);

		contacts = empathy_tp_group_get_members (group);
		for (l = contacts; l; l = l->next) {
			tp_contact_list_group_member_added_cb (group, l->data,
							       NULL, 0, NULL,
							       list);
			g_object_unref (l->data);
		}
		g_list_free (contacts);
	} else {
		DEBUG ("Unknown handle type (%d) for contact list channel",
			handle_type);
	}

OUT:
	g_object_unref (group);
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
	EmpathyTpContactListPriv *priv = GET_PRIV (list);

	if (!suppress_handler && priv->ready) {
		tp_contact_list_add_channel (EMPATHY_TP_CONTACT_LIST (list),
					     object_path, channel_type,
					     handle_type, handle);
	}
}

static void
tp_contact_list_list_channels_cb (TpConnection    *connection,
				  const GPtrArray *channels,
				  const GError    *error,
				  gpointer         user_data,
				  GObject         *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	guint                     i;

	if (error) {
		DEBUG ("Failed to get list of open channels: %s",
			error ? error->message : "No error given");
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

		tp_contact_list_add_channel (EMPATHY_TP_CONTACT_LIST (list),
					     object_path, channel_type,
					     handle_type, handle);
	}

	priv->ready = TRUE;
}

static void
tp_contact_list_finalize (GObject *object)
{
	EmpathyTpContactListPriv *priv;
	EmpathyTpContactList     *list;

	list = EMPATHY_TP_CONTACT_LIST (object);
	priv = GET_PRIV (list);

	DEBUG ("finalize: %p", object);

	if (priv->subscribe) {
		g_object_unref (priv->subscribe);
	}
	if (priv->publish) {
		g_object_unref (priv->publish);
	}
	if (priv->account) {
		g_object_unref (priv->account);
	}
	if (priv->connection) {
		g_signal_handlers_disconnect_by_func (priv->connection,
						      tp_contact_list_invalidated_cb,
						      object);
		g_object_unref (priv->connection);
	}

	g_hash_table_destroy (priv->contacts_groups);
	g_list_foreach (priv->groups, (GFunc) g_object_unref, NULL);
	g_list_free (priv->groups);
	g_list_foreach (priv->members, (GFunc) g_object_unref, NULL);
	g_list_free (priv->members);
	g_list_foreach (priv->pendings, (GFunc) g_object_unref, NULL);
	g_list_free (priv->pendings);

	G_OBJECT_CLASS (empathy_tp_contact_list_parent_class)->finalize (object);
}

static void
tp_contact_list_ready_cb (EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);

	tp_cli_connection_call_list_channels (priv->connection, -1,
					      tp_contact_list_list_channels_cb,
					      NULL, NULL,
					      G_OBJECT (list));

	tp_cli_connection_connect_to_new_channel (priv->connection,
						  tp_contact_list_new_channel_cb,
						  NULL, NULL,
						  G_OBJECT (list), NULL);
}

static void
tp_contact_list_constructed (GObject *list)
{

	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	MissionControl           *mc;
	guint                     status;
	gboolean                  ready;
	McProfile                *profile;
	const gchar              *protocol_name;

	/* Get the connection. status==0 means CONNECTED */
	mc = empathy_mission_control_new ();
	status = mission_control_get_connection_status (mc, priv->account, NULL);
	g_return_if_fail (status == 0);
	priv->connection = mission_control_get_tpconnection (mc, priv->account, NULL);
	g_return_if_fail (priv->connection != NULL);
	g_object_unref (mc);

	g_signal_connect (priv->connection, "invalidated",
			  G_CALLBACK (tp_contact_list_invalidated_cb),
			  list);
	g_object_get (priv->connection, "connection-ready", &ready, NULL);
	if (ready) {
		tp_contact_list_ready_cb (EMPATHY_TP_CONTACT_LIST (list));
	} else {
		g_signal_connect_swapped (priv->connection, "notify::connection-ready",
					  G_CALLBACK (tp_contact_list_ready_cb),
					  list);
	}

	/* Check for protocols that does not support contact groups. We can
	 * put all contacts into a special group in that case.
	 * FIXME: Default group should be an information in the profile */
	profile = mc_account_get_profile (priv->account);
	protocol_name = mc_profile_get_protocol_name (profile);
	if (strcmp (protocol_name, "local-xmpp") == 0) {
		priv->protocol_group = _("People nearby");
	}
	g_object_unref (profile);
}

static void
tp_contact_list_get_property (GObject    *object,
			      guint       param_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (object);

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
tp_contact_list_set_property (GObject      *object,
			      guint         param_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (object);

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
empathy_tp_contact_list_class_init (EmpathyTpContactListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tp_contact_list_finalize;
	object_class->constructed = tp_contact_list_constructed;
	object_class->get_property = tp_contact_list_get_property;
	object_class->set_property = tp_contact_list_set_property;

	g_object_class_install_property (object_class,
					 PROP_ACCOUNT,
					 g_param_spec_object ("account",
							      "The Account",
							      "The account associated with the contact list",
							      MC_TYPE_ACCOUNT,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

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
empathy_tp_contact_list_init (EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);

	priv->contacts_groups = g_hash_table_new_full (g_direct_hash,
						       g_direct_equal,
						       (GDestroyNotify) g_object_unref,
						       (GDestroyNotify) tp_contact_list_group_list_free);
}

EmpathyTpContactList *
empathy_tp_contact_list_new (McAccount *account)
{
	return g_object_new (EMPATHY_TYPE_TP_CONTACT_LIST,
			     "account", account,
			     NULL);
}

McAccount *
empathy_tp_contact_list_get_account (EmpathyTpContactList *list)
{
	EmpathyTpContactListPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);

	return priv->account;
}

static void
tp_contact_list_add (EmpathyContactList *list,
		     EmpathyContact     *contact,
		     const gchar        *message)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);

	g_return_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list));

	empathy_tp_group_add_member (priv->subscribe, contact, message);
	if (g_list_find (priv->pendings, contact)) {
		empathy_tp_group_add_member (priv->publish, contact, message);		
	}
}

static void
tp_contact_list_remove (EmpathyContactList *list,
			EmpathyContact     *contact,
			const gchar        *message)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);

	g_return_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list));

	empathy_tp_group_remove_member (priv->subscribe, contact, message);
	empathy_tp_group_remove_member (priv->publish, contact, message);		
}

static GList *
tp_contact_list_get_members (EmpathyContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);

	g_list_foreach (priv->members, (GFunc) g_object_ref, NULL);
	return g_list_copy (priv->members);
}

static GList *
tp_contact_list_get_pendings (EmpathyContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);

	g_list_foreach (priv->pendings, (GFunc) g_object_ref, NULL);
	return g_list_copy (priv->pendings);
}

static GList *
tp_contact_list_get_all_groups (EmpathyContactList *list)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	GList                    *groups = NULL, *l;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);

	if (priv->protocol_group) {
		groups = g_list_prepend (groups, g_strdup (priv->protocol_group));
	}

	for (l = priv->groups; l; l = l->next) {
		const gchar *name;

		name = empathy_tp_group_get_name (l->data);
		groups = g_list_prepend (groups, g_strdup (name));
	}

	return groups;
}

static GList *
tp_contact_list_get_groups (EmpathyContactList *list,
			    EmpathyContact     *contact)
{
	EmpathyTpContactListPriv  *priv = GET_PRIV (list);
	GList                    **groups;
	GList                     *ret = NULL, *l;

	g_return_val_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list), NULL);

	if (priv->protocol_group) {
		ret = g_list_prepend (ret, g_strdup (priv->protocol_group));
	}

	groups = g_hash_table_lookup (priv->contacts_groups, contact);
	if (!groups) {
		return ret;
	}

	for (l = *groups; l; l = l->next) {
		ret = g_list_prepend (ret, g_strdup (l->data));
	}


	return ret;
}

static EmpathyTpGroup *
tp_contact_list_get_group (EmpathyTpContactList *list,
			   const gchar          *group)
{
	EmpathyTpContactListPriv *priv = GET_PRIV (list);
	EmpathyTpGroup           *tp_group;
	gchar                    *object_path;
	guint                     handle;
	GArray                   *handles;
	const char               *names[2] = {group, NULL};
	GError                   *error = NULL;

	tp_group = tp_contact_list_find_group (list, group);
	if (tp_group) {
		return tp_group;
	}

	DEBUG ("creating new group: %s", group);

	if (!tp_cli_connection_run_request_handles (priv->connection, -1,
						    TP_HANDLE_TYPE_GROUP,
						    names,
						    &handles,
						    &error, NULL)) {
		DEBUG ("Failed to RequestHandles: %s",
			error ? error->message : "No error given");
		g_clear_error (&error);
		return NULL;
	}
	handle = g_array_index (handles, guint, 0);
	g_array_free (handles, TRUE);

	if (!tp_cli_connection_run_request_channel (priv->connection, -1,
						    TP_IFACE_CHANNEL_TYPE_CONTACT_LIST,
						    TP_HANDLE_TYPE_GROUP,
						    handle,
						    TRUE,
						    &object_path,
						    &error, NULL)) {
		DEBUG ("Failed to RequestChannel: %s",
			error ? error->message : "No error given");
		g_clear_error (&error);
		return NULL;
	}

	tp_contact_list_add_channel (EMPATHY_TP_CONTACT_LIST (list),
				     object_path,
				     TP_IFACE_CHANNEL_TYPE_CONTACT_LIST,
				     TP_HANDLE_TYPE_GROUP, handle);

	g_free (object_path);

	return tp_contact_list_find_group (list, group);
}

static void
tp_contact_list_add_to_group (EmpathyContactList *list,
			      EmpathyContact     *contact,
			      const gchar        *group)
{
	EmpathyTpGroup *tp_group;

	g_return_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list));

	tp_group = tp_contact_list_get_group (EMPATHY_TP_CONTACT_LIST (list),
					      group);

	empathy_tp_group_add_member (tp_group, contact, "");
}

static void
tp_contact_list_remove_from_group (EmpathyContactList *list,
				   EmpathyContact     *contact,
				   const gchar        *group)
{
	EmpathyTpGroup *tp_group;

	g_return_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list));

	tp_group = tp_contact_list_find_group (EMPATHY_TP_CONTACT_LIST (list),
					       group);

	if (tp_group) {
		empathy_tp_group_remove_member (tp_group, contact, "");
	}
}

static void
tp_contact_list_rename_group (EmpathyContactList *list,
			      const gchar        *old_group,
			      const gchar        *new_group)
{
	EmpathyTpGroup *tp_group;
	GList          *members;

	g_return_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list));

	tp_group = tp_contact_list_find_group (EMPATHY_TP_CONTACT_LIST (list),
					       old_group);
	if (!tp_group) {
		return;
	}

	DEBUG ("rename group %s to %s", old_group, new_group);

	/* Remove all members from the old group */
	members = empathy_tp_group_get_members (tp_group);
	empathy_tp_group_remove_members (tp_group, members, "");
	empathy_tp_group_close (tp_group);

	/* Add all members to the new group */
	tp_group = tp_contact_list_get_group (EMPATHY_TP_CONTACT_LIST (list),
					      new_group);
	empathy_tp_group_add_members (tp_group, members, "");

	g_list_foreach (members, (GFunc) g_object_unref, NULL);
	g_list_free (members);
}

static void
tp_contact_list_remove_group (EmpathyContactList *list,
			      const gchar *group)
{
	EmpathyTpGroup *tp_group;
	GList	       *members;

	g_return_if_fail (EMPATHY_IS_TP_CONTACT_LIST (list));

	tp_group = tp_contact_list_find_group (EMPATHY_TP_CONTACT_LIST (list),
					       group);
	
	if (!tp_group) {
		return;
	}

	DEBUG ("remove group %s", group);

	/* Remove all members of the group */
	members = empathy_tp_group_get_members (tp_group);
	empathy_tp_group_remove_members (tp_group, members, "");
	empathy_tp_group_close (tp_group);

	g_list_foreach (members, (GFunc) g_object_unref, NULL);
	g_list_free (members);
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

