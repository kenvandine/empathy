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
#include <gdk/gdkkeysyms.h>

#include <libnotify/notify.h>
#include <libnotify/notification.h>

#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-idle.h>
#include <libempathy/empathy-account-manager.h>

#include <libempathy-gtk/empathy-presence-chooser.h>
#include <libempathy-gtk/empathy-conf.h>
#include <libempathy-gtk/empathy-ui-utils.h>
#include <libempathy-gtk/empathy-images.h>
#include <libempathy-gtk/empathy-new-message-dialog.h>

#include "empathy-accounts-dialog.h"
#include "empathy-status-icon.h"
#include "empathy-preferences.h"
#include "empathy-event-manager.h"
#include "empathy-misc.h"

#ifdef HAVE_LIBINDICATE
#include "empathy-indicator-manager.h"
#endif

#define DEBUG_FLAG EMPATHY_DEBUG_DISPATCHER
#include <libempathy/empathy-debug.h>

/* Number of ms to wait when blinking */
#define BLINK_TIMEOUT 500

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyStatusIcon)
typedef struct {
	GtkStatusIcon       *icon;
	EmpathyIdle         *idle;
	EmpathyAccountManager *account_manager;
	gboolean             showing_event_icon;
	guint                blink_timeout;
	EmpathyEventManager *event_manager;
	EmpathyEvent        *event;
	NotifyNotification  *notification;

	GtkWindow           *window;
	GtkUIManager        *ui_manager;
	GtkWidget           *popup_menu;
	GtkAction           *show_window_item;
	GtkAction           *new_message_item;
	GtkAction           *status_item;
#ifdef HAVE_LIBINDICATE
       EmpathyIndicatorManager *indicator_manager;
#endif
} EmpathyStatusIconPriv;

G_DEFINE_TYPE (EmpathyStatusIcon, empathy_status_icon, G_TYPE_OBJECT);

#ifdef HAVE_LIBINDICATE
static void indicate_server_activate_cb (EmpathyIndicatorManager *, EmpathyStatusIcon   *);
#endif

static gboolean
activate_event (EmpathyEvent *event)
{
	empathy_event_activate (event);

	return FALSE;
}

static gboolean
notification_server_supports_actions (void)
{
	GList * caps = notify_get_server_caps ();
	GList * l;
	gboolean ret = FALSE;

	for (l = caps; l; l = l->next) {
		gchar *cap = (gchar *) l->data;
		if (!cap) {
			continue;
		}
		if (!strcmp (cap, "actions")) {
			ret = TRUE;
			break;
		}
	}
	g_list_foreach (caps, (GFunc)g_free, NULL);
	g_list_free (caps);

	return ret;
}

static void
status_icon_notification_closed_cb (NotifyNotification *notification,
				    EmpathyStatusIcon  *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	EmpathyNotificationClosedReason reason = 0;

#ifdef notify_notification_get_closed_reason
	reason = notify_notification_get_closed_reason (notification);
#endif
	if (priv->notification) {
		g_object_unref (priv->notification);
		priv->notification = NULL;
	}

	if (!priv->event) {
		return;
	}

	/* the notification has been closed by the user, see the
	 * DesktopNotification spec.
	 */
	if (reason == EMPATHY_NOTIFICATION_CLOSED_DISMISSED) {
		/* use an idle here, as this callback is called from a
		 * DBus signal handler inside libnotify, and we might call
		 * a *_run_* method when activating the event.
		 */
		g_idle_add ((GSourceFunc) activate_event, priv->event);
	} else {
		if (notification_server_supports_actions ()) {
			empathy_event_inhibit_updates (priv->event);
		}
	}
}

static void
notification_close_helper (EmpathyStatusIconPriv *priv)
{
	if (priv->notification) {
		notify_notification_close (priv->notification, NULL);
		g_object_unref (priv->notification);
		priv->notification = NULL;
	}
}

static void
status_icon_update_notification (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	GdkPixbuf *pixbuf = NULL;

	if (!empathy_notification_is_enabled ()) {
		/* always close the notification if this happens */
		notification_close_helper (priv);
		return;
	}

	if (priv->event) {
		pixbuf = empathy_misc_get_pixbuf_for_notification (priv->event->contact,
								   priv->event->icon_name);

		if (priv->notification) {
			notify_notification_update (priv->notification,
						    priv->event->header, priv->event->message,
						    NULL);
		} else {
			priv->notification = notify_notification_new_with_status_icon
				(priv->event->header, priv->event->message, NULL, priv->icon);
			notify_notification_set_timeout (priv->notification,
							 NOTIFY_EXPIRES_DEFAULT);

			g_signal_connect (priv->notification, "closed",
					  G_CALLBACK (status_icon_notification_closed_cb), icon);

 		}
		notify_notification_set_icon_from_pixbuf (priv->notification,
							  pixbuf);
		notify_notification_show (priv->notification, NULL);

		g_object_unref (pixbuf);
	} else {
		notification_close_helper (priv);
	}
}

static void
status_icon_update_tooltip (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	gchar                 *tooltip = NULL;

	if (priv->event) {
		if (priv->event->message != NULL)
				tooltip = g_strdup_printf ("<i>%s</i>\n%s",
							   priv->event->header,
							   priv->event->message);
		else
				tooltip = g_strdup_printf ("<i>%s</i>",
							   priv->event->header);
		gtk_status_icon_set_tooltip_markup (priv->icon, tooltip);
	} else {
		tooltip = g_strdup (empathy_idle_get_status (priv->idle));
		gtk_status_icon_set_tooltip_text (priv->icon, tooltip);
	}

	g_free (tooltip);
}

static void
status_icon_update_icon (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	const gchar           *icon_name;

	if (priv->event && priv->showing_event_icon) {
		icon_name = priv->event->icon_name;
	} else {
		TpConnectionPresenceType state;

		state = empathy_idle_get_state (priv->idle);
		icon_name = empathy_icon_name_for_presence (state);
	}

	if (icon_name != NULL)
		gtk_status_icon_set_from_icon_name (priv->icon, icon_name);
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
status_icon_event_added_cb (EmpathyEventManager *manager,
			    EmpathyEvent        *event,
			    EmpathyStatusIcon   *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);

	if (priv->event) {
		return;
	}

	DEBUG ("New event %p", event);

	priv->event = event;
	if (event->must_ack) {
		priv->showing_event_icon = TRUE;
		status_icon_update_icon (icon);
		status_icon_update_tooltip (icon);
	}
	status_icon_update_notification (icon);

	if (!priv->blink_timeout && priv->showing_event_icon) {
		priv->blink_timeout = g_timeout_add (BLINK_TIMEOUT,
						     (GSourceFunc) status_icon_blink_timeout_cb,
						     icon);
	}
}

static void
status_icon_event_removed_cb (EmpathyEventManager *manager,
			      EmpathyEvent        *event,
			      EmpathyStatusIcon   *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);

	if (event != priv->event) {
		return;
	}

	priv->event = empathy_event_manager_get_top_event (priv->event_manager);

	status_icon_update_tooltip (icon);
	status_icon_update_icon (icon);

	/* update notification anyway, as it's safe and we might have been
	 * changed presence in the meanwhile
	 */
	status_icon_update_notification (icon);

	if (!priv->event && priv->blink_timeout) {
		g_source_remove (priv->blink_timeout);
		priv->blink_timeout = 0;
	}
}

static void
status_icon_event_updated_cb (EmpathyEventManager *manager,
			      EmpathyEvent        *event,
			      EmpathyStatusIcon   *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);

	if (event != priv->event) {
		return;
	}

	if (empathy_notification_is_enabled ()) {
		status_icon_update_notification (icon);
	}

	status_icon_update_tooltip (icon);
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
			empathy_accounts_dialog_show (GTK_WINDOW (priv->window), NULL);
		}
	}
}

#ifdef HAVE_LIBINDICATE
static void
status_icon_set_use_libindicate (EmpathyStatusIcon *icon,
			    gboolean           use_libindicate)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);

	if (use_libindicate) {
		empathy_indicator_manager_set_server_visible (priv->indicator_manager,
				TRUE);
		gtk_status_icon_set_visible (priv->icon, FALSE);
	} else {
		empathy_indicator_manager_set_server_visible (priv->indicator_manager,
				FALSE);
		gtk_status_icon_set_visible (priv->icon, TRUE);
	}
}
#endif

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

#ifdef HAVE_LIBINDICATE
static void
status_icon_notify_libindicate_cb (EmpathyConf *conf,
				  const gchar *key,
				  gpointer     user_data)
{
	EmpathyStatusIcon *icon = user_data;
	gboolean           use_libindicate = FALSE;

	if (empathy_conf_get_bool (conf, key, &use_libindicate)) {
		status_icon_set_use_libindicate (icon, use_libindicate);
	}
}
#endif

static void
status_icon_toggle_visibility (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	gboolean               visible;
#ifdef HAVE_LIBINDICATE
	gboolean               use_libindicate;
#endif

	visible = gtk_window_is_active (priv->window);
#ifdef HAVE_LIBINDICATE
	empathy_conf_get_bool (empathy_conf_get (),
			       EMPATHY_PREFS_UI_USE_LIBINDICATE,
		               &use_libindicate);
	if (use_libindicate) {
		visible = GTK_WIDGET_VISIBLE (priv->window);
	}
#endif
	status_icon_set_visibility (icon, !visible, TRUE);
}

#ifdef HAVE_LIBINDICATE
static void
indicate_server_activate_cb (EmpathyIndicatorManager *manager,
				  EmpathyStatusIcon *icon)
{
	status_icon_toggle_visibility (icon);
}
#endif

static void
status_icon_idle_notify_cb (EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);

	status_icon_update_icon (icon);
	status_icon_update_tooltip (icon);

	if (!empathy_notification_is_enabled ()) {
		/* dismiss the outstanding notification if present */

		if (priv->notification) {
			notify_notification_close (priv->notification, NULL);
			g_object_unref (priv->notification);
			priv->notification = NULL;
		}
	}
}

static gboolean
status_icon_delete_event_cb (GtkWidget         *widget,
			     GdkEvent          *event,
			     EmpathyStatusIcon *icon)
{
	status_icon_set_visibility (icon, FALSE, TRUE);
	return TRUE;
}

static gboolean
status_icon_key_press_event_cb  (GtkWidget *window,
				 GdkEventKey *event,
				 EmpathyStatusIcon *icon)
{
	if (event->keyval == GDK_Escape) {
		status_icon_set_visibility (icon, FALSE, TRUE);
	}
	return FALSE;
}

static void
status_icon_activate_cb (GtkStatusIcon     *status_icon,
			 EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);

	DEBUG ("%s", priv->event ? "event" : "toggle");

	if (priv->event) {
		empathy_event_activate (priv->event);
	} else {
		status_icon_toggle_visibility (icon);
	}
}

static void
status_icon_show_hide_window_cb (GtkToggleAction   *action,
				 EmpathyStatusIcon *icon)
{
	gboolean visible;

	visible = gtk_toggle_action_get_active (action);
	status_icon_set_visibility (icon, visible, TRUE);
}

static void
status_icon_new_message_cb (GtkAction         *action,
			    EmpathyStatusIcon *icon)
{
	empathy_new_message_dialog_show (NULL);
}

static void
status_icon_quit_cb (GtkAction         *action,
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
	GtkWidget             *menu_item;
	GtkWidget             *submenu;
	gboolean               show;

	show = empathy_window_get_is_visible (GTK_WINDOW (priv->window));

	g_signal_handlers_block_by_func (priv->show_window_item,
					 status_icon_show_hide_window_cb,
					 icon);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (priv->show_window_item),
				      show);
	g_signal_handlers_unblock_by_func (priv->show_window_item,
					   status_icon_show_hide_window_cb,
					   icon);

	menu_item = gtk_ui_manager_get_widget (priv->ui_manager, "/menu/status");
	submenu = empathy_presence_chooser_create_menu ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), submenu);

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
	GtkBuilder            *gui;
	gchar                 *filename;

	filename = empathy_file_lookup ("empathy-status-icon.ui", "src");
	gui = empathy_builder_get_file (filename,
					"ui_manager", &priv->ui_manager,
					"menu", &priv->popup_menu,
					"show_list", &priv->show_window_item,
					"new_message", &priv->new_message_item,
					"status", &priv->status_item,
				       NULL);
	g_free (filename);

	empathy_builder_connect (gui, icon,
			      "show_list", "toggled", status_icon_show_hide_window_cb,
			      "new_message", "activate", status_icon_new_message_cb,
			      "quit", "activate", status_icon_quit_cb,
			      NULL);

	g_object_ref (priv->ui_manager);
	g_object_unref (gui);
}

static void
status_icon_connection_changed_cb (EmpathyAccountManager *manager,
				   McAccount *account,
				   TpConnectionStatusReason reason,
				   TpConnectionStatus current,
				   TpConnectionStatus previous,
				   EmpathyStatusIcon *icon)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (icon);
	int connected_accounts;

	/* Check for a connected account */
	connected_accounts = empathy_account_manager_get_connected_accounts (manager);

	gtk_action_set_sensitive (priv->new_message_item, connected_accounts > 0);
}

static void
status_icon_finalize (GObject *object)
{
	EmpathyStatusIconPriv *priv = GET_PRIV (object);

	if (priv->blink_timeout) {
		g_source_remove (priv->blink_timeout);
	}

	g_signal_handlers_disconnect_by_func (priv->account_manager,
					      status_icon_connection_changed_cb,
					      object);

	if (priv->notification) {
		notify_notification_close (priv->notification, NULL);
		g_object_unref (priv->notification);
		priv->notification = NULL;
	}

	g_object_unref (priv->icon);
	g_object_unref (priv->idle);
	g_object_unref (priv->account_manager);
	g_object_unref (priv->event_manager);
	g_object_unref (priv->ui_manager);
}

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
	EmpathyStatusIconPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (icon,
		EMPATHY_TYPE_STATUS_ICON, EmpathyStatusIconPriv);

	icon->priv = priv;
	priv->icon = gtk_status_icon_new ();
	priv->account_manager = empathy_account_manager_dup_singleton ();
	priv->idle = empathy_idle_dup_singleton ();
	priv->event_manager = empathy_event_manager_dup_singleton ();

	g_signal_connect (priv->account_manager,
			  "account-connection-changed",
			  G_CALLBACK (status_icon_connection_changed_cb), icon);

	/* make icon listen and respond to MAIN_WINDOW_HIDDEN changes */
	empathy_conf_notify_add (empathy_conf_get (),
				 EMPATHY_PREFS_UI_MAIN_WINDOW_HIDDEN,
				 status_icon_notify_visibility_cb,
				 icon);

#ifdef HAVE_LIBINDICATE
	empathy_conf_notify_add (empathy_conf_get (),
				 EMPATHY_PREFS_UI_USE_LIBINDICATE,
				 status_icon_notify_libindicate_cb,
				 icon);
#endif

	status_icon_create_menu (icon);
	status_icon_idle_notify_cb (icon);

	g_signal_connect_swapped (priv->idle, "notify",
				  G_CALLBACK (status_icon_idle_notify_cb),
				  icon);
	g_signal_connect (priv->event_manager, "event-added",
			  G_CALLBACK (status_icon_event_added_cb),
			  icon);
	g_signal_connect (priv->event_manager, "event-removed",
			  G_CALLBACK (status_icon_event_removed_cb),
			  icon);
	g_signal_connect (priv->event_manager, "event-updated",
			  G_CALLBACK (status_icon_event_updated_cb),
			  icon);
	g_signal_connect (priv->icon, "activate",
			  G_CALLBACK (status_icon_activate_cb),
			  icon);
	g_signal_connect (priv->icon, "popup-menu",
			  G_CALLBACK (status_icon_popup_menu_cb),
			  icon);
}

EmpathyStatusIcon *
empathy_status_icon_new (GtkWindow *window, gboolean hide_contact_list)
{
	EmpathyStatusIconPriv *priv;
	EmpathyStatusIcon     *icon;
	gboolean               should_hide;
#ifdef HAVE_LIBINDICATE
	gboolean               use_libindicate;
#endif

	g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

	icon = g_object_new (EMPATHY_TYPE_STATUS_ICON, NULL);
	priv = GET_PRIV (icon);

	priv->window = g_object_ref (window);
#ifdef HAVE_LIBINDICATE
	priv->indicator_manager = empathy_indicator_manager_dup_singleton ();
	g_signal_connect (priv->indicator_manager, "server-activate",
			  G_CALLBACK (indicate_server_activate_cb),
			  icon);
#endif

	g_signal_connect_after (priv->window, "key-press-event",
			  G_CALLBACK (status_icon_key_press_event_cb),
			  icon);

	g_signal_connect (priv->window, "delete-event",
			  G_CALLBACK (status_icon_delete_event_cb),
			  icon);

	if (!hide_contact_list) {
		empathy_conf_get_bool (empathy_conf_get (),
				       EMPATHY_PREFS_UI_MAIN_WINDOW_HIDDEN,
			               &should_hide);
	} else {
		should_hide = TRUE;
	}

#ifdef HAVE_LIBINDICATE
	empathy_conf_get_bool (empathy_conf_get (),
			       EMPATHY_PREFS_UI_USE_LIBINDICATE,
		               &use_libindicate);
	status_icon_set_use_libindicate (icon, use_libindicate);
#endif

	if (gtk_window_is_active (priv->window) == should_hide) {
		status_icon_set_visibility (icon, !should_hide, FALSE);
	}

	return icon;
}

