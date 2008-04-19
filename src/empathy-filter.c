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

#include <libmissioncontrol/mission-control.h>
#include <libmissioncontrol/mc-account.h>

#include <libempathy/empathy-tp-chat.h>
#include <libempathy/empathy-tp-call.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-debug.h>

#include <libempathy-gtk/empathy-chat.h>
#include <libempathy-gtk/empathy-images.h>

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

#if 0
static void
status_icon_pendings_changed_cb (EmpathyContactManager *manager,
				 EmpathyContact        *contact,
				 EmpathyContact        *actor,
				 guint                  reason,
				 gchar                 *message,
				 gboolean               is_pending,
				 EmpathyStatusIcon     *icon)
{
	EmpathyStatusIconPriv *priv;
	StatusIconEvent       *event;
	GString               *str;

	priv = GET_PRIV (icon);

	if (!is_pending) {
		/* FIXME: We should remove the event */
		return;
	}

	empathy_contact_run_until_ready (contact,
					 EMPATHY_CONTACT_READY_NAME,
					 NULL);

	str = g_string_new (NULL);
	g_string_printf (str, _("Subscription requested by %s"),
			 empathy_contact_get_name (contact));	
	if (!G_STR_EMPTY (message)) {
		g_string_append_printf (str, _("\nMessage: %s"), message);
	}

	event = status_icon_event_new (icon, GTK_STOCK_DIALOG_QUESTION, str->str);
	event->user_data = g_object_ref (contact);
	event->func = status_icon_event_subscribe_cb;

	g_string_free (str, TRUE);
}

static void
status_icon_event_subscribe_cb (StatusIconEvent *event)
{
	EmpathyContact *contact;

	contact = EMPATHY_CONTACT (event->user_data);

	empathy_subscription_dialog_show (contact, NULL);

	g_object_unref (contact);
}
	g_signal_connect (priv->manager, "pendings-changed",
			  G_CALLBACK (status_icon_pendings_changed_cb),
			  icon);

	pendings = empathy_contact_list_get_pendings (EMPATHY_CONTACT_LIST (priv->manager));
	for (l = pendings; l; l = l->next) {
		EmpathyPendingInfo *info;

		info = l->data;
		status_icon_pendings_changed_cb (priv->manager,
						 info->member,
						 info->actor,
						 0,
						 info->message,
						 TRUE,
						 icon);
		empathy_pending_info_free (info);
	}
	g_list_free (pendings);
#endif

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
	
	if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_TEXT)) {
		func = filter_chat_handle_channel;
	}
	else if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA)) {
		func = filter_call_handle_channel;
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
filter_connection_ready_cb (TpConnection  *connection,
			    gpointer       unused,
			    EmpathyFilter *filter)
{
	empathy_debug (DEBUG_DOMAIN, "Connection ready, accepting new channels");
	tp_cli_connection_connect_to_new_channel (connection,
						  filter_conection_new_channel_cb,
						  NULL, NULL,
						  G_OBJECT (filter), NULL);
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

