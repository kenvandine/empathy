/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
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

#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-account-manager.h>
#include <libempathy/empathy-dispatcher.h>
#include <libempathy/empathy-chatroom-manager.h>
#include <libempathy/empathy-chatroom.h>
#include <libempathy/empathy-contact-list.h>
#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-contact-factory.h>
#include <libempathy/empathy-status-presets.h>

#include <libempathy-gtk/empathy-contact-dialogs.h>
#include <libempathy-gtk/empathy-contact-list-store.h>
#include <libempathy-gtk/empathy-contact-list-view.h>
#include <libempathy-gtk/empathy-presence-chooser.h>
#include <libempathy-gtk/empathy-ui-utils.h>
#include <libempathy-gtk/empathy-geometry.h>
#include <libempathy-gtk/empathy-conf.h>
#include <libempathy-gtk/empathy-log-window.h>
#include <libempathy-gtk/empathy-new-message-dialog.h>
#include <libempathy-gtk/empathy-gtk-enum-types.h>

#include <libmissioncontrol/mission-control.h>

#include "empathy-accounts-dialog.h"
#include "empathy-main-window.h"
#include "ephy-spinner.h"
#include "empathy-preferences.h"
#include "empathy-about-dialog.h"
#include "empathy-new-chatroom-dialog.h"
#include "empathy-chatrooms-window.h"
#include "empathy-event-manager.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

/* Flashing delay for icons (milliseconds). */
#define FLASH_TIMEOUT 500

/* Minimum width of roster window if something goes wrong. */
#define MIN_WIDTH 50

/* Accels (menu shortcuts) can be configured and saved */
#define ACCELS_FILENAME "accels.txt"

/* Name in the geometry file */
#define GEOMETRY_NAME "main-window"

typedef struct {
	EmpathyContactListView  *list_view;
	EmpathyContactListStore *list_store;
	MissionControl          *mc;
	EmpathyAccountManager   *account_manager;
	EmpathyChatroomManager  *chatroom_manager;
	EmpathyEventManager     *event_manager;
	guint                    flash_timeout_id;
	gboolean                 flash_on;

	GtkWidget              *window;
	GtkWidget              *main_vbox;
	GtkWidget              *throbber;
	GtkWidget              *presence_toolbar;
	GtkWidget              *presence_chooser;
	GtkWidget              *errors_vbox;

	GtkWidget              *room;
	GtkWidget              *room_menu;
	GtkWidget              *room_sep;
	GtkWidget              *room_join_favorites;
	GtkWidget              *edit_context;
	GtkWidget              *edit_context_separator;
	GtkWidget              *chat_history_menu_item;

	guint                   size_timeout_id;
	GHashTable             *errors;

	/* Widgets that are enabled when there are connected accounts */
	GList                  *widgets_connected;
} EmpathyMainWindow;

static EmpathyMainWindow *window = NULL;

static void     main_window_destroy_cb                         (GtkWidget                *widget,
								EmpathyMainWindow        *window);
static void     main_window_favorite_chatroom_menu_setup       (EmpathyMainWindow        *window);
static void     main_window_favorite_chatroom_menu_added_cb    (EmpathyChatroomManager   *manager,
								EmpathyChatroom          *chatroom,
								EmpathyMainWindow        *window);
static void     main_window_favorite_chatroom_menu_removed_cb  (EmpathyChatroomManager   *manager,
								EmpathyChatroom          *chatroom,
								EmpathyMainWindow        *window);
static void     main_window_favorite_chatroom_menu_activate_cb (GtkMenuItem              *menu_item,
								EmpathyChatroom          *chatroom);
static void     main_window_favorite_chatroom_menu_update      (EmpathyMainWindow        *window);
static void     main_window_favorite_chatroom_menu_add         (EmpathyMainWindow        *window,
								EmpathyChatroom          *chatroom);
static void     main_window_favorite_chatroom_join             (EmpathyChatroom          *chatroom);
static void     main_window_chat_quit_cb                       (GtkWidget                *widget,
								EmpathyMainWindow        *window);
static void     main_window_chat_new_message_cb                (GtkWidget                *widget,
								EmpathyMainWindow        *window);
static void     main_window_chat_history_cb                    (GtkWidget                *widget,
								EmpathyMainWindow        *window);
static void     main_window_room_join_new_cb                   (GtkWidget                *widget,
								EmpathyMainWindow        *window);
static void     main_window_room_join_favorites_cb             (GtkWidget                *widget,
								EmpathyMainWindow        *window);
static void     main_window_room_manage_favorites_cb           (GtkWidget                *widget,
								EmpathyMainWindow        *window);
static void     main_window_chat_add_contact_cb                (GtkWidget                *widget,
								EmpathyMainWindow        *window);
static void     main_window_chat_show_offline_cb               (GtkCheckMenuItem         *item,
								EmpathyMainWindow        *window);
static gboolean main_window_edit_button_press_event_cb         (GtkWidget                *widget,
								GdkEventButton           *event,
								EmpathyMainWindow        *window);
static void     main_window_edit_accounts_cb                   (GtkWidget                *widget,
								EmpathyMainWindow        *window);
static void     main_window_edit_personal_information_cb       (GtkWidget                *widget,
								EmpathyMainWindow        *window);
static void     main_window_edit_preferences_cb                (GtkWidget                *widget,
								EmpathyMainWindow        *window);
static void     main_window_help_about_cb                      (GtkWidget                *widget,
								EmpathyMainWindow        *window);
static void     main_window_help_contents_cb                   (GtkWidget                *widget,
								EmpathyMainWindow        *window);
static gboolean main_window_throbber_button_press_event_cb     (GtkWidget                *throbber_ebox,
								GdkEventButton           *event,
								EmpathyMainWindow        *window);
static void     main_window_update_status                      (EmpathyMainWindow        *window,
								EmpathyAccountManager    *manager);
static void     main_window_error_display		       (EmpathyMainWindow        *window,
								McAccount                *account,
								const gchar              *message);
static void     main_window_accels_load                        (void);
static void     main_window_accels_save                        (void);
static void     main_window_connection_items_setup             (EmpathyMainWindow        *window,
								GladeXML                 *glade);
static gboolean main_window_configure_event_timeout_cb         (EmpathyMainWindow        *window);
static gboolean main_window_configure_event_cb                 (GtkWidget                *widget,
								GdkEventConfigure        *event,
								EmpathyMainWindow        *window);
static void     main_window_notify_show_offline_cb             (EmpathyConf              *conf,
								const gchar              *key,
								gpointer                  check_menu_item);
static void     main_window_notify_show_avatars_cb             (EmpathyConf              *conf,
								const gchar              *key,
								EmpathyMainWindow        *window);
static void     main_window_notify_compact_contact_list_cb     (EmpathyConf              *conf,
								const gchar              *key,
								EmpathyMainWindow        *window);
static void     main_window_notify_sort_criterium_cb           (EmpathyConf              *conf,
								const gchar              *key,
								EmpathyMainWindow        *window);
static void     main_window_account_created_or_deleted_cb      (EmpathyAccountManager    *manager,
								McAccount                *account,
								EmpathyMainWindow        *window);

static void
main_window_flash_stop (EmpathyMainWindow *window)
{
	if (window->flash_timeout_id == 0) {
		return;
	}

	DEBUG ("Stop flashing");
	g_source_remove (window->flash_timeout_id);
	window->flash_timeout_id = 0;
	window->flash_on = FALSE;
}

typedef struct {
	EmpathyEvent *event;
	gboolean      on;
} FlashForeachData;

static gboolean
main_window_flash_foreach (GtkTreeModel *model,
			   GtkTreePath  *path,
			   GtkTreeIter  *iter,
			   gpointer      user_data)
{
	FlashForeachData *data = (FlashForeachData*) user_data;
	EmpathyContact   *contact;
	const gchar      *icon_name;
	GtkTreePath      *parent_path = NULL;
	GtkTreeIter       parent_iter;

	/* To be used with gtk_tree_model_foreach, update the status icon
	 * of the contact to show the event icon (on=TRUE) or the presence
	 * (on=FALSE) */
	gtk_tree_model_get (model, iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, &contact,
			    -1);

	if (contact != data->event->contact) {
		if (contact) {
			g_object_unref (contact);
		}
		return FALSE;
	}

	if (data->on) {
		icon_name = data->event->icon_name;
	} else {
		icon_name = empathy_icon_name_for_contact (contact);
	}

	gtk_tree_store_set (GTK_TREE_STORE (model), iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_ICON_STATUS, icon_name,
			    -1);

	/* To make sure the parent is shown correctly, we emit
	 * the row-changed signal on the parent so it prompts
	 * it to be refreshed by the filter func. 
	 */
	if (gtk_tree_model_iter_parent (model, &parent_iter, iter)) {
		parent_path = gtk_tree_model_get_path (model, &parent_iter);
	}
	if (parent_path) {
		gtk_tree_model_row_changed (model, parent_path, &parent_iter);
		gtk_tree_path_free (parent_path);
	}

	g_object_unref (contact);

	return FALSE;
}

static gboolean
main_window_flash_cb (EmpathyMainWindow *window)
{
	GtkTreeModel     *model;
	GSList           *events, *l;
	gboolean          found_event = FALSE;
	FlashForeachData  data;

	window->flash_on = !window->flash_on;
	data.on = window->flash_on;
	model = GTK_TREE_MODEL (window->list_store);

	events = empathy_event_manager_get_events (window->event_manager);
	for (l = events; l; l = l->next) {
		data.event = l->data;
		if (!data.event->contact) {
			continue;
		}

		found_event = TRUE;
		gtk_tree_model_foreach (model,
					main_window_flash_foreach,
					&data);
	}

	if (!found_event) {
		main_window_flash_stop (window);
	}

	return TRUE;
}

static void
main_window_flash_start (EmpathyMainWindow *window)
{
	if (window->flash_timeout_id != 0) {
		return;
	}

	DEBUG ("Start flashing");
	window->flash_timeout_id = g_timeout_add (FLASH_TIMEOUT,
						  (GSourceFunc) main_window_flash_cb,
						  window);
	main_window_flash_cb (window);
}

static void
main_window_event_added_cb (EmpathyEventManager *manager,
			    EmpathyEvent        *event,
			    EmpathyMainWindow   *window)
{
	if (event->contact) {
		main_window_flash_start (window);
	}
}

static void
main_window_event_removed_cb (EmpathyEventManager *manager,
			      EmpathyEvent        *event,
			      EmpathyMainWindow   *window)
{
	FlashForeachData data;

	if (!event->contact) {
		return;
	}

	data.on = FALSE;
	data.event = event;
	gtk_tree_model_foreach (GTK_TREE_MODEL (window->list_store),
				main_window_flash_foreach,
				&data);
}

static void
main_window_row_activated_cb (EmpathyContactListView *view,
			      GtkTreePath            *path,
			      GtkTreeViewColumn      *col,
			      EmpathyMainWindow      *window)
{
	EmpathyContact *contact;
	GtkTreeModel   *model;
	GtkTreeIter     iter;
	GSList         *events, *l;

	model = GTK_TREE_MODEL (window->list_store);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, &contact,
			    -1);

	if (!contact) {
		return;
	}

	/* If the contact has an event activate it, otherwise the
	 * default handler of row-activated will be called. */
	events = empathy_event_manager_get_events (window->event_manager);
	for (l = events; l; l = l->next) {
		EmpathyEvent *event = l->data;

		if (event->contact == contact) {
			DEBUG ("Activate event");
			empathy_event_activate (event);

			/* We don't want the default handler of this signal
			 * (e.g. open a chat) */
			g_signal_stop_emission_by_name (view, "row-activated");
			break;
		}
	}

	g_object_unref (contact);
}

static void
main_window_connection_changed_cb (EmpathyAccountManager *manager,
				   McAccount *account,
				   TpConnectionStatusReason reason,
				   TpConnectionStatus current,
				   TpConnectionStatus previous,
				   EmpathyMainWindow *window)
{
	main_window_update_status (window, manager);

	if (current == TP_CONNECTION_STATUS_DISCONNECTED &&
	    reason != TP_CONNECTION_STATUS_REASON_REQUESTED) {
		const gchar *message;

		switch (reason) {
		case TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED:
			message = _("No error specified");
			break;
		case TP_CONNECTION_STATUS_REASON_NETWORK_ERROR:
			message = _("Network error");
			break;
		case TP_CONNECTION_STATUS_REASON_AUTHENTICATION_FAILED:
			message = _("Authentication failed");
			break;
		case TP_CONNECTION_STATUS_REASON_ENCRYPTION_ERROR:
			message = _("Encryption error");
			break;
		case TP_CONNECTION_STATUS_REASON_NAME_IN_USE:
			message = _("Name in use");
			break;
		case TP_CONNECTION_STATUS_REASON_CERT_NOT_PROVIDED:
			message = _("Certificate not provided");
			break;
		case TP_CONNECTION_STATUS_REASON_CERT_UNTRUSTED:
			message = _("Certificate untrusted");
			break;
		case TP_CONNECTION_STATUS_REASON_CERT_EXPIRED:
			message = _("Certificate expired");
			break;
		case TP_CONNECTION_STATUS_REASON_CERT_NOT_ACTIVATED:
			message = _("Certificate not activated");
			break;
		case TP_CONNECTION_STATUS_REASON_CERT_HOSTNAME_MISMATCH:
			message = _("Certificate hostname mismatch");
			break;
		case TP_CONNECTION_STATUS_REASON_CERT_FINGERPRINT_MISMATCH:
			message = _("Certificate fingerprint mismatch");
			break;
		case TP_CONNECTION_STATUS_REASON_CERT_SELF_SIGNED:
			message = _("Certificate self-signed");
			break;
		case TP_CONNECTION_STATUS_REASON_CERT_OTHER_ERROR:
			message = _("Certificate error");
			break;
		default:
			message = _("Unknown error");
			break;
		}

		main_window_error_display (window, account, message);
	}

	if (current == TP_CONNECTION_STATUS_DISCONNECTED) {
		empathy_sound_play (GTK_WIDGET (window->window),
				    EMPATHY_SOUND_ACCOUNT_DISCONNECTED);
	}

	if (current == TP_CONNECTION_STATUS_CONNECTED) {
		GtkWidget *error_widget;

		empathy_sound_play (GTK_WIDGET (window->window),
				    EMPATHY_SOUND_ACCOUNT_CONNECTED);

		/* Account connected without error, remove error message if any */
		error_widget = g_hash_table_lookup (window->errors, account);
		if (error_widget) {
			gtk_widget_destroy (error_widget);
			g_hash_table_remove (window->errors, account);
		}
	}
}

static void
main_window_contact_presence_changed_cb (EmpathyContactMonitor *monitor,
					 EmpathyContact *contact,
					 McPresence current,
					 McPresence previous,
					 EmpathyMainWindow *window)
{
	McAccount *account;
	gboolean should_play;

	account = empathy_contact_get_account (contact);
	should_play = !empathy_account_manager_is_account_just_connected (window->account_manager, account);

	if (!should_play) {
		return;
	}

	if (previous < MC_PRESENCE_AVAILABLE && current > MC_PRESENCE_OFFLINE) {
		/* someone is logging in */
		empathy_sound_play (GTK_WIDGET (window->window),
				    EMPATHY_SOUND_CONTACT_CONNECTED);
		return;
	}

	if (previous > MC_PRESENCE_OFFLINE && current < MC_PRESENCE_AVAILABLE) {
		/* someone is logging off */
		empathy_sound_play (GTK_WIDGET (window->window),
				    EMPATHY_SOUND_CONTACT_DISCONNECTED);
	}
}

GtkWidget *
empathy_main_window_get (void)
{
  return window != NULL ? window->window : NULL;
}

GtkWidget *
empathy_main_window_show (void)
{
	EmpathyContactList       *list_iface;
	EmpathyContactMonitor    *monitor;
	GladeXML                 *glade;
	EmpathyConf               *conf;
	GtkWidget                *sw;
	GtkWidget                *show_offline_widget;
	GtkWidget                *ebox;
	GtkToolItem              *item;
	gboolean                  show_offline;
	gboolean                  show_avatars;
	gboolean                  compact_contact_list;
	gint                      x, y, w, h;
	gchar                    *filename;
	GSList                   *l;

	if (window) {
		empathy_window_present (GTK_WINDOW (window->window), TRUE);
		return window->window;
	}

	window = g_new0 (EmpathyMainWindow, 1);

	/* Set up interface */
	filename = empathy_file_lookup ("empathy-main-window.glade", "src");
	glade = empathy_glade_get_file (filename,
				       "main_window",
				       NULL,
				       "main_window", &window->window,
				       "main_vbox", &window->main_vbox,
				       "errors_vbox", &window->errors_vbox,
				       "chat_show_offline", &show_offline_widget,
				       "room", &window->room,
				       "room_sep", &window->room_sep,
				       "room_join_favorites", &window->room_join_favorites,
				       "edit_context", &window->edit_context,
				       "edit_context_separator", &window->edit_context_separator,
				       "presence_toolbar", &window->presence_toolbar,
				       "roster_scrolledwindow", &sw,
				       "chat_history", &window->chat_history_menu_item,
				       NULL);
	g_free (filename);

	empathy_glade_connect (glade,
			      window,
			      "main_window", "destroy", main_window_destroy_cb,
			      "main_window", "configure_event", main_window_configure_event_cb,
			      "chat_quit", "activate", main_window_chat_quit_cb,
			      "chat_new_message", "activate", main_window_chat_new_message_cb,
			      "chat_history", "activate", main_window_chat_history_cb,
			      "room_join_new", "activate", main_window_room_join_new_cb,
			      "room_join_favorites", "activate", main_window_room_join_favorites_cb,
			      "room_manage_favorites", "activate", main_window_room_manage_favorites_cb,
			      "chat_add_contact", "activate", main_window_chat_add_contact_cb,
			      "chat_show_offline", "toggled", main_window_chat_show_offline_cb,
			      "edit", "button-press-event", main_window_edit_button_press_event_cb,
			      "edit_accounts", "activate", main_window_edit_accounts_cb,
			      "edit_personal_information", "activate", main_window_edit_personal_information_cb,
			      "edit_preferences", "activate", main_window_edit_preferences_cb,
			      "help_about", "activate", main_window_help_about_cb,
			      "help_contents", "activate", main_window_help_contents_cb,
			      NULL);

	/* Set up connection related widgets. */
	main_window_connection_items_setup (window, glade);
	g_object_unref (glade);

	window->mc = empathy_mission_control_dup_singleton ();
	window->account_manager = empathy_account_manager_dup_singleton ();

	g_signal_connect (window->account_manager,
			  "account-connection-changed",
			  G_CALLBACK (main_window_connection_changed_cb), window);

	window->errors = g_hash_table_new_full (empathy_account_hash,
						empathy_account_equal,
						g_object_unref,
						NULL);

	/* Set up menu */
	main_window_favorite_chatroom_menu_setup (window);

	gtk_widget_hide (window->edit_context);
	gtk_widget_hide (window->edit_context_separator);

	/* Set up presence chooser */
	window->presence_chooser = empathy_presence_chooser_new ();
	gtk_widget_show (window->presence_chooser);
	item = gtk_tool_item_new ();
	gtk_widget_show (GTK_WIDGET (item));
	gtk_container_add (GTK_CONTAINER (item), window->presence_chooser);
	gtk_tool_item_set_is_important (item, TRUE);
	gtk_tool_item_set_expand (item, TRUE);
	gtk_toolbar_insert (GTK_TOOLBAR (window->presence_toolbar), item, -1);

	/* Set up the throbber */
	ebox = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);
	gtk_widget_set_tooltip_text (ebox, _("Show and edit accounts"));
	g_signal_connect (ebox,
			  "button-press-event",
			  G_CALLBACK (main_window_throbber_button_press_event_cb),
			  window);
	gtk_widget_show (ebox);

	window->throbber = ephy_spinner_new ();
	ephy_spinner_set_size (EPHY_SPINNER (window->throbber), GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_container_add (GTK_CONTAINER (ebox), window->throbber);
	gtk_widget_show (window->throbber);

	item = gtk_tool_item_new ();
	gtk_container_add (GTK_CONTAINER (item), ebox);
	gtk_toolbar_insert (GTK_TOOLBAR (window->presence_toolbar), item, -1);
	gtk_widget_show (GTK_WIDGET (item));

	/* Set up contact list. */
	empathy_status_presets_get_all ();

	list_iface = EMPATHY_CONTACT_LIST (empathy_contact_manager_dup_singleton ());
	monitor = empathy_contact_list_get_monitor (list_iface);
	window->list_store = empathy_contact_list_store_new (list_iface);
	window->list_view = empathy_contact_list_view_new (window->list_store,
							   EMPATHY_CONTACT_LIST_FEATURE_ALL,
							   EMPATHY_CONTACT_FEATURE_ALL);
	g_signal_connect (monitor, "contact-presence-changed",
			  G_CALLBACK (main_window_contact_presence_changed_cb), window);
	g_object_unref (list_iface);

	gtk_widget_show (GTK_WIDGET (window->list_view));
	gtk_container_add (GTK_CONTAINER (sw),
			   GTK_WIDGET (window->list_view));
	g_signal_connect (window->list_view, "row-activated",
			  G_CALLBACK (main_window_row_activated_cb),
			  window);

	/* Load user-defined accelerators. */
	main_window_accels_load ();

	/* Set window size. */
	empathy_geometry_load (GEOMETRY_NAME, &x, &y, &w, &h);

	if (w >= 1 && h >= 1) {
		/* Use the defaults from the glade file if we
		 * don't have good w, h geometry.
		 */
		DEBUG ("Configuring window default size w:%d, h:%d", w, h);
		gtk_window_set_default_size (GTK_WINDOW (window->window), w, h);
	}

	if (x >= 0 && y >= 0) {
		/* Let the window manager position it if we
		 * don't have good x, y coordinates.
		 */
		DEBUG ("Configuring window default position x:%d, y:%d", x, y);
		gtk_window_move (GTK_WINDOW (window->window), x, y);
	}

	/* Enable event handling */
	window->event_manager = empathy_event_manager_dup_singleton ();
	g_signal_connect (window->event_manager, "event-added",
			  G_CALLBACK (main_window_event_added_cb),
			  window);
	g_signal_connect (window->event_manager, "event-removed",
			  G_CALLBACK (main_window_event_removed_cb),
			  window);

	g_signal_connect (window->account_manager, "account-created",
			  G_CALLBACK (main_window_account_created_or_deleted_cb),
			  window);
	g_signal_connect (window->account_manager, "account-deleted",
			  G_CALLBACK (main_window_account_created_or_deleted_cb),
			  window);
	main_window_account_created_or_deleted_cb (window->account_manager, NULL, window);

	l = empathy_event_manager_get_events (window->event_manager);
	while (l) {
		main_window_event_added_cb (window->event_manager,
					    l->data, window);
		l = l->next;
	}

	conf = empathy_conf_get ();

	/* Show offline ? */
	empathy_conf_get_bool (conf,
			      EMPATHY_PREFS_CONTACTS_SHOW_OFFLINE,
			      &show_offline);
	empathy_conf_notify_add (conf,
				EMPATHY_PREFS_CONTACTS_SHOW_OFFLINE,
				main_window_notify_show_offline_cb,
				show_offline_widget);

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (show_offline_widget),
					show_offline);

	/* Show avatars ? */
	empathy_conf_get_bool (conf,
			      EMPATHY_PREFS_UI_SHOW_AVATARS,
			      &show_avatars);
	empathy_conf_notify_add (conf,
				EMPATHY_PREFS_UI_SHOW_AVATARS,
				(EmpathyConfNotifyFunc) main_window_notify_show_avatars_cb,
				window);
	empathy_contact_list_store_set_show_avatars (window->list_store, show_avatars);

	/* Is compact ? */
	empathy_conf_get_bool (conf,
			      EMPATHY_PREFS_UI_COMPACT_CONTACT_LIST,
			      &compact_contact_list);
	empathy_conf_notify_add (conf,
				EMPATHY_PREFS_UI_COMPACT_CONTACT_LIST,
				(EmpathyConfNotifyFunc) main_window_notify_compact_contact_list_cb,
				window);
	empathy_contact_list_store_set_is_compact (window->list_store, compact_contact_list);

	/* Sort criterium */
	empathy_conf_notify_add (conf,
				EMPATHY_PREFS_CONTACTS_SORT_CRITERIUM,
				(EmpathyConfNotifyFunc) main_window_notify_sort_criterium_cb,
				window);
	main_window_notify_sort_criterium_cb (conf,
					      EMPATHY_PREFS_CONTACTS_SORT_CRITERIUM,
					      window);

	main_window_update_status (window, window->account_manager);

	return window->window;
}

static void
main_window_destroy_cb (GtkWidget         *widget,
			EmpathyMainWindow *window)
{
	/* Save user-defined accelerators. */
	main_window_accels_save ();

	g_signal_handlers_disconnect_by_func (window->account_manager,
					      main_window_connection_changed_cb,
					      window);

	if (window->size_timeout_id) {
		g_source_remove (window->size_timeout_id);
	}

	g_list_free (window->widgets_connected);

	g_object_unref (window->mc);
	g_object_unref (window->account_manager);
	g_object_unref (window->list_store);
	g_hash_table_destroy (window->errors);

	g_signal_handlers_disconnect_by_func (window->event_manager,
			  		      main_window_event_added_cb,
			  		      window);
	g_signal_handlers_disconnect_by_func (window->event_manager,
			  		      main_window_event_removed_cb,
			  		      window);
	g_object_unref (window->event_manager);

	g_free (window);
}

static void
main_window_favorite_chatroom_menu_setup (EmpathyMainWindow *window)
{
	GList *chatrooms, *l;

	window->chatroom_manager = empathy_chatroom_manager_dup_singleton (NULL);
	chatrooms = empathy_chatroom_manager_get_chatrooms (window->chatroom_manager, NULL);
	window->room_menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (window->room));

	for (l = chatrooms; l; l = l->next) {
		main_window_favorite_chatroom_menu_add (window, l->data);
	}

	if (!chatrooms) {
		gtk_widget_hide (window->room_sep);
	}

	gtk_widget_set_sensitive (window->room_join_favorites, chatrooms != NULL);

	g_signal_connect (window->chatroom_manager, "chatroom-added",
			  G_CALLBACK (main_window_favorite_chatroom_menu_added_cb),
			  window);
	g_signal_connect (window->chatroom_manager, "chatroom-removed",
			  G_CALLBACK (main_window_favorite_chatroom_menu_removed_cb),
			  window);

	g_list_free (chatrooms);
}

static void
main_window_favorite_chatroom_menu_added_cb (EmpathyChatroomManager *manager,
					     EmpathyChatroom        *chatroom,
					     EmpathyMainWindow     *window)
{
	main_window_favorite_chatroom_menu_add (window, chatroom);
	gtk_widget_show (window->room_sep);
	gtk_widget_set_sensitive (window->room_join_favorites, TRUE);
}

static void
main_window_favorite_chatroom_menu_removed_cb (EmpathyChatroomManager *manager,
					       EmpathyChatroom        *chatroom,
					       EmpathyMainWindow     *window)
{
	GtkWidget *menu_item;

	menu_item = g_object_get_data (G_OBJECT (chatroom), "menu_item");

	g_object_set_data (G_OBJECT (chatroom), "menu_item", NULL);
	gtk_widget_destroy (menu_item);

	main_window_favorite_chatroom_menu_update (window);
}

static void
main_window_favorite_chatroom_menu_activate_cb (GtkMenuItem    *menu_item,
						EmpathyChatroom *chatroom)
{
	main_window_favorite_chatroom_join (chatroom);
}

static void
main_window_favorite_chatroom_menu_update (EmpathyMainWindow *window)
{
	GList *chatrooms;

	chatrooms = empathy_chatroom_manager_get_chatrooms (window->chatroom_manager, NULL);

	if (chatrooms) {
		gtk_widget_show (window->room_sep);
	} else {
		gtk_widget_hide (window->room_sep);
	}

	gtk_widget_set_sensitive (window->room_join_favorites, chatrooms != NULL);
	g_list_free (chatrooms);
}

static void
main_window_favorite_chatroom_menu_add (EmpathyMainWindow *window,
					EmpathyChatroom    *chatroom)
{
	GtkWidget   *menu_item;
	const gchar *name;

	if (g_object_get_data (G_OBJECT (chatroom), "menu_item")) {
		return;
	}

	name = empathy_chatroom_get_name (chatroom);
	menu_item = gtk_menu_item_new_with_label (name);

	g_object_set_data (G_OBJECT (chatroom), "menu_item", menu_item);
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (main_window_favorite_chatroom_menu_activate_cb),
			  chatroom);

	gtk_menu_shell_insert (GTK_MENU_SHELL (window->room_menu),
			       menu_item, 3);

	gtk_widget_show (menu_item);
}

static void
main_window_favorite_chatroom_join (EmpathyChatroom *chatroom)
{
	MissionControl *mc;
	McAccount      *account;
	const gchar    *room;

	mc = empathy_mission_control_dup_singleton ();
	account = empathy_chatroom_get_account (chatroom);
	room = empathy_chatroom_get_room (chatroom);

	 if (mission_control_get_connection_status (mc, account, NULL) !=
			TP_CONNECTION_STATUS_CONNECTED) {
		return;
	}

	DEBUG ("Requesting channel for '%s'", room);
	empathy_dispatcher_join_muc (account, room, NULL, NULL);

	g_object_unref (mc);
}

static void
main_window_chat_quit_cb (GtkWidget         *widget,
			  EmpathyMainWindow *window)
{
	gtk_main_quit ();
}

static void
main_window_chat_new_message_cb (GtkWidget         *widget,
				 EmpathyMainWindow *window)
{
	empathy_new_message_dialog_show (GTK_WINDOW (window->window));
}

static void
main_window_chat_history_cb (GtkWidget         *widget,
			     EmpathyMainWindow *window)
{
	empathy_log_window_show (NULL, NULL, FALSE, GTK_WINDOW (window->window));
}

static void
main_window_room_join_new_cb (GtkWidget         *widget,
			      EmpathyMainWindow *window)
{
	empathy_new_chatroom_dialog_show (GTK_WINDOW (window->window));
}

static void
main_window_room_join_favorites_cb (GtkWidget         *widget,
				    EmpathyMainWindow *window)
{
	GList *chatrooms, *l;

	chatrooms = empathy_chatroom_manager_get_chatrooms (window->chatroom_manager, NULL);
	for (l = chatrooms; l; l = l->next) {
		main_window_favorite_chatroom_join (l->data);
	}
	g_list_free (chatrooms);
}

static void
main_window_room_manage_favorites_cb (GtkWidget         *widget,
				      EmpathyMainWindow *window)
{
	empathy_chatrooms_window_show (GTK_WINDOW (window->window));
}

static void
main_window_chat_add_contact_cb (GtkWidget         *widget,
				 EmpathyMainWindow *window)
{
	empathy_new_contact_dialog_show (GTK_WINDOW (window->window));
}

static void
main_window_chat_show_offline_cb (GtkCheckMenuItem  *item,
				  EmpathyMainWindow *window)
{
	gboolean current;

	current = gtk_check_menu_item_get_active (item);

	empathy_conf_set_bool (empathy_conf_get (),
			      EMPATHY_PREFS_CONTACTS_SHOW_OFFLINE,
			      current);

	/* Turn off sound just while we alter the contact list. */
	// FIXME: empathy_sound_set_enabled (FALSE);
	empathy_contact_list_store_set_show_offline (window->list_store, current);
	//empathy_sound_set_enabled (TRUE);
}

static gboolean
main_window_edit_button_press_event_cb (GtkWidget         *widget,
					GdkEventButton    *event,
					EmpathyMainWindow *window)
{
	GtkWidget *submenu;

	if (!event->button == 1) {
		return FALSE;
	}

	submenu = empathy_contact_list_view_get_contact_menu (window->list_view);
	if (submenu) {
		GtkMenuItem *item;
		GtkWidget   *label;

		item = GTK_MENU_ITEM (window->edit_context);
		label = gtk_bin_get_child (GTK_BIN (item));
		gtk_label_set_text (GTK_LABEL (label), _("Contact"));

		gtk_widget_show (window->edit_context);
		gtk_widget_show (window->edit_context_separator);

		gtk_menu_item_set_submenu (item, submenu);

		return FALSE;
	}

	submenu = empathy_contact_list_view_get_group_menu (window->list_view);
	if (submenu) {
		GtkMenuItem *item;
		GtkWidget   *label;

		item = GTK_MENU_ITEM (window->edit_context);
		label = gtk_bin_get_child (GTK_BIN (item));
		gtk_label_set_text (GTK_LABEL (label), _("Group"));

		gtk_widget_show (window->edit_context);
		gtk_widget_show (window->edit_context_separator);

		gtk_menu_item_set_submenu (item, submenu);

		return FALSE;
	}

	gtk_widget_hide (window->edit_context);
	gtk_widget_hide (window->edit_context_separator);

	return FALSE;
}

static void
main_window_edit_accounts_cb (GtkWidget         *widget,
			      EmpathyMainWindow *window)
{
	empathy_accounts_dialog_show (GTK_WINDOW (window->window), NULL);
}

static void
main_window_edit_personal_information_cb (GtkWidget         *widget,
					  EmpathyMainWindow *window)
{
	GSList *accounts;

	accounts = mission_control_get_online_connections (window->mc, NULL);
	if (accounts) {
		EmpathyContactFactory *factory;
		EmpathyContact        *contact;
		McAccount             *account;

		account = accounts->data;
		factory = empathy_contact_factory_dup_singleton ();
		contact = empathy_contact_factory_get_user (factory, account);
		empathy_contact_run_until_ready (contact,
						 EMPATHY_CONTACT_READY_HANDLE |
						 EMPATHY_CONTACT_READY_ID,
						 NULL);

		empathy_contact_information_dialog_show (contact,
							 GTK_WINDOW (window->window),
							 TRUE, TRUE);

		g_slist_foreach (accounts, (GFunc) g_object_unref, NULL);
		g_slist_free (accounts);
		g_object_unref (factory);
		g_object_unref (contact);
	}
}

static void
main_window_edit_preferences_cb (GtkWidget         *widget,
				 EmpathyMainWindow *window)
{
	empathy_preferences_show (GTK_WINDOW (window->window));
}

static void
main_window_help_about_cb (GtkWidget         *widget,
			   EmpathyMainWindow *window)
{
	empathy_about_dialog_new (GTK_WINDOW (window->window));
}

static void
main_window_help_contents_cb (GtkWidget         *widget,
			      EmpathyMainWindow *window)
{
	empathy_url_show (widget, "ghelp:empathy");
}

static gboolean
main_window_throbber_button_press_event_cb (GtkWidget         *throbber_ebox,
					    GdkEventButton    *event,
					    EmpathyMainWindow *window)
{
	if (event->type != GDK_BUTTON_PRESS ||
	    event->button != 1) {
		return FALSE;
	}

	empathy_accounts_dialog_show (GTK_WINDOW (window->window), NULL);

	return FALSE;
}

static void
main_window_error_edit_clicked_cb (GtkButton         *button,
				   EmpathyMainWindow *window)
{
	McAccount *account;
	GtkWidget *error_widget;

	account = g_object_get_data (G_OBJECT (button), "account");
	empathy_accounts_dialog_show (GTK_WINDOW (window->window), account);

	error_widget = g_hash_table_lookup (window->errors, account);
	gtk_widget_destroy (error_widget);
	g_hash_table_remove (window->errors, account);
}

static void
main_window_error_clear_clicked_cb (GtkButton         *button,
				    EmpathyMainWindow *window)
{
	McAccount *account;
	GtkWidget *error_widget;

	account = g_object_get_data (G_OBJECT (button), "account");
	error_widget = g_hash_table_lookup (window->errors, account);
	gtk_widget_destroy (error_widget);
	g_hash_table_remove (window->errors, account);
}

static void
main_window_error_display (EmpathyMainWindow *window,
			   McAccount         *account,
			   const gchar       *message)
{
	GtkWidget *child;
	GtkWidget *table;
	GtkWidget *image;
	GtkWidget *button_edit;
	GtkWidget *alignment;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *fixed;
	GtkWidget *vbox;
	GtkWidget *button_close;
	gchar     *str;

	child = g_hash_table_lookup (window->errors, account);
	if (child) {
		label = g_object_get_data (G_OBJECT (child), "label");

		/* Just set the latest error and return */
		str = g_markup_printf_escaped ("<b>%s</b>\n%s",
					       mc_account_get_display_name (account),
					       message);
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);

		return;
	}

	child = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (window->errors_vbox), child, FALSE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (child), 6);
	gtk_widget_show (child);

	table = gtk_table_new (2, 4, FALSE);
	gtk_widget_show (table);
	gtk_box_pack_start (GTK_BOX (child), table, TRUE, TRUE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (table), 12);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);

	image = gtk_image_new_from_stock (GTK_STOCK_DISCONNECT, GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);
	gtk_table_attach (GTK_TABLE (table), image, 0, 1, 0, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0);

	button_edit = gtk_button_new ();
	gtk_widget_show (button_edit);
	gtk_table_attach (GTK_TABLE (table), button_edit, 1, 2, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);

	alignment = gtk_alignment_new (0.5, 0.5, 0, 0);
	gtk_widget_show (alignment);
	gtk_container_add (GTK_CONTAINER (button_edit), alignment);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (alignment), hbox);

	image = gtk_image_new_from_stock (GTK_STOCK_EDIT, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

	label = gtk_label_new_with_mnemonic (_("_Edit account"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	fixed = gtk_fixed_new ();
	gtk_widget_show (fixed);
	gtk_table_attach (GTK_TABLE (table), fixed, 2, 3, 1, 2,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_table_attach (GTK_TABLE (table), vbox, 3, 4, 0, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);

	button_close = gtk_button_new ();
	gtk_widget_show (button_close);
	gtk_box_pack_start (GTK_BOX (vbox), button_close, FALSE, FALSE, 0);
	gtk_button_set_relief (GTK_BUTTON (button_close), GTK_RELIEF_NONE);


	image = gtk_image_new_from_stock ("gtk-close", GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);
	gtk_container_add (GTK_CONTAINER (button_close), image);

	label = gtk_label_new ("");
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label, 1, 3, 0, 1,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
			  (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL), 0, 0);
	gtk_widget_set_size_request (label, 175, -1);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);

	str = g_markup_printf_escaped ("<b>%s</b>\n%s",
				       mc_account_get_display_name (account),
				       message);
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);

	g_object_set_data (G_OBJECT (child), "label", label);
	g_object_set_data_full (G_OBJECT (button_edit),
				"account", g_object_ref (account),
				g_object_unref);
	g_object_set_data_full (G_OBJECT (button_close),
				"account", g_object_ref (account),
				g_object_unref);

	g_signal_connect (button_edit, "clicked",
			  G_CALLBACK (main_window_error_edit_clicked_cb),
			  window);

	g_signal_connect (button_close, "clicked",
			  G_CALLBACK (main_window_error_clear_clicked_cb),
			  window);

	gtk_widget_show (window->errors_vbox);

	g_hash_table_insert (window->errors, g_object_ref (account), child);
}

static void
main_window_update_status (EmpathyMainWindow *window, EmpathyAccountManager *manager)
{
	int  connected;
	int  connecting;
	GList *l;

	/* Count number of connected/connecting/disconnected accounts */
	connected = empathy_account_manager_get_connected_accounts (manager);
	connecting = empathy_account_manager_get_connecting_accounts (manager);

	/* Update the spinner state */
	if (connecting > 0) {
		ephy_spinner_start (EPHY_SPINNER (window->throbber));
	} else {
		ephy_spinner_stop (EPHY_SPINNER (window->throbber));
	}

	/* Update widgets sensibility */
	for (l = window->widgets_connected; l; l = l->next) {
		gtk_widget_set_sensitive (l->data, (connected > 0));
	}
}

/*
 * Accels
 */
static void
main_window_accels_load (void)
{
	gchar *filename;

	filename = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, ACCELS_FILENAME, NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		DEBUG ("Loading from:'%s'", filename);
		gtk_accel_map_load (filename);
	}

	g_free (filename);
}

static void
main_window_accels_save (void)
{
	gchar *dir;
	gchar *file_with_path;

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
	g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	file_with_path = g_build_filename (dir, ACCELS_FILENAME, NULL);
	g_free (dir);

	DEBUG ("Saving to:'%s'", file_with_path);
	gtk_accel_map_save (file_with_path);

	g_free (file_with_path);
}

static void
main_window_connection_items_setup (EmpathyMainWindow *window,
				    GladeXML          *glade)
{
	GList         *list;
	GtkWidget     *w;
	gint           i;
	const gchar *widgets_connected[] = {
		"room",
		"chat_new_message",
		"chat_add_contact",
		"edit_personal_information"
	};

	for (i = 0, list = NULL; i < G_N_ELEMENTS (widgets_connected); i++) {
		w = glade_xml_get_widget (glade, widgets_connected[i]);
		list = g_list_prepend (list, w);
	}

	window->widgets_connected = list;
}

static gboolean
main_window_configure_event_timeout_cb (EmpathyMainWindow *window)
{
	gint x, y, w, h;

	gtk_window_get_size (GTK_WINDOW (window->window), &w, &h);
	gtk_window_get_position (GTK_WINDOW (window->window), &x, &y);

	empathy_geometry_save (GEOMETRY_NAME, x, y, w, h);

	window->size_timeout_id = 0;

	return FALSE;
}

static gboolean
main_window_configure_event_cb (GtkWidget         *widget,
				GdkEventConfigure *event,
				EmpathyMainWindow *window)
{
	if (window->size_timeout_id) {
		g_source_remove (window->size_timeout_id);
	}

	window->size_timeout_id = g_timeout_add_seconds (1,
							 (GSourceFunc) main_window_configure_event_timeout_cb,
							 window);

	return FALSE;
}

static void
main_window_notify_show_offline_cb (EmpathyConf  *conf,
				    const gchar *key,
				    gpointer     check_menu_item)
{
	gboolean show_offline;

	if (empathy_conf_get_bool (conf, key, &show_offline)) {
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (check_menu_item),
						show_offline);
	}
}

static void
main_window_notify_show_avatars_cb (EmpathyConf        *conf,
				    const gchar       *key,
				    EmpathyMainWindow *window)
{
	gboolean show_avatars;

	if (empathy_conf_get_bool (conf, key, &show_avatars)) {
		empathy_contact_list_store_set_show_avatars (window->list_store,
							    show_avatars);
	}
}

static void
main_window_notify_compact_contact_list_cb (EmpathyConf        *conf,
					    const gchar       *key,
					    EmpathyMainWindow *window)
{
	gboolean compact_contact_list;

	if (empathy_conf_get_bool (conf, key, &compact_contact_list)) {
		empathy_contact_list_store_set_is_compact (window->list_store,
							  compact_contact_list);
	}
}

static void
main_window_notify_sort_criterium_cb (EmpathyConf       *conf,
				      const gchar       *key,
				      EmpathyMainWindow *window)
{
	gchar *str = NULL;

	if (empathy_conf_get_string (conf, key, &str) && str) {
		GType       type;
		GEnumClass *enum_class;
		GEnumValue *enum_value;

		type = empathy_contact_list_store_sort_get_type ();
		enum_class = G_ENUM_CLASS (g_type_class_peek (type));
		enum_value = g_enum_get_value_by_nick (enum_class, str);
		g_free (str);

		if (enum_value) {
			empathy_contact_list_store_set_sort_criterium (window->list_store, 
								       enum_value->value);
		}
	}
}

static void
main_window_account_created_or_deleted_cb (EmpathyAccountManager  *manager,
					   McAccount              *account,
					   EmpathyMainWindow      *window)
{
	gtk_widget_set_sensitive (GTK_WIDGET (window->chat_history_menu_item),
		empathy_account_manager_get_count (manager) > 0);
}
