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

#include <glib/gi18n.h>

#include <telepathy-glib/enums.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/proxy-subclass.h>

#include <libmissioncontrol/mission-control.h>
#include <libmissioncontrol/mc-account.h>

#include <extensions/extensions.h>

#include "empathy-dispatcher.h"
#include "empathy-utils.h"
#include "empathy-tube-handler.h"
#include "empathy-contact-factory.h"
#include "empathy-tp-group.h"
#include "empathy-chatroom-manager.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_DISPATCHER
#include <libempathy/empathy-debug.h>

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyDispatcher)
typedef struct {
	GHashTable     *connections;
	gpointer        token;
	MissionControl *mc;
	GSList         *tubes;
  EmpathyChatroomManager *chatroom_mgr;
} EmpathyDispatcherPriv;

G_DEFINE_TYPE (EmpathyDispatcher, empathy_dispatcher, G_TYPE_OBJECT);

enum {
	DISPATCH_CHANNEL,
	FILTER_CHANNEL,
	FILTER_TUBE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static EmpathyDispatcher *dispatcher = NULL;

void
empathy_dispatcher_channel_process (EmpathyDispatcher *dispatcher,
				    TpChannel         *channel)
{
	g_signal_emit (dispatcher, signals[DISPATCH_CHANNEL], 0, channel);
}

typedef struct {
	EmpathyDispatcherTube  public;
	EmpathyContactFactory *factory;
	gchar                 *bus_name;
	gchar                 *object_path;
	guint                  ref_count;
	gboolean               handled;
} DispatcherTube;

GType
empathy_dispatcher_tube_get_type (void)
{
	static GType type_id = 0;

	if (!type_id) {
		type_id = g_boxed_type_register_static ("EmpathyDispatcherTube",
							(GBoxedCopyFunc) empathy_dispatcher_tube_ref,
							(GBoxedFreeFunc) empathy_dispatcher_tube_unref);
	}

	return type_id;
}

EmpathyDispatcherTube *
empathy_dispatcher_tube_ref (EmpathyDispatcherTube *data)
{
	DispatcherTube *tube = (DispatcherTube*) data;

	g_return_val_if_fail (tube != NULL, NULL);

	tube->ref_count++;

	return data;
}

void
empathy_dispatcher_tube_unref (EmpathyDispatcherTube *data)
{
	DispatcherTube *tube = (DispatcherTube*) data;

	g_return_if_fail (tube != NULL);

	if (--tube->ref_count == 0) {
		if (!tube->handled) {
			DEBUG ("Tube can't be handled, closing");
			tp_cli_channel_type_tubes_call_close_tube (tube->public.channel, -1,
								   tube->public.id,
								   NULL, NULL, NULL,
								   NULL);
		}

		g_free (tube->bus_name);
		g_free (tube->object_path);
		g_object_unref (tube->factory);
		g_object_unref (tube->public.channel);
		g_object_unref (tube->public.initiator);
		g_slice_free (DispatcherTube, tube);
	}
}

static void
dispatcher_tubes_handle_tube_cb (TpProxy      *channel,
				 const GError *error,
				 gpointer      user_data,
				 GObject      *dispatcher)
{
	DispatcherTube *tube = user_data;

	if (error) {
		DEBUG ("Error: %s", error->message);
	} else {
		tube->handled = TRUE;
	}
}

void
empathy_dispatcher_tube_process (EmpathyDispatcher     *dispatcher,
				 EmpathyDispatcherTube *user_data)
{
	DispatcherTube *tube = (DispatcherTube*) user_data;

	if (tube->public.activatable) {
		TpProxy *connection;
		TpProxy *thandler;
		gchar   *object_path;
		guint    handle_type;
		guint    handle;

		/* Create the proxy for the tube handler */
		thandler = g_object_new (TP_TYPE_PROXY,
					 "dbus-connection", tp_get_bus (),
					 "bus-name", tube->bus_name,
					 "object-path", tube->object_path,
					 NULL);
		tp_proxy_add_interface_by_id (thandler, EMP_IFACE_QUARK_TUBE_HANDLER);

		/* Give the tube to the handler */
		g_object_get (tube->public.channel,
			      "connection", &connection,
			      "object-path", &object_path,
			      "handle_type", &handle_type,
			      "handle", &handle,
			      NULL);

		DEBUG ("Dispatching tube");
		emp_cli_tube_handler_call_handle_tube (thandler, -1,
						       connection->bus_name,
						       connection->object_path,
						       object_path, handle_type,
						       handle, tube->public.id,
						       dispatcher_tubes_handle_tube_cb,
						       empathy_dispatcher_tube_ref (user_data),
						       (GDestroyNotify) empathy_dispatcher_tube_unref,
						       G_OBJECT (dispatcher));

		g_object_unref (thandler);
		g_object_unref (connection);
		g_free (object_path);
	}
}

static void
dispatcher_tubes_new_tube_cb (TpChannel   *channel,
			      guint        id,
			      guint        initiator,
			      guint        type,
			      const gchar *service,
			      GHashTable  *parameters,
			      guint        state,
			      gpointer     user_data,
			      GObject     *dispatcher)
{
	static TpDBusDaemon   *daemon = NULL;
	DispatcherTube        *tube;
	McAccount             *account;
	guint                  number;
	gchar                **names;
	gboolean               running = FALSE;
	GError                *error = NULL;

	/* Increase tube count */
	number = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (channel), "tube-count"));
	g_object_set_data (G_OBJECT (channel), "tube-count", GUINT_TO_POINTER (++number));
	DEBUG ("Increased tube count for channel %p: %d", channel, number);

	/* We dispatch only local pending tubes */
	if (state != TP_TUBE_STATE_LOCAL_PENDING) {
		return;
	}

	if (!daemon) {
		daemon = tp_dbus_daemon_new (tp_get_bus ());
	}

	account = empathy_channel_get_account (channel);
	tube = g_slice_new (DispatcherTube);
	tube->ref_count = 1;
	tube->handled = FALSE;
	tube->factory = empathy_contact_factory_new ();
	tube->bus_name = empathy_tube_handler_build_bus_name (type, service);
	tube->object_path = empathy_tube_handler_build_object_path (type, service);
	tube->public.activatable = FALSE;
	tube->public.id = id;
	tube->public.channel = g_object_ref (channel);
	tube->public.initiator = empathy_contact_factory_get_from_handle (tube->factory,
									  account,
									  initiator);
	g_object_unref (account);

	/* Check if that bus-name has an owner, if it has one that means the
	 * app is already running and we can directly give the channel. */
	tp_cli_dbus_daemon_run_name_has_owner (daemon, -1, tube->bus_name,
					       &running, NULL, NULL);
	if (running) {
		DEBUG ("Tube handler running");
		tube->public.activatable = TRUE;
		empathy_dispatcher_tube_process (EMPATHY_DISPATCHER (dispatcher),
						 (EmpathyDispatcherTube*) tube);
		empathy_dispatcher_tube_unref ((EmpathyDispatcherTube*) tube);
		return;
	}

	/* Check if that bus-name is activatable, if not that means the
	 * application needed to handle this tube isn't installed. */
	if (!tp_cli_dbus_daemon_run_list_activatable_names (daemon, -1,
							    &names, &error,
							    NULL)) {
		DEBUG ("Error listing activatable names: %s", error->message);
		g_clear_error (&error);
	} else {
		gchar **name;

		for (name = names; *name; name++) {
			if (!tp_strdiff (*name, tube->bus_name)) {
				tube->public.activatable = TRUE;
				break;
			}
		}
		g_strfreev (names);
	}

	g_signal_emit (dispatcher, signals[FILTER_TUBE], 0, tube);
	empathy_dispatcher_tube_unref ((EmpathyDispatcherTube*) tube);
}

static void
dispatcher_tubes_list_tubes_cb (TpChannel       *channel,
				const GPtrArray *tubes,
				const GError    *error,
				gpointer         user_data,
				GObject         *dispatcher)
{
	guint i;

	if (error) {
		DEBUG ("Error: %s", error->message);
		return;
	}

	for (i = 0; i < tubes->len; i++) {
		GValueArray *values;

		values = g_ptr_array_index (tubes, i);
		dispatcher_tubes_new_tube_cb (channel,
					      g_value_get_uint (g_value_array_get_nth (values, 0)),
					      g_value_get_uint (g_value_array_get_nth (values, 1)),
					      g_value_get_uint (g_value_array_get_nth (values, 2)),
					      g_value_get_string (g_value_array_get_nth (values, 3)),
					      g_value_get_boxed (g_value_array_get_nth (values, 4)),
					      g_value_get_uint (g_value_array_get_nth (values, 5)),
					      user_data, dispatcher);
	}
}

static void
dispatcher_tubes_channel_invalidated_cb (TpProxy           *proxy,
					 guint              domain,
					 gint               code,
					 gchar             *message,
					 EmpathyDispatcher *dispatcher)
{
	EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);

	DEBUG ("%s", message);

	priv->tubes = g_slist_remove (priv->tubes, proxy);
	g_object_unref (proxy);
}

static void
dispatcher_tubes_tube_closed_cb (TpChannel *channel,
				 guint      id,
				 gpointer   user_data,
				 GObject   *dispatcher)
{
	guint number;

	number = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (channel), "tube-count"));
	if (number == 1) {
		DEBUG ("No more tube, closing channel");
		tp_cli_channel_call_close (channel, -1, NULL, NULL, NULL, NULL);
	}
	else if (number > 1) {
		DEBUG ("Decrease tube count: %d", number);
		g_object_set_data (G_OBJECT (channel), "tube-count", GUINT_TO_POINTER (--number));
	}
}

static void
dispatcher_tubes_handle_channel (EmpathyDispatcher *dispatcher,
				 TpChannel         *channel)
{
	EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);

	DEBUG ("Called");

	priv->tubes = g_slist_prepend (priv->tubes, g_object_ref (channel));
	g_signal_connect (channel, "invalidated",
			  G_CALLBACK (dispatcher_tubes_channel_invalidated_cb),
			  dispatcher);

	tp_cli_channel_type_tubes_connect_to_tube_closed (channel,
							  dispatcher_tubes_tube_closed_cb,
							  NULL, NULL,
							  G_OBJECT (dispatcher), NULL);
	tp_cli_channel_type_tubes_connect_to_new_tube (channel,
						       dispatcher_tubes_new_tube_cb,
						       NULL, NULL,
						       G_OBJECT (dispatcher), NULL);
	tp_cli_channel_type_tubes_call_list_tubes (channel, -1,
						   dispatcher_tubes_list_tubes_cb,
						   NULL, NULL,
						   G_OBJECT (dispatcher));
}

static void
dispatcher_connection_invalidated_cb (TpConnection  *connection,
				      guint          domain,
				      gint           code,
				      gchar         *message,
				      EmpathyDispatcher *dispatcher)
{
	EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
	GHashTableIter         iter;
	gpointer               key, value;

	DEBUG ("Error: %s", message);

	g_hash_table_iter_init (&iter, priv->connections);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		if (value == connection) {
			g_hash_table_remove (priv->connections, key);
			break;
		}
	}
}

typedef struct
{
  EmpathyDispatcher *self;
  EmpathyChatroom *chatroom;
} dispatcher_connection_invalidated_cb_ctx;

static dispatcher_connection_invalidated_cb_ctx *
dispatcher_connection_invalidated_cb_ctx_new (EmpathyDispatcher *dispatcher,
                                              EmpathyChatroom *chatroom)
{
  dispatcher_connection_invalidated_cb_ctx *ctx;

  ctx = g_slice_new (dispatcher_connection_invalidated_cb_ctx);

  ctx->self = g_object_ref (dispatcher);
  ctx->chatroom = g_object_ref (chatroom);

  return ctx;
}

static void
dispatcher_connection_invalidated_cb_ctx_free (
    dispatcher_connection_invalidated_cb_ctx *ctx)
{
  g_object_unref (ctx->self);
  g_object_unref (ctx->chatroom);

  g_slice_free (dispatcher_connection_invalidated_cb_ctx, ctx);
}

static void dispatcher_chatroom_invalidated_cb (
    TpProxy *channel,
    guint domain,
    gint code,
    gchar *message,
    dispatcher_connection_invalidated_cb_ctx *ctx)
{
  EmpathyDispatcherPriv *priv = GET_PRIV (ctx->self);
  gboolean favorite;

  g_object_get (ctx->chatroom, "favorite", &favorite, NULL);

  if (favorite)
    {
      /* Chatroom is in favorites so don't remove it from the manager */
      g_object_set (ctx->chatroom, "tp-channel", NULL, NULL);
    }
  else
    {
      empathy_chatroom_manager_remove (priv->chatroom_mgr, ctx->chatroom);
    }
}

static void
dispatcher_connection_new_channel_cb (TpConnection *connection,
				      const gchar  *object_path,
				      const gchar  *channel_type,
				      guint         handle_type,
				      guint         handle,
				      gboolean      suppress_handler,
				      gpointer      user_data,
				      GObject      *object)
{
	EmpathyDispatcher *dispatcher = EMPATHY_DISPATCHER (object);
  EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
	TpChannel         *channel;
	gpointer           had_channels;

	had_channels = g_object_get_data (G_OBJECT (connection), "had-channels");
	if (had_channels == NULL) {
		/* ListChannels didn't return yet, return to avoid duplicate
		 * dispatching */
		return;
	}

	channel = tp_channel_new (connection, object_path, channel_type,
				  handle_type, handle, NULL);
	tp_channel_run_until_ready (channel, NULL, NULL);

	if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_TUBES)) {
		dispatcher_tubes_handle_channel (dispatcher, channel);
	}

  if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_TEXT) &&
      handle_type == TP_HANDLE_TYPE_ROOM)
    {
      /* Add the chatroom to the chatroom manager */
      EmpathyChatroom *chatroom;
      GArray *handles;
      gchar **room_ids;
      MissionControl *mc;
      McAccount *account;
      dispatcher_connection_invalidated_cb_ctx *ctx;

      handles = g_array_sized_new (FALSE, FALSE, sizeof (TpHandle), 1);
      g_array_append_val (handles, handle);

      tp_cli_connection_run_inspect_handles (connection, -1,
          TP_HANDLE_TYPE_ROOM, handles, &room_ids, NULL, NULL);

      mc = empathy_mission_control_new ();
      account = mission_control_get_account_for_tpconnection (mc, connection,
          NULL);

      chatroom = empathy_chatroom_manager_find (priv->chatroom_mgr, account,
          room_ids[0]);
      if (chatroom == NULL)
        {
          chatroom = empathy_chatroom_new (account);
          empathy_chatroom_set_name (chatroom, room_ids[0]);
          empathy_chatroom_manager_add (priv->chatroom_mgr, chatroom);
        }
      else
        {
          g_object_ref (chatroom);
        }

      g_object_set (chatroom, "tp-channel", channel, NULL);

      ctx = dispatcher_connection_invalidated_cb_ctx_new (dispatcher, chatroom);

      g_signal_connect_data (channel, "invalidated",
          G_CALLBACK (dispatcher_chatroom_invalidated_cb), ctx,
          (GClosureNotify) dispatcher_connection_invalidated_cb_ctx_free, 0);

      g_free (room_ids[0]);
      g_free (room_ids);
      g_array_free (handles, TRUE);
      g_object_unref (mc);
      g_object_unref (account);
      g_object_unref (chatroom);
    }

	if (suppress_handler) {
		g_signal_emit (dispatcher, signals[DISPATCH_CHANNEL], 0, channel);
	} else {
		g_signal_emit (dispatcher, signals[FILTER_CHANNEL], 0, channel);
	}

	g_object_unref (channel);
}

static void
dispatcher_connection_list_channels_cb (TpConnection    *connection,
					const GPtrArray *channels,
					const GError    *error,
					gpointer         user_data,
					GObject         *dispatcher)
{
	guint i;

	if (error) {
		DEBUG ("Error: %s", error->message);
		return;
	}

	g_object_set_data (G_OBJECT (connection), "had-channels",
			   GUINT_TO_POINTER (1));

	for (i = 0; i < channels->len; i++) {
		GValueArray *values;

		values = g_ptr_array_index (channels, i);
		dispatcher_connection_new_channel_cb (connection,
			g_value_get_boxed (g_value_array_get_nth (values, 0)),
			g_value_get_string (g_value_array_get_nth (values, 1)),
			g_value_get_uint (g_value_array_get_nth (values, 2)),
			g_value_get_uint (g_value_array_get_nth (values, 3)),
			FALSE, user_data, dispatcher);
	}
}

static void
dispatcher_connection_advertise_capabilities_cb (TpConnection    *connection,
						 const GPtrArray *capabilities,
						 const GError    *error,
						 gpointer         user_data,
						 GObject         *dispatcher)
{
	if (error) {
		DEBUG ("Error: %s", error->message);
	}
}

static void
dispatcher_connection_ready_cb (TpConnection  *connection,
				const GError  *error,
				gpointer       dispatcher)
{
	GPtrArray   *capabilities;
	GType        cap_type;
	GValue       cap = {0, };
	const gchar *remove = NULL;

	if (error) {
		dispatcher_connection_invalidated_cb (connection,
						      error->domain,
						      error->code,
						      error->message,
						      dispatcher);
		return;
	}

	g_signal_connect (connection, "invalidated",
			  G_CALLBACK (dispatcher_connection_invalidated_cb),
			  dispatcher);
	tp_cli_connection_connect_to_new_channel (connection,
						  dispatcher_connection_new_channel_cb,
						  NULL, NULL,
						  G_OBJECT (dispatcher), NULL);
	tp_cli_connection_call_list_channels (connection, -1,
					      dispatcher_connection_list_channels_cb,
					      NULL, NULL,
					      G_OBJECT (dispatcher));

	/* Advertise VoIP capabilities */
	capabilities = g_ptr_array_sized_new (1);
	cap_type = dbus_g_type_get_struct ("GValueArray", G_TYPE_STRING,
					   G_TYPE_UINT, G_TYPE_INVALID);
	g_value_init (&cap, cap_type);
	g_value_take_boxed (&cap, dbus_g_type_specialized_construct (cap_type));
	dbus_g_type_struct_set (&cap,
				0, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
				1, TP_CHANNEL_MEDIA_CAPABILITY_AUDIO |
				   TP_CHANNEL_MEDIA_CAPABILITY_VIDEO |
				   TP_CHANNEL_MEDIA_CAPABILITY_NAT_TRAVERSAL_STUN  |
				   TP_CHANNEL_MEDIA_CAPABILITY_NAT_TRAVERSAL_GTALK_P2P,
				G_MAXUINT);
	g_ptr_array_add (capabilities, g_value_get_boxed (&cap));

	tp_cli_connection_interface_capabilities_call_advertise_capabilities (
		connection, -1,
		capabilities, &remove,
		dispatcher_connection_advertise_capabilities_cb,
		NULL, NULL, G_OBJECT (dispatcher));
	/* FIXME: Is that leaked? */
}

static void
dispatcher_update_account (EmpathyDispatcher *dispatcher,
			   McAccount         *account)
{
	EmpathyDispatcherPriv *priv = GET_PRIV (dispatcher);
	TpConnection          *connection;

	connection = g_hash_table_lookup (priv->connections, account);
	if (connection) {
		return;
	}

	connection = mission_control_get_tpconnection (priv->mc, account, NULL);
	if (!connection) {
		return;
	}

	g_hash_table_insert (priv->connections, g_object_ref (account), connection);
	tp_connection_call_when_ready (connection,
				       dispatcher_connection_ready_cb,
				       dispatcher);
}

static void
dispatcher_status_changed_cb (MissionControl           *mc,
			      TpConnectionStatus        status,
			      McPresence                presence,
			      TpConnectionStatusReason  reason,
			      const gchar              *unique_name,
			      EmpathyDispatcher            *dispatcher)
{
	McAccount *account;

	account = mc_account_lookup (unique_name);
	dispatcher_update_account (dispatcher, account);
	g_object_unref (account);
}

static void
dispatcher_finalize (GObject *object)
{
	EmpathyDispatcherPriv *priv = GET_PRIV (object);
	GSList                *l;

	empathy_disconnect_account_status_changed (priv->token);
	g_object_unref (priv->mc);

	for (l = priv->tubes; l; l = l->next) {
		g_signal_handlers_disconnect_by_func (l->data,
						      dispatcher_tubes_channel_invalidated_cb,
						      object);
		g_object_unref (l->data);
	}
	g_slist_free (priv->tubes);

	g_hash_table_destroy (priv->connections);

  g_object_unref (priv->chatroom_mgr);
}

static void
empathy_dispatcher_class_init (EmpathyDispatcherClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = dispatcher_finalize;

	signals[DISPATCH_CHANNEL] =
		g_signal_new ("dispatch-channel",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, TP_TYPE_CHANNEL);
	signals[FILTER_CHANNEL] =
		g_signal_new ("filter-channel",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1, TP_TYPE_CHANNEL);
	signals[FILTER_TUBE] =
		g_signal_new ("filter-tube",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOXED,
			      G_TYPE_NONE,
			      1, EMPATHY_TYPE_DISPATCHER_TUBE);

	g_type_class_add_private (object_class, sizeof (EmpathyDispatcherPriv));
}

static void
empathy_dispatcher_init (EmpathyDispatcher *dispatcher)
{
	GList                 *accounts, *l;
	EmpathyDispatcherPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (dispatcher,
		EMPATHY_TYPE_DISPATCHER, EmpathyDispatcherPriv);

	dispatcher->priv = priv;
	priv->mc = empathy_mission_control_new ();
	priv->token = empathy_connect_to_account_status_changed (priv->mc,
		G_CALLBACK (dispatcher_status_changed_cb),
		dispatcher, NULL);

	priv->connections = g_hash_table_new_full (empathy_account_hash,
						   empathy_account_equal,
						   g_object_unref,
						   g_object_unref);
	accounts = mc_accounts_list_by_enabled (TRUE);
	for (l = accounts; l; l = l->next) {
		dispatcher_update_account (dispatcher, l->data);
		g_object_unref (l->data);
	}
	g_list_free (accounts);

  priv->chatroom_mgr = empathy_chatroom_manager_new (NULL);
}

EmpathyDispatcher *
empathy_dispatcher_new (void)
{
	if (!dispatcher) {
		dispatcher = g_object_new (EMPATHY_TYPE_DISPATCHER, NULL);
		g_object_add_weak_pointer (G_OBJECT (dispatcher), (gpointer) &dispatcher);
	} else {
		g_object_ref (dispatcher);
	}

	return dispatcher;
}

typedef struct {
	const gchar *channel_type;
	guint        handle_type;
	guint        handle;
} DispatcherRequestData;

static void
dispatcher_request_channel_cb (TpConnection *connection,
			       const gchar  *object_path,
			       const GError *error,
			       gpointer      user_data,
			       GObject      *weak_object)
{
	DispatcherRequestData *data = (DispatcherRequestData*) user_data;

	if (error) {
		DEBUG ("Error: %s", error->message);
		return;
	}

	if (dispatcher) {
		TpChannel *channel;

		channel = tp_channel_new (connection, object_path,
					  data->channel_type,
					  data->handle_type,
					  data->handle, NULL);

		g_signal_emit (dispatcher, signals[DISPATCH_CHANNEL], 0, channel);
	}
}

void
empathy_dispatcher_call_with_contact (EmpathyContact *contact)
{
	MissionControl        *mc;
	McAccount             *account;
	TpConnection          *connection;
	gchar                 *object_path;
	TpChannel             *channel;
	EmpathyContactFactory *factory;
	EmpathyTpGroup        *group;
	EmpathyContact        *self_contact;
	GError                *error = NULL;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	mc = empathy_mission_control_new ();
	account = empathy_contact_get_account (contact);
	connection = mission_control_get_tpconnection (mc, account, NULL);
	tp_connection_run_until_ready (connection, FALSE, NULL, NULL);
	g_object_unref (mc);

	/* We abuse of suppress_handler, TRUE means OUTGOING. The channel
	 * will be catched in EmpathyFilter */
	if (!tp_cli_connection_run_request_channel (connection, -1,
						    TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
						    TP_HANDLE_TYPE_NONE,
						    0,
						    TRUE,
						    &object_path,
						    &error,
						    NULL)) {
		DEBUG ("Couldn't request channel: %s",
			error ? error->message : "No error given");
		g_clear_error (&error);
		g_object_unref (connection);
		return;
	}

	channel = tp_channel_new (connection,
				  object_path, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
				  TP_HANDLE_TYPE_NONE, 0, NULL);

	group = empathy_tp_group_new (channel);
	empathy_run_until_ready (group);

	factory = empathy_contact_factory_new ();
	self_contact = empathy_contact_factory_get_user (factory, account);
	empathy_contact_run_until_ready (self_contact,
					 EMPATHY_CONTACT_READY_HANDLE,
					 NULL);

	empathy_tp_group_add_member (group, contact, "");
	empathy_tp_group_add_member (group, self_contact, "");	

	g_object_unref (factory);
	g_object_unref (self_contact);
	g_object_unref (group);
	g_object_unref (connection);
	g_object_unref (channel);
	g_free (object_path);
}

void
empathy_dispatcher_call_with_contact_id (McAccount *account, const gchar *contact_id)
{
	EmpathyContactFactory *factory;
	EmpathyContact        *contact;

	factory = empathy_contact_factory_new ();
	contact = empathy_contact_factory_get_from_id (factory, account, contact_id);
	empathy_contact_run_until_ready (contact, EMPATHY_CONTACT_READY_HANDLE, NULL);

	empathy_dispatcher_call_with_contact (contact);

	g_object_unref (contact);
	g_object_unref (factory);
}

void
empathy_dispatcher_chat_with_contact (EmpathyContact  *contact)
{
	MissionControl        *mc;
	McAccount             *account;
	TpConnection          *connection;
	DispatcherRequestData *data;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	mc = empathy_mission_control_new ();
	account = empathy_contact_get_account (contact);
	connection = mission_control_get_tpconnection (mc, account, NULL);
	tp_connection_run_until_ready (connection, FALSE, NULL, NULL);
	g_object_unref (mc);

	/* We abuse of suppress_handler, TRUE means OUTGOING. */
	data = g_new (DispatcherRequestData, 1);
	data->channel_type = TP_IFACE_CHANNEL_TYPE_TEXT;
	data->handle_type = TP_HANDLE_TYPE_CONTACT;
	data->handle = empathy_contact_get_handle (contact);
	tp_cli_connection_call_request_channel (connection, -1,
						data->channel_type,
						data->handle_type,
						data->handle,
						TRUE,
						dispatcher_request_channel_cb,
						data, g_free,
						NULL);
	g_object_unref (connection);
}

void
empathy_dispatcher_chat_with_contact_id (McAccount   *account,
					 const gchar *contact_id)
{
	EmpathyContactFactory *factory;
	EmpathyContact        *contact;

	factory = empathy_contact_factory_new ();
	contact = empathy_contact_factory_get_from_id (factory, account, contact_id);
	empathy_contact_run_until_ready (contact, EMPATHY_CONTACT_READY_HANDLE, NULL);

	empathy_dispatcher_chat_with_contact (contact);

	g_object_unref (contact);
	g_object_unref (factory);
}

typedef struct {
	GFile *gfile;
	TpHandle handle;
	EmpathyContact *contact;
} FileChannelRequest;

static void
file_channel_create_cb (TpConnection *connection,
			 const gchar  *object_path,
       GHashTable *properties,
			 const GError *error,
			 gpointer      user_data,
			 GObject      *weak_object)
{
	TpChannel *channel;
	EmpathyTpFile *tp_file;
	FileChannelRequest *request = (FileChannelRequest *) user_data;

	if (error) {
		DEBUG ("Couldn't request channel: %s", error->message);
		return;
	}

	channel = tp_channel_new (connection,
				 object_path,
				 EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER,
				 TP_HANDLE_TYPE_CONTACT,
				 request->handle,
				 NULL);

	tp_file = empathy_tp_file_new (channel);

	if (tp_file) {
		empathy_tp_file_set_gfile (tp_file, request->gfile, NULL);
	}

	empathy_tp_file_offer (tp_file);

	g_object_unref (request->gfile);
	g_slice_free (FileChannelRequest, request);
	g_object_unref (channel);
}

void
empathy_dispatcher_send_file (EmpathyContact *contact,
			      GFile          *gfile)
{
	MissionControl *mc;
	McAccount      *account;
	TpConnection   *connection;
	guint           handle;
	FileChannelRequest *request;
  GHashTable *args;
  GValue *value;
	GFileInfo *info;
	guint64 size;
	gchar *filename;
  GTimeVal last_modif;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));
	g_return_if_fail (G_IS_FILE (gfile));

	mc = empathy_mission_control_new ();
	account = empathy_contact_get_account (contact);
	connection = mission_control_get_tpconnection (mc, account, NULL);
	handle = empathy_contact_get_handle (contact);

	request = g_slice_new0 (FileChannelRequest);
	request->gfile = g_object_ref (gfile);
	request->handle = handle;
	request->contact = contact;

	info = g_file_query_info (request->gfile,
				  G_FILE_ATTRIBUTE_STANDARD_SIZE ","
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
          G_FILE_ATTRIBUTE_TIME_MODIFIED,
				  0, NULL, NULL);
	size = info ? g_file_info_get_size (info) : EMPATHY_TP_FILE_UNKNOWN_SIZE;
	filename = g_file_get_basename (request->gfile);
	tp_connection_run_until_ready (connection, FALSE, NULL, NULL);

	DEBUG ("Sending %s from a stream to %s (size %llu, content-type %s)",
	       filename, empathy_contact_get_name (request->contact), size,
	       g_file_info_get_content_type (info));

  args = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);

  /* org.freedesktop.Telepathy.Channel.ChannelType */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER);
  g_hash_table_insert (args, TP_IFACE_CHANNEL ".ChannelType", value);

  /* org.freedesktop.Telepathy.Channel.TargetHandleType */
  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, TP_HANDLE_TYPE_CONTACT);
  g_hash_table_insert (args, TP_IFACE_CHANNEL ".TargetHandleType", value);

  /* org.freedesktop.Telepathy.Channel.TargetHandle */
  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, handle);
  g_hash_table_insert (args, TP_IFACE_CHANNEL ".TargetHandle", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.ContentType */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, g_file_info_get_content_type (info));
  g_hash_table_insert (args,
      EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".ContentType", value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Filename */
  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, g_filename_display_basename (filename));
  g_hash_table_insert (args, EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Filename",
      value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Size */
  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, size);
  g_hash_table_insert (args, EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Size",
      value);

  /* org.freedesktop.Telepathy.Channel.Type.FileTransfer.Date */
  g_file_info_get_modification_time (info, &last_modif);
  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, last_modif.tv_sec);
  g_hash_table_insert (args, EMP_IFACE_CHANNEL_TYPE_FILE_TRANSFER ".Date",
      value);

  /* TODO: Description ? */
  /* TODO: ContentHashType and ContentHash ? */

	tp_cli_connection_interface_requests_call_create_channel (connection, -1,
						args,
						file_channel_create_cb,
						request,
						NULL,
						NULL);

  g_hash_table_destroy (args);
	g_free (filename);
	g_object_unref (mc);
	g_object_unref (connection);
}
