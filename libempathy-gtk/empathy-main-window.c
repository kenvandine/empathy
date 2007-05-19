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

#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libempathy/gossip-conf.h>
#include <libempathy/gossip-contact.h>
#include <libempathy/gossip-debug.h>
#include <libempathy/gossip-utils.h>
#include <libempathy/empathy-contact-list.h>
#include <libempathy/empathy-contact-manager.h>

#include "empathy-main-window.h"
#include "ephy-spinner.h"
#include "gossip-contact-list-store.h"
#include "gossip-contact-list-view.h"
#include "gossip-presence-chooser.h"
#include "gossip-ui-utils.h"
#include "gossip-status-presets.h"
#include "gossip-geometry.h"
#include "gossip-preferences.h"
#include "gossip-accounts-dialog.h"
#include "gossip-about-dialog.h"


#define DEBUG_DOMAIN "MainWindow"

/* Minimum width of roster window if something goes wrong. */
#define MIN_WIDTH 50

/* Accels (menu shortcuts) can be configured and saved */
#define ACCELS_FILENAME "accels.txt"

/* Name in the geometry file */
#define GEOMETRY_NAME "main-window"

typedef struct {
	GossipContactListView  *list_view;
	GossipContactListStore *list_store;
	MissionControl         *mc;

	/* Main widgets */
	GtkWidget              *window;
	GtkWidget              *main_vbox;

	/* Tooltips for all widgets */
	GtkTooltips            *tooltips;

	/* Menu widgets */
	GtkWidget              *room;
	GtkWidget              *room_menu;
	GtkWidget              *room_sep;
	GtkWidget              *room_join_favorites;
	GtkWidget              *edit_context;
	GtkWidget              *edit_context_separator;

	/* Throbber */
	GtkWidget              *throbber;

	/* Widgets that are enabled when there is... */
	GList                  *widgets_connected;		/* ... connected accounts */
	GList                  *widgets_disconnected;	/* ... disconnected accounts */

	/* Status popup */
	GtkWidget              *presence_toolbar;
	GtkWidget              *presence_chooser;

	/* Misc */
	guint                   size_timeout_id;
} EmpathyMainWindow;

static void     main_window_destroy_cb                     (GtkWidget                       *widget,
							    EmpathyMainWindow               *window);
static void     main_window_favorite_chatroom_menu_setup   (void);
static void     main_window_chat_quit_cb                   (GtkWidget                       *widget,
							    EmpathyMainWindow               *window);
static void     main_window_chat_new_message_cb            (GtkWidget                       *widget,
							    EmpathyMainWindow               *window);
static void     main_window_chat_history_cb                (GtkWidget                       *widget,
							    EmpathyMainWindow               *window);
static void     main_window_room_join_new_cb               (GtkWidget                       *widget,
							    EmpathyMainWindow               *window);
static void     main_window_room_join_favorites_cb         (GtkWidget                       *widget,
							    EmpathyMainWindow               *window);
static void     main_window_room_manage_favorites_cb       (GtkWidget                       *widget,
							    EmpathyMainWindow               *window);
static void     main_window_chat_add_contact_cb            (GtkWidget                       *widget,
							    EmpathyMainWindow               *window);
static void     main_window_chat_show_offline_cb           (GtkCheckMenuItem                *item,
							    EmpathyMainWindow               *window);
static gboolean main_window_edit_button_press_event_cb     (GtkWidget                       *widget,
							    GdkEventButton                  *event,
							    EmpathyMainWindow               *window);
static void     main_window_edit_accounts_cb               (GtkWidget                       *widget,
							    EmpathyMainWindow               *window);
static void     main_window_edit_personal_information_cb   (GtkWidget                       *widget,
							    EmpathyMainWindow               *window);
static void     main_window_edit_preferences_cb            (GtkWidget                       *widget,
							    EmpathyMainWindow               *window);
static void     main_window_help_about_cb                  (GtkWidget                       *widget,
							    EmpathyMainWindow               *window);
static void     main_window_help_contents_cb               (GtkWidget                       *widget,
							    EmpathyMainWindow               *window);
static gboolean main_window_throbber_button_press_event_cb (GtkWidget                       *throbber_ebox,
							    GdkEventButton                  *event,
							    gpointer                         user_data);
static void     main_window_status_changed_cb              (MissionControl                  *mc,
							    TelepathyConnectionStatus        status,
							    McPresence                       presence,
							    TelepathyConnectionStatusReason  reason,
							    const gchar                     *unique_name,
							    EmpathyMainWindow               *window);
static void     main_window_update_status                  (EmpathyMainWindow               *window);
static void     main_window_accels_load                    (void);
static void     main_window_accels_save                    (void);
static void     main_window_connection_items_setup         (EmpathyMainWindow               *window,
							    GladeXML                        *glade);
static gboolean main_window_configure_event_timeout_cb     (EmpathyMainWindow               *window);
static gboolean main_window_configure_event_cb             (GtkWidget                       *widget,
							    GdkEventConfigure               *event,
							    EmpathyMainWindow               *window);
static void     main_window_notify_show_offline_cb         (GossipConf                      *conf,
							    const gchar                     *key,
							    gpointer                         check_menu_item);
static void     main_window_notify_show_avatars_cb         (GossipConf                      *conf,
							    const gchar                     *key,
							    EmpathyMainWindow               *window);
static void     main_window_notify_compact_contact_list_cb (GossipConf                      *conf,
							    const gchar                     *key,
							    EmpathyMainWindow               *window);
static void     main_window_notify_sort_criterium_cb       (GossipConf                      *conf,
							    const gchar                     *key,
							    EmpathyMainWindow               *window);

GtkWidget *
empathy_main_window_show (void)
{
	static EmpathyMainWindow *window = NULL;
	EmpathyContactList       *list_iface;
	GladeXML                 *glade;
	GossipConf               *conf;
	GtkWidget                *sw;
	GtkWidget                *show_offline_widget;
	GtkWidget                *ebox;
	GtkToolItem              *item;
	gchar                    *str;
	gboolean                  show_offline;
	gboolean                  show_avatars;
	gboolean                  compact_contact_list;
	gint                      x, y, w, h;

	if (window) {
		gtk_window_present (GTK_WINDOW (window->window));
		return window->window;
	}

	window = g_new0 (EmpathyMainWindow, 1);

	/* Set up interface */
	glade = gossip_glade_get_file ("empathy-main-window.glade",
				       "main_window",
				       NULL,
				       "main_window", &window->window,
				       "main_vbox", &window->main_vbox,
				       "chat_show_offline", &show_offline_widget,
				       "room", &window->room,
				       "room_sep", &window->room_sep,
				       "room_join_favorites", &window->room_join_favorites,
				       "edit_context", &window->edit_context,
				       "edit_context_separator", &window->edit_context_separator,
				       "presence_toolbar", &window->presence_toolbar,
				       "roster_scrolledwindow", &sw,
				       NULL);

	gossip_glade_connect (glade,
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

	window->tooltips = g_object_ref_sink (gtk_tooltips_new ());
	window->mc = gossip_mission_control_new ();
	dbus_g_proxy_connect_signal (DBUS_G_PROXY (window->mc), "AccountStatusChanged",
				     G_CALLBACK (main_window_status_changed_cb),
				     window, NULL);

	/* Set up menu */
	main_window_favorite_chatroom_menu_setup ();

	gtk_widget_hide (window->edit_context);
	gtk_widget_hide (window->edit_context_separator);

	/* Set up presence chooser */
	window->presence_chooser = gossip_presence_chooser_new ();
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

	window->throbber = ephy_spinner_new ();
	ephy_spinner_set_size (EPHY_SPINNER (window->throbber), GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_container_add (GTK_CONTAINER (ebox), window->throbber);

	item = gtk_tool_item_new ();
	gtk_container_add (GTK_CONTAINER (item), ebox);
	gtk_widget_show_all (GTK_WIDGET (item));

	gtk_toolbar_insert (GTK_TOOLBAR (window->presence_toolbar), item, -1);

	str = _("Show and edit accounts");
	gtk_tooltips_set_tip (GTK_TOOLTIPS (window->tooltips),
			      ebox, str, str);

	g_signal_connect (ebox,
			  "button-press-event",
			  G_CALLBACK (main_window_throbber_button_press_event_cb),
			  NULL);

	/* Set up contact list. */
	gossip_status_presets_get_all ();

	list_iface = EMPATHY_CONTACT_LIST (empathy_contact_manager_new ());
	empathy_contact_list_setup (list_iface);
	window->list_store = gossip_contact_list_store_new (list_iface);
	window->list_view = gossip_contact_list_view_new (window->list_store);
	g_object_unref (list_iface);

	gtk_widget_show (GTK_WIDGET (window->list_view));
	gtk_container_add (GTK_CONTAINER (sw),
			   GTK_WIDGET (window->list_view));

	/* Load user-defined accelerators. */
	main_window_accels_load ();

	/* Set window size. */
	gossip_geometry_load (GEOMETRY_NAME, &x, &y, &w, &h);

	if (w >= 1 && h >= 1) {
		/* Use the defaults from the glade file if we
		 * don't have good w, h geometry.
		 */
		gossip_debug (DEBUG_DOMAIN, "Configuring window default size w:%d, h:%d", w, h);
		gtk_window_set_default_size (GTK_WINDOW (window->window), w, h);
	}

	if (x >= 0 && y >= 0) {
		/* Let the window manager position it if we
		 * don't have good x, y coordinates.
		 */
		gossip_debug (DEBUG_DOMAIN, "Configuring window default position x:%d, y:%d", x, y);
		gtk_window_move (GTK_WINDOW (window->window), x, y);
	}

	conf = gossip_conf_get ();
	
	/* Show offline ? */
	gossip_conf_get_bool (conf,
			      GOSSIP_PREFS_CONTACTS_SHOW_OFFLINE,
			      &show_offline);
	gossip_conf_notify_add (conf,
				GOSSIP_PREFS_CONTACTS_SHOW_OFFLINE,
				main_window_notify_show_offline_cb,
				show_offline_widget);

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (show_offline_widget),
					show_offline);

	/* Show avatars ? */
	gossip_conf_get_bool (conf,
			      GOSSIP_PREFS_UI_SHOW_AVATARS,
			      &show_avatars);
	gossip_conf_notify_add (conf,
				GOSSIP_PREFS_UI_SHOW_AVATARS,
				(GossipConfNotifyFunc) main_window_notify_show_avatars_cb,
				window);
	gossip_contact_list_store_set_show_avatars (window->list_store, show_avatars);

	/* Is compact ? */
	gossip_conf_get_bool (conf,
			      GOSSIP_PREFS_UI_COMPACT_CONTACT_LIST,
			      &compact_contact_list);
	gossip_conf_notify_add (conf,
				GOSSIP_PREFS_UI_COMPACT_CONTACT_LIST,
				(GossipConfNotifyFunc) main_window_notify_compact_contact_list_cb,
				window);
	gossip_contact_list_store_set_is_compact (window->list_store, compact_contact_list);

	/* Sort criterium */
	gossip_conf_notify_add (conf,
				GOSSIP_PREFS_CONTACTS_SORT_CRITERIUM,
				(GossipConfNotifyFunc) main_window_notify_sort_criterium_cb,
				window);
	main_window_notify_sort_criterium_cb (conf,
					      GOSSIP_PREFS_CONTACTS_SORT_CRITERIUM,
					      window);

	main_window_update_status (window);

	return window->window;
}

static void
main_window_destroy_cb (GtkWidget         *widget,
			EmpathyMainWindow *window)
{
	/* Save user-defined accelerators. */
	main_window_accels_save ();

	dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (window->mc), "AccountStatusChanged",
					G_CALLBACK (main_window_status_changed_cb),
					window);

	if (window->size_timeout_id) {
		g_source_remove (window->size_timeout_id);
	}

	g_list_free (window->widgets_connected);
	g_list_free (window->widgets_disconnected);

	g_object_unref (window->tooltips);
	g_object_unref (window->mc);
	g_object_unref (window->list_store);

	g_free (window);
}

static void
main_window_favorite_chatroom_menu_setup (void)
{
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
	//gossip_new_message_dialog_show (GTK_WINDOW (window->window));
}

static void
main_window_chat_history_cb (GtkWidget         *widget,
			     EmpathyMainWindow *window)
{
	//gossip_log_window_show (NULL, NULL);
}

static void
main_window_room_join_new_cb (GtkWidget         *widget,
			      EmpathyMainWindow *window)
{
	//gossip_new_chatroom_dialog_show (GTK_WINDOW (window->window));
}

static void
main_window_room_join_favorites_cb (GtkWidget         *widget,
				    EmpathyMainWindow *window)
{
	//gossip_session_chatroom_join_favorites (window->session);
}

static void
main_window_room_manage_favorites_cb (GtkWidget         *widget,
				      EmpathyMainWindow *window)
{
	//gossip_chatrooms_window_show (NULL, FALSE);
}

static void
main_window_chat_add_contact_cb (GtkWidget         *widget,
				 EmpathyMainWindow *window)
{
	//gossip_add_contact_dialog_show (GTK_WINDOW (window->window), NULL);
}

static void
main_window_chat_show_offline_cb (GtkCheckMenuItem  *item,
				  EmpathyMainWindow *window)
{
	gboolean current;

	current = gtk_check_menu_item_get_active (item);

	gossip_conf_set_bool (gossip_conf_get (),
			      GOSSIP_PREFS_CONTACTS_SHOW_OFFLINE,
			      current);

	/* Turn off sound just while we alter the contact list. */
	// FIXME: gossip_sound_set_enabled (FALSE);
	gossip_contact_list_store_set_show_offline (window->list_store, current);
	//gossip_sound_set_enabled (TRUE);
}

static gboolean
main_window_edit_button_press_event_cb (GtkWidget         *widget,
					GdkEventButton    *event,
					EmpathyMainWindow *window)
{
	GossipContact *contact;
	gchar         *group;

	if (!event->button == 1) {
		return FALSE;
	}

	group = gossip_contact_list_view_get_selected_group (window->list_view);
	if (group) {
		GtkMenuItem *item;
		GtkWidget   *label;
		GtkWidget   *submenu;

		item = GTK_MENU_ITEM (window->edit_context);
		label = gtk_bin_get_child (GTK_BIN (item));
		gtk_label_set_text (GTK_LABEL (label), _("Group"));

		gtk_widget_show (window->edit_context);
		gtk_widget_show (window->edit_context_separator);

		submenu = gossip_contact_list_view_get_group_menu (window->list_view);
		gtk_menu_item_set_submenu (item, submenu);

		g_free (group);

		return FALSE;
	}

	contact = gossip_contact_list_view_get_selected (window->list_view);
	if (contact) {
		GtkMenuItem *item;
		GtkWidget   *label;
		GtkWidget   *submenu;

		item = GTK_MENU_ITEM (window->edit_context);
		label = gtk_bin_get_child (GTK_BIN (item));
		gtk_label_set_text (GTK_LABEL (label), _("Contact"));

		gtk_widget_show (window->edit_context);
		gtk_widget_show (window->edit_context_separator);

		submenu = gossip_contact_list_view_get_contact_menu (window->list_view,
								     contact);
		gtk_menu_item_set_submenu (item, submenu);

		g_object_unref (contact);

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
	gossip_accounts_dialog_show ();
}

static void
main_window_edit_personal_information_cb (GtkWidget         *widget,
					  EmpathyMainWindow *window)
{
	//gossip_vcard_dialog_show (GTK_WINDOW (window->window));
}

static void
main_window_edit_preferences_cb (GtkWidget         *widget,
				 EmpathyMainWindow *window)
{
	gossip_preferences_show ();
}

static void
main_window_help_about_cb (GtkWidget         *widget,
			   EmpathyMainWindow *window)
{
	gossip_about_dialog_new (GTK_WINDOW (window->window));
}

static void
main_window_help_contents_cb (GtkWidget         *widget,
			      EmpathyMainWindow *window)
{
	//gossip_help_show ();
}

static gboolean
main_window_throbber_button_press_event_cb (GtkWidget      *throbber_ebox,
					    GdkEventButton *event,
					    gpointer        user_data)
{
	if (event->type != GDK_BUTTON_PRESS ||
	    event->button != 1) {
		return FALSE;
	}

	gossip_accounts_dialog_show ();

	return FALSE;
}

static void
main_window_status_changed_cb (MissionControl                  *mc,
			       TelepathyConnectionStatus        status,
			       McPresence                       presence,
			       TelepathyConnectionStatusReason  reason,
			       const gchar                     *unique_name,
			       EmpathyMainWindow               *window)
{
	main_window_update_status (window);
}

static void
main_window_update_status (EmpathyMainWindow *window)
{
	GList *accounts, *l;
	guint  connected = 0;
	guint  connecting = 0;
	guint  disconnected = 0;

	/* Count number of connected/connecting/disconnected accounts */
	accounts = mc_accounts_list ();	
	for (l = accounts; l; l = l->next) {
		McAccount *account;
		guint      status;

		account = l->data;

		status = mission_control_get_connection_status (window->mc,
								account,
								NULL);

		if (status == 0) {
			connected++;
		} else if (status == 1) {
			connecting++;
		} else if (status == 2) {
			disconnected++;
		}

		g_object_unref (account);
	}
	g_list_free (accounts);

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

	for (l = window->widgets_disconnected; l; l = l->next) {
		gtk_widget_set_sensitive (l->data, (disconnected > 0));
	}

	//app_favorite_chatroom_menu_update ();	
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
		gossip_debug (DEBUG_DOMAIN, "Loading from:'%s'", filename);
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

	gossip_debug (DEBUG_DOMAIN, "Saving to:'%s'", file_with_path);
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
	const gchar *widgets_disconnected[] = {
	};

	for (i = 0, list = NULL; i < G_N_ELEMENTS (widgets_connected); i++) {
		w = glade_xml_get_widget (glade, widgets_connected[i]);
		list = g_list_prepend (list, w);
	}

	window->widgets_connected = list;

	for (i = 0, list = NULL; i < G_N_ELEMENTS (widgets_disconnected); i++) {
		w = glade_xml_get_widget (glade, widgets_disconnected[i]);
		list = g_list_prepend (list, w);
	}

	window->widgets_disconnected = list;
}

static gboolean
main_window_configure_event_timeout_cb (EmpathyMainWindow *window)
{
	gint x, y, w, h;

	gtk_window_get_size (GTK_WINDOW (window->window), &w, &h);
	gtk_window_get_position (GTK_WINDOW (window->window), &x, &y);

	gossip_geometry_save (GEOMETRY_NAME, x, y, w, h);

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

	window->size_timeout_id = g_timeout_add (500,
					       (GSourceFunc) main_window_configure_event_timeout_cb,
					       window);

	return FALSE;
}

static void
main_window_notify_show_offline_cb (GossipConf  *conf,
				    const gchar *key,
				    gpointer     check_menu_item)
{
	gboolean show_offline;

	if (gossip_conf_get_bool (conf, key, &show_offline)) {
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (check_menu_item),
						show_offline);
	}
}

static void
main_window_notify_show_avatars_cb (GossipConf        *conf,
				    const gchar       *key,
				    EmpathyMainWindow *window)
{
	gboolean show_avatars;

	if (gossip_conf_get_bool (conf, key, &show_avatars)) {
		gossip_contact_list_store_set_show_avatars (window->list_store,
							    show_avatars);
	}
}

static void
main_window_notify_compact_contact_list_cb (GossipConf        *conf,
					    const gchar       *key,
					    EmpathyMainWindow *window)
{
	gboolean compact_contact_list;

	if (gossip_conf_get_bool (conf, key, &compact_contact_list)) {
		gossip_contact_list_store_set_is_compact (window->list_store,
							  compact_contact_list);
	}
}

static void
main_window_notify_sort_criterium_cb (GossipConf        *conf,
				      const gchar       *key,
				      EmpathyMainWindow *window)
{
	gchar *str = NULL;

	if (gossip_conf_get_string (conf, key, &str)) {
		GType       type;
		GEnumClass *enum_class;
		GEnumValue *enum_value;

		type = gossip_contact_list_store_sort_get_type ();
		enum_class = G_ENUM_CLASS (g_type_class_peek (type));
		enum_value = g_enum_get_value_by_nick (enum_class, str);

		if (enum_value) {
			gossip_contact_list_store_set_sort_criterium (window->list_store, 
								      enum_value->value);
		}
	}
}

