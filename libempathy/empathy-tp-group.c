/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Xavier Claessens <xclaesse@gmail.com>
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

#include <dbus/dbus-glib.h>
#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-chan-gen.h>
#include <libtelepathy/tp-chan-iface-group-gen.h>
#include <libtelepathy/tp-constants.h>
#include <libtelepathy/tp-conn.h>

#include "empathy-tp-group.h"
#include "empathy-contact-factory.h"
#include "empathy-debug.h"
#include "empathy-utils.h"
#include "empathy-marshal.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_TP_GROUP, EmpathyTpGroupPriv))

#define DEBUG_DOMAIN "TpGroup"

struct _EmpathyTpGroupPriv {
	EmpathyContactFactory *factory;
	McAccount             *account;
	DBusGProxy            *group_iface;
	TpChan                *tp_chan;
	gchar                 *group_name;
	guint                  self_handle;

	GList                 *members;
	GList                 *local_pendings;
	GList                 *remote_pendings;
};

static void empathy_tp_group_class_init (EmpathyTpGroupClass *klass);
static void empathy_tp_group_init       (EmpathyTpGroup      *group);

enum {
	MEMBER_ADDED,
	MEMBER_REMOVED,
	LOCAL_PENDING,
	REMOTE_PENDING,
	DESTROY,
	LAST_SIGNAL
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

	if (contact && empathy_contact_get_handle (contact) == priv->self_handle) {
		empathy_contact_set_is_user (contact, TRUE);
	}

	return contact;
}

static GList *
tp_group_get_contacts (EmpathyTpGroup *group,
		       GArray         *handles)
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
tp_group_members_changed_cb (DBusGProxy     *group_iface,
			     const gchar    *message,
			     GArray         *added,
			     GArray         *removed,
			     GArray         *local_pending,
			     GArray         *remote_pending,
			     guint           actor,
			     guint           reason,
			     EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	EmpathyContact     *actor_contact = NULL;
	GList              *contacts, *l, *ll;

	actor_contact = tp_group_get_contact (group, actor);

	empathy_debug (DEBUG_DOMAIN, "Members changed for list %s:\n"
				     "  added-len=%d, current-len=%d\n"
				     "  removed-len=%d\n"
				     "  local-pending-len=%d, current-len=%d\n"
				     "  remote-pending-len=%d, current-len=%d",
		       empathy_tp_group_get_name (group),
		       added ? added->len : 0, g_list_length (priv->members),
		       removed ? removed->len : 0,
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

	empathy_debug (DEBUG_DOMAIN, "Members changed done for list %s:\n"
				     "  members-len=%d\n"
				     "  local-pendings-len=%d\n"
				     "  remote-pendings-len=%d",
		       empathy_tp_group_get_name (group),
		       g_list_length (priv->members),
		       g_list_length (priv->local_pendings),
		       g_list_length (priv->remote_pendings));
}

static void
tp_group_destroy_cb (TpChan         *tp_chan,
		     EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	empathy_debug (DEBUG_DOMAIN, "Account disconnected or CM crashed");

	g_object_unref (priv->tp_chan);
	priv->group_iface = NULL;
	priv->tp_chan = NULL;

	g_signal_emit (group, signals[DESTROY], 0);
}

static void tp_group_closed_cb (DBusGProxy     *proxy,
				EmpathyTpGroup *group);

static void
tp_group_disconnect (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	if (priv->group_iface) {
		dbus_g_proxy_disconnect_signal (priv->group_iface, "MembersChanged",
						G_CALLBACK (tp_group_members_changed_cb),
						group);
	}
	if (priv->tp_chan) {
		g_signal_handlers_disconnect_by_func (priv->tp_chan,
						      tp_group_destroy_cb,
						      group);
		dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (priv->tp_chan), "Closed",
						G_CALLBACK (tp_group_closed_cb),
						group);
	}
}

static void
tp_group_closed_cb (DBusGProxy     *proxy,
		    EmpathyTpGroup *group)
{
	tp_group_disconnect (group);
	tp_group_destroy_cb (TELEPATHY_CHAN (proxy), group);
}

static void
tp_group_get_members_cb (DBusGProxy *proxy,
			 GArray     *handles,
			 GError     *error,
			 gpointer    user_data)
{
	EmpathyTpGroup     *group = user_data;
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Failed to get members: %s",
			       error->message);
		return;
	}

	tp_group_members_changed_cb (priv->group_iface,
				     NULL,    /* message */
				     handles, /* added */
				     NULL,    /* removed */
				     NULL,    /* local_pending */
				     NULL,    /* remote_pending */
				     0,       /* actor */
				     0,       /* reason */
				     group);

	g_array_free (handles, TRUE);
}

static void
tp_group_get_local_pending_cb (DBusGProxy *proxy,
			       GPtrArray  *array,
			       GError     *error,
			       gpointer    user_data)
{
	EmpathyTpGroup     *group = user_data;
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	GArray             *handles;
	guint               i;
	
	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Failed to get local pendings: %s",
			       error->message);
		return;
	}

	handles = g_array_sized_new (FALSE, FALSE, sizeof (guint), 1);
	for (i = 0; array->len > i; i++) {
		GValueArray        *pending_struct;
		const gchar        *message;
		guint               member_handle;
		guint               actor_handle;
		guint               reason;

		pending_struct = g_ptr_array_index (array, i);
		member_handle = g_value_get_uint (g_value_array_get_nth (pending_struct, 0));
		actor_handle = g_value_get_uint (g_value_array_get_nth (pending_struct, 1));
		reason = g_value_get_uint (g_value_array_get_nth (pending_struct, 2));
		message = g_value_get_string (g_value_array_get_nth (pending_struct, 3));

		g_array_insert_val (handles, 0, member_handle);
		tp_group_members_changed_cb (priv->group_iface,
					     message,      /* message */
					     NULL,         /* added */
					     NULL,         /* removed */
					     handles,      /* local_pending */
					     NULL,         /* remote_pending */
					     actor_handle, /* actor */
					     reason,       /* reason */
					     group);

		g_value_array_free (pending_struct);
	}
	g_ptr_array_free (array, TRUE);
	g_array_free (handles, TRUE);
}

static void
tp_group_get_remote_pending_cb (DBusGProxy *proxy,
				GArray     *handles,
				GError     *error,
				gpointer    user_data)
{
	EmpathyTpGroup     *group = user_data;
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	
	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Failed to get remote pendings: %s",
			       error->message);
		return;
	}

	tp_group_members_changed_cb (priv->group_iface,
				     NULL,    /* message */
				     NULL,    /* added */
				     NULL,    /* removed */
				     NULL,    /* local_pending */
				     handles, /* remote_pending */
				     0,       /* actor */
				     0,       /* reason */
				     group);

	g_array_free (handles, TRUE);
}

static void
tp_group_finalize (GObject *object)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (object);

	tp_group_disconnect (EMPATHY_TP_GROUP (object));

	if (priv->tp_chan) {
		g_object_unref (priv->tp_chan);
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
empathy_tp_group_class_init (EmpathyTpGroupClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tp_group_finalize;

	signals[MEMBER_ADDED] =
		g_signal_new ("member-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__OBJECT_OBJECT_UINT_STRING,
			      G_TYPE_NONE,
			      4, EMPATHY_TYPE_CONTACT, EMPATHY_TYPE_CONTACT, G_TYPE_UINT, G_TYPE_STRING);

	signals[MEMBER_REMOVED] =
		g_signal_new ("member-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__OBJECT_OBJECT_UINT_STRING,
			      G_TYPE_NONE,
			      4, EMPATHY_TYPE_CONTACT, EMPATHY_TYPE_CONTACT, G_TYPE_UINT, G_TYPE_STRING);

	signals[LOCAL_PENDING] =
		g_signal_new ("local-pending",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__OBJECT_OBJECT_UINT_STRING,
			      G_TYPE_NONE,
			      4, EMPATHY_TYPE_CONTACT, EMPATHY_TYPE_CONTACT, G_TYPE_UINT, G_TYPE_STRING);

	signals[REMOTE_PENDING] =
		g_signal_new ("remote-pending",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__OBJECT_OBJECT_UINT_STRING,
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
}

EmpathyTpGroup *
empathy_tp_group_new (McAccount *account,
		      TpChan    *tp_chan)
{
	EmpathyTpGroup     *group;
	EmpathyTpGroupPriv *priv;
	DBusGProxy         *group_iface;
	GError             *error;

	g_return_val_if_fail (TELEPATHY_IS_CHAN (tp_chan), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

	group_iface = tp_chan_get_interface (tp_chan,
					     TELEPATHY_CHAN_IFACE_GROUP_QUARK);
	g_return_val_if_fail (group_iface != NULL, NULL);

	group = g_object_new (EMPATHY_TYPE_TP_GROUP, NULL);
	priv = GET_PRIV (group);

	priv->account = g_object_ref (account);
	priv->tp_chan = g_object_ref (tp_chan);
	priv->group_iface = group_iface;
	priv->factory = empathy_contact_factory_new ();

	if (!tp_chan_iface_group_get_self_handle (priv->group_iface,
						  &priv->self_handle,
						  &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "Failed to get self handle: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}

	dbus_g_proxy_connect_signal (priv->group_iface, "MembersChanged",
				     G_CALLBACK (tp_group_members_changed_cb),
				     group, NULL);
	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->tp_chan), "Closed",
				     G_CALLBACK (tp_group_closed_cb),
				     group, NULL);
	g_signal_connect (priv->tp_chan, "destroy",
			  G_CALLBACK (tp_group_destroy_cb),
			  group);

	tp_chan_iface_group_get_members_async (priv->group_iface,
					       tp_group_get_members_cb,
					       group);
	tp_chan_iface_group_get_local_pending_members_with_info_async (priv->group_iface,
								       tp_group_get_local_pending_cb,
								       group);
	tp_chan_iface_group_get_remote_pending_members_async (priv->group_iface,
							      tp_group_get_remote_pending_cb,
							      group);

	return group;
}

static void
tp_group_async_cb (DBusGProxy *proxy, GError *error, gpointer user_data)
{
	const gchar *msg = user_data;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "%s: %s", msg, error->message);
	}
}

void
empathy_tp_group_close  (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	tp_chan_close_async (DBUS_G_PROXY (priv->tp_chan),
			     tp_group_async_cb,
			     "Failed to close");
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

	handles = tp_group_get_handles (contacts);
	tp_chan_iface_group_add_members_async (priv->group_iface,
					       handles,
					       message,
					       tp_group_async_cb,
					       "Failed to add members");

	g_array_free (handles, TRUE);
}

void
empathy_tp_group_add_member (EmpathyTpGroup *group,
			     EmpathyContact *contact,
			     const gchar    *message)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	GArray             *handles;
	guint               handle;

	handle = empathy_contact_get_handle (contact);
	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);

	tp_chan_iface_group_add_members_async (priv->group_iface,
					       handles,
					       message,
					       tp_group_async_cb,
					       "Failed to add member");

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

	handles = tp_group_get_handles (contacts);
	tp_chan_iface_group_remove_members_async (priv->group_iface,
						  handles,
						  message,
						  tp_group_async_cb,
						  "Failed to remove members");

	g_array_free (handles, TRUE);
}

void
empathy_tp_group_remove_member (EmpathyTpGroup *group,
				EmpathyContact *contact,
				const gchar    *message)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	GArray             *handles;
	guint               handle;

	handle = empathy_contact_get_handle (contact);
	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);

	tp_chan_iface_group_remove_members_async (priv->group_iface,
						  handles,
						  message,
						  tp_group_async_cb,
						  "Failed to remove member");

	g_array_free (handles, TRUE);
}

GList *
empathy_tp_group_get_members (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	g_list_foreach (priv->members, (GFunc) g_object_ref, NULL);

	return g_list_copy (priv->members);
}

GList *
empathy_tp_group_get_local_pendings (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);
	GList              *pendings = NULL, *l;

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

	g_list_foreach (priv->remote_pendings, (GFunc) g_object_ref, NULL);

	return g_list_copy (priv->remote_pendings);
}

const gchar *
empathy_tp_group_get_name (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), NULL);

	priv = GET_PRIV (group);

	/* Lazy initialisation */
	if (!priv->group_name) {
		priv->group_name = empathy_inspect_channel (priv->account, priv->tp_chan);
	}

	return priv->group_name;
}

EmpathyContact *
empathy_tp_group_get_self_contact (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), NULL);

	return tp_group_get_contact (group, priv->self_handle);
}

const gchar *
empathy_tp_group_get_object_path (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), NULL);

	priv = GET_PRIV (group);

	return dbus_g_proxy_get_path (DBUS_G_PROXY (priv->tp_chan));
}

gboolean
empathy_tp_group_is_member (EmpathyTpGroup *group,
			    EmpathyContact *contact)
{
	EmpathyTpGroupPriv *priv = GET_PRIV (group);

	return g_list_find (priv->members, contact) != NULL;
}

