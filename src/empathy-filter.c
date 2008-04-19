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
#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/proxy-subclass.h>

#include <libmissioncontrol/mission-control.h>
#include <libmissioncontrol/mc-account.h>

#include <extensions/extensions.h>

#include <libempathy/empathy-tp-chat.h>
#include <libempathy/empathy-tp-call.h>
#include <libempathy/empathy-tp-group.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-tube-handler.h>

#include <libempathy-gtk/empathy-chat.h>
#include <libempathy-gtk/empathy-images.h>
#include <libempathy-gtk/empathy-contact-dialogs.h>

#include "empathy-filter.h"
#include "empathy-chat-window.h"
#include "empathy-call-window.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_FILTER, EmpathyFilterPriv))

#define DEBUG_DOMAIN "Filter"

struct _EmpathyFilterPriv {
	GSList         *events;
	GHashTable     *accounts;
	gpointer        token;
	MissionControl *mc;
	GHashTable     *tubes;
};

static void empathy_filter_class_init (EmpathyFilterClass *klass);
static void empathy_filter_init       (EmpathyFilter      *filter);

G_DEFINE_TYPE (EmpathyFilter, empathy_filter, G_TYPE_OBJECT);

enum {
	PROP_0,
	PROP_TOP_EVENT,
};

typedef void (*FilterFunc) (EmpathyFilter *filter,
			    gpointer       user_data);

typedef struct {
	EmpathyFilterEvent public;
	FilterFunc         func;
	gpointer           user_data;
} EmpathyFilterEventExt;

static guint
filter_channel_hash (gconstpointer key)
{
	TpProxy *channel = TP_PROXY (key);

	return g_str_hash (channel->object_path);
}

static gboolean
filter_channel_equal (gconstpointer a,
		      gconstpointer b)
{
	TpProxy *channel_a = TP_PROXY (a);
	TpProxy *channel_b = TP_PROXY (b);

	return g_str_equal (channel_a->object_path, channel_b->object_path);
}

static void
filter_event_free (EmpathyFilterEventExt *event)
{
	g_free (event->public.icon_name);
	g_free (event->public.message);
	g_slice_free (EmpathyFilterEventExt, event);
}

static void
filter_emit_event (EmpathyFilter *filter,
		   const gchar   *icon_name,
		   const gchar   *message,
		   FilterFunc     func,
		   gpointer       user_data)
{
	EmpathyFilterPriv     *priv = GET_PRIV (filter);
	EmpathyFilterEventExt *event;

	empathy_debug (DEBUG_DOMAIN, "Emit event, icon_name=%s message='%s'",
		       icon_name, message);

	event = g_slice_new0 (EmpathyFilterEventExt);
	event->func = func;
	event->user_data = user_data;
	event->public.icon_name = g_strdup (icon_name);
	event->public.message = g_strdup (message);

	priv->events = g_slist_append (priv->events, event);
	if (priv->events->data == event) {
		g_object_notify (G_OBJECT (filter), "top-event");
	}
}

void
empathy_filter_activate_event (EmpathyFilter      *filter,
			       EmpathyFilterEvent *event)
{
	EmpathyFilterPriv     *priv = GET_PRIV (filter);
	EmpathyFilterEventExt *event_ext;
	GSList                *l;
	gboolean               is_top;

	g_return_if_fail (EMPATHY_IS_FILTER (filter));
	g_return_if_fail (event != NULL);

	if (!(l = g_slist_find (priv->events, event))) {
		return;
	}

	empathy_debug (DEBUG_DOMAIN, "Activating event");

	event_ext = (EmpathyFilterEventExt*) event;
	if (event_ext->func) {
		event_ext->func (filter, event_ext->user_data);
	}

	is_top = (l == priv->events);
	priv->events = g_slist_delete_link (priv->events, l);
	if (is_top) {
		g_object_notify (G_OBJECT (filter), "top-event");
	}

	filter_event_free (event_ext);
}

EmpathyFilterEvent *
empathy_filter_get_top_event (EmpathyFilter *filter)
{
	EmpathyFilterPriv *priv = GET_PRIV (filter);

	g_return_val_if_fail (EMPATHY_IS_FILTER (filter), NULL);

	return priv->events ? priv->events->data : NULL;
}

static void
filter_chat_dispatch (EmpathyFilter *filter,
		      gpointer       user_data)
{
	EmpathyTpChat *tp_chat = EMPATHY_TP_CHAT (user_data);
	McAccount     *account;
	EmpathyChat   *chat;
	const gchar   *id;

	id = empathy_tp_chat_get_id (tp_chat);
	account = empathy_tp_chat_get_account (tp_chat);
	chat = empathy_chat_window_find_chat (account, id);

	if (chat) {
		empathy_chat_set_tp_chat (chat, tp_chat);
	} else {
		chat = empathy_chat_new (tp_chat);
	}

	empathy_chat_window_present_chat (chat);
	g_object_unref (tp_chat);
}

static void
filter_chat_message_received_cb (EmpathyTpChat   *tp_chat,
				 EmpathyMessage  *message,
				 EmpathyFilter   *filter)
{
	EmpathyContact  *sender;
	gchar           *msg;

	g_signal_handlers_disconnect_by_func (tp_chat,
			  		      filter_chat_message_received_cb,
			  		      filter);

	sender = empathy_message_get_sender (message);
	msg = g_strdup_printf (_("New message from %s:\n%s"),
			       empathy_contact_get_name (sender),
			       empathy_message_get_body (message));

	filter_emit_event (filter, EMPATHY_IMAGE_NEW_MESSAGE, msg,
			   filter_chat_dispatch, tp_chat);

	g_free (msg);
}

static void
filter_chat_handle_channel (EmpathyFilter *filter,
			    TpChannel     *channel,
			    gboolean       is_incoming)
{
	EmpathyTpChat *tp_chat;

	empathy_debug (DEBUG_DOMAIN, "New text channel to be filtered: %p",
		       channel);

	tp_chat = empathy_tp_chat_new (channel, FALSE);
	if (is_incoming) {
		filter_chat_dispatch (filter, tp_chat);
	} else {
		g_signal_connect (tp_chat, "message-received",
				  G_CALLBACK (filter_chat_message_received_cb),
				  filter);
	}
}

#ifdef HAVE_VOIP
static void
filter_call_dispatch (EmpathyFilter *filter,
		      gpointer       user_data)
{
	EmpathyTpCall *call = EMPATHY_TP_CALL (user_data);

	empathy_call_window_new (call);
	g_object_unref (call);
}

static void
filter_call_contact_notify_cb (EmpathyTpCall *call,
			       gpointer       unused,
			       EmpathyFilter *filter)
{
	EmpathyContact *contact;
	gchar          *msg;

	g_object_get (call, "contact", &contact, NULL);
	if (!contact) {
		return;
	}

	empathy_contact_run_until_ready (contact,
					 EMPATHY_CONTACT_READY_NAME,
					 NULL);

	msg = g_strdup_printf (_("Incoming call from %s"),
			       empathy_contact_get_name (contact));

	filter_emit_event (filter, EMPATHY_IMAGE_VOIP, msg,
			   filter_call_dispatch, call);

	g_free (msg);
	g_object_unref (contact);
}

static void
filter_call_handle_channel (EmpathyFilter *filter,
			    TpChannel     *channel,
			    gboolean       is_incoming)
{
	EmpathyTpCall *call;

	empathy_debug (DEBUG_DOMAIN, "New media channel to be filtered: %p",
		       channel);

	call = empathy_tp_call_new (channel);
	if (is_incoming) {
		filter_call_dispatch (filter, call);
	} else {
		g_signal_connect (call, "notify::contact",
				  G_CALLBACK (filter_call_contact_notify_cb),
				  filter);	
	}
}
#endif

static void
filter_contact_list_subscribe (EmpathyFilter *filter,
			       gpointer       user_data)
{
	EmpathyContact *contact = EMPATHY_CONTACT (user_data);

	empathy_subscription_dialog_show (contact, NULL);
	g_object_unref (contact);
}

static void
filter_contact_list_local_pending_cb (EmpathyTpGroup *group,
				      EmpathyContact *contact,
				      EmpathyContact *actor,
				      guint           reason,
				      gchar          *message,
				      EmpathyFilter  *filter)
{
	GString *str;

	empathy_debug (DEBUG_DOMAIN, "New local pending contact");

	empathy_contact_run_until_ready (contact,
					 EMPATHY_CONTACT_READY_NAME,
					 NULL);

	str = g_string_new (NULL);
	g_string_printf (str, _("Subscription requested by %s"),
			 empathy_contact_get_name (contact));	
	if (!G_STR_EMPTY (message)) {
		g_string_append_printf (str, _("\nMessage: %s"), message);
	}

	filter_emit_event (filter, GTK_STOCK_DIALOG_QUESTION, str->str,
			   filter_contact_list_subscribe,
			   g_object_ref (contact));

	g_string_free (str, TRUE);
}

static void
filter_contact_list_ready_cb (EmpathyTpGroup *group,
			      gpointer        unused,
			      EmpathyFilter  *filter)
{
	GList *pendings, *l;

	if (tp_strdiff ("publish", empathy_tp_group_get_name (group))) {
		g_object_unref (group);
		return;
	}

	empathy_debug (DEBUG_DOMAIN, "Publish contact list ready");

	g_signal_connect (group, "local-pending",
			  G_CALLBACK (filter_contact_list_local_pending_cb),
			  filter);

	pendings = empathy_tp_group_get_local_pendings (group);
	for (l = pendings; l; l = l->next) {
		EmpathyPendingInfo *info = l->data;

		filter_contact_list_local_pending_cb (group, info->member,
						      info->actor, info->reason,
						      info->message, filter);
		empathy_pending_info_free (info);
	}
	g_list_free (pendings);
}

static void
filter_tubes_async_cb (TpProxy      *channel,
		       const GError *error,
		       gpointer      user_data,
		       GObject      *filter)
{
	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error %s: %s",
			       user_data, error->message);
	}
}

typedef struct {
	TpChannel *channel;
	gchar     *service;
	guint      type;
	guint      id;
} FilterTubeData;

static void
filter_tubes_dispatch (EmpathyFilter *filter,
		       gpointer       user_data)
{
	FilterTubeData *data = user_data;
	TpProxy        *connection;
	gchar          *object_path;
	guint           handle_type;
	guint           handle;
	TpProxy        *thandler;
	gchar          *thandler_bus_name;
	gchar          *thandler_object_path;

	thandler_bus_name = empathy_tube_handler_build_bus_name (data->type, data->service);
	thandler_object_path = empathy_tube_handler_build_object_path (data->type, data->service);

	/* Create the proxy for the tube handler */
	thandler = g_object_new (TP_TYPE_PROXY,
				 "dbus-connection", tp_get_bus (),
				 "bus-name", thandler_bus_name,
				 "object-path", thandler_object_path,
				 NULL);
	tp_proxy_add_interface_by_id (thandler, EMP_IFACE_QUARK_TUBE_HANDLER);

	/* Give the tube to the handler */
	g_object_get (data->channel,
		      "connection", &connection,
		      "object-path", &object_path,
		      "handle_type", &handle_type,
		      "handle", &handle,
		      NULL);

	emp_cli_tube_handler_call_handle_tube (thandler, -1,
					       connection->bus_name,
					       connection->object_path,
					       object_path, handle_type, handle,
					       data->id,
					       filter_tubes_async_cb,
					       "handling tube", NULL,
					       G_OBJECT (filter));

	g_free (thandler_bus_name);
	g_free (thandler_object_path);
	g_object_unref (thandler);
	g_object_unref (connection);
	g_free (object_path);

	g_free (data->service);
	g_object_unref (data->channel);
	g_slice_free (FilterTubeData, data);

}

static void
filter_tubes_new_tube_cb (TpChannel   *channel,
			  guint        id,
			  guint        initiator,
			  guint        type,
			  const gchar *service,
			  GHashTable  *parameters,
			  guint        state,
			  gpointer     user_data,
			  GObject     *filter)
{
	EmpathyFilterPriv *priv = GET_PRIV (filter);
	guint              number;
	gchar             *msg;
	FilterTubeData    *data;

	/* Increase tube count */
	number = GPOINTER_TO_UINT (g_hash_table_lookup (priv->tubes, channel));
	g_hash_table_replace (priv->tubes, g_object_ref (channel),
			      GUINT_TO_POINTER (++number));
	empathy_debug (DEBUG_DOMAIN, "Increased tube count for channel %p: %d",
		       channel, number);

	/* We dispatch only local pending tubes */
	if (state != TP_TUBE_STATE_LOCAL_PENDING) {
		return;
	}

	data = g_slice_new (FilterTubeData);
	data->channel = g_object_ref (channel);
	data->service = g_strdup (service);
	data->type = type;
	data->id = id;
	msg = g_strdup_printf (_("Incoming tube for application %s"), service);
	filter_emit_event (EMPATHY_FILTER (filter), GTK_STOCK_EXECUTE, msg,
			   filter_tubes_dispatch,
			   data);
	g_free (msg);
}

static void
filter_tubes_list_tubes_cb (TpChannel       *channel,
			    const GPtrArray *tubes,
			    const GError    *error,
			    gpointer         user_data,
			    GObject         *filter)
{
	guint i;

	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error listing tubes: %s",
			       error->message);
		return;
	}

	for (i = 0; i < tubes->len; i++) {
		GValueArray *values;

		values = g_ptr_array_index (tubes, i);
		filter_tubes_new_tube_cb (channel,
					  g_value_get_uint (g_value_array_get_nth (values, 0)),
					  g_value_get_uint (g_value_array_get_nth (values, 1)),
					  g_value_get_uint (g_value_array_get_nth (values, 2)),
					  g_value_get_string (g_value_array_get_nth (values, 3)),
					  g_value_get_boxed (g_value_array_get_nth (values, 4)),
					  g_value_get_uint (g_value_array_get_nth (values, 5)),
					  user_data, filter);
	}
}

static void
filter_tubes_channel_invalidated_cb (TpProxy       *proxy,
				     guint          domain,
				     gint           code,
				     gchar         *message,
				     EmpathyFilter *filter)
{
	EmpathyFilterPriv *priv = GET_PRIV (filter);

	empathy_debug (DEBUG_DOMAIN, "Channel %p invalidated: %s", proxy, message);

	g_hash_table_remove (priv->tubes, proxy);
}

static void
filter_tubes_tube_closed_cb (TpChannel *channel,
			     guint      id,
			     gpointer   user_data,
			     GObject   *filter)
{
	EmpathyFilterPriv *priv = GET_PRIV (filter);
	guint              number;

	number = GPOINTER_TO_UINT (g_hash_table_lookup (priv->tubes, channel));
	if (number == 1) {
		empathy_debug (DEBUG_DOMAIN, "Ended tube count for channel %p, "
			       "closing channel", channel);
		tp_cli_channel_call_close (channel, -1, NULL, NULL, NULL, NULL);
	}
	else if (number > 1) {
		empathy_debug (DEBUG_DOMAIN, "Decrease tube count for channel %p: %d",
			       channel, number);
		g_hash_table_replace (priv->tubes, g_object_ref (channel),
				      GUINT_TO_POINTER (--number));
	}
}

static void
filter_tubes_handle_channel (EmpathyFilter *filter,
			     TpChannel     *channel,
			     gboolean       is_incoming)
{
	EmpathyFilterPriv *priv = GET_PRIV (filter);

	if (g_hash_table_lookup (priv->tubes, channel)) {
		return;
	}

	empathy_debug (DEBUG_DOMAIN, "Handling new channel");

	g_hash_table_insert (priv->tubes, g_object_ref (channel),
			     GUINT_TO_POINTER (0));

	g_signal_connect (channel, "invalidated",
			  G_CALLBACK (filter_tubes_channel_invalidated_cb),
			  filter);

	tp_cli_channel_type_tubes_connect_to_tube_closed (channel,
							  filter_tubes_tube_closed_cb,
							  NULL, NULL,
							  G_OBJECT (filter), NULL);
	tp_cli_channel_type_tubes_connect_to_new_tube (channel,
						       filter_tubes_new_tube_cb,
						       NULL, NULL,
						       G_OBJECT (filter), NULL);
	tp_cli_channel_type_tubes_call_list_tubes (channel, -1,
						   filter_tubes_list_tubes_cb,
						   NULL, NULL,
						   G_OBJECT (filter));
}

static void
filter_contact_list_destroy_cb (EmpathyTpGroup *group,
				EmpathyFilter  *filter)
{
	g_object_unref (group);
}

static void
filter_contact_list_handle_channel (EmpathyFilter *filter,
				    TpChannel     *channel,
				    gboolean       is_incoming)
{
	EmpathyTpGroup *group;

	group = empathy_tp_group_new (channel);
	g_signal_connect (group, "notify::ready",
			  G_CALLBACK (filter_contact_list_ready_cb),
			  filter);	
	g_signal_connect (group, "destroy",
			  G_CALLBACK (filter_contact_list_destroy_cb),
			  filter);
}

static void
filter_connection_invalidated_cb (TpConnection  *connection,
				  guint          domain,
				  gint           code,
				  gchar         *message,
				  EmpathyFilter *filter)
{
	EmpathyFilterPriv *priv = GET_PRIV (filter);
	GHashTableIter     iter;
	gpointer           key, value;

	empathy_debug (DEBUG_DOMAIN, "connection invalidated: %s", message);

	g_hash_table_iter_init (&iter, priv->accounts);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		if (value == connection) {
			g_hash_table_remove (priv->accounts, key);
			break;
		}
	}
}

typedef void (*HandleChannelFunc) (EmpathyFilter *filter,
				   TpChannel     *channel,
				   gboolean       is_incoming);

static void
filter_conection_new_channel_cb (TpConnection *connection,
				 const gchar  *object_path,
				 const gchar  *channel_type,
				 guint         handle_type,
				 guint         handle,
				 gboolean      suppress_handler,
				 gpointer      user_data,
				 GObject      *filter)
{
	HandleChannelFunc  func = NULL;
	TpChannel         *channel;
	gpointer           had_channels;
	
	had_channels = g_object_get_data (G_OBJECT (connection), "had-channels");
	if (had_channels == NULL) {
		return;
	}

	if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_TEXT)) {
		func = filter_chat_handle_channel;
	}
#ifdef HAVE_VOIP
	else if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA)) {
		func = filter_call_handle_channel;
	}
#endif
	else if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_CONTACT_LIST)) {
		func = filter_contact_list_handle_channel;
	}
	else if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_TUBES)) {
		func = filter_tubes_handle_channel;
	} else {
		empathy_debug (DEBUG_DOMAIN, "Unknown channel type %s",
			       channel_type);
		return;
	}

	channel = tp_channel_new (connection, object_path, channel_type,
				  handle_type, handle, NULL);
	tp_channel_run_until_ready (channel, NULL, NULL);

	/* We abuse of suppress_handler, TRUE means OUTGOING */
	func (EMPATHY_FILTER (filter), channel, suppress_handler);

	g_object_unref (channel);
}

static void
filter_connection_list_channels_cb (TpConnection    *connection,
				    const GPtrArray *channels,
				    const GError    *error,
				    gpointer         user_data,
				    GObject         *filter)
{
	guint i;

	g_object_set_data (G_OBJECT (connection), "had-channels",
			   GUINT_TO_POINTER (1));

	for (i = 0; i < channels->len; i++) {
		GValueArray *values;

		values = g_ptr_array_index (channels, i);
		filter_conection_new_channel_cb (connection,
			g_value_get_boxed (g_value_array_get_nth (values, 0)),
			g_value_get_string (g_value_array_get_nth (values, 1)),
			g_value_get_uint (g_value_array_get_nth (values, 2)),
			g_value_get_uint (g_value_array_get_nth (values, 3)),
			TRUE, user_data, filter);
	}
}

#ifdef HAVE_VOIP
static void
filter_connection_advertise_capabilities_cb (TpConnection    *connection,
					     const GPtrArray *capabilities,
					     const GError    *error,
					     gpointer         user_data,
					     GObject         *filter)
{
	if (error) {
		empathy_debug (DEBUG_DOMAIN, "Error advertising capabilities: %s",
			       error->message);
	}
}
#endif

static void
filter_connection_ready_cb (TpConnection  *connection,
			    gpointer       unused,
			    EmpathyFilter *filter)
{
#ifdef HAVE_VOIP
	GPtrArray   *capabilities;
	GType        cap_type;
	GValue       cap = {0, };
	const gchar *remove = NULL;
#endif

	empathy_debug (DEBUG_DOMAIN, "Connection ready, accepting new channels");

	tp_cli_connection_connect_to_new_channel (connection,
						  filter_conection_new_channel_cb,
						  NULL, NULL,
						  G_OBJECT (filter), NULL);
	tp_cli_connection_call_list_channels (connection, -1,
					      filter_connection_list_channels_cb,
					      NULL, NULL,
					      G_OBJECT (filter));

#ifdef HAVE_VOIP
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
		filter_connection_advertise_capabilities_cb,
		NULL, NULL, G_OBJECT (filter));
#endif
}

static void
filter_update_account (EmpathyFilter *filter,
		       McAccount     *account)
{
	EmpathyFilterPriv *priv = GET_PRIV (filter);
	TpConnection      *connection;
	gboolean           ready;

	connection = g_hash_table_lookup (priv->accounts, account);
	if (connection) {
		return;
	}

	connection = mission_control_get_tpconnection (priv->mc, account, NULL);
	if (!connection) {
		return;
	}

	g_hash_table_insert (priv->accounts, g_object_ref (account), connection);
	g_signal_connect (connection, "invalidated",
			  G_CALLBACK (filter_connection_invalidated_cb),
			  filter);

	g_object_get (connection, "connection-ready", &ready, NULL);
	if (ready) {
		filter_connection_ready_cb (connection, NULL, filter);
	} else {
		g_signal_connect (connection, "notify::connection-ready",
				  G_CALLBACK (filter_connection_ready_cb),
				  filter);
	}
}

static void
filter_status_changed_cb (MissionControl           *mc,
			  TpConnectionStatus        status,
			  McPresence                presence,
			  TpConnectionStatusReason  reason,
			  const gchar              *unique_name,
			  EmpathyFilter            *filter)
{
	McAccount *account;

	account = mc_account_lookup (unique_name);
	filter_update_account (filter, account);
	g_object_unref (account);
}

static void
filter_finalize (GObject *object)
{
	EmpathyFilterPriv *priv = GET_PRIV (object);

	empathy_disconnect_account_status_changed (priv->token);
	g_object_unref (priv->mc);

	g_slist_foreach (priv->events, (GFunc) filter_event_free, NULL);
	g_slist_free (priv->events);

	g_hash_table_destroy (priv->accounts);
	g_hash_table_destroy (priv->tubes);
}

static void
filter_get_property (GObject    *object,
		     guint       param_id,
		     GValue     *value,
		     GParamSpec *pspec)
{
	EmpathyFilterPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_TOP_EVENT:
		g_value_set_pointer (value, priv->events ? priv->events->data : NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
empathy_filter_class_init (EmpathyFilterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = filter_finalize;
	object_class->get_property = filter_get_property;

	g_object_class_install_property (object_class,
					 PROP_TOP_EVENT,
					 g_param_spec_pointer ("top-event",
							       "The top event",
							       "The first event in the events list",
							       G_PARAM_READABLE));

	g_type_class_add_private (object_class, sizeof (EmpathyFilterPriv));
}

static void
empathy_filter_init (EmpathyFilter *filter)
{
	EmpathyFilterPriv *priv = GET_PRIV (filter);
	GList             *accounts, *l;

	priv->tubes = g_hash_table_new_full (filter_channel_hash,
					     filter_channel_equal,
					     g_object_unref, NULL);

	priv->mc = empathy_mission_control_new ();
	priv->token = empathy_connect_to_account_status_changed (priv->mc,
		G_CALLBACK (filter_status_changed_cb),
		filter, NULL);

	priv->accounts = g_hash_table_new_full (empathy_account_hash,
						empathy_account_equal,
						g_object_unref,
						g_object_unref);
	accounts = mc_accounts_list_by_enabled (TRUE);
	for (l = accounts; l; l = l->next) {
		filter_update_account (filter, l->data);
		g_object_unref (l->data);
	}
	g_list_free (accounts);
}

EmpathyFilter *
empathy_filter_new (void)
{
	static EmpathyFilter *filter = NULL;

	if (!filter) {
		filter = g_object_new (EMPATHY_TYPE_FILTER, NULL);
		g_object_add_weak_pointer (G_OBJECT (filter), (gpointer) &filter);
	} else {
		g_object_ref (filter);
	}

	return filter;
}

