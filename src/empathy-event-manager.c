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

#include <telepathy-glib/util.h>

#include <libempathy/empathy-dispatcher.h>
#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-tp-chat.h>
#include <libempathy/empathy-tp-group.h>
#include <libempathy/empathy-utils.h>
#include <libempathy-gtk/empathy-images.h>
#include <libempathy-gtk/empathy-contact-dialogs.h>

#include "empathy-event-manager.h"

#define DEBUG_FLAG EMPATHY_DEBUG_DISPATCHER
#include <libempathy/empathy-debug.h>

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyEventManager)
typedef struct {
	EmpathyDispatcher     *dispatcher;
	EmpathyContactManager *contact_manager;
	GSList                *events;
} EmpathyEventManagerPriv;

typedef struct _EventPriv EventPriv;
typedef void (*EventFunc) (EventPriv *event);

struct _EventPriv {
	EmpathyEvent         public;
	TpChannel           *channel;
	EmpathyEventManager *manager;
	EventFunc            func;
	gpointer             user_data;
};

enum {
	EVENT_ADDED,
	EVENT_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyEventManager, empathy_event_manager, G_TYPE_OBJECT);

static void event_remove (EventPriv *event);

static void
event_free (EventPriv *event)
{
	g_free (event->public.icon_name);
	g_free (event->public.message);

	if (event->public.contact) {
		g_object_unref (event->public.contact);
	}

	if (event->channel) {
		g_signal_handlers_disconnect_by_func (event->channel,
						      event_remove,
						      event);
		g_object_unref (event->channel);
	}
	g_slice_free (EventPriv, event);
}

static void
event_remove (EventPriv *event)
{
	EmpathyEventManagerPriv *priv = GET_PRIV (event->manager);

	DEBUG ("Removing event %p", event);
	priv->events = g_slist_remove (priv->events, event);
	g_signal_emit (event->manager, signals[EVENT_REMOVED], 0, event);
	event_free (event);
}

static void
event_manager_add (EmpathyEventManager *manager,
		   EmpathyContact      *contact,
		   const gchar         *icon_name,
		   const gchar         *message,
		   TpChannel           *channel,
		   EventFunc            func,
		   gpointer             user_data)
{
	EmpathyEventManagerPriv *priv = GET_PRIV (manager);
	EventPriv               *event;

	event = g_slice_new0 (EventPriv);
	event->public.contact = contact ? g_object_ref (contact) : NULL;
	event->public.icon_name = g_strdup (icon_name);
	event->public.message = g_strdup (message);
	event->manager = manager;
	event->func = func;
	event->user_data = user_data;

	if (channel) {
		event->channel = g_object_ref (channel);
		g_signal_connect_swapped (channel, "invalidated",
					  G_CALLBACK (event_remove),
					  event);
	}

	DEBUG ("Adding event %p", event);
	priv->events = g_slist_prepend (priv->events, event);
	g_signal_emit (event->manager, signals[EVENT_ADDED], 0, event);
}

static void
event_channel_process_func (EventPriv *event)
{
	EmpathyEventManagerPriv *priv = GET_PRIV (event->manager);

	/* This will emit "dispatch-channel" and the event will be removed
	 * in the callback of that signal, no need to remove the event here. */	
	empathy_dispatcher_channel_process (priv->dispatcher, event->channel);
}

static gboolean
event_manager_chat_unref_idle (gpointer user_data)
{
	g_object_unref (user_data);
	return FALSE;
}

static void
event_manager_chat_message_received_cb (EmpathyTpChat       *tp_chat,
					EmpathyMessage      *message,
					EmpathyEventManager *manager)
{
	EmpathyContact  *sender;
	gchar           *msg;
	TpChannel       *channel;

	g_idle_add (event_manager_chat_unref_idle, tp_chat);
	g_signal_handlers_disconnect_by_func (tp_chat,
					      event_manager_chat_message_received_cb,
					      manager);

	sender = empathy_message_get_sender (message);
	msg = g_strdup_printf (_("New message from %s:\n%s"),
			       empathy_contact_get_name (sender),
			       empathy_message_get_body (message));

	channel = empathy_tp_chat_get_channel (tp_chat);
	event_manager_add (manager, sender, EMPATHY_IMAGE_NEW_MESSAGE, msg,
			   channel, event_channel_process_func, NULL);

	g_free (msg);
}

static void
event_manager_filter_channel_cb (EmpathyDispatcher   *dispatcher,
				 TpChannel           *channel,
				 EmpathyEventManager *manager)
{
	gchar *channel_type;

	g_object_get (channel, "channel-type", &channel_type, NULL);
	if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_TEXT)) {
		EmpathyTpChat *tp_chat;

		tp_chat = empathy_tp_chat_new (channel);
		g_signal_connect (tp_chat, "message-received",
				  G_CALLBACK (event_manager_chat_message_received_cb),
				  manager);
	}
	else if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA)) {
		EmpathyTpGroup *tp_group;
		EmpathyContact *contact;
		gchar          *msg;

		tp_group = empathy_tp_group_new (channel);
		empathy_run_until_ready (tp_group);
		empathy_tp_group_get_invitation (tp_group, &contact);
		empathy_contact_run_until_ready (contact,
						 EMPATHY_CONTACT_READY_NAME,
						 NULL);

		msg = g_strdup_printf (_("Incoming call from %s"),
				       empathy_contact_get_name (contact));

		event_manager_add (manager, contact, EMPATHY_IMAGE_VOIP, msg,
				   channel, event_channel_process_func, NULL);

		g_free (msg);
		g_object_unref (contact);
		g_object_unref (tp_group);
	}

	g_free (channel_type);
}

static void
event_manager_dispatch_channel_cb (EmpathyDispatcher   *dispatcher,
				   TpChannel           *channel,
				   EmpathyEventManager *manager)
{
	EmpathyEventManagerPriv *priv = GET_PRIV (manager);
	GSList                  *l;

	for (l = priv->events; l; l = l->next) {
		EventPriv *event = l->data;

		if (event->channel &&
		    empathy_proxy_equal (channel, event->channel)) {
			event_remove (event);
			break;
		}
	}
}

static void
event_tube_process_func (EventPriv *event)
{
	EmpathyEventManagerPriv *priv = GET_PRIV (event->manager);
	EmpathyDispatcherTube   *tube = (EmpathyDispatcherTube*) event->user_data;

	if (tube->activatable) {
		empathy_dispatcher_tube_process (priv->dispatcher, tube);
	} else {
		GtkWidget *dialog;
		gchar     *str;

		/* Tell the user that the tube can't be handled */
		str = g_strdup_printf (_("%s offered you an invitation, but "
					 "you don't have the needed external "
					 "application to handle it."),
				       empathy_contact_get_name (tube->initiator));

		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 "%s", str);
		gtk_window_set_title (GTK_WINDOW (dialog),
				      _("Invitation Error"));
		g_free (str);

		gtk_widget_show (dialog);
		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);
	}

	empathy_dispatcher_tube_unref (tube);
	event_remove (event);
}

static void
event_manager_filter_tube_cb (EmpathyDispatcher     *dispatcher,
			      EmpathyDispatcherTube *tube,
			      EmpathyEventManager   *manager)
{
	const gchar *icon_name;
	gchar       *msg;

	empathy_contact_run_until_ready (tube->initiator,
					 EMPATHY_CONTACT_READY_NAME, NULL);

	if (tube->activatable) {
		icon_name = GTK_STOCK_EXECUTE;
		msg = g_strdup_printf (_("%s is offering you an invitation. An external "
					 "application will be started to handle it."),
				       empathy_contact_get_name (tube->initiator));
	} else {
		icon_name = GTK_STOCK_DIALOG_ERROR;
		msg = g_strdup_printf (_("%s is offering you an invitation, but "
					 "you don't have the needed external "
					 "application to handle it."),
				       empathy_contact_get_name (tube->initiator));
	}

	event_manager_add (manager, tube->initiator, icon_name, msg,
			   tube->channel, event_tube_process_func,
			   empathy_dispatcher_tube_ref (tube));

	g_free (msg);
}

static void
event_pending_subscribe_func (EventPriv *event)
{
	empathy_subscription_dialog_show (event->public.contact, NULL);
	event_remove (event);
}

static void
event_manager_pendings_changed_cb (EmpathyContactList  *list,
				   EmpathyContact      *contact,
				   EmpathyContact      *actor,
				   guint                reason,
				   gchar               *message,
				   gboolean             is_pending,
				   EmpathyEventManager *manager)
{
	EmpathyEventManagerPriv *priv = GET_PRIV (manager);
	GString                 *str;

	if (!is_pending) {
		GSList *l;

		for (l = priv->events; l; l = l->next) {
			EventPriv *event = l->data;

			if (event->public.contact == contact &&
			    event->func == event_pending_subscribe_func) {
				event_remove (event);
				break;
			}
		}

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

	event_manager_add (manager, contact, GTK_STOCK_DIALOG_QUESTION, str->str,
			   NULL, event_pending_subscribe_func, NULL);

	g_string_free (str, TRUE);
}

static void
event_manager_finalize (GObject *object)
{
	EmpathyEventManagerPriv *priv = GET_PRIV (object);

	g_slist_foreach (priv->events, (GFunc) event_free, NULL);
	g_slist_free (priv->events);
	g_object_unref (priv->contact_manager);
	g_object_unref (priv->dispatcher);
}

static void
empathy_event_manager_class_init (EmpathyEventManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = event_manager_finalize;

	signals[EVENT_ADDED] =
		g_signal_new ("event-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	signals[EVENT_REMOVED] =
		g_signal_new ("event-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1, G_TYPE_POINTER);

	g_type_class_add_private (object_class, sizeof (EmpathyEventManagerPriv));
}

static void
empathy_event_manager_init (EmpathyEventManager *manager)
{
	EmpathyEventManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
		EMPATHY_TYPE_EVENT_MANAGER, EmpathyEventManagerPriv);

	manager->priv = priv;

	priv->dispatcher = empathy_dispatcher_new ();
	priv->contact_manager = empathy_contact_manager_new ();
	g_signal_connect (priv->dispatcher, "filter-channel",
			  G_CALLBACK (event_manager_filter_channel_cb),
			  manager);
	g_signal_connect (priv->dispatcher, "dispatch-channel",
			  G_CALLBACK (event_manager_dispatch_channel_cb),
			  manager);
	g_signal_connect (priv->dispatcher, "filter-tube",
			  G_CALLBACK (event_manager_filter_tube_cb),
			  manager);
	g_signal_connect (priv->contact_manager, "pendings-changed",
			  G_CALLBACK (event_manager_pendings_changed_cb),
			  manager);
}

EmpathyEventManager *
empathy_event_manager_new (void)
{
	static EmpathyEventManager *manager = NULL;

	if (!manager) {
		manager = g_object_new (EMPATHY_TYPE_EVENT_MANAGER, NULL);
		g_object_add_weak_pointer (G_OBJECT (manager), (gpointer) &manager);
	} else {
		g_object_ref (manager);
	}

	return manager;
}

GSList *
empathy_event_manager_get_events (EmpathyEventManager *manager)
{
	EmpathyEventManagerPriv *priv = GET_PRIV (manager);

	g_return_val_if_fail (EMPATHY_IS_EVENT_MANAGER (manager), NULL);

	return priv->events;
}

EmpathyEvent *
empathy_event_manager_get_top_event (EmpathyEventManager *manager)
{
	EmpathyEventManagerPriv *priv = GET_PRIV (manager);

	g_return_val_if_fail (EMPATHY_IS_EVENT_MANAGER (manager), NULL);

	return priv->events ? priv->events->data : NULL;
}

void
empathy_event_activate (EmpathyEvent *event_public)
{
	EventPriv *event = (EventPriv*) event_public;

	g_return_if_fail (event_public != NULL);

	if (event->func) {
		event->func (event);
	} else {
		event_remove (event);
	}
}

