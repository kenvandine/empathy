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

#include <telepathy-glib/channel.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/util.h>

#include <libmissioncontrol/mission-control.h>

#include "empathy-tp-roomlist.h"
#include "empathy-chatroom.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_TP
#include "empathy-debug.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyTpRoomlist)
typedef struct {
	TpConnection *connection;
	TpChannel    *channel;
	McAccount    *account;
	gboolean      is_listing;
} EmpathyTpRoomlistPriv;

enum {
	NEW_ROOM,
	DESTROY,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_CONNECTION,
	PROP_IS_LISTING,
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyTpRoomlist, empathy_tp_roomlist, G_TYPE_OBJECT);

static void
tp_roomlist_listing_cb (TpChannel *channel,
			gboolean   listing,
			gpointer   user_data,
			GObject   *list)
{
	EmpathyTpRoomlistPriv *priv = GET_PRIV (list);

	DEBUG ("Listing: %s", listing ? "Yes" : "No");
	priv->is_listing = listing;
	g_object_notify (list, "is-listing");
}

static void
tp_roomlist_chatrooms_free (gpointer data)
{
	GSList *chatrooms = data;

	g_slist_foreach (chatrooms, (GFunc) g_object_unref, NULL);
	g_slist_free (chatrooms);
}

static void
tp_roomlist_inspect_handles_cb (TpConnection *connection,
				const gchar **names,
				const GError *error,
				gpointer      user_data,
				GObject      *list)
{
	GSList *chatrooms = user_data;

	if (error != NULL) {
		DEBUG ("Error: %s", error->message);
		return;
	}

	while (*names != NULL) {
		EmpathyChatroom *chatroom = chatrooms->data;

		empathy_chatroom_set_room (chatroom, *names);
		g_signal_emit (list, signals[NEW_ROOM], 0, chatroom);

		names++;
		chatrooms = chatrooms->next;
	}
}

static void
tp_roomlist_got_rooms_cb (TpChannel       *channel,
			  const GPtrArray *rooms,
			  gpointer         user_data,
			  GObject         *list)
{
	EmpathyTpRoomlistPriv *priv = GET_PRIV (list);
	EmpathyChatroom       *chatroom;
	guint                  i;
	GArray                *handles = NULL;
	GSList                *chatrooms = NULL;

	for (i = 0; i < rooms->len; i++) {
		const GValue *room_name_value;
		const GValue *handle_name_value;
		GValueArray  *room_struct;
		guint         handle;
		const gchar  *channel_type;
		GHashTable   *info;

		/* Get information */
		room_struct = g_ptr_array_index (rooms, i);
		handle = g_value_get_uint (g_value_array_get_nth (room_struct, 0));
		channel_type = g_value_get_string (g_value_array_get_nth (room_struct, 1));
		info = g_value_get_boxed (g_value_array_get_nth (room_struct, 2));
		room_name_value = g_hash_table_lookup (info, "name");
		handle_name_value = g_hash_table_lookup (info, "handle-name");

		if (tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_TEXT)) {
			continue;
		}

		chatroom = empathy_chatroom_new (priv->account);

		if (room_name_value != NULL) {
			empathy_chatroom_set_name (chatroom,
						   g_value_get_string (room_name_value));
		}

		if (handle_name_value != NULL) {
			empathy_chatroom_set_room (chatroom,
						   g_value_get_string (handle_name_value));

			/* We have the room ID, we can directly emit it */
			g_signal_emit (list, signals[NEW_ROOM], 0, chatroom);
			g_object_unref (chatroom);
		} else {
			/* We don't have the room ID, we'll inspect all handles
			 * at once and then emit rooms */
			if (handles == NULL) {
				handles = g_array_new (FALSE, FALSE, sizeof (guint));
			}

			g_array_append_val (handles, handle);
			chatrooms = g_slist_prepend (chatrooms, chatroom);
		}
	}

	if (handles != NULL) {
		chatrooms = g_slist_reverse (chatrooms);
		tp_cli_connection_call_inspect_handles (priv->connection, -1,
						       TP_HANDLE_TYPE_ROOM,
						       handles,
						       tp_roomlist_inspect_handles_cb,
						       chatrooms,
						       tp_roomlist_chatrooms_free,
						       list);
		g_array_free (handles, TRUE);
	}
}

static void
tp_roomlist_get_listing_rooms_cb (TpChannel    *channel,
				  gboolean      is_listing,
				  const GError *error,
				  gpointer      user_data,
				  GObject      *list)
{
	EmpathyTpRoomlistPriv *priv = GET_PRIV (list);

	if (error) {
		DEBUG ("Error geting listing rooms: %s", error->message);
		return;
	}

	priv->is_listing = is_listing;
	g_object_notify (list, "is-listing");
}

static void
tp_roomlist_invalidated_cb (TpChannel         *channel,
			    guint              domain,
			    gint               code,
			    gchar             *message,
			    EmpathyTpRoomlist *list)
{
	DEBUG ("Channel invalidated: %s", message);
	g_signal_emit (list, signals[DESTROY], 0);
}

static void
tp_roomlist_request_channel_cb (TpConnection *connection,
				const gchar  *object_path,
				const GError *error,
				gpointer      user_data,
				GObject      *list)
{
	EmpathyTpRoomlistPriv *priv = GET_PRIV (list);

	if (error) {
		DEBUG ("Error requesting channel: %s", error->message);
		return;
	}

	priv->channel = tp_channel_new (priv->connection, object_path,
					TP_IFACE_CHANNEL_TYPE_ROOM_LIST,
					TP_HANDLE_TYPE_NONE,
					0, NULL);
	tp_channel_run_until_ready (priv->channel, NULL, NULL);

	g_signal_connect (priv->channel, "invalidated",
			  G_CALLBACK (tp_roomlist_invalidated_cb),
			  list);

	tp_cli_channel_type_room_list_connect_to_listing_rooms (priv->channel,
								tp_roomlist_listing_cb,
								NULL, NULL,
								G_OBJECT (list),
								NULL);
	tp_cli_channel_type_room_list_connect_to_got_rooms (priv->channel,
							    tp_roomlist_got_rooms_cb,
							    NULL, NULL,
							    G_OBJECT (list),
							    NULL);

	tp_cli_channel_type_room_list_call_get_listing_rooms (priv->channel, -1,
							      tp_roomlist_get_listing_rooms_cb,
							      NULL, NULL,
							      G_OBJECT (list));
}

static void
tp_roomlist_finalize (GObject *object)
{
	EmpathyTpRoomlistPriv *priv = GET_PRIV (object);

	if (priv->channel) {
		DEBUG ("Closing channel...");
		g_signal_handlers_disconnect_by_func (priv->channel,
						      tp_roomlist_invalidated_cb,
						      object);
		tp_cli_channel_call_close (priv->channel, -1,
					   NULL, NULL, NULL, NULL);
		g_object_unref (priv->channel);
	}

	if (priv->account) {
		g_object_unref (priv->account);
	}
	if (priv->connection) {
		g_object_unref (priv->connection);
	}

	G_OBJECT_CLASS (empathy_tp_roomlist_parent_class)->finalize (object);
}

static void
tp_roomlist_constructed (GObject *list)
{
	EmpathyTpRoomlistPriv *priv = GET_PRIV (list);
	MissionControl        *mc;

	mc = empathy_mission_control_dup_singleton ();
	priv->account = mission_control_get_account_for_tpconnection (mc,
								      priv->connection,
								      NULL);
	g_object_unref (mc);

	tp_cli_connection_call_request_channel (priv->connection, -1,
						TP_IFACE_CHANNEL_TYPE_ROOM_LIST,
						TP_HANDLE_TYPE_NONE,
						0,
						TRUE,
						tp_roomlist_request_channel_cb,
						NULL, NULL,
						list);
}

static void
tp_roomlist_get_property (GObject    *object,
			  guint       param_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
	EmpathyTpRoomlistPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_CONNECTION:
		g_value_set_object (value, priv->connection);
		break;
	case PROP_IS_LISTING:
		g_value_set_boolean (value, priv->is_listing);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
tp_roomlist_set_property (GObject      *object,
			  guint         param_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
	EmpathyTpRoomlistPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_CONNECTION:
		priv->connection = g_object_ref (g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
empathy_tp_roomlist_class_init (EmpathyTpRoomlistClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = tp_roomlist_finalize;
	object_class->constructed = tp_roomlist_constructed;
	object_class->get_property = tp_roomlist_get_property;
	object_class->set_property = tp_roomlist_set_property;

	g_object_class_install_property (object_class,
					 PROP_CONNECTION,
					 g_param_spec_object ("connection",
							      "The Connection",
							      "The connection on which it lists rooms",
							      TP_TYPE_CONNECTION,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
					 PROP_IS_LISTING,
					 g_param_spec_boolean ("is-listing",
							       "Is listing",
							       "Are we listing rooms",
							       FALSE,
							       G_PARAM_READABLE));

	signals[NEW_ROOM] =
		g_signal_new ("new-room",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, EMPATHY_TYPE_CHATROOM);

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
	EmpathyTpRoomlistPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (list,
		EMPATHY_TYPE_TP_ROOMLIST, EmpathyTpRoomlistPriv);

	list->priv = priv;
}

EmpathyTpRoomlist *
empathy_tp_roomlist_new (McAccount *account)
{
	EmpathyTpRoomlist *list;
	MissionControl    *mc;
	TpConnection      *connection;

	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

	mc = empathy_mission_control_dup_singleton ();
	connection = mission_control_get_tpconnection (mc, account, NULL);

	list = g_object_new (EMPATHY_TYPE_TP_ROOMLIST,
			     "connection", connection,
			     NULL);

	g_object_unref (mc);
	g_object_unref (connection);

	return list;
}

gboolean
empathy_tp_roomlist_is_listing (EmpathyTpRoomlist *list)
{
	EmpathyTpRoomlistPriv *priv = GET_PRIV (list);

	g_return_val_if_fail (EMPATHY_IS_TP_ROOMLIST (list), FALSE);

	return priv->is_listing;
}

void
empathy_tp_roomlist_start (EmpathyTpRoomlist *list)
{
	EmpathyTpRoomlistPriv *priv = GET_PRIV (list);

	g_return_if_fail (EMPATHY_IS_TP_ROOMLIST (list));
	g_return_if_fail (TP_IS_CHANNEL (priv->channel));

	tp_cli_channel_type_room_list_call_list_rooms (priv->channel, -1,
						       NULL, NULL, NULL, NULL);
}

void
empathy_tp_roomlist_stop (EmpathyTpRoomlist *list)
{
	EmpathyTpRoomlistPriv *priv = GET_PRIV (list);

	g_return_if_fail (EMPATHY_IS_TP_ROOMLIST (list));
	g_return_if_fail (TP_IS_CHANNEL (priv->channel));

	tp_cli_channel_type_room_list_call_stop_listing (priv->channel, -1,
							 NULL, NULL, NULL, NULL);
}

