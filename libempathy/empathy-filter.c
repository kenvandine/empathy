/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-conn.h>

#include "empathy-filter.h"
#include "empathy-debug.h"
#include "empathy-utils.h"
#include "empathy-marshal.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_FILTER, EmpathyFilterPriv))

#define DEBUG_DOMAIN "EmpathyFilter"

struct _EmpathyFilterPriv {
	GHashTable *table;
};

static void     empathy_filter_class_init     (EmpathyFilterClass *klass);
static void     empathy_filter_init           (EmpathyFilter      *filter);
static void     filter_finalize               (GObject            *object);
static gboolean empathy_filter_filter_channel (EmpathyFilter      *filter,
					       const gchar        *bus_name,
					       const gchar        *connection,
					       const gchar        *channel_type,
					       const gchar        *channel,
					       guint               handle_type,
					       guint               handle,
					       guint               id,
					       GError            **error);

#include "empathy-filter-glue.h"

enum {
	PROCESS,
	NEW_CHANNEL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyFilter, empathy_filter, G_TYPE_OBJECT)

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
			      empathy_marshal_VOID__OBJECT_OBJECT,
			      G_TYPE_NONE,
			      2, TELEPATHY_CONN_TYPE, TELEPATHY_CHAN_TYPE);

	signals[PROCESS] =
		g_signal_new ("process",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__UINT_BOOLEAN,
			      G_TYPE_NONE,
			      2, G_TYPE_UINT, G_TYPE_BOOLEAN);

	g_type_class_add_private (object_class, sizeof (EmpathyFilterPriv));
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

static void
filter_finalize (GObject *object)
{
	EmpathyFilterPriv *priv;

	priv = GET_PRIV (object);

	g_hash_table_destroy (priv->table);
}

EmpathyFilter *
empathy_filter_new (const gchar *bus_name,
		    const gchar *object_path,
		    const gchar *channel_type,
		    guint        priority,
		    guint        flags)
{
	static gboolean  initialized = FALSE;
	MissionControl  *mc;
	EmpathyFilter   *filter;
	DBusGProxy      *proxy;
	guint            result;
	GError          *error = NULL;

	if (!initialized) {
		dbus_g_object_type_install_info (EMPATHY_TYPE_FILTER,
						 &dbus_glib_empathy_filter_object_info);
		initialized = TRUE;
	}

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
			TpChan        *tp_chan,
			gboolean       process)
{
	EmpathyFilterPriv *priv;
	guint              id;

	g_return_if_fail (EMPATHY_IS_FILTER (filter));
	g_return_if_fail (TELEPATHY_IS_CHAN (tp_chan));

	priv = GET_PRIV (filter);

	id = GPOINTER_TO_UINT (g_hash_table_lookup (priv->table, tp_chan));
	g_return_if_fail (id != 0);

	empathy_debug (DEBUG_DOMAIN, "Processing channel id %d: %s",
		       id, process ? "Yes" : "No");
	g_signal_emit (filter, signals[PROCESS], 0, id, process);
	g_hash_table_remove (priv->table, tp_chan);
}

static gboolean
empathy_filter_filter_channel (EmpathyFilter  *filter,
			       const gchar    *bus_name,
			       const gchar    *connection,
			       const gchar    *channel_type,
			       const gchar    *channel,
			       guint           handle_type,
			       guint           handle,
			       guint           id,
			       GError        **error)
{
	EmpathyFilterPriv *priv;
	TpChan            *tp_chan;
	TpConn            *tp_conn;

	priv = GET_PRIV (filter);

	tp_conn = tp_conn_new (tp_get_bus (),
			       bus_name,
			       connection);

	tp_chan = tp_chan_new (tp_get_bus(),
			       bus_name,
			       channel,
			       channel_type,
			       handle_type,
			       handle);

	g_hash_table_insert (priv->table, tp_chan, GUINT_TO_POINTER (id));

	empathy_debug (DEBUG_DOMAIN, "New channel to be filtred: %d", id);
	g_signal_emit (filter, signals[NEW_CHANNEL], 0, tp_conn, tp_chan);

	g_object_unref (tp_conn);

	return TRUE;
}

