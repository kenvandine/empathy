/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Geert-Jan Van den Bogaerde <geertjan@gnome.org>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <libnotify/notification.h>

#include <telepathy-glib/util.h>
#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-message.h>
#include <libempathy/empathy-dispatcher.h>
#include <libempathy/empathy-chatroom-manager.h>
#include <libempathy/empathy-account-manager.h>
#include <libempathy/empathy-utils.h>

#include <libempathy-gtk/empathy-images.h>
#include <libempathy-gtk/empathy-conf.h>
#include <libempathy-gtk/empathy-contact-dialogs.h>
#include <libempathy-gtk/empathy-log-window.h>
#include <libempathy-gtk/empathy-geometry.h>
#include <libempathy-gtk/empathy-smiley-manager.h>
#include <libempathy-gtk/empathy-ui-utils.h>

#include "empathy-chat-window.h"
#include "empathy-about-dialog.h"
#include "empathy-misc.h"

#ifdef HAVE_LIBINDICATE
#include "empathy-indicator.h"
#include "empathy-indicator-manager.h"
#endif

#define DEBUG_FLAG EMPATHY_DEBUG_CHAT
#include <libempathy/empathy-debug.h>

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyChatWindow)
typedef struct {
	EmpathyChat *current_chat;
	GList       *chats;
	GList       *chats_new_msg;
	GList       *chats_composing;
	gboolean     page_added;
	gboolean     dnd_same_window;
	guint        save_geometry_id;
	EmpathyChatroomManager *chatroom_manager;
	GtkWidget   *dialog;
	GtkWidget   *notebook;
	NotifyNotification *notification;
#ifdef HAVE_LIBINDICATE
	EmpathyIndicatorManager *indicator_manager;
	GHashTable  *indicators;
#endif

	/* Menu items. */
	GtkUIManager *ui_manager;
	GtkAction   *menu_conv_insert_smiley;
	GtkAction   *menu_conv_favorite;

	GtkAction   *menu_edit_cut;
	GtkAction   *menu_edit_copy;
	GtkAction   *menu_edit_paste;

	GtkAction   *menu_tabs_next;
	GtkAction   *menu_tabs_prev;
	GtkAction   *menu_tabs_left;
	GtkAction   *menu_tabs_right;
	GtkAction   *menu_tabs_detach;
} EmpathyChatWindowPriv;

static GList *chat_windows = NULL;

static const guint tab_accel_keys[] = {
	GDK_1, GDK_2, GDK_3, GDK_4, GDK_5,
	GDK_6, GDK_7, GDK_8, GDK_9, GDK_0
};

typedef enum {
	DND_DRAG_TYPE_CONTACT_ID,
	DND_DRAG_TYPE_TAB
} DndDragType;

static const GtkTargetEntry drag_types_dest[] = {
	{ "text/contact-id", 0, DND_DRAG_TYPE_CONTACT_ID },
	{ "GTK_NOTEBOOK_TAB", GTK_TARGET_SAME_APP, DND_DRAG_TYPE_TAB },
};

G_DEFINE_TYPE (EmpathyChatWindow, empathy_chat_window, G_TYPE_OBJECT);

static void
chat_window_accel_cb (GtkAccelGroup    *accelgroup,
		      GObject          *object,
		      guint             key,
		      GdkModifierType   mod,
		      EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;
	gint                  num = -1;
	gint                  i;

	priv = GET_PRIV (window);

	for (i = 0; i < G_N_ELEMENTS (tab_accel_keys); i++) {
		if (tab_accel_keys[i] == key) {
			num = i;
			break;
		}
	}

	if (num != -1) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), num);
	}
}

static EmpathyChatWindow *
chat_window_find_chat (EmpathyChat *chat)
{
	EmpathyChatWindowPriv *priv;
	GList                 *l, *ll;

	for (l = chat_windows; l; l = l->next) {
		priv = GET_PRIV (l->data);
		ll = g_list_find (priv->chats, chat);
		if (ll) {
			return l->data;
		}
	}

	return NULL;
}

static void
chat_window_close_clicked_cb (GtkAction   *action,
			      EmpathyChat *chat)
{
	EmpathyChatWindow *window;

	window = chat_window_find_chat (chat);
	empathy_chat_window_remove_chat (window, chat);
}

static void
chat_tab_style_set_cb (GtkWidget *hbox,
				       GtkStyle  *previous_style,
				       gpointer   user_data)
{
	GtkWidget *button;
	int char_width, h, w;
	PangoContext *context;
	PangoFontMetrics *metrics;

	button = g_object_get_data (G_OBJECT (user_data),
		"chat-window-tab-close-button");
	context = gtk_widget_get_pango_context (hbox);

	metrics = pango_context_get_metrics (context, hbox->style->font_desc,
		pango_context_get_language (context));
	char_width = pango_font_metrics_get_approximate_char_width (metrics);
	pango_font_metrics_unref (metrics);

	gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (button),
					   GTK_ICON_SIZE_MENU, &w, &h);

	/* Request at least about 12 chars width plus at least space for the status
	 * image and the close button */
	gtk_widget_set_size_request (hbox,
		12 * PANGO_PIXELS (char_width) + 2 * w, -1);

	gtk_widget_set_size_request (button, w, h);
}

static GtkWidget *
chat_window_create_label (EmpathyChatWindow *window,
			  EmpathyChat       *chat,
			  gboolean           is_tab_label)
{
	EmpathyChatWindowPriv *priv;
	GtkWidget            *hbox;
	GtkWidget            *name_label;
	GtkWidget            *status_image;
	GtkWidget            *close_button;
	GtkWidget            *close_image;
	GtkWidget            *event_box;
	GtkWidget            *event_box_hbox;
	PangoAttrList        *attr_list;
	PangoAttribute       *attr;

	priv = GET_PRIV (window);

	/* The spacing between the button and the label. */
	hbox = gtk_hbox_new (FALSE, 0);

	event_box = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box), FALSE);

	name_label = gtk_label_new (NULL);
	if (is_tab_label)
		gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_END);

	attr_list = pango_attr_list_new ();
	attr = pango_attr_scale_new (1/1.2);
	attr->start_index = 0;
	attr->end_index = -1;
	pango_attr_list_insert (attr_list, attr);
	gtk_label_set_attributes (GTK_LABEL (name_label), attr_list);
	pango_attr_list_unref (attr_list);

	gtk_misc_set_padding (GTK_MISC (name_label), 2, 0);
	gtk_misc_set_alignment (GTK_MISC (name_label), 0.0, 0.5);
	g_object_set_data (G_OBJECT (chat),
		is_tab_label ? "chat-window-tab-label" : "chat-window-menu-label",
		name_label);

	status_image = gtk_image_new ();

	/* Spacing between the icon and label. */
	event_box_hbox = gtk_hbox_new (FALSE, 0);

	gtk_box_pack_start (GTK_BOX (event_box_hbox), status_image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (event_box_hbox), name_label, TRUE, TRUE, 0);

	g_object_set_data (G_OBJECT (chat),
		is_tab_label ? "chat-window-tab-image" : "chat-window-menu-image",
		status_image);
	g_object_set_data (G_OBJECT (chat),
		is_tab_label ? "chat-window-tab-tooltip-widget" : "chat-window-menu-tooltip-widget",
		event_box);

	gtk_container_add (GTK_CONTAINER (event_box), event_box_hbox);
	gtk_box_pack_start (GTK_BOX (hbox), event_box, TRUE, TRUE, 0);

	if (is_tab_label) {
		close_button = gtk_button_new ();
		gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
		g_object_set_data (G_OBJECT (chat), "chat-window-tab-close-button", close_button);

		/* We don't want focus/keynav for the button to avoid clutter, and
		 * Ctrl-W works anyway.
		 */
		GTK_WIDGET_UNSET_FLAGS (close_button, GTK_CAN_FOCUS);
		GTK_WIDGET_UNSET_FLAGS (close_button, GTK_CAN_DEFAULT);

		/* Set the name to make the special rc style match. */
		gtk_widget_set_name (close_button, "empathy-close-button");

		close_image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);

		gtk_container_add (GTK_CONTAINER (close_button), close_image);

		gtk_box_pack_end (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);

		g_signal_connect (close_button,
				  "clicked",
				  G_CALLBACK (chat_window_close_clicked_cb),
				  chat);

		/* React to theme changes and also setup the size correctly.  */
		g_signal_connect (hbox,
				  "style-set",
				  G_CALLBACK (chat_tab_style_set_cb),
				  chat);
	}

	gtk_widget_show_all (hbox);

	return hbox;
}

static void
chat_window_update (EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv = GET_PRIV (window);
	gboolean               first_page;
	gboolean               last_page;
	gboolean               is_connected;
	gint                   num_pages;
	gint                   page_num;
	gint                   i;
	const gchar           *name;
	guint                  n_chats;
	GdkPixbuf             *icon;
	EmpathyContact        *remote_contact;
	gboolean               avatar_in_icon;
	GtkWidget             *chat;
	GtkWidget             *chat_close_button;
	GtkWidget             *submenu;
	GtkWidget             *menu;

	/* Get information */
	page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
	num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));
	first_page = (page_num == 0);
	last_page = (page_num == (num_pages - 1));
	is_connected = empathy_chat_get_tp_chat (priv->current_chat) != NULL;
	name = empathy_chat_get_name (priv->current_chat);
	n_chats = g_list_length (priv->chats);

	DEBUG ("Update window");

	/* Update menu */
	gtk_action_set_sensitive (priv->menu_tabs_next, !last_page);
	gtk_action_set_sensitive (priv->menu_tabs_prev, !first_page);
	gtk_action_set_sensitive (priv->menu_tabs_detach, num_pages > 1);
	gtk_action_set_sensitive (priv->menu_tabs_left, !first_page);
	gtk_action_set_sensitive (priv->menu_tabs_right, !last_page);
	gtk_action_set_sensitive (priv->menu_conv_insert_smiley, is_connected);

	/* Update Contact menu */
	menu = gtk_ui_manager_get_widget (priv->ui_manager,
		"/chats_menubar/menu_contact");
	submenu = empathy_chat_get_contact_menu (priv->current_chat);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu), submenu);
	gtk_widget_show (menu);

	/* Update window title */
	if (n_chats == 1) {
		gtk_window_set_title (GTK_WINDOW (priv->dialog), name);
	} else {
		gchar *title;

		title = g_strdup_printf (_("Conversations (%d)"), n_chats);
		gtk_window_set_title (GTK_WINDOW (priv->dialog), title);
		g_free (title);
	}

	/* Update window icon */
	if (priv->chats_new_msg) {
		gtk_window_set_icon_name (GTK_WINDOW (priv->dialog),
					  EMPATHY_IMAGE_MESSAGE);
	} else {
		empathy_conf_get_bool (empathy_conf_get (),
				       EMPATHY_PREFS_CHAT_AVATAR_IN_ICON,
				       &avatar_in_icon);

		if (n_chats == 1 && avatar_in_icon) {
			remote_contact = empathy_chat_get_remote_contact (priv->current_chat);
			icon = empathy_pixbuf_avatar_from_contact_scaled (remote_contact, 0, 0);
			gtk_window_set_icon (GTK_WINDOW (priv->dialog), icon);

			if (icon != NULL) {
				g_object_unref (icon);
			}
		} else {
			gtk_window_set_icon_name (GTK_WINDOW (priv->dialog), NULL);
		}
	}

	if (num_pages == 1) {
		chat = gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->notebook), 0);
		chat_close_button = g_object_get_data (G_OBJECT (chat),
				"chat-window-tab-close-button");
		gtk_widget_hide (chat_close_button);
	} else {
		for (i=0; i<num_pages; i++) {
			chat = gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->notebook), i);
			chat_close_button = g_object_get_data (G_OBJECT (chat),
					"chat-window-tab-close-button");
			gtk_widget_show (chat_close_button);
		}
	}
}

static void
append_markup_printf (GString    *string,
		      const char *format,
		      ...)
{
	gchar *tmp;
	va_list args;

	va_start (args, format);

	tmp = g_markup_vprintf_escaped (format, args);
	g_string_append (string, tmp);
	g_free (tmp);

	va_end (args);
}

static void
chat_window_update_chat_tab (EmpathyChat *chat)
{
	EmpathyChatWindow     *window;
	EmpathyChatWindowPriv *priv;
	EmpathyContact        *remote_contact;
	const gchar           *name;
	const gchar           *id;
	McAccount             *account;
	const gchar           *subject;
	const gchar           *status = NULL;
	GtkWidget             *widget;
	GString               *tooltip;
	gchar                 *markup;
	const gchar           *icon_name;

	window = chat_window_find_chat (chat);
	if (!window) {
		return;
	}
	priv = GET_PRIV (window);

	/* Get information */
	name = empathy_chat_get_name (chat);
	account = empathy_chat_get_account (chat);
	subject = empathy_chat_get_subject (chat);
	remote_contact = empathy_chat_get_remote_contact (chat);

	DEBUG ("Updating chat tab, name=%s, account=%s, subject=%s, remote_contact=%p",
		name, mc_account_get_unique_name (account), subject, remote_contact);

	/* Update tab image */
	if (g_list_find (priv->chats_new_msg, chat)) {
		icon_name = EMPATHY_IMAGE_MESSAGE;
	}
	else if (g_list_find (priv->chats_composing, chat)) {
		icon_name = EMPATHY_IMAGE_TYPING;
	}
	else if (remote_contact) {
		icon_name = empathy_icon_name_for_contact (remote_contact);
	} else {
		icon_name = EMPATHY_IMAGE_GROUP_MESSAGE;
	}
	widget = g_object_get_data (G_OBJECT (chat), "chat-window-tab-image");
	gtk_image_set_from_icon_name (GTK_IMAGE (widget), icon_name, GTK_ICON_SIZE_MENU);
	widget = g_object_get_data (G_OBJECT (chat), "chat-window-menu-image");
	gtk_image_set_from_icon_name (GTK_IMAGE (widget), icon_name, GTK_ICON_SIZE_MENU);

	/* Update tab tooltip */
	tooltip = g_string_new (NULL);

	if (remote_contact) {
		id = empathy_contact_get_id (remote_contact);
		status = empathy_contact_get_presence_message (remote_contact);
	} else {
		id = name;
	}

	append_markup_printf (tooltip,
			      "<b>%s</b><small> (%s)</small>",
			      id,
			      mc_account_get_display_name (account));

	if (!EMP_STR_EMPTY (status)) {
		append_markup_printf (tooltip, "\n<i>%s</i>", status);
	}

	if (subject) {
		append_markup_printf (tooltip, "\n<b>%s</b> %s",
				      _("Topic:"), subject);
	}

	if (g_list_find (priv->chats_composing, chat)) {
		append_markup_printf (tooltip, "\n%s", _("Typing a message."));
	}

	markup = g_string_free (tooltip, FALSE);
	widget = g_object_get_data (G_OBJECT (chat), "chat-window-tab-tooltip-widget");
	gtk_widget_set_tooltip_markup (widget, markup);
	widget = g_object_get_data (G_OBJECT (chat), "chat-window-menu-tooltip-widget");
	gtk_widget_set_tooltip_markup (widget, markup);
	g_free (markup);

	/* Update tab and menu label */
	widget = g_object_get_data (G_OBJECT (chat), "chat-window-tab-label");
	gtk_label_set_text (GTK_LABEL (widget), name);
	widget = g_object_get_data (G_OBJECT (chat), "chat-window-menu-label");
	gtk_label_set_text (GTK_LABEL (widget), name);

	/* Update the window if it's the current chat */
	if (priv->current_chat == chat) {
		chat_window_update (window);
	}
}

static void
chat_window_chat_notify_cb (EmpathyChat *chat)
{
	EmpathyContact *old_remote_contact;
	EmpathyContact *remote_contact = NULL;

	old_remote_contact = g_object_get_data (G_OBJECT (chat), "chat-window-remote-contact");
	remote_contact = empathy_chat_get_remote_contact (chat);

	if (old_remote_contact != remote_contact) {
		/* The remote-contact associated with the chat changed, we need
		 * to keep track of any change of that contact and update the
		 * window each time. */
		if (remote_contact) {
			g_signal_connect_swapped (remote_contact, "notify",
						  G_CALLBACK (chat_window_update_chat_tab),
						  chat);
		}
		if (old_remote_contact) {
			g_signal_handlers_disconnect_by_func (old_remote_contact,
							      chat_window_update_chat_tab,
							      chat);
		}

		g_object_set_data (G_OBJECT (chat), "chat-window-remote-contact",
				   remote_contact);
	}

	chat_window_update_chat_tab (chat);
}

static void
chat_window_insert_smiley_activate_cb (EmpathySmileyManager *manager,
				       EmpathySmiley        *smiley,
				       gpointer              window)
{
	EmpathyChatWindowPriv *priv = GET_PRIV (window);
	EmpathyChat           *chat;
	GtkTextBuffer         *buffer;
	GtkTextIter            iter;

	chat = priv->current_chat;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert (buffer, &iter, smiley->str, -1);
}

static void
chat_window_conv_activate_cb (GtkAction         *action,
			      EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv = GET_PRIV (window);
	gboolean               is_room;

	/* Favorite room menu */
	is_room = empathy_chat_is_room (priv->current_chat);
	if (is_room) {
		const gchar *room;
		McAccount   *account;
		gboolean     found;

		room = empathy_chat_get_id (priv->current_chat);
		account = empathy_chat_get_account (priv->current_chat);
		found = empathy_chatroom_manager_find (priv->chatroom_manager,
						       account, room) != NULL;

		DEBUG ("This room %s favorite", found ? "is" : "is not");
		gtk_toggle_action_set_active (
			GTK_TOGGLE_ACTION (priv->menu_conv_favorite), found);
	}
	gtk_action_set_visible (priv->menu_conv_favorite, is_room);
}

static void
chat_window_clear_activate_cb (GtkAction         *action,
			       EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv = GET_PRIV (window);

	empathy_chat_clear (priv->current_chat);
}

static void
chat_window_favorite_toggled_cb (GtkToggleAction   *toggle_action,
				 EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv = GET_PRIV (window);
	gboolean               active;
	McAccount             *account;
	const gchar           *room;
	EmpathyChatroom       *chatroom;

	active = gtk_toggle_action_get_active (toggle_action);
	account = empathy_chat_get_account (priv->current_chat);
	room = empathy_chat_get_id (priv->current_chat);

	chatroom = empathy_chatroom_manager_find (priv->chatroom_manager,
						  account, room);

	if (active && !chatroom) {
		const gchar *name;

		name = empathy_chat_get_name (priv->current_chat);
		chatroom = empathy_chatroom_new_full (account, room, name, FALSE);
		empathy_chatroom_manager_add (priv->chatroom_manager, chatroom);
		g_object_unref (chatroom);
		return;
	}
	
	if (!active && chatroom) {
		empathy_chatroom_manager_remove (priv->chatroom_manager, chatroom);
	}
}

static const gchar *
chat_get_window_id_for_geometry (EmpathyChat *chat)
{
	const gchar *res = NULL;
	gboolean     separate_windows;

	empathy_conf_get_bool (empathy_conf_get (),
			       EMPATHY_PREFS_UI_SEPARATE_CHAT_WINDOWS,
			       &separate_windows);

	if (separate_windows) {
		res = empathy_chat_get_id (chat);
	}

	return res ? res : "chat-window";
}

static gboolean
chat_window_save_geometry_timeout_cb (EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;
	gint                  x, y, w, h;

	priv = GET_PRIV (window);

	gtk_window_get_size (GTK_WINDOW (priv->dialog), &w, &h);
	gtk_window_get_position (GTK_WINDOW (priv->dialog), &x, &y);

	empathy_geometry_save (chat_get_window_id_for_geometry (priv->current_chat),
			       x, y, w, h);

	priv->save_geometry_id = 0;

	return FALSE;
}

static gboolean
chat_window_configure_event_cb (GtkWidget         *widget,
				GdkEventConfigure *event,
				EmpathyChatWindow  *window)
{
	EmpathyChatWindowPriv *priv;

	priv = GET_PRIV (window);

	if (priv->save_geometry_id != 0) {
		g_source_remove (priv->save_geometry_id);
	}

	priv->save_geometry_id =
		g_timeout_add_seconds (1,
				       (GSourceFunc) chat_window_save_geometry_timeout_cb,
				       window);

	return FALSE;
}

static void
chat_window_close_activate_cb (GtkAction         *action,
			       EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;

	priv = GET_PRIV (window);

	g_return_if_fail (priv->current_chat != NULL);

	empathy_chat_window_remove_chat (window, priv->current_chat);
}

static void
chat_window_edit_activate_cb (GtkAction         *action,
			      EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;
	GtkClipboard         *clipboard;
	GtkTextBuffer        *buffer;
	gboolean              text_available;

	priv = GET_PRIV (window);

	g_return_if_fail (priv->current_chat != NULL);

	if (!empathy_chat_get_tp_chat (priv->current_chat)) {
		gtk_action_set_sensitive (priv->menu_edit_copy, FALSE);
		gtk_action_set_sensitive (priv->menu_edit_cut, FALSE);
		gtk_action_set_sensitive (priv->menu_edit_paste, FALSE);
		return;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->current_chat->input_text_view));
	if (gtk_text_buffer_get_has_selection (buffer)) {
		gtk_action_set_sensitive (priv->menu_edit_copy, TRUE);
		gtk_action_set_sensitive (priv->menu_edit_cut, TRUE);
	} else {
		gboolean selection;

		selection = empathy_chat_view_get_has_selection (priv->current_chat->view);

		gtk_action_set_sensitive (priv->menu_edit_cut, FALSE);
		gtk_action_set_sensitive (priv->menu_edit_copy, selection);
	}

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	text_available = gtk_clipboard_wait_is_text_available (clipboard);
	gtk_action_set_sensitive (priv->menu_edit_paste, text_available);
}

static void
chat_window_cut_activate_cb (GtkAction         *action,
			     EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;

	g_return_if_fail (EMPATHY_IS_CHAT_WINDOW (window));

	priv = GET_PRIV (window);

	empathy_chat_cut (priv->current_chat);
}

static void
chat_window_copy_activate_cb (GtkAction         *action,
			      EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;

	g_return_if_fail (EMPATHY_IS_CHAT_WINDOW (window));

	priv = GET_PRIV (window);

	empathy_chat_copy (priv->current_chat);
}

static void
chat_window_paste_activate_cb (GtkAction         *action,
			       EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;

	g_return_if_fail (EMPATHY_IS_CHAT_WINDOW (window));

	priv = GET_PRIV (window);

	empathy_chat_paste (priv->current_chat);
}

static void
chat_window_tabs_left_activate_cb (GtkAction         *action,
				   EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;
	EmpathyChat           *chat;
	gint                  index;

	priv = GET_PRIV (window);

	chat = priv->current_chat;
	index = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
	if (index <= 0) {
		return;
	}

	gtk_notebook_reorder_child (GTK_NOTEBOOK (priv->notebook),
				    GTK_WIDGET (chat),
				    index - 1);
}

static void
chat_window_tabs_right_activate_cb (GtkAction         *action,
				    EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;
	EmpathyChat           *chat;
	gint                  index;

	priv = GET_PRIV (window);

	chat = priv->current_chat;
	index = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));

	gtk_notebook_reorder_child (GTK_NOTEBOOK (priv->notebook),
				    GTK_WIDGET (chat),
				    index + 1);
}

static void
chat_window_detach_activate_cb (GtkAction         *action,
				EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;
	EmpathyChatWindow     *new_window;
	EmpathyChat           *chat;

	priv = GET_PRIV (window);

	chat = priv->current_chat;
	new_window = empathy_chat_window_new ();

	empathy_chat_window_move_chat (window, new_window, chat);

	priv = GET_PRIV (new_window);
	gtk_widget_show (priv->dialog);
}

static void
chat_window_help_contents_activate_cb (GtkAction         *action,
				       EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv = GET_PRIV (window);

	empathy_url_show (priv->dialog, "ghelp:empathy?chat");
}

static void
chat_window_help_about_activate_cb (GtkAction         *action,
				    EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv = GET_PRIV (window);

	empathy_about_dialog_new (GTK_WINDOW (priv->dialog));
}

static gboolean
chat_window_delete_event_cb (GtkWidget        *dialog,
			     GdkEvent         *event,
			     EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv = GET_PRIV (window);

	DEBUG ("Delete event received");

	g_object_ref (window);
	while (priv->chats) {
		empathy_chat_window_remove_chat (window, priv->chats->data);
	}
	g_object_unref (window);

	return TRUE;
}

static void
chat_window_composing_cb (EmpathyChat       *chat,
			  gboolean          is_composing,
			  EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;

	priv = GET_PRIV (window);

	if (is_composing && !g_list_find (priv->chats_composing, chat)) {
		priv->chats_composing = g_list_prepend (priv->chats_composing, chat);
	} else {
		priv->chats_composing = g_list_remove (priv->chats_composing, chat);
	}

	chat_window_update_chat_tab (chat);
}

static void
chat_window_set_urgency_hint (EmpathyChatWindow *window,
			      gboolean          urgent)
{
	EmpathyChatWindowPriv *priv;

	priv = GET_PRIV (window);

	DEBUG ("Turning %s urgency hint", urgent ? "on" : "off");
	gtk_window_set_urgency_hint (GTK_WINDOW (priv->dialog), urgent);
}

typedef struct {
	EmpathyChatWindow *window;
	EmpathyChat *chat;
} NotificationData;

static void
chat_window_notification_closed_cb (NotifyNotification *notify,
				    NotificationData *cb_data)
{
	EmpathyNotificationClosedReason reason = 0;
	EmpathyChatWindowPriv *priv = GET_PRIV (cb_data->window);

#ifdef notify_notification_get_closed_reason
	reason = notify_notification_get_closed_reason (notify);
#endif
	if (reason == EMPATHY_NOTIFICATION_CLOSED_DISMISSED) {
		empathy_chat_window_present_chat (cb_data->chat);
	}

	g_object_unref (notify);
	priv->notification = NULL;
	g_object_unref (cb_data->chat);
	g_slice_free (NotificationData, cb_data);
}

#ifdef HAVE_LIBINDICATE
static void
chat_window_indicator_activate_cb (EmpathyIndicator *indicator,
				    NotificationData *cb_data)
{
	empathy_chat_window_present_chat (cb_data->chat);

	empathy_indicator_hide (indicator);
	g_object_unref (indicator);
	g_object_unref (cb_data->chat);
	g_slice_free (NotificationData, cb_data);
}

static void
chat_window_add_indicator (EmpathyChatWindow *window,
					 EmpathyMessage *message,
					 EmpathyChat    *chat)
{
	EmpathyChatWindowPriv *priv = GET_PRIV (window);
	EmpathyContact *sender;
	const char *body;
	NotificationData *cb_data;
	gboolean use_libindicate;
	EmpathyIndicator *indicator;

	empathy_conf_get_bool (empathy_conf_get (),
			       EMPATHY_PREFS_UI_USE_LIBINDICATE,
		               &use_libindicate);

	if (!use_libindicate) {
		return;
	}

	cb_data = g_slice_new0 (NotificationData);
	cb_data->chat = g_object_ref (chat);
	cb_data->window = window;

	sender = empathy_message_get_sender (message);
	body = empathy_message_get_body (message);

	indicator = g_hash_table_lookup (priv->indicators, chat);
	if (indicator) {
		empathy_indicator_update (indicator, body);
	} else {
		indicator = empathy_indicator_manager_add_indicator (priv->indicator_manager,
			sender, body);
		g_signal_connect (indicator, "activate",
				  G_CALLBACK (chat_window_indicator_activate_cb), cb_data);
		g_hash_table_insert (priv->indicators, chat, g_object_ref (indicator));
	}
	empathy_indicator_show (indicator);
}

static void
chat_window_remove_indicator (EmpathyChatWindow *window, EmpathyChat *chat)
{
	EmpathyIndicator *indicator;
	EmpathyChatWindowPriv *priv = GET_PRIV (window);

	indicator = g_hash_table_lookup (priv->indicators, chat);

	if (!indicator) {
		return;
	}

	empathy_indicator_hide (indicator);
	g_hash_table_remove (priv->indicators, chat);
	g_object_unref (indicator);
}
#endif

static void
chat_window_show_or_update_notification (EmpathyChatWindow *window,
					 EmpathyMessage *message,
					 EmpathyChat *chat)
{
	EmpathyContact *sender;
	char *header, *escaped;
	const char *body;
	GdkPixbuf *pixbuf;
	NotificationData *cb_data;
	EmpathyChatWindowPriv *priv = GET_PRIV (window);
	gboolean res;

	if (!empathy_notification_is_enabled ()) {
		return;
	} else {
		empathy_conf_get_bool (empathy_conf_get (),
				       EMPATHY_PREFS_NOTIFICATIONS_FOCUS, &res);
		if (!res) {
			return;
		}
	}

	cb_data = g_slice_new0 (NotificationData);
	cb_data->chat = g_object_ref (chat);
	cb_data->window = window;

	sender = empathy_message_get_sender (message);
	header = g_strdup_printf (_("New message from %s"),
				  empathy_contact_get_name (sender));
	body = empathy_message_get_body (message);
	escaped = g_markup_escape_text (body, -1);

	pixbuf = empathy_misc_get_pixbuf_for_notification (sender, EMPATHY_IMAGE_NEW_MESSAGE);

	if (priv->notification != NULL) {
		notify_notification_update (priv->notification,
					    header, escaped, NULL);
		notify_notification_set_icon_from_pixbuf (priv->notification, pixbuf);
	} else {
		priv->notification = notify_notification_new (header, escaped, NULL, NULL);
		notify_notification_set_timeout (priv->notification, NOTIFY_EXPIRES_DEFAULT);
		notify_notification_set_icon_from_pixbuf (priv->notification, pixbuf);

		g_signal_connect (priv->notification, "closed",
				  G_CALLBACK (chat_window_notification_closed_cb), cb_data);
	}

	notify_notification_show (priv->notification, NULL);

	g_object_unref (pixbuf);
	g_free (header);
	g_free (escaped);
}

static void
chat_window_new_message_cb (EmpathyChat       *chat,
			    EmpathyMessage    *message,
			    EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;
	gboolean              has_focus;
	gboolean              needs_urgency;
	EmpathyContact        *sender;

	priv = GET_PRIV (window);

	has_focus = empathy_chat_window_has_focus (window);

	/* - if we're the sender, we play the sound if it's specified in the
	 *   preferences and we're not away.
	 * - if we receive a message, we play the sound if it's specified in the
	 *   preferences and the window does not have focus on the chat receiving
	 *   the message.
	 */

	sender = empathy_message_get_sender (message);

	if (empathy_contact_is_user (sender)) {
		empathy_sound_play (GTK_WIDGET (priv->dialog),
				    EMPATHY_SOUND_MESSAGE_OUTGOING);
	}

	if (has_focus && priv->current_chat == chat) {
		return;
	}

	/* If empathy_chat_is_room () returns TRUE, that means it's a named MUC.
	 * If empathy_chat_get_remote_contact () returns NULL, that means it's
	 * an unamed MUC (msn-like).
	 * In case of a MUC, we set urgency only if the message contains our
	 * alias. */
	if (empathy_chat_is_room (chat) ||
	    empathy_chat_get_remote_contact (chat) == NULL) {
		needs_urgency = empathy_message_should_highlight (message);
	} else {
		needs_urgency = TRUE;
	}

	if (needs_urgency) {
		if (!has_focus)
			chat_window_set_urgency_hint (window, TRUE);

		empathy_sound_play (GTK_WIDGET (priv->dialog),
		    EMPATHY_SOUND_MESSAGE_INCOMING);
		chat_window_show_or_update_notification (window, message, chat);
#ifdef HAVE_LIBINDICATE
		chat_window_add_indicator (window, message, chat);
#endif
	}

	if (!g_list_find (priv->chats_new_msg, chat)) {
		priv->chats_new_msg = g_list_prepend (priv->chats_new_msg, chat);
		chat_window_update_chat_tab (chat);
	}
}

static GtkNotebook *
chat_window_detach_hook (GtkNotebook *source,
			 GtkWidget   *page,
			 gint         x,
			 gint         y,
			 gpointer     user_data)
{
	EmpathyChatWindowPriv *priv;
	EmpathyChatWindow     *window, *new_window;
	EmpathyChat           *chat;

	chat = EMPATHY_CHAT (page);
	window = chat_window_find_chat (chat);

	new_window = empathy_chat_window_new ();
	priv = GET_PRIV (new_window);

	DEBUG ("Detach hook called");

	empathy_chat_window_move_chat (window, new_window, chat);

	gtk_window_move (GTK_WINDOW (priv->dialog), x, y);
	gtk_widget_show (priv->dialog);

	return NULL;
}

static void
chat_window_page_switched_cb (GtkNotebook      *notebook,
			      GtkNotebookPage  *page,
			      gint	        page_num,
			      EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;
	EmpathyChat           *chat;
	GtkWidget            *child;

	DEBUG ("Page switched");

	priv = GET_PRIV (window);

	child = gtk_notebook_get_nth_page (notebook, page_num);
	chat = EMPATHY_CHAT (child);

	if (priv->page_added) {
		priv->page_added = FALSE;
		empathy_chat_scroll_down (chat);
	}
	else if (priv->current_chat == chat) {
		return;
	}

	priv->current_chat = chat;
	priv->chats_new_msg = g_list_remove (priv->chats_new_msg, chat);

	chat_window_update_chat_tab (chat);

#ifdef HAVE_LIBINDICATE
	chat_window_remove_indicator (window, chat);
#endif
}

static void
chat_window_page_added_cb (GtkNotebook      *notebook,
			   GtkWidget	    *child,
			   guint             page_num,
			   EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;
	EmpathyChat           *chat;

	priv = GET_PRIV (window);

	/* If we just received DND to the same window, we don't want
	 * to do anything here like removing the tab and then readding
	 * it, so we return here and in "page-added".
	 */
	if (priv->dnd_same_window) {
		DEBUG ("Page added (back to the same window)");
		priv->dnd_same_window = FALSE;
		return;
	}

	DEBUG ("Page added");

	/* Get chat object */
	chat = EMPATHY_CHAT (child);

	/* Connect chat signals for this window */
	g_signal_connect (chat, "composing",
			  G_CALLBACK (chat_window_composing_cb),
			  window);
	g_signal_connect (chat, "new-message",
			  G_CALLBACK (chat_window_new_message_cb),
			  window);

	/* Set flag so we know to perform some special operations on
	 * switch page due to the new page being added.
	 */
	priv->page_added = TRUE;

	/* Get list of chats up to date */
	priv->chats = g_list_append (priv->chats, chat);

	chat_window_update_chat_tab (chat);
}

static void
chat_window_page_removed_cb (GtkNotebook      *notebook,
			     GtkWidget	      *child,
			     guint             page_num,
			     EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;
	EmpathyChat           *chat;

	priv = GET_PRIV (window);

	/* If we just received DND to the same window, we don't want
	 * to do anything here like removing the tab and then readding
	 * it, so we return here and in "page-added".
	 */
	if (priv->dnd_same_window) {
		DEBUG ("Page removed (and will be readded to same window)");
		return;
	}

	DEBUG ("Page removed");

	/* Get chat object */
	chat = EMPATHY_CHAT (child);

	/* Disconnect all signal handlers for this chat and this window */
	g_signal_handlers_disconnect_by_func (chat,
					      G_CALLBACK (chat_window_composing_cb),
					      window);
	g_signal_handlers_disconnect_by_func (chat,
					      G_CALLBACK (chat_window_new_message_cb),
					      window);

	/* Keep list of chats up to date */
	priv->chats = g_list_remove (priv->chats, chat);
	priv->chats_new_msg = g_list_remove (priv->chats_new_msg, chat);
	priv->chats_composing = g_list_remove (priv->chats_composing, chat);

	if (priv->chats == NULL) {
		g_object_unref (window);
	} else {
		chat_window_update (window);
	}
}

static gboolean
chat_window_focus_in_event_cb (GtkWidget        *widget,
			       GdkEvent         *event,
			       EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;

	DEBUG ("Focus in event, updating title");

	priv = GET_PRIV (window);

	priv->chats_new_msg = g_list_remove (priv->chats_new_msg, priv->current_chat);

	chat_window_set_urgency_hint (window, FALSE);
	
	/* Update the title, since we now mark all unread messages as read. */
	chat_window_update_chat_tab (priv->current_chat);

#ifdef HAVE_LIBINDICATE
	/* Remove the indicator for the active chat */
	chat_window_remove_indicator (window, priv->current_chat);
#endif

	return FALSE;
}

static void
chat_window_drag_data_received (GtkWidget        *widget,
				GdkDragContext   *context,
				int               x,
				int               y,
				GtkSelectionData *selection,
				guint             info,
				guint             time,
				EmpathyChatWindow *window)
{
	if (info == DND_DRAG_TYPE_CONTACT_ID) {
		EmpathyChat           *chat;
		EmpathyChatWindow     *old_window;
		McAccount             *account;
		const gchar           *id;
		gchar                **strv;
		const gchar           *account_id;
		const gchar           *contact_id;

		id = (const gchar*) selection->data;

		DEBUG ("DND contact from roster with id:'%s'", id);
		
		strv = g_strsplit (id, "/", 2);
		account_id = strv[0];
		contact_id = strv[1];
		account = mc_account_lookup (account_id);
		chat = empathy_chat_window_find_chat (account, contact_id);

		if (!chat) {
			EmpathyAccountManager *account_manager;
			TpConnection *connection;

			account_manager = empathy_account_manager_dup_singleton ();
			connection = empathy_account_manager_get_connection (
				account_manager, account);

			if (connection) {
				empathy_dispatcher_chat_with_contact_id (
					connection, contact_id, NULL, NULL);
			}

			g_object_unref (account_manager);
			g_object_unref (account);
			g_strfreev (strv);
			return;
		}
		g_object_unref (account);
		g_strfreev (strv);

		old_window = chat_window_find_chat (chat);		
		if (old_window) {
			if (old_window == window) {
				gtk_drag_finish (context, TRUE, FALSE, time);
				return;
			}
			
			empathy_chat_window_move_chat (old_window, window, chat);
		} else {
			empathy_chat_window_add_chat (window, chat);
		}
		
		/* Added to take care of any outstanding chat events */
		empathy_chat_window_present_chat (chat);

		/* We should return TRUE to remove the data when doing
		 * GDK_ACTION_MOVE, but we don't here otherwise it has
		 * weird consequences, and we handle that internally
		 * anyway with add_chat () and remove_chat ().
		 */
		gtk_drag_finish (context, TRUE, FALSE, time);
	}
	else if (info == DND_DRAG_TYPE_TAB) {
		EmpathyChat        **chat;
		EmpathyChatWindow   *old_window = NULL;

		DEBUG ("DND tab");

		chat = (void *) selection->data;
		old_window = chat_window_find_chat (*chat);

		if (old_window) {
			EmpathyChatWindowPriv *priv;

			priv = GET_PRIV (window);

			if (old_window == window) {
				DEBUG ("DND tab (within same window)");
				priv->dnd_same_window = TRUE;
				gtk_drag_finish (context, TRUE, FALSE, time);
				return;
			}
			
			priv->dnd_same_window = FALSE;
		}

		/* We should return TRUE to remove the data when doing
		 * GDK_ACTION_MOVE, but we don't here otherwise it has
		 * weird consequences, and we handle that internally
		 * anyway with add_chat () and remove_chat ().
		 */
		gtk_drag_finish (context, TRUE, FALSE, time);
	} else {
		DEBUG ("DND from unknown source");
		gtk_drag_finish (context, FALSE, FALSE, time);
	}
}

static void
chat_window_finalize (GObject *object)
{
	EmpathyChatWindow     *window;
	EmpathyChatWindowPriv *priv;

	window = EMPATHY_CHAT_WINDOW (object);
	priv = GET_PRIV (window);

	DEBUG ("Finalized: %p", object);

	g_object_unref (priv->ui_manager);
	g_object_unref (priv->chatroom_manager);
	g_object_unref (priv->indicator_manager);
	g_object_unref (priv->indicators);
	if (priv->save_geometry_id != 0) {
		g_source_remove (priv->save_geometry_id);
	}

	if (priv->notification != NULL) {
		notify_notification_close (priv->notification, NULL);
		g_object_unref (priv->notification);
		priv->notification = NULL;
	}

	chat_windows = g_list_remove (chat_windows, window);
	gtk_widget_destroy (priv->dialog);

	G_OBJECT_CLASS (empathy_chat_window_parent_class)->finalize (object);
}

static void
empathy_chat_window_class_init (EmpathyChatWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = chat_window_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyChatWindowPriv));

	/* Set up a style for the close button with no focus padding. */
	gtk_rc_parse_string (
		"style \"empathy-close-button-style\"\n"
		"{\n"
		"  GtkWidget::focus-padding = 0\n"
		"  xthickness = 0\n"
		"  ythickness = 0\n"
		"}\n"
		"widget \"*.empathy-close-button\" style \"empathy-close-button-style\"");

	gtk_notebook_set_window_creation_hook (chat_window_detach_hook, NULL, NULL);
}

static void
empathy_chat_window_init (EmpathyChatWindow *window)
{
	GtkBuilder            *gui;
	GtkAccelGroup         *accel_group;
	GClosure              *closure;
	GtkWidget             *menu;
	GtkWidget             *submenu;
	gint                   i;
	GtkWidget             *chat_vbox;
	gchar                 *filename;
	EmpathySmileyManager  *smiley_manager;
	EmpathyChatWindowPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (window,
		EMPATHY_TYPE_CHAT_WINDOW, EmpathyChatWindowPriv);

	window->priv = priv;
	filename = empathy_file_lookup ("empathy-chat-window.ui", "src");
	gui = empathy_builder_get_file (filename,
				       "chat_window", &priv->dialog,
				       "chat_vbox", &chat_vbox,
				       "ui_manager", &priv->ui_manager,
				       "menu_conv_insert_smiley", &priv->menu_conv_insert_smiley,
				       "menu_conv_favorite", &priv->menu_conv_favorite,
				       "menu_edit_cut", &priv->menu_edit_cut,
				       "menu_edit_copy", &priv->menu_edit_copy,
				       "menu_edit_paste", &priv->menu_edit_paste,
				       "menu_tabs_next", &priv->menu_tabs_next,
				       "menu_tabs_prev", &priv->menu_tabs_prev,
				       "menu_tabs_left", &priv->menu_tabs_left,
				       "menu_tabs_right", &priv->menu_tabs_right,
				       "menu_tabs_detach", &priv->menu_tabs_detach,
				       NULL);
	g_free (filename);

	empathy_builder_connect (gui, window,
			      "chat_window", "configure-event", chat_window_configure_event_cb,
			      "menu_conv", "activate", chat_window_conv_activate_cb,
			      "menu_conv_clear", "activate", chat_window_clear_activate_cb,
			      "menu_conv_favorite", "toggled", chat_window_favorite_toggled_cb,
			      "menu_conv_close", "activate", chat_window_close_activate_cb,
			      "menu_edit", "activate", chat_window_edit_activate_cb,
			      "menu_edit_cut", "activate", chat_window_cut_activate_cb,
			      "menu_edit_copy", "activate", chat_window_copy_activate_cb,
			      "menu_edit_paste", "activate", chat_window_paste_activate_cb,
			      "menu_tabs_left", "activate", chat_window_tabs_left_activate_cb,
			      "menu_tabs_right", "activate", chat_window_tabs_right_activate_cb,
			      "menu_tabs_detach", "activate", chat_window_detach_activate_cb,
			      "menu_help_contents", "activate", chat_window_help_contents_activate_cb,
			      "menu_help_about", "activate", chat_window_help_about_activate_cb,
			      NULL);

	g_object_ref (priv->ui_manager);
	g_object_unref (gui);

	priv->chatroom_manager = empathy_chatroom_manager_dup_singleton (NULL);
#ifdef HAVE_LIBINDICATE
	priv->indicator_manager = empathy_indicator_manager_dup_singleton ();
	priv->indicators = g_hash_table_new (NULL, NULL);
#endif

	priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_group (GTK_NOTEBOOK (priv->notebook), "EmpathyChatWindow");
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (priv->notebook), TRUE);
	gtk_notebook_popup_enable (GTK_NOTEBOOK (priv->notebook));
	gtk_box_pack_start (GTK_BOX (chat_vbox), priv->notebook, TRUE, TRUE, 0);
	gtk_widget_show (priv->notebook);

	/* Set up accels */
	accel_group = gtk_accel_group_new ();
	gtk_window_add_accel_group (GTK_WINDOW (priv->dialog), accel_group);

	for (i = 0; i < G_N_ELEMENTS (tab_accel_keys); i++) {
		closure =  g_cclosure_new (G_CALLBACK (chat_window_accel_cb),
					   window,
					   NULL);
		gtk_accel_group_connect (accel_group,
					 tab_accel_keys[i],
					 GDK_MOD1_MASK,
					 0,
					 closure);
	}

	g_object_unref (accel_group);

	/* Set up smiley menu */
	smiley_manager = empathy_smiley_manager_dup_singleton ();
	submenu = empathy_smiley_menu_new (smiley_manager,
					   chat_window_insert_smiley_activate_cb,
					   window);
	menu = gtk_ui_manager_get_widget (priv->ui_manager,
		"/chats_menubar/menu_conv/menu_conv_insert_smiley");
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu), submenu);
	g_object_unref (smiley_manager);

	/* Set up signals we can't do with ui file since we may need to
	 * block/unblock them at some later stage.
	 */

	g_signal_connect (priv->dialog,
			  "delete_event",
			  G_CALLBACK (chat_window_delete_event_cb),
			  window);

	g_signal_connect_swapped (priv->menu_tabs_prev,
				  "activate",
				  G_CALLBACK (gtk_notebook_prev_page),
				  priv->notebook);
	g_signal_connect_swapped (priv->menu_tabs_next,
				  "activate",
				  G_CALLBACK (gtk_notebook_next_page),
				  priv->notebook);

	g_signal_connect (priv->dialog,
			  "focus_in_event",
			  G_CALLBACK (chat_window_focus_in_event_cb),
			  window);
	g_signal_connect_after (priv->notebook,
				"switch_page",
				G_CALLBACK (chat_window_page_switched_cb),
				window);
	g_signal_connect (priv->notebook,
			  "page_added",
			  G_CALLBACK (chat_window_page_added_cb),
			  window);
	g_signal_connect (priv->notebook,
			  "page_removed",
			  G_CALLBACK (chat_window_page_removed_cb),
			  window);

	/* Set up drag and drop */
	gtk_drag_dest_set (GTK_WIDGET (priv->notebook),
			   GTK_DEST_DEFAULT_ALL,
			   drag_types_dest,
			   G_N_ELEMENTS (drag_types_dest),
			   GDK_ACTION_MOVE);

	g_signal_connect (priv->notebook,
			  "drag-data-received",
			  G_CALLBACK (chat_window_drag_data_received),
			  window);

	chat_windows = g_list_prepend (chat_windows, window);

	/* Set up private details */
	priv->chats = NULL;
	priv->chats_new_msg = NULL;
	priv->chats_composing = NULL;
	priv->current_chat = NULL;
}

EmpathyChatWindow *
empathy_chat_window_new (void)
{
	return EMPATHY_CHAT_WINDOW (g_object_new (EMPATHY_TYPE_CHAT_WINDOW, NULL));
}

/* Returns the window to open a new tab in if there is only one window
 * visble, otherwise, returns NULL indicating that a new window should
 * be added.
 */
EmpathyChatWindow *
empathy_chat_window_get_default (void)
{
	GList    *l;
	gboolean  separate_windows = TRUE;

	empathy_conf_get_bool (empathy_conf_get (),
			      EMPATHY_PREFS_UI_SEPARATE_CHAT_WINDOWS,
			      &separate_windows);

	if (separate_windows) {
		/* Always create a new window */
		return NULL;
	}

	for (l = chat_windows; l; l = l->next) {
		EmpathyChatWindow *chat_window;
		GtkWidget         *dialog;

		chat_window = l->data;

		dialog = empathy_chat_window_get_dialog (chat_window);
		if (empathy_window_get_is_visible (GTK_WINDOW (dialog))) {
			/* Found a visible window on this desktop */
			return chat_window;
		}
	}

	return NULL;
}

GtkWidget *
empathy_chat_window_get_dialog (EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;

	g_return_val_if_fail (window != NULL, NULL);

	priv = GET_PRIV (window);

	return priv->dialog;
}

void
empathy_chat_window_add_chat (EmpathyChatWindow *window,
			      EmpathyChat	*chat)
{
	EmpathyChatWindowPriv *priv;
	GtkWidget             *label;
	GtkWidget             *popup_label;
	GtkWidget             *child;
	gint                   x, y, w, h;

	g_return_if_fail (window != NULL);
	g_return_if_fail (EMPATHY_IS_CHAT (chat));

	priv = GET_PRIV (window);

	/* Reference the chat object */
	g_object_ref (chat);

	/* If this window has just been created, position it */
	if (priv->chats == NULL) {
		empathy_geometry_load (chat_get_window_id_for_geometry (chat), &x, &y, &w, &h);
		
		if (x >= 0 && y >= 0) {
			/* Let the window manager position it if we don't have
			 * good x, y coordinates.
			 */
			gtk_window_move (GTK_WINDOW (priv->dialog), x, y);
		}
		
		if (w > 0 && h > 0) {
			/* Use the defaults from the ui file if we don't have
			 * good w, h geometry.
			 */
			gtk_window_resize (GTK_WINDOW (priv->dialog), w, h);
		}
	}

	child = GTK_WIDGET (chat);
	label = chat_window_create_label (window, chat, TRUE);
	popup_label = chat_window_create_label (window, chat, FALSE);
	gtk_widget_show (child);

	g_signal_connect (chat, "notify::name",
			  G_CALLBACK (chat_window_chat_notify_cb),
			  NULL);
	g_signal_connect (chat, "notify::subject",
			  G_CALLBACK (chat_window_chat_notify_cb),
			  NULL);
	g_signal_connect (chat, "notify::remote-contact",
			  G_CALLBACK (chat_window_chat_notify_cb),
			  NULL);
	chat_window_chat_notify_cb (chat);

	gtk_notebook_append_page_menu (GTK_NOTEBOOK (priv->notebook), child, label, popup_label);
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (priv->notebook), child, TRUE);
	gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (priv->notebook), child, TRUE);
	gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (priv->notebook), child,
					    TRUE, TRUE, GTK_PACK_START);

	DEBUG ("Chat added (%d references)", G_OBJECT (chat)->ref_count);
}

void
empathy_chat_window_remove_chat (EmpathyChatWindow *window,
				 EmpathyChat	   *chat)
{
	EmpathyChatWindowPriv *priv;
	gint                   position;
	EmpathyContact        *remote_contact;

	g_return_if_fail (window != NULL);
	g_return_if_fail (EMPATHY_IS_CHAT (chat));

	priv = GET_PRIV (window);

	g_signal_handlers_disconnect_by_func (chat,
					      chat_window_chat_notify_cb,
					      NULL);
	remote_contact = g_object_get_data (G_OBJECT (chat),
					    "chat-window-remote-contact");
	if (remote_contact) {
		g_signal_handlers_disconnect_by_func (remote_contact,
						      chat_window_update_chat_tab,
						      chat);
	}

	position = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook),
					  GTK_WIDGET (chat));
	gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook), position);

	DEBUG ("Chat removed (%d references)", G_OBJECT (chat)->ref_count - 1);

	g_object_unref (chat);
}

void
empathy_chat_window_move_chat (EmpathyChatWindow *old_window,
			       EmpathyChatWindow *new_window,
			       EmpathyChat       *chat)
{
	GtkWidget *widget;

	g_return_if_fail (EMPATHY_IS_CHAT_WINDOW (old_window));
	g_return_if_fail (EMPATHY_IS_CHAT_WINDOW (new_window));
	g_return_if_fail (EMPATHY_IS_CHAT (chat));

	widget = GTK_WIDGET (chat);

	DEBUG ("Chat moving with widget:%p (%d references)", widget,
		G_OBJECT (widget)->ref_count);

	/* We reference here to make sure we don't loose the widget
	 * and the EmpathyChat object during the move.
	 */
	g_object_ref (chat);
	g_object_ref (widget);

	empathy_chat_window_remove_chat (old_window, chat);
	empathy_chat_window_add_chat (new_window, chat);

	g_object_unref (widget);
	g_object_unref (chat);
}

void
empathy_chat_window_switch_to_chat (EmpathyChatWindow *window,
				    EmpathyChat	      *chat)
{
	EmpathyChatWindowPriv *priv;
	gint                  page_num;

	g_return_if_fail (window != NULL);
	g_return_if_fail (EMPATHY_IS_CHAT (chat));

	priv = GET_PRIV (window);

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook),
					  GTK_WIDGET (chat));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
				       page_num);
}

gboolean
empathy_chat_window_has_focus (EmpathyChatWindow *window)
{
	EmpathyChatWindowPriv *priv;
	gboolean              has_focus;

	g_return_val_if_fail (EMPATHY_IS_CHAT_WINDOW (window), FALSE);

	priv = GET_PRIV (window);

	g_object_get (priv->dialog, "has-toplevel-focus", &has_focus, NULL);

	return has_focus;
}

EmpathyChat *
empathy_chat_window_find_chat (McAccount   *account,
			       const gchar *id)
{
	GList *l;

	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (!EMP_STR_EMPTY (id), NULL);

	for (l = chat_windows; l; l = l->next) {
		EmpathyChatWindowPriv *priv;
		EmpathyChatWindow     *window;
		GList                *ll;

		window = l->data;
		priv = GET_PRIV (window);

		for (ll = priv->chats; ll; ll = ll->next) {
			EmpathyChat *chat;

			chat = ll->data;

			if (empathy_account_equal (account, empathy_chat_get_account (chat)) &&
			    !tp_strdiff (id, empathy_chat_get_id (chat))) {
				return chat;
			}
		}
	}

	return NULL;
}

void
empathy_chat_window_present_chat (EmpathyChat *chat)
{
	EmpathyChatWindow     *window;
	EmpathyChatWindowPriv *priv;

	g_return_if_fail (EMPATHY_IS_CHAT (chat));

	window = chat_window_find_chat (chat);

	/* If the chat has no window, create one */
	if (window == NULL) {
		window = empathy_chat_window_get_default ();
		if (!window) {
			window = empathy_chat_window_new ();
		}

		empathy_chat_window_add_chat (window, chat);
	}

	priv = GET_PRIV (window);
	empathy_chat_window_switch_to_chat (window, chat);
	empathy_window_present (GTK_WINDOW (priv->dialog), TRUE);

 	gtk_widget_grab_focus (chat->input_text_view);
}

