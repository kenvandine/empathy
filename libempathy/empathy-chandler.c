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
#include <telepathy-glib/channel.h>

#include <extensions/extensions.h>
#include "empathy-chandler.h"
#include "empathy-debug.h"

#define DEBUG_DOMAIN "EmpathyChandler"

static void chandler_iface_init (EmpSvcChandlerClass *klass);

enum {
	NEW_CHANNEL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (EmpathyChandler, empathy_chandler, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (EMP_TYPE_SVC_CHANDLER,
						chandler_iface_init));

static void
my_handle_channel (EmpSvcChandler        *self,
		   const gchar           *bus_name,
		   const gchar           *connection,
		   const gchar           *channel_type,
		   const gchar           *channel,
		   guint                  handle_type,
		   guint                  handle,
		   DBusGMethodInvocation *context)
{
	EmpathyChandler     *chandler = EMPATHY_CHANDLER (self);
	TpChannel           *chan;
	TpConnection        *conn;
	static TpDBusDaemon *daemon = NULL;

	if (!daemon) {
		daemon = tp_dbus_daemon_new (tp_get_bus ());
	}

	conn = tp_connection_new (daemon, bus_name, connection, NULL);
	chan = tp_channel_new (conn, channel, channel_type, handle_type, handle, NULL);
	tp_channel_run_until_ready (chan, NULL, NULL);

	empathy_debug (DEBUG_DOMAIN, "New channel to be handled: "
				     "type=%s handle=%d",
				     channel_type, handle);
	g_signal_emit (chandler, signals[NEW_CHANNEL], 0, chan);

	g_object_unref (chan);
	g_object_unref (conn);

	emp_svc_chandler_return_from_handle_channel (context);
}

static void
empathy_chandler_class_init (EmpathyChandlerClass *klass)
{
	signals[NEW_CHANNEL] =
		g_signal_new ("new-channel",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, TP_TYPE_CHANNEL);
}

static void
chandler_iface_init (EmpSvcChandlerClass *klass)
{
#define IMPLEMENT(x) emp_svc_chandler_implement_##x \
    (klass, my_##x)
  IMPLEMENT (handle_channel);
#undef IMPLEMENT
}

static void
empathy_chandler_init (EmpathyChandler *chandler)
{
}

EmpathyChandler *
empathy_chandler_new (const gchar *bus_name,
		      const gchar *object_path)
{
	EmpathyChandler *chandler;
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

	chandler = g_object_new (EMPATHY_TYPE_CHANDLER, NULL);
	dbus_g_connection_register_g_object (tp_get_bus (),
					     object_path,
					     G_OBJECT (chandler));

	return chandler;
}

