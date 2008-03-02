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

#include <string.h>

#include <libtelepathy/tp-chan-type-room-list-gen.h>
#include <libtelepathy/tp-helpers.h>
#include <libtelepathy/tp-conn.h>
#include <libtelepathy/tp-chan.h>

#include <libmissioncontrol/mission-control.h>

#include "empathy-tp-roomlist.h"
#include "empathy-chatroom.h"
#include "empathy-utils.h"
#include "empathy-debug.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_TP_ROOMLIST, EmpathyTpRoomlistPriv))

#define DEBUG_DOMAIN "TpRoomlist"

struct _EmpathyTpRoomlistPriv {
	McAccount  *account;
	TpChan     *tp_chan;
	DBusGProxy *roomlist_iface;
};

static void empathy_tp_roomlist_class_init (EmpathyTpRoomlistClass *klass);
static void empathy_tp_roomlist_init       (EmpathyTpRoomlist      *chat);
static void tp_roomlist_finalize           (GObject                *object);
static void tp_roomlist_destroy_cb         (TpChan                 *tp_chan,
					    EmpathyTpRoomlist      *list);
static void tp_roomlist_closed_cb          (TpChan                 *tp_chan,
					    EmpathyTpRoomlist      *list);
static void tp_roomlist_listing_cb         (DBusGProxy             *roomlist_iface,
					    gboolean                listing,
					    EmpathyTpRoomlist      *list);
static void tp_roomlist_got_rooms_cb       (DBusGProxy             *roomlist_iface,
					    GPtrArray              *room_list,
					    EmpathyTpRoomlist      *list);

enum {
	NEW_ROOM,
	LISTING,
	DESTROY,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyTpRoomlist, empathy_tp_roomlist, G_TYPE_OBJECT);

static void
empathy_tp_roomlist_class_init (EmpathyTpRoomlistClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tp_roomlist_finalize;

	signals[NEW_ROOM] =
		g_signal_new ("new-room",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, EMPATHY_TYPE_CHATROOM);

	signals[LISTING] =
		g_signal_new ("listing",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1, G_TYPE_BOOLEAN);

	signals[DESTROY] =
		g_signal_new ("destroy",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_type_class_add_private (object_class, sizeof (EmpathyTpRoomlistPriv));
}

static void
empathy_tp_roomlist_init (EmpathyTpRoomlist *list)
{
}

static void
tp_roomlist_finalize (GObject *object)
{
	EmpathyTpRoomlistPriv *priv;
	GError                *error = NULL;

	priv = GET_PRIV (object);

	if (priv->tp_chan) {
		g_signal_handlers_disconnect_by_func (priv->tp_chan,
						      tp_roomlist_destroy_cb,
						      object);

		empathy_debug (DEBUG_DOMAIN, "Closing channel...");
		if (!tp_chan_close (DBUS_G_PROXY (priv->tp_chan), &error)) {
			empathy_debug (DEBUG_DOMAIN, 
				      "Error closing roomlist channel: %s",
				      error ? error->message : "No error given");
			g_clear_error (&error);
		}
		g_object_unref (priv->tp_chan);
	}

	if (priv->account) {
		g_object_unref (priv->account);
	}

	G_OBJECT_CLASS (empathy_tp_roomlist_parent_class)->finalize (object);
}

EmpathyTpRoomlist *
empathy_tp_roomlist_new (McAccount *account)
{
	EmpathyTpRoomlist     *list;
	EmpathyTpRoomlistPriv *priv;
	TpConn                *tp_conn;
	MissionControl        *mc;
	const gchar           *bus_name;

	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

	list = g_object_new (EMPATHY_TYPE_TP_ROOMLIST, NULL);
	priv = GET_PRIV (list);

	mc = empathy_mission_control_new ();
	tp_conn = mission_control_get_connection (mc, account, NULL);
	g_object_unref (mc);

	bus_name = dbus_g_proxy_get_bus_name (DBUS_G_PROXY (tp_conn));
	priv->tp_chan = tp_conn_new_channel (tp_get_bus (),
					     tp_conn,
					     bus_name,
					     TP_IFACE_CHANNEL_TYPE_ROOM_LIST,
					     TP_HANDLE_TYPE_NONE,
					     0,
					     TRUE);
	g_object_unref (tp_conn);

	if (!priv->tp_chan) {
		empathy_debug (DEBUG_DOMAIN, "Failed to get roomlist channel");
		g_object_unref (list);
		return NULL;
	}

	priv->account = g_object_ref (account);
	priv->roomlist_iface = tp_chan_get_interface (priv->tp_chan,
						      TP_IFACE_QUARK_CHANNEL_TYPE_ROOM_LIST);

	g_signal_connect (priv->tp_chan, "destroy",
			  G_CALLBACK (tp_roomlist_destroy_cb),
			  list);
	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->tp_chan), "Closed",
				     G_CALLBACK (tp_roomlist_closed_cb),
				     list, NULL);
	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->roomlist_iface), "ListingRooms",
				     G_CALLBACK (tp_roomlist_listing_cb),
				     list, NULL);
	dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->roomlist_iface), "GotRooms",
				     G_CALLBACK (tp_roomlist_got_rooms_cb),
				     list, NULL);

	return list;
}

gboolean
empathy_tp_roomlist_is_listing (EmpathyTpRoomlist *list)
{
	EmpathyTpRoomlistPriv *priv;
	GError                *error = NULL;
	gboolean               listing = FALSE;

	g_return_val_if_fail (EMPATHY_IS_TP_ROOMLIST (list), FALSE);

	priv = GET_PRIV (list);

	if (!tp_chan_type_room_list_get_listing_rooms (priv->roomlist_iface,
						       &listing,
						       &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "Error GetListingRooms: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		return FALSE;
	}

	return listing;
}

void
empathy_tp_roomlist_start (EmpathyTpRoomlist *list)
{
	EmpathyTpRoomlistPriv *priv;
	GError                *error = NULL;

	g_return_if_fail (EMPATHY_IS_TP_ROOMLIST (list));

	priv = GET_PRIV (list);

	if (!tp_chan_type_room_list_list_rooms (priv->roomlist_iface, &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "Error ListRooms: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}
}

void
empathy_tp_roomlist_stop (EmpathyTpRoomlist *list)
{
	EmpathyTpRoomlistPriv *priv;
	GError                *error = NULL;

	g_return_if_fail (EMPATHY_IS_TP_ROOMLIST (list));

	priv = GET_PRIV (list);

	if (!tp_chan_type_room_list_stop_listing (priv->roomlist_iface, &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "Error StopListing: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
	}
}

static void
tp_roomlist_destroy_cb (TpChan            *tp_chan,
			EmpathyTpRoomlist *list)
{
	EmpathyTpRoomlistPriv *priv;

	priv = GET_PRIV (list);

	empathy_debug (DEBUG_DOMAIN, "Channel Closed or CM crashed");

	tp_roomlist_listing_cb (priv->roomlist_iface, FALSE, list);

	g_object_unref  (priv->tp_chan);
	priv->tp_chan = NULL;
	priv->roomlist_iface = NULL;

	g_signal_emit (list, signals[DESTROY], 0);
}

static void
tp_roomlist_closed_cb (TpChan            *tp_chan,
		       EmpathyTpRoomlist *list)
{
	EmpathyTpRoomlistPriv *priv;

	priv = GET_PRIV (list);

	/* The channel is closed, do just like if the proxy was destroyed */
	g_signal_handlers_disconnect_by_func (priv->tp_chan,
					      tp_roomlist_destroy_cb,
					      list);
	tp_roomlist_destroy_cb (priv->tp_chan, list);
}

static void
tp_roomlist_listing_cb (DBusGProxy        *roomlist_iface,
			gboolean           listing,
			EmpathyTpRoomlist *list)
{
	empathy_debug (DEBUG_DOMAIN, "Listing: %s", listing ? "Yes" : "No");
	g_signal_emit (list, signals[LISTING], 0, listing);
}

static void
tp_roomlist_got_rooms_cb (DBusGProxy        *roomlist_iface,
			  GPtrArray         *room_list,
			  EmpathyTpRoomlist *list)
{
	EmpathyTpRoomlistPriv *priv;
	guint                  i;

	priv = GET_PRIV (list);

	for (i = 0; i < room_list->len; i++) {
		EmpathyChatroom *chatroom;
		gchar           *room_id;
		const gchar     *room_name;
		const GValue    *room_name_value;
		GValueArray     *room_struct;
		guint            handle;
		const gchar     *channel_type;
		GHashTable      *info;

		/* Get information */
		room_struct = g_ptr_array_index (room_list, i);
		handle = g_value_get_uint (g_value_array_get_nth (room_struct, 0));
		channel_type = g_value_get_string (g_value_array_get_nth (room_struct, 1));
		info = g_value_get_boxed (g_value_array_get_nth (room_struct, 2));

		/* Create the chatroom */
		room_name_value = g_hash_table_lookup (info, "name");
		room_name = g_value_get_string (room_name_value);
		room_id = empathy_inspect_handle (priv->account,
						  handle,
						  TP_HANDLE_TYPE_ROOM);
		chatroom = empathy_chatroom_new_full (priv->account,
						      room_id,
						      room_name,
						      FALSE);

		/* Tells the world */
		g_signal_emit (list, signals[NEW_ROOM], 0, chatroom);

		g_object_unref (chatroom);
	}
}

