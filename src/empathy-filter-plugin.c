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

#include <glib.h>

#include <dbus/dbus-glib.h>

#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-conn.h>

#include <mission-control/mcd-dispatcher.h>
#include <mission-control/mcd-dispatcher-context.h>
#include <mission-control/mcd-channel.h>

#include <libempathy/empathy-marshal-main.c>

#include "empathy-filter-gen.h"

static void filter_plugin_text_channel      (McdDispatcherContext *ctx);
static void filter_plugin_handle_channel_cb (DBusGProxy           *proxy,
					     GError               *error,
					     McdDispatcherContext *ctx);
static void filter_plugin_process_cb        (DBusGProxy           *filter,
					     guint                 context_handle,
					     gboolean              result,
					     McdDispatcher        *dispatcher);

static McdFilter text_in_filters[] = {
	{filter_plugin_text_channel, MCD_FILTER_PRIORITY_USER},
	{NULL, 0}
};

static DBusGProxy *filter = NULL;
static GHashTable *contexts = NULL;
static guint n_contexts = 0;

void
mcd_filters_init (McdDispatcher *dispatcher)
{
	static gboolean initialized = FALSE;

	if (initialized) {
		return;
	}

	filter = dbus_g_proxy_new_for_name (tp_get_bus (),
					    "org.gnome.Empathy.Filter",
					    "/org/gnome/Empathy/Filter",
					    "org.gnome.Empathy.Filter");

	dbus_g_object_register_marshaller (
		empathy_marshal_VOID__UINT_BOOLEAN,
		G_TYPE_NONE,
		G_TYPE_UINT,
		G_TYPE_BOOLEAN,
		G_TYPE_INVALID);

	dbus_g_proxy_add_signal (filter, "Process",
				 G_TYPE_UINT,
				 G_TYPE_BOOLEAN,
				 G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (filter, "Process",
				     G_CALLBACK (filter_plugin_process_cb),
				     dispatcher,
				     NULL);
	contexts = g_hash_table_new_full (g_direct_hash,
					  g_direct_equal,
					  NULL,
					  (GDestroyNotify) g_object_unref);

	mcd_dispatcher_register_filters (dispatcher,
					 text_in_filters,
					 TELEPATHY_CHAN_IFACE_TEXT_QUARK,
					 MCD_FILTER_IN);

	initialized = TRUE;
}

static void
filter_plugin_text_channel (McdDispatcherContext *ctx)
{
	const TpConn *tp_conn;
	McdChannel   *channel;

	tp_conn = mcd_dispatcher_context_get_connection_object (ctx);
	channel = mcd_dispatcher_context_get_channel (ctx);

	n_contexts++;
	g_hash_table_insert (contexts,
			     GUINT_TO_POINTER (n_contexts),
			     ctx);

	empathy_filter_handle_channel_async (filter,
					     dbus_g_proxy_get_bus_name (DBUS_G_PROXY (tp_conn)),
					     dbus_g_proxy_get_path (DBUS_G_PROXY (tp_conn)),
					     mcd_channel_get_channel_type (channel),
					     mcd_channel_get_object_path (channel),
					     mcd_channel_get_handle_type (channel),
					     mcd_channel_get_handle (channel),
					     n_contexts,
					     (empathy_filter_handle_channel_reply) filter_plugin_handle_channel_cb,
					     ctx);
}

static void
filter_plugin_handle_channel_cb (DBusGProxy           *proxy,
				 GError               *error,
				 McdDispatcherContext *ctx)
{
}

static void
filter_plugin_process_cb (DBusGProxy    *filter,
			  guint          context_handle,
			  gboolean       result,
			  McdDispatcher *dispatcher)
{
	McdDispatcherContext *ctx;

	g_print ("****processing\n");
	ctx = g_hash_table_lookup (contexts, GUINT_TO_POINTER (context_handle));
	mcd_dispatcher_context_process (ctx, result);
}

