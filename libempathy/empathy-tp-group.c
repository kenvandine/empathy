/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Xavier Claessens <xclaesse@gmail.com>
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

#include <dbus/dbus-glib.h>
#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-chan-iface-group-gen.h>
#include <libtelepathy/tp-constants.h>
#include <libtelepathy/tp-conn.h>

#include "empathy-debug.h"
#include "empathy-tp-group.h"
#include "empathy-marshal.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_TP_GROUP, EmpathyTpGroupPriv))

#define DEBUG_DOMAIN "TpGroup"

struct _EmpathyTpGroupPriv {
	DBusGProxy *group_iface;
	TpConn     *tp_conn;
	TpChan     *tp_chan;
	gchar      *group_name;
};

static void empathy_tp_group_class_init (EmpathyTpGroupClass *klass);
static void empathy_tp_group_init       (EmpathyTpGroup      *group);
static void tp_group_finalize           (GObject             *object);
static void tp_group_destroy_cb         (DBusGProxy          *proxy,
					 EmpathyTpGroup      *group);
static void tp_group_members_changed_cb (DBusGProxy          *group_iface,
					 gchar               *message,
					 GArray              *added,
					 GArray              *removed,
					 GArray              *local_pending,
					 GArray              *remote_pending,
					 guint                actor,
					 guint                reason,
					 EmpathyTpGroup      *group);

enum {
	MEMBERS_ADDED,
	MEMBERS_REMOVED,
	LOCAL_PENDING,
	REMOTE_PENDING,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyTpGroup, empathy_tp_group, G_TYPE_OBJECT);

static void
empathy_tp_group_class_init (EmpathyTpGroupClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tp_group_finalize;

	signals[MEMBERS_ADDED] =
		g_signal_new ("members-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__POINTER_UINT_UINT_STRING,
			      G_TYPE_NONE,
			      4, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING);

	signals[MEMBERS_REMOVED] =
		g_signal_new ("members-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__POINTER_UINT_UINT_STRING,
			      G_TYPE_NONE,
			      4, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING);

	signals[LOCAL_PENDING] =
		g_signal_new ("local-pending",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__POINTER_UINT_UINT_STRING,
			      G_TYPE_NONE,
			      4, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING);

	signals[REMOTE_PENDING] =
		g_signal_new ("remote-pending",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__POINTER_UINT_UINT_STRING,
			      G_TYPE_NONE,
			      4, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (EmpathyTpGroupPriv));
}

static void
empathy_tp_group_init (EmpathyTpGroup *group)
{
}

static void
tp_group_finalize (GObject *object)
{
	EmpathyTpGroupPriv *priv;

	priv = GET_PRIV (object);

	if (priv->group_iface) {
		g_signal_handlers_disconnect_by_func (priv->group_iface,
						      tp_group_destroy_cb,
						      object);
		dbus_g_proxy_disconnect_signal (priv->group_iface, "MembersChanged",
						G_CALLBACK (tp_group_members_changed_cb),
						object);
		g_object_unref (priv->group_iface);
	}

	if (priv->tp_conn) {
		g_object_unref (priv->tp_conn);
	}

	if (priv->tp_chan) {
		g_object_unref (priv->tp_chan);
	}

	g_free (priv->group_name);

	G_OBJECT_CLASS (empathy_tp_group_parent_class)->finalize (object);
}

EmpathyTpGroup *
empathy_tp_group_new (TpChan *tp_chan,
		      TpConn *tp_conn)
{
	EmpathyTpGroup     *group;
	EmpathyTpGroupPriv *priv;
	DBusGProxy         *group_iface;

	g_return_val_if_fail (TELEPATHY_IS_CHAN (tp_chan), NULL);
	g_return_val_if_fail (TELEPATHY_IS_CONN (tp_conn), NULL);

	group_iface = tp_chan_get_interface (tp_chan,
					     TELEPATHY_CHAN_IFACE_GROUP_QUARK);
	g_return_val_if_fail (group_iface != NULL, NULL);

	group = g_object_new (EMPATHY_TYPE_TP_GROUP, NULL);
	priv = GET_PRIV (group);

	priv->tp_conn = g_object_ref (tp_conn);
	priv->tp_chan = g_object_ref (tp_chan);
	priv->group_iface = g_object_ref (group_iface);

	dbus_g_proxy_connect_signal (priv->group_iface, "MembersChanged",
				     G_CALLBACK (tp_group_members_changed_cb),
				     group, NULL);
	g_signal_connect (group_iface, "destroy",
			  G_CALLBACK (tp_group_destroy_cb),
			  group);

	return group;
}

void
empathy_tp_group_add_members (EmpathyTpGroup *group,
			      GArray         *handles,
			      const gchar    *message)
{
	EmpathyTpGroupPriv *priv;
	GError             *error = NULL;

	g_return_if_fail (EMPATHY_IS_TP_GROUP (group));
	g_return_if_fail (handles != NULL);

	priv = GET_PRIV (group);

	if (!tp_chan_iface_group_add_members (priv->group_iface,
					      handles,
					      message,
					      &error)) {
		empathy_debug (DEBUG_DOMAIN,
			      "Failed to add members: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}
}

void
empathy_tp_group_add_member (EmpathyTpGroup *group,
			     guint           handle,
			     const gchar    *message)
{
	GArray *handles;

	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);

	empathy_tp_group_add_members (group, handles, message);

	g_array_free (handles, TRUE);
}

void
empathy_tp_group_remove_members (EmpathyTpGroup *group,
				 GArray         *handles,
				 const gchar    *message)
{
	EmpathyTpGroupPriv *priv;
	GError             *error = NULL;

	g_return_if_fail (EMPATHY_IS_TP_GROUP (group));

	priv = GET_PRIV (group);

	if (!tp_chan_iface_group_remove_members (priv->group_iface,
						 handles,
						 message,
						 &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "Failed to remove members: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}
}

void
empathy_tp_group_remove_member (EmpathyTpGroup *group,
				guint           handle,
				const gchar    *message)
{
	GArray *handles;

	g_return_if_fail (EMPATHY_IS_TP_GROUP (group));

	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);

	empathy_tp_group_remove_members (group, handles, message);

	g_array_free (handles, TRUE);
}

GArray *
empathy_tp_group_get_members (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv;
	GArray             *members;
	GError             *error = NULL;

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), NULL);

	priv = GET_PRIV (group);

	if (!tp_chan_iface_group_get_members (priv->group_iface,
					      &members,
					      &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "Couldn't get members: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		return NULL;
	}

	return members;
}

void
empathy_tp_group_get_all_members (EmpathyTpGroup  *group,
				  GArray         **members,
				  GArray         **local_pending,
				  GArray         **remote_pending)
{
	EmpathyTpGroupPriv *priv;
	GError             *error = NULL;

	g_return_if_fail (EMPATHY_IS_TP_GROUP (group));

	priv = GET_PRIV (group);

	if (!tp_chan_iface_group_get_all_members (priv->group_iface,
						  members,
						  local_pending,
						  remote_pending,
						  &error)) {
		empathy_debug (DEBUG_DOMAIN,
			      "Couldn't get all members: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}
}

GList *
empathy_tp_group_get_local_pending_members_with_info (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv;
	GPtrArray          *array;
	guint               i;
	GList              *infos = NULL;
	GError             *error = NULL;

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), NULL);

	priv = GET_PRIV (group);

	if (!tp_chan_iface_group_get_local_pending_members_with_info (priv->group_iface,
								      &array,
								      &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "GetLocalPendingMembersWithInfo failed: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);

		return NULL;
	}

	if (!array) {
		/* This happens with butterfly because
		 * GetLocalPendingMembersWithInfo is not 
		 * implemented */
		return NULL;
	}

	for (i = 0; array->len > i; i++) {
		GValueArray        *pending_struct;
		EmpathyTpGroupInfo *info;
		const gchar        *message;

		info = g_slice_new (EmpathyTpGroupInfo);

		pending_struct = g_ptr_array_index (array, i);
		info->member = g_value_get_uint (g_value_array_get_nth (pending_struct, 0));
		info->actor = g_value_get_uint (g_value_array_get_nth (pending_struct, 1));
		info->reason = g_value_get_uint (g_value_array_get_nth (pending_struct, 2));
		message = g_value_get_string (g_value_array_get_nth (pending_struct, 3));
		info->message = g_strdup (message);
		g_value_array_free (pending_struct);

		infos = g_list_prepend (infos, info);
	}
	g_ptr_array_free (array, TRUE);

	return infos;
}

void
empathy_tp_group_info_list_free (GList *infos)
{
	GList *l;

	for (l = infos; l; l = l->next) {
		EmpathyTpGroupInfo *info;

		info = l->data;

		g_free (info->message);
		g_slice_free (EmpathyTpGroupInfo, info);
	}
	g_list_free (infos);
}


static void
tp_group_destroy_cb (DBusGProxy     *proxy,
		     EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv;

	priv = GET_PRIV (group);

	g_object_unref (priv->group_iface);
	g_object_unref (priv->tp_conn);
	g_object_unref (priv->tp_chan);
	priv->group_iface = NULL;
	priv->tp_chan = NULL;
	priv->tp_conn = NULL;
}

static void
tp_group_members_changed_cb (DBusGProxy     *group_iface,
			     gchar          *message,
			     GArray         *added,
			     GArray         *removed,
			     GArray         *local_pending,
			     GArray         *remote_pending,
			     guint           actor,
			     guint           reason,
			     EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv;

	priv = GET_PRIV (group);

	/* emit signals */
	if (added->len > 0) {
		g_signal_emit (group, signals[MEMBERS_ADDED], 0, 
		               added, actor, reason, message);
	}
	if (removed->len > 0) {
		g_signal_emit (group, signals[MEMBERS_REMOVED], 0, 
		               removed, actor, reason, message);
	}
	if (local_pending->len > 0) {
		g_signal_emit (group, signals[LOCAL_PENDING], 0,
		               local_pending, actor, reason, message);
	}
	if (remote_pending->len > 0) {
		g_signal_emit (group, signals[REMOTE_PENDING], 0,
		               remote_pending, actor, reason, message);
	}
}

const gchar *
empathy_tp_group_get_name (EmpathyTpGroup *group)
{
	TelepathyHandleType  handle_type;
	guint                channel_handle;
	GArray              *group_handles;
	gchar              **group_names;
	GError              *error = NULL;

	EmpathyTpGroupPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), NULL);

	priv = GET_PRIV (group);

	/* Lazy initialisation */
	if (priv->group_name) {
		return priv->group_name;
	}

	if (!tp_chan_get_handle (DBUS_G_PROXY (priv->tp_chan),
				 &handle_type,
				 &channel_handle,
				 &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "Couldn't retreive channel handle for group: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		return NULL;
	}

	group_handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (group_handles, channel_handle);
	if (!tp_conn_inspect_handles (DBUS_G_PROXY (priv->tp_conn),
				      handle_type,
				      group_handles,
				      &group_names,
				      &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "Couldn't get group name: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		g_array_free (group_handles, TRUE);
		return NULL;
	}

	priv->group_name = *group_names;
	g_array_free (group_handles, TRUE);
	g_free (group_names);

	return priv->group_name;
}

guint 
empathy_tp_group_get_self_handle (EmpathyTpGroup *group)
{
	EmpathyTpGroupPriv *priv;
	guint               handle;
	GError             *error = NULL;

	g_return_val_if_fail (EMPATHY_IS_TP_GROUP (group), 0 );

	priv = GET_PRIV (group);

	if (!tp_chan_iface_group_get_self_handle (priv->group_iface, &handle, &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "Failed to get self handle: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		return 0;
	}

	return handle;
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
			    guint           handle)
{
	GArray   *members;
	guint     i;
	gboolean  found = FALSE;

	members = empathy_tp_group_get_members (group);
	for (i = 0; i < members->len; i++) {
		if (g_array_index (members, guint, i) == handle) {
			found = TRUE;
			break;
		}
	}
	g_array_free (members, TRUE);
	
	return found;
}

