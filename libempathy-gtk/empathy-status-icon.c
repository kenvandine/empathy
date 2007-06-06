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

#include <config.h>

#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-contact-list.h>
#include <libempathy/empathy-contact-manager.h>
#include <libempathy/gossip-contact.h>
#include <libempathy/gossip-debug.h>
#include <libempathy/gossip-utils.h>
#include <libempathy/gossip-conf.h>
#include <libempathy/empathy-idle.h>

#include "empathy-status-icon.h"
#include "empathy-contact-widget.h"
#include "gossip-presence-chooser.h"
#include "gossip-preferences.h"
#include "gossip-ui-utils.h"
#include "gossip-accounts-dialog.h"


#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_STATUS_ICON, EmpathyStatusIconPriv))

#define DEBUG_DOMAIN "StatusIcon"

/* Number of ms to wait when blinking */
#define BLINK_TIMEOUT 500

struct _EmpathyStatusIconPriv {
	GtkStatusIcon         *icon;
	EmpathyContactManager *manager;
	EmpathyIdle           *idle;
	GList                 *events;
	guint                  blink_timeout;
	gboolean               showing_state_icon;

	GtkWindow             *window;

	GtkWidget             *popup_menu;
	GtkWidget             *show_window_item;
	GtkWidget             *message_item;
	GtkWidget             *status_item;
};

typedef struct _StatusIconEvent StatusIconEvent;

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
static void       status_icon_idle_notify_cb      (EmpathyIdle            *idle,
						   GParamSpec             *param,
						   EmpathyStatusIcon      *icon);
static void       status_icon_update_tooltip      (EmpathyStatusIcon      *icon);
static void       status_icon_set_from_state      (EmpathyStatusIcon      *icon);
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
static void       status_icon_local_pending_cb    (EmpathyContactManager  *manager,
						   GossipContact          *contact,
						   gchar                  *message,
						   EmpathyStatusIcon      *icon);
static void       status_icon_event_subscribe_cb  (StatusIconEvent        *event);
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
	GList                 *pending, *l;

	priv = GET_PRIV (icon);

	priv->icon = gtk_status_icon_new ();
	priv->idle = empathy_idle_new ();
	priv->manager = empathy_contact_manager_new ();
	priv->showing_state_icon = TRUE;

	status_icon_create_menu (icon);
	status_icon_set_from_state (icon);
	status_icon_update_tooltip (icon);

	g_signal_connect (priv->idle, "notify",
			  G_CALLBACK (status_icon_idle_notify_cb),
			  icon);
	g_signal_connect (priv->icon, "activate",
			  G_CALLBACK (status_icon_activate_cb),
			  icon);
	g_signal_connect (priv->icon, "popup-menu",
			  G_CALLBACK (status_icon_popup_menu_cb),
			  icon);
	g_signal_connect (priv->manager, "local-pending",
			  G_CALLBACK (status_icon_local_pending_cb),
			  icon);

	pending = empathy_contact_list_get_local_pending (EMPATHY_CONTACT_LIST (priv->manager));
	for (l = pending; l; l = l->next) {
		EmpathyContactListInfo *info;

		info = l->data;
		status_icon_local_pending_cb (priv->manager,
					      info->contact,
					      info->message,
					      icon);
	}
	g_list_free (pending);
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
}

EmpathyStatusIcon *
empathy_status_icon_new (GtkWindow *window)
{
	EmpathyStatusIconPriv *priv;
	EmpathyStatusIcon     *icon;
	gboolean               should_hide;
	gboolean               visible;

	g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

	icon = g_object_new (EMPATHY_TYPE_STATUS_ICON, NULL);
	priv = GET_PRIV (icon);

	priv->window = g_object_ref (window);

	g_signal_connect (priv->window, "delete-event",
			  G_CALLBACK (status_icon_delete_event_cb),
			  icon);

	gossip_conf_get_bool (gossip_conf_get (),
			      GOSSIP_PREFS_UI_MAIN_WINDOW_HIDDEN,
			      &should_hide);
	visible = gossip_window_get_is_visible (window);

	if ((!should_hide && !visible) || (should_hide && visible)) {
		status_icon_toggle_visibility (icon);
	}

	return icon;
}

static void
status_icon_idle_notify_cb (EmpathyIdle       *idle,
			    GParamSpec        *param,
			    EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;

	priv = GET_PRIV (icon);

	if (priv->showing_state_icon) {
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
	icon_name = gossip_icon_name_for_presence_state (state);
	gtk_status_icon_set_from_icon_name (priv->icon, icon_name);
}

static void
status_icon_toggle_visibility (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;
	gboolean               visible;

	priv = GET_PRIV (icon);

	visible = gossip_window_get_is_visible (GTK_WINDOW (priv->window));

	if (visible) {
		gtk_widget_hide (GTK_WIDGET (priv->window));
		gossip_conf_set_bool (gossip_conf_get (),
				      GOSSIP_PREFS_UI_MAIN_WINDOW_HIDDEN, TRUE);
	} else {
		GList *accounts;

		gossip_window_present (GTK_WINDOW (priv->window), TRUE);
		gossip_conf_set_bool (gossip_conf_get (),
				      GOSSIP_PREFS_UI_MAIN_WINDOW_HIDDEN, FALSE);
	
		/* Show the accounts dialog if there is no enabled accounts */
		accounts = mc_accounts_list_by_enabled (TRUE);
		if (accounts) {
			mc_accounts_list_free (accounts);
		} else {
			gossip_debug (DEBUG_DOMAIN,
				      "No enabled account, Showing account dialog");
			gossip_accounts_dialog_show (GTK_WINDOW (priv->window));
		}
	}
}

static void
status_icon_activate_cb (GtkStatusIcon     *status_icon,
			 EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;

	priv = GET_PRIV (icon);

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
	status_icon_toggle_visibility (icon);

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

	show = gossip_window_get_is_visible (GTK_WINDOW (priv->window));

	g_signal_handlers_block_by_func (priv->show_window_item,
					 status_icon_show_hide_window_cb,
					 icon);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (priv->show_window_item),
					show);
	g_signal_handlers_unblock_by_func (priv->show_window_item,
					   status_icon_show_hide_window_cb,
					   icon);

	submenu = gossip_presence_chooser_create_menu ();
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

	glade = gossip_glade_get_file ("empathy-status-icon.glade",
				       "tray_menu",
				       NULL,
				       "tray_menu", &priv->popup_menu,
				       "tray_show_list", &priv->show_window_item,
				       "tray_new_message", &priv->message_item,
				       "tray_status", &priv->status_item,
				       NULL);

	gossip_glade_connect (glade,
			      icon,
			      "tray_new_message", "activate", status_icon_new_message_cb,
			      "tray_quit", "activate", status_icon_quit_cb,
			      NULL);

	g_signal_connect (priv->show_window_item, "toggled",
			  G_CALLBACK (status_icon_show_hide_window_cb),
			  icon);

	g_object_unref (glade);
}

static void
status_icon_new_message_cb (GtkWidget         *widget,
			    EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv;

	priv = GET_PRIV (icon);

	//gossip_new_message_dialog_show (GTK_WINDOW (priv->window));
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
	status_icon_toggle_visibility (icon);
}

static void
status_icon_local_pending_cb (EmpathyContactManager *manager,
			      GossipContact         *contact,
			      gchar                 *message,
			      EmpathyStatusIcon     *icon)
{
	EmpathyStatusIconPriv *priv;
	StatusIconEvent       *event;
	gchar                 *str;
	GList                 *l;

	priv = GET_PRIV (icon);

	for (l = priv->events; l; l = l->next) {
		if (gossip_contact_equal (contact, ((StatusIconEvent*)l->data)->user_data)) {
			return;
		}
	}

	str = g_strdup_printf (_("Subscription requested for %s\n"
				 "Message: %s"),
			       gossip_contact_get_name (contact),
			       message);

	event = status_icon_event_new (icon, GTK_STOCK_DIALOG_QUESTION, str);
	event->user_data = g_object_ref (contact);
	event->func = status_icon_event_subscribe_cb;

	g_free (str);
}

static void
status_icon_event_subscribe_cb (StatusIconEvent *event)
{
	GossipContact *contact;

	contact = GOSSIP_CONTACT (event->user_data);

	//empathy_subscription_dialog_show (contact);

	g_object_unref (contact);
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
		priv->blink_timeout = g_timeout_add (BLINK_TIMEOUT,
						     (GSourceFunc) status_icon_event_timeout_cb,
						     icon);
		status_icon_event_timeout_cb (icon);
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
	status_icon_update_tooltip (icon);

	if (priv->events) {
		return;
	}

	status_icon_set_from_state (icon);
	priv->showing_state_icon = TRUE;

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

	priv->showing_state_icon = !priv->showing_state_icon;

	if (priv->showing_state_icon) {
		status_icon_set_from_state (icon);
	} else {
		StatusIconEvent *event;

		event = priv->events->data;
		gtk_status_icon_set_from_icon_name (priv->icon, event->icon_name);
	}
	status_icon_update_tooltip (icon);

	return TRUE;
}

static void
status_icon_event_free (StatusIconEvent *event)
{
	g_free (event->icon_name);
	g_free (event->message);
	g_slice_free (StatusIconEvent, event);
}

