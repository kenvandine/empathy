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
#include <libtelepathy/tp-chan.h>

#include <libempathy/gossip-debug.h>
#include <libempathy/empathy-marshal.h>

#include "empathy-filter.h"

#define DEBUG_DOMAIN "EmpathyFilter"

static gboolean empathy_filter_handle_channel (EmpathyFilter  *filter,
					       const gchar    *bus_name,
					       const gchar    *connection,
					       const gchar    *channel_type,
					       const gchar    *channel,
					       guint           handle_type,
					       guint           handle,
					       guint           context_handle,
					       GError        **error);

#include "empathy-filter-glue.h"

enum {
	NEW_CHANNEL,
	PROCESS,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyFilter, empathy_filter, G_TYPE_OBJECT)

static void
empathy_filter_class_init (EmpathyFilterClass *klass)
{
	signals[NEW_CHANNEL] =
		g_signal_new ("new-channel",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__OBJECT_OBJECT_UINT,
			      G_TYPE_NONE,
			      3, TELEPATHY_CONN_TYPE, TELEPATHY_CHAN_TYPE, G_TYPE_UINT);

	signals[PROCESS] =
		g_signal_new ("process",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__UINT_BOOLEAN,
			      G_TYPE_NONE,
			      2, G_TYPE_UINT, G_TYPE_BOOLEAN);
}

static void
empathy_filter_init (EmpathyFilter *filter)
{
}

EmpathyFilter *
empathy_filter_new (void)
{
	static gboolean  initialized = FALSE;
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
				G_TYPE_STRING, "org.gnome.Empathy.Filter",
				G_TYPE_UINT, DBUS_NAME_FLAG_DO_NOT_QUEUE,
				G_TYPE_INVALID,
				G_TYPE_UINT, &result,
				G_TYPE_INVALID)) {
		gossip_debug (DEBUG_DOMAIN,
			      "Failed to request name: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);

		return NULL;
	}
	g_object_unref (proxy);

	filter = g_object_new (EMPATHY_TYPE_FILTER, NULL);
	dbus_g_connection_register_g_object (tp_get_bus (),
					     "/org/gnome/Empathy/Filter",
					     G_OBJECT (filter));

	return filter;
}

void
empathy_filter_process (EmpathyFilter *filter,
			guint          context_handle,
			gboolean       result)
{
	g_print ("hello\n");
	g_signal_emit (filter, signals[PROCESS], 0, context_handle, result);
}

static gboolean
empathy_filter_handle_channel (EmpathyFilter  *filter,
				 const gchar  *bus_name,
				 const gchar  *connection,
				 const gchar  *channel_type,
				 const gchar  *channel,
				 guint         handle_type,
				 guint         handle,
				 guint         context_handle,
				 GError      **error)
{
	TpChan *tp_chan;
	TpConn *tp_conn;

	tp_conn = tp_conn_new (tp_get_bus (),
			       bus_name,
			       connection);

	tp_chan = tp_chan_new (tp_get_bus(),
			       bus_name,
			       channel,
			       channel_type,
			       handle_type,
			       handle);
g_print ("new channel\n");
	g_signal_emit (filter, signals[NEW_CHANNEL], 0, tp_conn, tp_chan, context_handle);

	g_object_unref (tp_chan);
	g_object_unref (tp_conn);

	return TRUE;
}

