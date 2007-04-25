/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Xavier Claessens <xclaesse@gmail.com>
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
 */

#include <config.h>

#include <dbus/dbus-glib.h>

#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-chan.h>

#include "empathy-chandler.h"
#include "gossip-debug.h"
#include "empathy-marshal.h"

#define DEBUG_DOMAIN "EmpathyChandler"

static gboolean empathy_chandler_handle_channel (EmpathyChandler *chandler,
						 const gchar     *bus_name,
						 const gchar     *connection,
						 const gchar     *channel_type,
						 const gchar     *channel,
						 guint            handle_type,
						 guint            handle,
						 GError         **error);

#include "empathy-chandler-glue.h"

enum {
	NEW_CHANNEL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyChandler, empathy_chandler, G_TYPE_OBJECT)

static void
empathy_chandler_class_init (EmpathyChandlerClass *klass)
{
	signals[NEW_CHANNEL] =
		g_signal_new ("new-channel",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__OBJECT_OBJECT,
			      G_TYPE_NONE,
			      2, TELEPATHY_CONN_TYPE, TELEPATHY_CHAN_TYPE);
}

static void
empathy_chandler_init (EmpathyChandler *chandler)
{
}

EmpathyChandler *
empathy_chandler_new (const gchar *bus_name,
		      const gchar *object_path)
{
	static gboolean  initialized = FALSE;
	EmpathyChandler *chandler;
	DBusGProxy      *proxy;
	guint            result;
	GError          *error = NULL;

	if (!initialized) {
		dbus_g_object_type_install_info (EMPATHY_TYPE_CHANDLER,
						 &dbus_glib_empathy_chandler_object_info);
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
		gossip_debug (DEBUG_DOMAIN,
			      "Failed to request name: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);

		return NULL;
	}
	g_object_unref (proxy);

	chandler = g_object_new (EMPATHY_TYPE_CHANDLER, NULL);
	dbus_g_connection_register_g_object (tp_get_bus (),
					     object_path,
					     G_OBJECT (chandler));

	return chandler;
}

static gboolean
empathy_chandler_handle_channel (EmpathyChandler  *chandler,
				 const gchar      *bus_name,
				 const gchar      *connection,
				 const gchar      *channel_type,
				 const gchar      *channel,
				 guint             handle_type,
				 guint             handle,
				 GError          **error)
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

	g_signal_emit (chandler, signals[NEW_CHANNEL], 0, tp_conn, tp_chan);

	g_object_unref (tp_chan);
	g_object_unref (tp_conn);

	return TRUE;
}

