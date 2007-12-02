/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
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

#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-contact-list.h>
#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-tp-chat.h>
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-conf.h>
#include <libempathy/empathy-idle.h>
#include <libempathy/empathy-filter.h>

#include "empathy-status-icon.h"
#include "empathy-contact-dialogs.h"
#include "empathy-presence-chooser.h"
#include "empathy-preferences.h"
#include "empathy-ui-utils.h"
#include "empathy-accounts-dialog.h"
#include "empathy-images.h"
#include "empathy-new-message-dialog.h"


#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_STATUS_ICON, EmpathyStatusIconPriv))

#define DEBUG_DOMAIN "StatusIcon"

/* Number of ms to wait when blinking */
#define BLINK_TIMEOUT 500

typedef struct _StatusIconEvent StatusIconEvent;

struct _EmpathyStatusIconPriv {
	GtkStatusIcon         *icon;
	EmpathyContactManager *manager;
	EmpathyFilter         *text_filter;
	EmpathyIdle           *idle;
	MissionControl        *mc;
	GList                 *events;
	gboolean               showing_event_icon;
	StatusIconEvent       *flash_state_event;
	guint                  blink_timeout;

	GtkWindow             *window;
	GtkWidget             *popup_menu;
	GtkWidget             *show_window_item;
	GtkWidget             *message_item;
	GtkWidget             *status_item;
};

typedef void (*EventActivatedFunc) (StatusIconEvent *event);

struct _StatusIconEvent {
	gchar              *icon_name;
	gchar              *message;
	EventActivatedFunc  func;
	gpointer            user_data;
};


static void       empathy_status_icon_class_init  (EmpathyStatusIconClass *klass);
static void       empathy_status_icon_init        (EmpathyStatusIcon      *icon);
static void       status_icon_finalize            (GObject                *object);
static void       status_icon_filter_new_channel  (EmpathyFilter          *filter,
						   TpConn                 *tp_conn,
						   TpChan                 *tp_chan,
						   EmpathyStatusIcon      *icon);
static void       status_icon_message_received_cb (EmpathyTpChat          *tp_chat,
						   EmpathyMessage         *message,
						   EmpathyStatusIcon      *icon);
static void       status_icon_idle_notify_cb      (EmpathyStatusIcon      *icon);
static void       status_icon_update_tooltip      (EmpathyStatusIcon      *icon);
static void       status_icon_set_from_state      (EmpathyStatusIcon      *icon);
static void       status_icon_set_visibility      (EmpathyStatusIcon      *icon,
						   gboolean                visible);
static void       status_icon_toggle_visibility   (EmpathyStatusIcon      *icon);
static void       status_icon_activate_cb         (GtkStatusIcon          *status_icon,
						   EmpathyStatusIcon      *icon);
static gboolean   status_icon_delete_event_cb     (GtkWidget              *widget,
						   GdkEvent               *event,
						   EmpathyStatusIcon      *icon);
static void       status_icon_popup_menu_cb       (GtkStatusIcon          *status_icon,
						   guint                   button,
						   guint                   activate_time,
						   EmpathyStatusIcon      *icon);
static void       status_icon_create_menu         (EmpathyStatusIcon      *icon);
static void       status_icon_new_message_cb      (GtkWidget              *widget,
						   EmpathyStatusIcon      *icon);
static void       status_icon_quit_cb             (GtkWidget              *window,
						   EmpathyStatusIcon      *icon);
static void       status_icon_show_hide_window_cb (GtkWidget              *widget,
						   EmpathyStatusIcon      *icon);
static void       status_icon_pendings_changed_cb (EmpathyContactManager  *manager,
						   EmpathyContact         *contact,
						   EmpathyContact         *actor,
						   guint                   reason,
						   gchar                  *message,
						   gboolean                is_pending,
						   EmpathyStatusIcon      *icon);
static void       status_icon_event_subscribe_cb  (StatusIconEvent        *event);
static void       status_icon_event_flash_state_cb (StatusIconEvent       *event);
static void       status_icon_event_msg_cb        (StatusIconEvent        *event);
static StatusIconEvent * status_icon_event_new    (EmpathyStatusIcon      *icon,
						   const gchar            *icon_name,
						   const gchar            *message);
static void       status_icon_event_remove        (EmpathyStatusIcon      *icon,
						   StatusIconEvent        *event);
static gboolean   status_icon_event_timeout_cb    (EmpathyStatusIcon      *icon);
static void       status_icon_event_free          (StatusIconEvent        *event);

G_DEFINE_TYPE (EmpathyStatusIcon, empathy_status_icon, G_TYPE_OBJECT);

static void
empathy_status_icon_class_init (EmpathyStatusIconClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = status_icon_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyStatusIconPriv));
}

static void
empathy_status_icon_init (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;
	GList                 *pendings, *l;

	priv = GET_PRIV (icon);

	priv->icon = gtk_status_icon_new ();
	priv->idle = empathy_idle_new ();
	empathy_idle_set_auto_away (priv->idle, TRUE);
	empathy_idle_set_auto_disconnect (priv->idle, TRUE);
	priv->manager = empathy_contact_manager_new ();
	priv->mc = empathy_mission_control_new ();
	priv->text_filter = empathy_filter_new ("org.gnome.Empathy.ChatFilter",
						"/org/gnome/Empathy/ChatFilter",
						TP_IFACE_CHANNEL_TYPE_TEXT,
						MC_FILTER_PRIORITY_DIALOG,
						MC_FILTER_FLAG_INCOMING);

	status_icon_create_menu (icon);
	status_icon_idle_notify_cb (icon);

	g_signal_connect (priv->text_filter, "new-channel",
			  G_CALLBACK (status_icon_filter_new_channel),
			  icon);
	g_signal_connect_swapped (priv->idle, "notify",
				  G_CALLBACK (status_icon_idle_notify_cb),
				  icon);
	g_signal_connect (priv->icon, "activate",
			  G_CALLBACK (status_icon_activate_cb),
			  icon);
	g_signal_connect (priv->icon, "popup-menu",
			  G_CALLBACK (status_icon_popup_menu_cb),
			  icon);
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
}

static void
status_icon_finalize (GObject *object)
{
	EmpathyStatusIconPriv *priv;

	priv = GET_PRIV (object);

	g_list_foreach (priv->events, (GFunc) status_icon_event_free, NULL);
	g_list_free (priv->events);

	if (priv->blink_timeout) {
		g_source_remove (priv->blink_timeout);
	}

	g_object_unref (priv->icon);
	g_object_unref (priv->window);
	g_object_unref (priv->idle);
	g_object_unref (priv->manager);
	g_object_unref (priv->mc);
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
		status_icon_set_visibility (icon, !should_hide);
	}

	return icon;
}

static void
status_icon_filter_new_channel (EmpathyFilter     *filter,
				TpConn            *tp_conn,
				TpChan            *tp_chan,
				EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;
	McAccount             *account;
	EmpathyTpChat         *tp_chat;
	GList                 *messages;

	priv = GET_PRIV (icon);

	account = mission_control_get_account_for_connection (priv->mc, tp_conn, NULL);

	empathy_debug (DEBUG_DOMAIN, "New text channel to be filtered for contact %s",
		       empathy_inspect_channel (account, tp_chan));

	tp_chat = empathy_tp_chat_new (account, tp_chan);
	g_object_set_data (G_OBJECT (tp_chat), "filter", filter);
	g_object_unref (account);

	messages = empathy_tp_chat_get_pendings (tp_chat);
	if (!messages) {
		empathy_debug (DEBUG_DOMAIN, "No pending msg, waiting...");
		g_signal_connect (tp_chat, "message-received",
				  G_CALLBACK (status_icon_message_received_cb),
				  icon);
		return;
	}

	status_icon_message_received_cb (tp_chat, messages->data, icon);

	g_list_foreach (messages, (GFunc) g_object_unref, NULL);
	g_list_free (messages);
}

static void
status_icon_message_received_cb (EmpathyTpChat     *tp_chat,
				 EmpathyMessage    *message,
				 EmpathyStatusIcon *icon)
{
	EmpathyContact  *sender;
	gchar           *msg;
	StatusIconEvent *event;

	empathy_debug (DEBUG_DOMAIN, "Message received, add event");

	g_signal_handlers_disconnect_by_func (tp_chat,
			  		      status_icon_message_received_cb,
			  		      icon);

	sender = empathy_message_get_sender (message);
	msg = g_strdup_printf (_("New message from %s:\n%s"),
			       empathy_contact_get_name (sender),
			       empathy_message_get_body (message));

	event = status_icon_event_new (icon, EMPATHY_IMAGE_NEW_MESSAGE, msg);
	event->func = status_icon_event_msg_cb;
	event->user_data = tp_chat;
}

static void
status_icon_idle_notify_cb (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;
	McPresence             flash_state;

	priv = GET_PRIV (icon);

	flash_state = empathy_idle_get_flash_state (priv->idle);
	if (flash_state != MC_PRESENCE_UNSET) {
		const gchar *icon_name;

		icon_name = empathy_icon_name_for_presence_state (flash_state);
		if (!priv->flash_state_event) {
			/* We are now flashing */
			priv->flash_state_event = status_icon_event_new (icon, icon_name, NULL);
			priv->flash_state_event->user_data = icon;
			priv->flash_state_event->func = status_icon_event_flash_state_cb;
		} else {
			/* We are still flashing but with another state */
			g_free (priv->flash_state_event->icon_name);
			priv->flash_state_event->icon_name = g_strdup (icon_name);
		}
	}
	else if (priv->flash_state_event) {
		/* We are no more flashing */
		status_icon_event_remove (icon, priv->flash_state_event);
		priv->flash_state_event = NULL;
	}

	if (!priv->showing_event_icon) {
		status_icon_set_from_state (icon);
	}

	status_icon_update_tooltip (icon);
}

static void
status_icon_update_tooltip (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;
	const gchar           *tooltip = NULL;

	priv = GET_PRIV (icon);

	if (priv->events) {
		StatusIconEvent *event;

		event = priv->events->data;
		tooltip = event->message;
	}

	if (!tooltip) {
		tooltip = empathy_idle_get_status (priv->idle);
	}

	gtk_status_icon_set_tooltip (priv->icon, tooltip);	
}

static void
status_icon_set_from_state (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;
	McPresence             state;
	const gchar           *icon_name;

	priv = GET_PRIV (icon);

	state = empathy_idle_get_state (priv->idle);
	icon_name = empathy_icon_name_for_presence_state (state);
	gtk_status_icon_set_from_icon_name (priv->icon, icon_name);
}

static void
status_icon_set_visibility (EmpathyStatusIcon *icon,
			    gboolean           visible)
{
	EmpathyStatusIconPriv *priv;

	priv = GET_PRIV (icon);

	if (!visible) {
		empathy_window_iconify (priv->window, priv->icon);
		empathy_conf_set_bool (empathy_conf_get (),
				      EMPATHY_PREFS_UI_MAIN_WINDOW_HIDDEN, TRUE);
	} else {
		GList *accounts;

		empathy_window_present (GTK_WINDOW (priv->window), TRUE);
		empathy_conf_set_bool (empathy_conf_get (),
				      EMPATHY_PREFS_UI_MAIN_WINDOW_HIDDEN, FALSE);
	
		/* Show the accounts dialog if there is no enabled accounts */
		accounts = mc_accounts_list_by_enabled (TRUE);
		if (accounts) {
			mc_accounts_list_free (accounts);
		} else {
			empathy_debug (DEBUG_DOMAIN,
				      "No enabled account, Showing account dialog");
			empathy_accounts_dialog_show (GTK_WINDOW (priv->window));
		}
	}
}

static void
status_icon_toggle_visibility (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	gboolean               visible;

	visible = gtk_window_is_active (priv->window);
	status_icon_set_visibility (icon, !visible);
}

static void
status_icon_activate_cb (GtkStatusIcon     *status_icon,
			 EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;

	priv = GET_PRIV (icon);

	empathy_debug (DEBUG_DOMAIN, "Activated: %s",
		       priv->events ? "event" : "toggle");

	if (priv->events) {
		status_icon_event_remove (icon, priv->events->data);
	} else {
		status_icon_toggle_visibility (icon);
	}
}

static gboolean
status_icon_delete_event_cb (GtkWidget         *widget,
			     GdkEvent          *event,
			     EmpathyStatusIcon *icon)
{
	status_icon_set_visibility (icon, FALSE);

	return TRUE;
}

static void
status_icon_popup_menu_cb (GtkStatusIcon     *status_icon,
			   guint              button,
			   guint              activate_time,
			   EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;
	GtkWidget             *submenu;
	gboolean               show;

	priv = GET_PRIV (icon);

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
	EmpathyStatusIconPriv *priv;
	GladeXML              *glade;

	priv = GET_PRIV (icon);

	glade = empathy_glade_get_file ("empathy-status-icon.glade",
				       "tray_menu",
				       NULL,
				       "tray_menu", &priv->popup_menu,
				       "tray_show_list", &priv->show_window_item,
				       "tray_new_message", &priv->message_item,
				       "tray_status", &priv->status_item,
				       NULL);

	empathy_glade_connect (glade,
			      icon,
			      "tray_show_list", "toggled", status_icon_show_hide_window_cb,
			      "tray_new_message", "activate", status_icon_new_message_cb,
			      "tray_quit", "activate", status_icon_quit_cb,
			      NULL);

	g_object_unref (glade);
}

static void
status_icon_new_message_cb (GtkWidget         *widget,
			    EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;

	priv = GET_PRIV (icon);

	empathy_new_message_dialog_show (GTK_WINDOW (priv->window));
}

static void
status_icon_quit_cb (GtkWidget         *window,
		     EmpathyStatusIcon *icon)
{
	gtk_main_quit ();
}

static void
status_icon_show_hide_window_cb (GtkWidget         *widget,
				 EmpathyStatusIcon *icon)
{
	gboolean visible;

	visible = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget));
	status_icon_set_visibility (icon, visible);
}

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
	GList                 *l;

	priv = GET_PRIV (icon);

	if (!is_pending) {
		/* FIXME: We should remove the event */
		return;
	}

	for (l = priv->events; l; l = l->next) {
		if (empathy_contact_equal (contact, ((StatusIconEvent*)l->data)->user_data)) {
			return;
		}
	}

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

static void
status_icon_event_flash_state_cb (StatusIconEvent *event)
{
	EmpathyStatusIconPriv *priv;

	priv = GET_PRIV (event->user_data);

	empathy_idle_set_flash_state (priv->idle, MC_PRESENCE_UNSET);
}

static void
status_icon_event_msg_cb (StatusIconEvent *event)
{
	EmpathyFilter *filter;
	EmpathyTpChat *tp_chat;

	empathy_debug (DEBUG_DOMAIN, "Dispatching text channel");

	tp_chat = event->user_data;
	filter = g_object_get_data (G_OBJECT (tp_chat), "filter");
	empathy_filter_process (filter,
				empathy_tp_chat_get_channel (tp_chat),
				TRUE);
	g_object_unref (tp_chat);
}

static StatusIconEvent *
status_icon_event_new (EmpathyStatusIcon *icon,
		       const gchar       *icon_name,
		       const gchar       *message)
{
	EmpathyStatusIconPriv *priv;
	StatusIconEvent       *event;

	priv = GET_PRIV (icon);

	event = g_slice_new0 (StatusIconEvent);
	event->icon_name = g_strdup (icon_name);	
	event->message = g_strdup (message);

	priv->events = g_list_append (priv->events, event);
	if (!priv->blink_timeout) {
		priv->showing_event_icon = FALSE;
		priv->blink_timeout = g_timeout_add (BLINK_TIMEOUT,
						     (GSourceFunc) status_icon_event_timeout_cb,
						     icon);
		status_icon_event_timeout_cb (icon);
		status_icon_update_tooltip (icon);
	}

	return event;
}

static void
status_icon_event_remove (EmpathyStatusIcon *icon,
			  StatusIconEvent   *event)
{
	EmpathyStatusIconPriv *priv;

	priv = GET_PRIV (icon);

	if (event->func) {
		event->func (event);
	}
	priv->events = g_list_remove (priv->events, event);
	status_icon_event_free (event);
	priv->showing_event_icon = FALSE;
	status_icon_update_tooltip (icon);
	status_icon_set_from_state (icon);

	if (priv->events) {
		return;
	}

	if (priv->blink_timeout) {
		g_source_remove (priv->blink_timeout);
		priv->blink_timeout = 0;
	}
}

static gboolean
status_icon_event_timeout_cb (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;

	priv = GET_PRIV (icon);

	priv->showing_event_icon = !priv->showing_event_icon;

	if (!priv->showing_event_icon) {
		status_icon_set_from_state (icon);
	} else {
		StatusIconEvent *event;

		event = priv->events->data;
		gtk_status_icon_set_from_icon_name (priv->icon, event->icon_name);
	}

	return TRUE;
}

static void
status_icon_event_free (StatusIconEvent *event)
{
	g_free (event->icon_name);
	g_free (event->message);
	g_slice_free (StatusIconEvent, event);
}

