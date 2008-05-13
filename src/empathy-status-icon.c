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

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <telepathy-glib/util.h>

#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-idle.h>
#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-dispatcher.h>
#include <libempathy/empathy-tp-chat.h>
#include <libempathy/empathy-tp-group.h>

#include <libempathy-gtk/empathy-presence-chooser.h>
#include <libempathy-gtk/empathy-conf.h>
#include <libempathy-gtk/empathy-ui-utils.h>
#include <libempathy-gtk/empathy-accounts-dialog.h>
#include <libempathy-gtk/empathy-images.h>
#include <libempathy-gtk/empathy-new-message-dialog.h>
#include <libempathy-gtk/empathy-contact-dialogs.h>

#include "empathy-status-icon.h"
#include "empathy-preferences.h"

#define DEBUG_FLAG EMPATHY_DEBUG_DISPATCHER
#include <libempathy/empathy-debug.h>

/* Number of ms to wait when blinking */
#define BLINK_TIMEOUT 500

typedef struct _StatusIconEvent StatusIconEvent;

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyStatusIcon)
typedef struct {
	GtkStatusIcon      *icon;
	EmpathyIdle        *idle;
	MissionControl     *mc;
	EmpathyDispatcher  *dispatcher;
	EmpathyContactManager *contact_manager;
	GSList             *events;
	gboolean            showing_event_icon;
	guint               blink_timeout;
	gpointer            token;

	GtkWindow          *window;
	GtkWidget          *popup_menu;
	GtkWidget          *show_window_item;
	GtkWidget          *message_item;
	GtkWidget          *status_item;
} EmpathyStatusIconPriv;

typedef void (*StatusIconEventFunc) (EmpathyStatusIcon *icon,
				     gpointer           user_data);

struct _StatusIconEvent {
	gchar               *icon_name;
	gchar               *message;
	StatusIconEventFunc  func;
	gpointer             user_data;
};

G_DEFINE_TYPE (EmpathyStatusIcon, empathy_status_icon, G_TYPE_OBJECT);

static void
status_icon_event_free (StatusIconEvent *event)
{
	g_free (event->icon_name);
	g_free (event->message);
	g_slice_free (StatusIconEvent, event);
}

static void
status_icon_set_visibility (EmpathyStatusIcon *icon,
			    gboolean           visible,
			    gboolean           store)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);

	if (store) {
		empathy_conf_set_bool (empathy_conf_get (),
				       EMPATHY_PREFS_UI_MAIN_WINDOW_HIDDEN, !visible);
	}

	if (!visible) {
		empathy_window_iconify (priv->window, priv->icon);
	} else {
		GList *accounts;

		empathy_window_present (GTK_WINDOW (priv->window), TRUE);
	
		/* Show the accounts dialog if there is no enabled accounts */
		accounts = mc_accounts_list_by_enabled (TRUE);
		if (accounts) {
			mc_accounts_list_free (accounts);
		} else {
			DEBUG ("No enabled account, Showing account dialog");
			empathy_accounts_dialog_show (GTK_WINDOW (priv->window));
		}
	}
}

static void
status_icon_notify_visibility_cb (EmpathyConf *conf,
				  const gchar *key,
				  gpointer     user_data)
{
	EmpathyStatusIcon *icon = user_data;
	gboolean           hidden = FALSE;

	if (empathy_conf_get_bool (conf, key, &hidden)) {
		status_icon_set_visibility (icon, !hidden, FALSE);
	}
}

static void
status_icon_toggle_visibility (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	gboolean               visible;

	visible = gtk_window_is_active (priv->window);
	status_icon_set_visibility (icon, !visible, TRUE);
}

static void
status_icon_update_tooltip (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	const gchar           *tooltip = NULL;

	if (priv->events) {
		tooltip = ((StatusIconEvent*)priv->events->data)->message;
	}

	if (!tooltip) {
		tooltip = empathy_idle_get_status (priv->idle);
	}

	gtk_status_icon_set_tooltip (priv->icon, tooltip);	
}

static void
status_icon_update_icon (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	const gchar           *icon_name;

	if (priv->events && priv->showing_event_icon) {
		icon_name = ((StatusIconEvent*)priv->events->data)->icon_name;
	} else {
		McPresence state;

		state = empathy_idle_get_state (priv->idle);
		icon_name = empathy_icon_name_for_presence (state);
	}

	gtk_status_icon_set_from_icon_name (priv->icon, icon_name);
}

static void
status_icon_idle_notify_cb (EmpathyStatusIcon *icon)
{
	status_icon_update_icon (icon);
	status_icon_update_tooltip (icon);
}

static gboolean
status_icon_delete_event_cb (GtkWidget         *widget,
			     GdkEvent          *event,
			     EmpathyStatusIcon *icon)
{
	status_icon_set_visibility (icon, FALSE, TRUE);
	return TRUE;
}

static void
status_icon_event_activate (EmpathyStatusIcon *icon,
			    StatusIconEvent   *event)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);

	if (event->func) {
		event->func (icon, event->user_data);
	}

	priv->events = g_slist_remove (priv->events, event);
	status_icon_event_free (event);
	status_icon_update_tooltip (icon);
	status_icon_update_icon (icon);

	if (!priv->events && priv->blink_timeout) {
		g_source_remove (priv->blink_timeout);
		priv->blink_timeout = 0;
	}
}

static void
status_icon_activate_cb (GtkStatusIcon     *status_icon,
			 EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);

	DEBUG ("Activated: %s", priv->events ? "event" : "toggle");

	if (priv->events) {
		status_icon_event_activate (icon, priv->events->data);
	} else {
		status_icon_toggle_visibility (icon);
	}
}

static void
status_icon_show_hide_window_cb (GtkWidget         *widget,
				 EmpathyStatusIcon *icon)
{
	gboolean visible;

	visible = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget));
	status_icon_set_visibility (icon, visible, TRUE);
}

static void
status_icon_new_message_cb (GtkWidget         *widget,
			    EmpathyStatusIcon *icon)
{
	empathy_new_message_dialog_show (NULL);
}

static void
status_icon_quit_cb (GtkWidget         *window,
		     EmpathyStatusIcon *icon)
{
	gtk_main_quit ();
}

static void
status_icon_popup_menu_cb (GtkStatusIcon     *status_icon,
			   guint              button,
			   guint              activate_time,
			   EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	GtkWidget             *submenu;
	gboolean               show;

	show = empathy_window_get_is_visible (GTK_WINDOW (priv->window));

	g_signal_handlers_block_by_func (priv->show_window_item,
					 status_icon_show_hide_window_cb,
					 icon);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (priv->show_window_item),
					show);
	g_signal_handlers_unblock_by_func (priv->show_window_item,
					   status_icon_show_hide_window_cb,
					   icon);

	submenu = empathy_presence_chooser_create_menu ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->status_item),
				   submenu);

	gtk_menu_popup (GTK_MENU (priv->popup_menu),
			NULL, NULL,
			gtk_status_icon_position_menu,
			priv->icon,
			button,
			activate_time);
}

static void
status_icon_create_menu (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	GladeXML              *glade;
	gchar                 *filename;

	filename = empathy_file_lookup ("empathy-status-icon.glade", "src");
	glade = empathy_glade_get_file (filename,
				       "tray_menu",
				       NULL,
				       "tray_menu", &priv->popup_menu,
				       "tray_show_list", &priv->show_window_item,
				       "tray_new_message", &priv->message_item,
				       "tray_status", &priv->status_item,
				       NULL);
	g_free (filename);

	empathy_glade_connect (glade,
			      icon,
			      "tray_show_list", "toggled", status_icon_show_hide_window_cb,
			      "tray_new_message", "activate", status_icon_new_message_cb,
			      "tray_quit", "activate", status_icon_quit_cb,
			      NULL);

	g_object_unref (glade);
}

static gboolean
status_icon_blink_timeout_cb (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);

	priv->showing_event_icon = !priv->showing_event_icon;
	status_icon_update_icon (icon);

	return TRUE;
}

static void
status_icon_event_add (EmpathyStatusIcon   *icon,
		       const gchar         *icon_name,
		       const gchar         *message,
		       StatusIconEventFunc  func,
		       gpointer             user_data)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	StatusIconEvent       *event;
	gboolean               had_events;

	DEBUG ("Adding event: %s", message);

	event = g_slice_new (StatusIconEvent);
	event->icon_name = g_strdup (icon_name);
	event->message = g_strdup (message);
	event->func = func;
	event->user_data = user_data;

	had_events = (priv->events != NULL);
	priv->events = g_slist_append (priv->events, event);
	if (!had_events) {
		priv->showing_event_icon = TRUE;
		status_icon_update_icon (icon);
		status_icon_update_tooltip (icon);

		if (!priv->blink_timeout) {
			priv->blink_timeout = g_timeout_add (BLINK_TIMEOUT,
							     (GSourceFunc) status_icon_blink_timeout_cb,
							     icon);
		}
	}
}

static void
status_icon_channel_process (EmpathyStatusIcon *icon,
			     gpointer           user_data)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	TpChannel             *channel = TP_CHANNEL (user_data);

	empathy_dispatcher_channel_process (priv->dispatcher, channel);
	g_object_unref (channel);
}

static gboolean
status_icon_chat_unref_idle (gpointer user_data)
{
	g_object_unref (user_data);
	return FALSE;
}

static void
status_icon_chat_message_received_cb (EmpathyTpChat     *tp_chat,
				      EmpathyMessage    *message,
				      EmpathyStatusIcon *icon)
{
	EmpathyContact  *sender;
	gchar           *msg;
	TpChannel       *channel;

	g_idle_add (status_icon_chat_unref_idle, tp_chat);
	g_signal_handlers_disconnect_by_func (tp_chat,
					      status_icon_chat_message_received_cb,
					      icon);

	sender = empathy_message_get_sender (message);
	msg = g_strdup_printf (_("New message from %s:\n%s"),
			       empathy_contact_get_name (sender),
			       empathy_message_get_body (message));

	channel = empathy_tp_chat_get_channel (tp_chat);
	status_icon_event_add (icon, EMPATHY_IMAGE_NEW_MESSAGE, msg,
			       status_icon_channel_process,
			       g_object_ref (channel));

	g_free (msg);
}

static void
status_icon_filter_channel_cb (EmpathyDispatcher *dispatcher,
			       TpChannel         *channel,
			       EmpathyStatusIcon *icon)
{
	gchar *channel_type;

	g_object_get (channel, "channel-type", &channel_type, NULL);
	if (!tp_strdiff (channel_type, TP_IFACE_CHANNEL_TYPE_TEXT)) {
		EmpathyTpChat *tp_chat;

		tp_chat = empathy_tp_chat_new (channel);
		g_signal_connect (tp_chat, "message-received",
				  G_CALLBACK (status_icon_chat_message_received_cb),
				  icon);
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

		status_icon_event_add (icon, EMPATHY_IMAGE_VOIP, msg,
				       status_icon_channel_process,
				       g_object_ref (channel));

		g_free (msg);
		g_object_unref (contact);
		g_object_unref (tp_group);
	}

	g_free (channel_type);
}

static void
status_icon_tube_process (EmpathyStatusIcon *icon,
			  gpointer           user_data)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	EmpathyDispatcherTube *tube = (EmpathyDispatcherTube*) user_data;

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
						 GTK_BUTTONS_OK, str);
		gtk_window_set_title (GTK_WINDOW (dialog),
				      _("Invitation Error"));
		g_free (str);

		gtk_widget_show (dialog);
		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);
	}

	empathy_dispatcher_tube_unref (tube);
}

static void
status_icon_filter_tube_cb (EmpathyDispatcher     *dispatcher,
			    EmpathyDispatcherTube *tube,
			    EmpathyStatusIcon     *icon)
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

	status_icon_event_add (icon, icon_name, msg, status_icon_tube_process,
			       empathy_dispatcher_tube_ref (tube));

	g_free (msg);
}

static void
status_icon_pending_subscribe (EmpathyStatusIcon *icon,
			       gpointer           user_data)
{
	EmpathyContact *contact = EMPATHY_CONTACT (user_data);

	empathy_subscription_dialog_show (contact, NULL);
	g_object_unref (contact);
}

static void
status_icon_pendings_changed_cb (EmpathyContactList *list,
				 EmpathyContact     *contact,
				 EmpathyContact     *actor,
				 guint               reason,
				 gchar              *message,
				 gboolean            is_pending,
				 EmpathyStatusIcon  *icon)
{
	GString *str;

	if (!is_pending) {
		/* FIXME: remove event if any */
		return;
	}

	DEBUG ("New local pending contact");

	empathy_contact_run_until_ready (contact,
					 EMPATHY_CONTACT_READY_NAME,
					 NULL);

	str = g_string_new (NULL);
	g_string_printf (str, _("Subscription requested by %s"),
			 empathy_contact_get_name (contact));	
	if (!G_STR_EMPTY (message)) {
		g_string_append_printf (str, _("\nMessage: %s"), message);
	}

	status_icon_event_add (icon, GTK_STOCK_DIALOG_QUESTION, str->str,
			       status_icon_pending_subscribe,
			       g_object_ref (contact));

	g_string_free (str, TRUE);
}

static void
status_icon_finalize (GObject *object)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (object);

	if (priv->blink_timeout) {
		g_source_remove (priv->blink_timeout);
	}

	empathy_disconnect_account_status_changed (priv->token);
	g_slist_foreach (priv->events, (GFunc) status_icon_event_free, NULL);
	g_slist_free (priv->events);

	g_object_unref (priv->icon);
	g_object_unref (priv->idle);
	g_object_unref (priv->mc);
	g_object_unref (priv->contact_manager);
}

static void
empathy_status_icon_class_init (EmpathyStatusIconClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = status_icon_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyStatusIconPriv));
}

static void
status_icon_status_changed_cb (MissionControl           *mc,
			       TpConnectionStatus        status,
			       McPresence                presence,
			       TpConnectionStatusReason  reason,
			       const gchar              *unique_name,
			       EmpathyStatusIcon        *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	GList                 *accounts, *l;
	guint                  connection_status = 1;

	/* Check for a connected account */
	accounts = mc_accounts_list_by_enabled (TRUE);
	for (l = accounts; l; l = l->next) {
		connection_status = mission_control_get_connection_status (priv->mc,
									   l->data,
									   NULL);
		if (connection_status == 0) {
			break;
		}
	}
	mc_accounts_list_free (accounts);

	gtk_widget_set_sensitive (priv->message_item, connection_status == 0);
}

static void
empathy_status_icon_init (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (icon,
		EMPATHY_TYPE_STATUS_ICON, EmpathyStatusIconPriv);

	icon->priv = priv;
	priv->icon = gtk_status_icon_new ();
	priv->mc = empathy_mission_control_new ();
	priv->idle = empathy_idle_new ();
	priv->dispatcher = empathy_dispatcher_new ();
	priv->contact_manager = empathy_contact_manager_new ();
	priv->token = empathy_connect_to_account_status_changed (priv->mc,
			G_CALLBACK (status_icon_status_changed_cb),
			icon, NULL);

	/* make icon listen and respond to MAIN_WINDOW_HIDDEN changes */
	empathy_conf_notify_add (empathy_conf_get (),
				 EMPATHY_PREFS_UI_MAIN_WINDOW_HIDDEN,
				 status_icon_notify_visibility_cb,
				 icon);

	status_icon_create_menu (icon);
	status_icon_idle_notify_cb (icon);

	g_signal_connect_swapped (priv->idle, "notify",
				  G_CALLBACK (status_icon_idle_notify_cb),
				  icon);
	g_signal_connect (priv->dispatcher, "filter-channel",
			  G_CALLBACK (status_icon_filter_channel_cb),
			  icon);
	g_signal_connect (priv->dispatcher, "filter-tube",
			  G_CALLBACK (status_icon_filter_tube_cb),
			  icon);
	g_signal_connect (priv->contact_manager, "pendings-changed",
			  G_CALLBACK (status_icon_pendings_changed_cb),
			  icon);
	g_signal_connect (priv->icon, "activate",
			  G_CALLBACK (status_icon_activate_cb),
			  icon);
	g_signal_connect (priv->icon, "popup-menu",
			  G_CALLBACK (status_icon_popup_menu_cb),
			  icon);
}

EmpathyStatusIcon *
empathy_status_icon_new (GtkWindow *window)
{
	EmpathyStatusIconPriv *priv;
	EmpathyStatusIcon     *icon;
	gboolean               should_hide;

	g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

	icon = g_object_new (EMPATHY_TYPE_STATUS_ICON, NULL);
	priv = GET_PRIV (icon);

	priv->window = g_object_ref (window);

	g_signal_connect (priv->window, "delete-event",
			  G_CALLBACK (status_icon_delete_event_cb),
			  icon);

	empathy_conf_get_bool (empathy_conf_get (),
			      EMPATHY_PREFS_UI_MAIN_WINDOW_HIDDEN,
			      &should_hide);

	if (gtk_window_is_active (priv->window) == should_hide) {
		status_icon_set_visibility (icon, !should_hide, FALSE);
	}

	return icon;
}

