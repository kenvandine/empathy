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

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/connection.h>

#include <extensions/extensions.h>
#include "empathy-filter.h"
#include "empathy-debug.h"
#include "empathy-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_FILTER, EmpathyFilterPriv))

#define DEBUG_DOMAIN "EmpathyFilter"

struct _EmpathyFilterPriv {
	GHashTable *table;
};

static void empathy_filter_class_init (EmpathyFilterClass *klass);
static void empathy_filter_init       (EmpathyFilter      *filter);
static void filter_iface_init         (EmpSvcFilterClass  *klass);

enum {
	NEW_CHANNEL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (EmpathyFilter, empathy_filter, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (EMP_TYPE_SVC_FILTER,
						filter_iface_init));

typedef struct {
	EmpathyFilter *filter;
	gchar         *bus_name;
	gchar         *connection;
	gchar         *channel_type;
	gchar         *channel;
	guint          handle_type;
	guint          handle;
	guint          id;
} IdleData;

static gboolean
filter_channel_idle_cb (gpointer data)
{
	IdleData            *idle_data = data;
	EmpathyFilterPriv   *priv = GET_PRIV (idle_data->filter);
	TpChannel           *chan;
	TpConnection        *conn;
	static TpDBusDaemon *daemon = NULL;

	if (!daemon) {
		daemon = tp_dbus_daemon_new (tp_get_bus ());
	}

	conn = tp_connection_new (daemon, idle_data->bus_name,
				  idle_data->connection, NULL);
	tp_connection_run_until_ready (conn, FALSE, NULL, NULL);
	chan = tp_channel_new (conn, idle_data->channel, idle_data->channel_type,
			       idle_data->handle_type, idle_data->handle, NULL);
	tp_channel_run_until_ready (chan, NULL, NULL);

	g_hash_table_insert (priv->table, chan, GUINT_TO_POINTER (idle_data->id));

	empathy_debug (DEBUG_DOMAIN, "New channel to be filtred: "
				     "type=%s handle=%d id=%d",
				     idle_data->channel_type, idle_data->handle,
				     idle_data->id);

	g_signal_emit (idle_data->filter, signals[NEW_CHANNEL], 0, chan);

	g_object_unref (conn);
	g_free (idle_data->bus_name);
	g_free (idle_data->connection);
	g_free (idle_data->channel_type);
	g_free (idle_data->channel);
	g_slice_free (IdleData, idle_data);

	return FALSE;
}

static void
my_filter_channel (EmpSvcFilter          *self,
		   const gchar           *bus_name,
		   const gchar           *connection,
		   const gchar           *channel_type,
		   const gchar           *channel,
		   guint                  handle_type,
		   guint                  handle,
		   guint                  id,
		   DBusGMethodInvocation *context)
{
	EmpathyFilter *filter = EMPATHY_FILTER (self);
	IdleData      *data;

	data = g_slice_new (IdleData);
	data->filter = filter;
	data->bus_name = g_strdup (bus_name);
	data->connection = g_strdup (connection);
	data->channel_type = g_strdup (channel_type);
	data->channel = g_strdup (channel);
	data->handle_type = handle_type;
	data->handle = handle;
	data->id = id;
	g_idle_add_full (G_PRIORITY_HIGH,
			 filter_channel_idle_cb,
			 data, NULL);

	emp_svc_filter_return_from_filter_channel (context);
}

static void
filter_finalize (GObject *object)
{
	EmpathyFilterPriv *priv;

	priv = GET_PRIV (object);

	g_hash_table_destroy (priv->table);
}

static void
empathy_filter_class_init (EmpathyFilterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = filter_finalize;

	signals[NEW_CHANNEL] =
		g_signal_new ("new-channel",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, TP_TYPE_CHANNEL);

	g_type_class_add_private (object_class, sizeof (EmpathyFilterPriv));
}

static void
filter_iface_init (EmpSvcFilterClass *klass)
{
#define IMPLEMENT(x) emp_svc_filter_implement_##x \
    (klass, my_##x)
  IMPLEMENT (filter_channel);
#undef IMPLEMENT
}

static void
empathy_filter_init (EmpathyFilter *filter)
{
	EmpathyFilterPriv *priv;

	priv = GET_PRIV (filter);

	priv->table = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					     (GDestroyNotify) g_object_unref,
					     NULL);
}

EmpathyFilter *
empathy_filter_new (const gchar *bus_name,
		    const gchar *object_path,
		    const gchar *channel_type,
		    guint        priority,
		    guint        flags)
{
	MissionControl  *mc;
	EmpathyFilter   *filter;
	DBusGProxy      *proxy;
	guint            result;
	GError          *error = NULL;

	proxy = dbus_g_proxy_new_for_name (tp_get_bus (),
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);

	if (!dbus_g_proxy_call (proxy, "RequestName", &error,
				G_TYPE_STRING, bus_name,
				G_TYPE_UINT, DBUS_NAME_FLAG_DO_NOT_QUEUE,
				G_TYPE_INVALID,
				G_TYPE_UINT, &result,
				G_TYPE_INVALID)) {
		empathy_debug (DEBUG_DOMAIN,
			       "Failed to request name: %s",
			       error ? error->message : "No error given");
		g_clear_error (&error);

		return NULL;
	}
	g_object_unref (proxy);

	filter = g_object_new (EMPATHY_TYPE_FILTER, NULL);
	dbus_g_connection_register_g_object (tp_get_bus (),
					     object_path,
					     G_OBJECT (filter));

	mc = empathy_mission_control_new ();

	mission_control_register_filter (mc,
					 bus_name,
					 object_path,
					 channel_type,
					 priority,
					 flags,
					 NULL);
	g_object_unref (mc);

	return filter;
}

void
empathy_filter_process (EmpathyFilter *filter,
			TpChannel     *channel,
			gboolean       process)
{
	EmpathyFilterPriv *priv;
	guint              id;

	g_return_if_fail (EMPATHY_IS_FILTER (filter));
	g_return_if_fail (TP_IS_CHANNEL (channel));

	priv = GET_PRIV (filter);

	id = GPOINTER_TO_UINT (g_hash_table_lookup (priv->table, channel));
	g_return_if_fail (id != 0);

	empathy_debug (DEBUG_DOMAIN, "Processing channel id %d: %s",
		       id, process ? "Yes" : "No");

	emp_svc_filter_emit_process (filter, id, process);

	g_hash_table_remove (priv->table, channel);
}

