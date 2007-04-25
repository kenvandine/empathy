/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Geert-Jan Van den Bogaerde <geertjan@gnome.org>
 */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libempathy/gossip-contact.h>
#include <libempathy/gossip-debug.h>
#include <libempathy/gossip-message.h>
#include <libempathy/gossip-conf.h>

#include "gossip-chat-window.h"
//#include "gossip-add-contact-dialog.h"
//#include "gossip-chat-invite.h"
//#include "gossip-contact-info-dialog.h"
//#include "gossip-log-window.h"
//#include "gossip-new-chatroom-dialog.h"
#include "gossip-preferences.h"
#include "gossip-private-chat.h"
//#include "gossip-sound.h"
#include "gossip-stock.h"
#include "gossip-ui-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CHAT_WINDOW, GossipChatWindowPriv))

#define DEBUG_DOMAIN "ChatWindow"

#define URGENCY_TIMEOUT 60*1000

struct _GossipChatWindowPriv {
	GList       *chats;
	GList       *chats_new_msg;
	GList       *chats_composing;

	GossipChat  *current_chat;

	gboolean     page_added;
	gboolean     dnd_same_window;

	guint        urgency_timeout_id;

	GtkWidget   *dialog;
	GtkWidget   *notebook;

	GtkTooltips *tooltips;

	/* Menu items. */
	GtkWidget   *menu_conv_clear;
	GtkWidget   *menu_conv_insert_smiley;
	GtkWidget   *menu_conv_log;
	GtkWidget   *menu_conv_separator;
	GtkWidget   *menu_conv_add_contact;
	GtkWidget   *menu_conv_info;
	GtkWidget   *menu_conv_close;

	GtkWidget   *menu_room;
	GtkWidget   *menu_room_set_topic;
	GtkWidget   *menu_room_join_new;
	GtkWidget   *menu_room_invite;
	GtkWidget   *menu_room_add;
	GtkWidget   *menu_room_show_contacts;

	GtkWidget   *menu_edit_cut;
	GtkWidget   *menu_edit_copy;
	GtkWidget   *menu_edit_paste;

	GtkWidget   *menu_tabs_next;
	GtkWidget   *menu_tabs_prev;
	GtkWidget   *menu_tabs_left;
	GtkWidget   *menu_tabs_right;
	GtkWidget   *menu_tabs_detach;

	guint        save_geometry_id;
};

static void       gossip_chat_window_class_init         (GossipChatWindowClass *klass);
static void       gossip_chat_window_init               (GossipChatWindow      *window);
static void       gossip_chat_window_finalize           (GObject               *object);
static GdkPixbuf *chat_window_get_status_pixbuf         (GossipChatWindow      *window,
							 GossipChat            *chat);
static void       chat_window_accel_cb                  (GtkAccelGroup         *accelgroup,
							 GObject               *object,
							 guint                  key,
							 GdkModifierType        mod,
							 GossipChatWindow      *window);
static void       chat_window_close_clicked_cb          (GtkWidget             *button,
							 GossipChat            *chat);
static GtkWidget *chat_window_create_label              (GossipChatWindow      *window,
							 GossipChat            *chat);
static void       chat_window_update_status             (GossipChatWindow      *window,
							 GossipChat            *chat);
static void       chat_window_update_title              (GossipChatWindow      *window,
							 GossipChat            *chat);
static void       chat_window_update_menu               (GossipChatWindow      *window);
static gboolean   chat_window_save_geometry_timeout_cb  (GossipChatWindow      *window);
static gboolean   chat_window_configure_event_cb        (GtkWidget             *widget,
							 GdkEventConfigure     *event,
							 GossipChatWindow      *window);
static void       chat_window_conv_activate_cb          (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_clear_activate_cb         (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_info_activate_cb          (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_add_contact_activate_cb   (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_log_activate_cb           (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_show_contacts_toggled_cb  (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_edit_activate_cb          (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_insert_smiley_activate_cb (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_close_activate_cb         (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_room_set_topic_activate_cb(GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_room_join_new_activate_cb (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_room_invite_activate_cb   (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_room_add_activate_cb      (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_cut_activate_cb           (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_copy_activate_cb          (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_paste_activate_cb         (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_tabs_left_activate_cb     (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_tabs_right_activate_cb    (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static void       chat_window_detach_activate_cb        (GtkWidget             *menuitem,
							 GossipChatWindow      *window);
static gboolean   chat_window_delete_event_cb           (GtkWidget             *dialog,
							 GdkEvent              *event,
							 GossipChatWindow      *window);
static void       chat_window_status_changed_cb         (GossipChat            *chat,
							 GossipChatWindow      *window);
static void       chat_window_update_tooltip            (GossipChatWindow      *window,
							 GossipChat            *chat);
static void       chat_window_name_changed_cb           (GossipChat            *chat,
							 const gchar           *name,
							 GossipChatWindow      *window);
static void       chat_window_composing_cb              (GossipChat            *chat,
							 gboolean               is_composing,
							 GossipChatWindow      *window);
static void       chat_window_new_message_cb            (GossipChat            *chat,
							 GossipMessage         *message,
							 GossipChatWindow      *window);
static GtkNotebook* chat_window_detach_hook             (GtkNotebook           *source,
							 GtkWidget             *page,
							 gint                   x,
							 gint                   y,
							 gpointer               user_data);
static void       chat_window_page_switched_cb          (GtkNotebook           *notebook,
							 GtkNotebookPage       *page,
							 gint                   page_num,
							 GossipChatWindow      *window);
static void       chat_window_page_reordered_cb         (GtkNotebook           *notebook,
							 GtkWidget             *widget,
							 guint                  page_num,
							 GossipChatWindow      *window);
static void       chat_window_page_added_cb             (GtkNotebook           *notebook,
							 GtkWidget             *child,
							 guint                  page_num,
							 GossipChatWindow      *window);
static void       chat_window_page_removed_cb           (GtkNotebook           *notebook,
							 GtkWidget             *child,
							 guint                  page_num,
							 GossipChatWindow      *window);
static gboolean   chat_window_focus_in_event_cb         (GtkWidget             *widget,
							 GdkEvent              *event,
							 GossipChatWindow      *window);
static void       chat_window_drag_data_received        (GtkWidget             *widget,
							 GdkDragContext        *context,
							 int                    x,
							 int                    y,
							 GtkSelectionData      *selection,
							 guint                  info,
							 guint                  time,
							 GossipChatWindow      *window);
static void       chat_window_set_urgency_hint          (GossipChatWindow      *window,
							 gboolean               urgent);


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
	{ "text/contact-id", GTK_TARGET_SAME_APP, DND_DRAG_TYPE_CONTACT_ID },
	{ "GTK_NOTEBOOK_TAB", GTK_TARGET_SAME_APP, DND_DRAG_TYPE_TAB },
};

G_DEFINE_TYPE (GossipChatWindow, gossip_chat_window, G_TYPE_OBJECT);

static void
gossip_chat_window_class_init (GossipChatWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gossip_chat_window_finalize;

	g_type_class_add_private (object_class, sizeof (GossipChatWindowPriv));

	/* Set up a style for the close button with no focus padding. */
	gtk_rc_parse_string (
		"style \"gossip-close-button-style\"\n"
		"{\n"
		"  GtkWidget::focus-padding = 0\n"
		"  xthickness = 0\n"
		"  ythickness = 0\n"
		"}\n"
		"widget \"*.gossip-close-button\" style \"gossip-close-button-style\"");

	gtk_notebook_set_window_creation_hook (chat_window_detach_hook, NULL, NULL);
}

static void
gossip_chat_window_init (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GladeXML             *glade;
	GtkAccelGroup        *accel_group;
	GtkWidget            *image;
	GClosure             *closure;
	GtkWidget            *menu_conv;
	GtkWidget            *menu;
	gint                  i;
	GtkWidget            *chat_vbox;

	priv = GET_PRIV (window);

	priv->tooltips = g_object_ref (gtk_tooltips_new ());
	gtk_object_sink (GTK_OBJECT (priv->tooltips));

	glade = gossip_glade_get_file ("empathy-chat.glade",
				       "chat_window",
				       NULL,
				       "chat_window", &priv->dialog,
				       "chat_vbox", &chat_vbox,
				       "menu_conv", &menu_conv,
				       "menu_conv_clear", &priv->menu_conv_clear,
				       "menu_conv_insert_smiley", &priv->menu_conv_insert_smiley,
				       "menu_conv_log", &priv->menu_conv_log,
				       "menu_conv_separator", &priv->menu_conv_separator,
				       "menu_conv_add_contact", &priv->menu_conv_add_contact,
				       "menu_conv_info", &priv->menu_conv_info,
				       "menu_conv_close", &priv->menu_conv_close,
				       "menu_room", &priv->menu_room,
				       "menu_room_set_topic", &priv->menu_room_set_topic,
				       "menu_room_join_new", &priv->menu_room_join_new,
				       "menu_room_invite", &priv->menu_room_invite,
				       "menu_room_add", &priv->menu_room_add,
				       "menu_room_show_contacts", &priv->menu_room_show_contacts,
				       "menu_edit_cut", &priv->menu_edit_cut,
				       "menu_edit_copy", &priv->menu_edit_copy,
				       "menu_edit_paste", &priv->menu_edit_paste,
				       "menu_tabs_next", &priv->menu_tabs_next,
				       "menu_tabs_prev", &priv->menu_tabs_prev,
				       "menu_tabs_left", &priv->menu_tabs_left,
				       "menu_tabs_right", &priv->menu_tabs_right,
				       "menu_tabs_detach", &priv->menu_tabs_detach,
				       NULL);

	gossip_glade_connect (glade,
			      window,
			      "chat_window", "configure-event", chat_window_configure_event_cb,
			      "menu_conv", "activate", chat_window_conv_activate_cb,
			      "menu_conv_clear", "activate", chat_window_clear_activate_cb,
			      "menu_conv_log", "activate", chat_window_log_activate_cb,
			      "menu_conv_add_contact", "activate", chat_window_add_contact_activate_cb,
			      "menu_conv_info", "activate", chat_window_info_activate_cb,
			      "menu_conv_close", "activate", chat_window_close_activate_cb,
			      "menu_room_set_topic", "activate", chat_window_room_set_topic_activate_cb,
			      "menu_room_join_new", "activate", chat_window_room_join_new_activate_cb,
			      "menu_room_invite", "activate", chat_window_room_invite_activate_cb,
			      "menu_room_add", "activate", chat_window_room_add_activate_cb,
			      "menu_edit", "activate", chat_window_edit_activate_cb,
			      "menu_edit_cut", "activate", chat_window_cut_activate_cb,
			      "menu_edit_copy", "activate", chat_window_copy_activate_cb,
			      "menu_edit_paste", "activate", chat_window_paste_activate_cb,
			      "menu_tabs_left", "activate", chat_window_tabs_left_activate_cb,
			      "menu_tabs_right", "activate", chat_window_tabs_right_activate_cb,
			      "menu_tabs_detach", "activate", chat_window_detach_activate_cb,
			      NULL);

	g_object_unref (glade);

	priv->notebook = gtk_notebook_new ();
 	gtk_notebook_set_group_id (GTK_NOTEBOOK (priv->notebook), 1); 
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

	/* Set the contact information menu item image to the Gossip
	 * stock image
	 */
	image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (priv->menu_conv_info));
	gtk_image_set_from_stock (GTK_IMAGE (image),
				  GOSSIP_STOCK_CONTACT_INFORMATION,
				  GTK_ICON_SIZE_MENU);

	/* Set up smiley menu */
	menu = gossip_chat_view_get_smiley_menu (
		G_CALLBACK (chat_window_insert_smiley_activate_cb),
		window,
		priv->tooltips);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->menu_conv_insert_smiley), menu);

	/* Set up signals we can't do with glade since we may need to
	 * block/unblock them at some later stage.
	 */

	g_signal_connect (priv->dialog,
			  "delete_event",
			  G_CALLBACK (chat_window_delete_event_cb),
			  window);

	g_signal_connect (priv->menu_room_show_contacts,
			  "toggled",
			  G_CALLBACK (chat_window_show_contacts_toggled_cb),
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
			  "page_reordered",
			  G_CALLBACK (chat_window_page_reordered_cb),
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

/* Returns the window to open a new tab in if there is only one window
 * visble, otherwise, returns NULL indicating that a new window should
 * be added.
 */
GossipChatWindow *
gossip_chat_window_get_default (void)
{
	GList    *l;
	gboolean  separate_windows = TRUE;

	gossip_conf_get_bool (gossip_conf_get (),
			      GOSSIP_PREFS_UI_SEPARATE_CHAT_WINDOWS,
			      &separate_windows);

	if (separate_windows) {
		/* Always create a new window */
		return NULL;
	}

	for (l = chat_windows; l; l = l->next) {
		GossipChatWindow *chat_window;
		GtkWidget        *dialog;
		GdkWindow        *window;
		gboolean          visible;

		chat_window = l->data;

		dialog = gossip_chat_window_get_dialog (chat_window);
		window = dialog->window;

		g_object_get (dialog,
			      "visible", &visible,
			      NULL);

		visible = visible && !(gdk_window_get_state (window) & GDK_WINDOW_STATE_ICONIFIED);

		if (visible) {
			/* Found a visible window on this desktop */
			return chat_window;
		}
	}

	return NULL;
}

static void
gossip_chat_window_finalize (GObject *object)
{
	GossipChatWindow     *window;
	GossipChatWindowPriv *priv;

	window = GOSSIP_CHAT_WINDOW (object);
	priv = GET_PRIV (window);

	gossip_debug (DEBUG_DOMAIN, "Finalized: %p", object);

	if (priv->save_geometry_id != 0) {
		g_source_remove (priv->save_geometry_id);
	}

	if (priv->urgency_timeout_id != 0) {
		g_source_remove (priv->urgency_timeout_id);
	}

	chat_windows = g_list_remove (chat_windows, window);
	gtk_widget_destroy (priv->dialog);

	g_object_unref (priv->tooltips);

	G_OBJECT_CLASS (gossip_chat_window_parent_class)->finalize (object);
}

static GdkPixbuf *
chat_window_get_status_pixbuf (GossipChatWindow *window,
			       GossipChat       *chat)
{
	GossipChatWindowPriv *priv;
	GdkPixbuf            *pixbuf;

	priv = GET_PRIV (window);

	if (g_list_find (priv->chats_new_msg, chat)) {
		pixbuf = gossip_stock_render (GOSSIP_STOCK_MESSAGE,
					      GTK_ICON_SIZE_MENU);
	}
	else if (g_list_find (priv->chats_composing, chat)) {
		pixbuf = gossip_stock_render (GOSSIP_STOCK_TYPING,
					      GTK_ICON_SIZE_MENU);
	}
	else {
		pixbuf = gossip_chat_get_status_pixbuf (chat);
	}

	return pixbuf;
}

static void
chat_window_accel_cb (GtkAccelGroup    *accelgroup,
		      GObject          *object,
		      guint             key,
		      GdkModifierType   mod,
		      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
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

static void
chat_window_close_clicked_cb (GtkWidget  *button,
			      GossipChat *chat)
{
	GossipChatWindow *window;

	window = gossip_chat_get_window (chat);
	gossip_chat_window_remove_chat (window, chat);
}

static void
chat_window_close_button_style_set_cb (GtkWidget *button,
				       GtkStyle  *previous_style,
				       gpointer   user_data)
{
	gint h, w;

	gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (button),
					   GTK_ICON_SIZE_MENU, &w, &h);

	gtk_widget_set_size_request (button, w, h);
}

static GtkWidget *
chat_window_create_label (GossipChatWindow *window,
			  GossipChat       *chat)
{
	GossipChatWindowPriv *priv;
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

	name_label = gtk_label_new (gossip_chat_get_name (chat));
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
	g_object_set_data (G_OBJECT (chat), "chat-window-tab-label", name_label);

	status_image = gtk_image_new ();

	/* Spacing between the icon and label. */
	event_box_hbox = gtk_hbox_new (FALSE, 0);

	gtk_box_pack_start (GTK_BOX (event_box_hbox), status_image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (event_box_hbox), name_label, TRUE, TRUE, 0);

	g_object_set_data (G_OBJECT (chat), "chat-window-tab-image", status_image);
	g_object_set_data (G_OBJECT (chat), "chat-window-tab-tooltip-widget", event_box);

	close_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);

	/* We don't want focus/keynav for the button to avoid clutter, and
	 * Ctrl-W works anyway.
	 */
	GTK_WIDGET_UNSET_FLAGS (close_button, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS (close_button, GTK_CAN_DEFAULT);

	/* Set the name to make the special rc style match. */
	gtk_widget_set_name (close_button, "gossip-close-button");

	close_image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);

	gtk_container_add (GTK_CONTAINER (close_button), close_image);

	gtk_container_add (GTK_CONTAINER (event_box), event_box_hbox);
	gtk_box_pack_start (GTK_BOX (hbox), event_box, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);

	/* React to theme changes and also used to setup the initial size
	 * correctly.
	 */
	g_signal_connect (close_button,
			  "style-set",
			  G_CALLBACK (chat_window_close_button_style_set_cb),
			  chat);

	g_signal_connect (close_button,
			  "clicked",
			  G_CALLBACK (chat_window_close_clicked_cb),
			  chat);

	/* Set up tooltip */
	chat_window_update_tooltip (window, chat);

	gtk_widget_show_all (hbox);

	return hbox;
}

static void
chat_window_update_status (GossipChatWindow *window,
			   GossipChat       *chat)
{
	GtkImage  *image;
	GdkPixbuf *pixbuf;

	pixbuf = chat_window_get_status_pixbuf (window, chat);
	image = g_object_get_data (G_OBJECT (chat), "chat-window-tab-image");
	gtk_image_set_from_pixbuf (image, pixbuf);

	g_object_unref (pixbuf);
	
	chat_window_update_title (window, chat);
	chat_window_update_tooltip (window, chat);
}

static void
chat_window_update_title (GossipChatWindow *window,
			  GossipChat       *chat)
{
	GossipChatWindowPriv	*priv;
	GdkPixbuf		*pixbuf = NULL;
	const gchar             *str;
	gchar			*title;
	gint  			 n_chats;
	
	priv = GET_PRIV (window);
	
	n_chats = g_list_length (priv->chats);
	if (n_chats == 1) {
		if (priv->chats_new_msg) {
			title = g_strdup_printf (
				"%s - %s",
				gossip_chat_get_name (priv->current_chat),
				_("New Message"));
		}
		else if (gossip_chat_is_group_chat (priv->current_chat)) {
			title = g_strdup_printf (
				"%s - %s", 
				gossip_chat_get_name (priv->current_chat),
				_("Chat Room"));
		} else {
			title = g_strdup_printf (
				"%s - %s", 
				gossip_chat_get_name (priv->current_chat),
				_("Conversation"));
		}
	} else {
		if (priv->chats_new_msg) {
			GString *names;
			GList   *l;
			gint     n_messages = 0;

			names = g_string_new (NULL);

			for (l = priv->chats_new_msg; l; l = l->next) {
				n_messages++;
				g_string_append (names,
						 gossip_chat_get_name (l->data));
				if (l->next) {
					g_string_append (names, ", ");
				}
			}
			
			str = ngettext ("New Message", "New Messages", n_messages);
			title = g_strdup_printf ("%s - %s", names->str, str);
			g_string_free (names, TRUE);
		} else {
			str = ngettext ("Conversation", "Conversations (%d)", n_chats);
			title = g_strdup_printf (str, n_chats);
		}
	}

	gtk_window_set_title (GTK_WINDOW (priv->dialog), title);
	g_free (title);

	if (priv->chats_new_msg) {
		pixbuf = gossip_stock_render (GOSSIP_STOCK_MESSAGE,
					      GTK_ICON_SIZE_MENU);
	} else {
		pixbuf = NULL;
	}

	gtk_window_set_icon (GTK_WINDOW (priv->dialog), pixbuf);

	if (pixbuf) {
		g_object_unref (pixbuf);
	}
}

static void
chat_window_update_menu (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	gboolean              first_page;
	gboolean              last_page;
	gboolean              is_connected;
	gint                  num_pages;
	gint                  page_num;

	priv = GET_PRIV (window);

	/* Notebook pages */
	page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
	num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));
	first_page = (page_num == 0);
	last_page = (page_num == (num_pages - 1));

	gtk_widget_set_sensitive (priv->menu_tabs_next, !last_page);
	gtk_widget_set_sensitive (priv->menu_tabs_prev, !first_page);
	gtk_widget_set_sensitive (priv->menu_tabs_detach, num_pages > 1);
	gtk_widget_set_sensitive (priv->menu_tabs_left, !first_page);
	gtk_widget_set_sensitive (priv->menu_tabs_right, !last_page);

	is_connected = gossip_chat_is_connected (priv->current_chat);

	if (gossip_chat_is_group_chat (priv->current_chat)) {
#if 0
FIXME:
		GossipGroupChat       *group_chat;
		GossipChatroomManager *manager;
		GossipChatroom        *chatroom;
		GossipChatroomId       id;
		gboolean               saved;

		group_chat = GOSSIP_GROUP_CHAT (priv->current_chat);
		chatroom = gossip_group_chat_get_chatroom (group_chat);

		/* Show / Hide widgets */
		gtk_widget_show (priv->menu_room);

		gtk_widget_hide (priv->menu_conv_add_contact);
		gtk_widget_hide (priv->menu_conv_info);
		gtk_widget_hide (priv->menu_conv_separator);

		/* Can we add this room to our favourites and are we
		 * connected to the room?
		 */
		manager = gossip_app_get_chatroom_manager ();
		id = gossip_chatroom_get_id (chatroom);
		saved = gossip_chatroom_manager_find (manager, id) != NULL;

		gtk_widget_set_sensitive (priv->menu_room_add, !saved);
		gtk_widget_set_sensitive (priv->menu_conv_insert_smiley, is_connected);
		gtk_widget_set_sensitive (priv->menu_room_join_new, is_connected);
		gtk_widget_set_sensitive (priv->menu_room_invite, is_connected);

		/* We need to block the signal here because all we are
		 * really trying to do is check or uncheck the menu
		 * item. If we don't do this we get funny behaviour
		 * with 2 or more group chat windows where showing
		 * contacts doesn't do anything.
		 */
		show_contacts = gossip_chat_get_show_contacts (priv->current_chat);

		g_signal_handlers_block_by_func (priv->menu_room_show_contacts,
						 chat_window_show_contacts_toggled_cb,
						 window);

		g_object_set (priv->menu_room_show_contacts,
			      "active", show_contacts,
			      NULL);

		g_signal_handlers_unblock_by_func (priv->menu_room_show_contacts,
						   chat_window_show_contacts_toggled_cb,
						   window);
#endif
	} else {
		GossipSubscription  subscription;
		GossipContact      *contact;

		/* Show / Hide widgets */
		gtk_widget_hide (priv->menu_room);

		contact = gossip_chat_get_contact (priv->current_chat);
		subscription = gossip_contact_get_subscription (contact);
		if (!(subscription & GOSSIP_SUBSCRIPTION_FROM)) {
			gtk_widget_show (priv->menu_conv_add_contact);
		} else {
			gtk_widget_hide (priv->menu_conv_add_contact);
		}

		gtk_widget_show (priv->menu_conv_separator);
		gtk_widget_show (priv->menu_conv_info);

		/* Are we connected? */
		gtk_widget_set_sensitive (priv->menu_conv_insert_smiley, is_connected);
		gtk_widget_set_sensitive (priv->menu_conv_add_contact, is_connected);
		gtk_widget_set_sensitive (priv->menu_conv_info, is_connected);
	}
}

static void
chat_window_insert_smiley_activate_cb (GtkWidget        *menuitem,
				       GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat           *chat;
	GtkTextBuffer        *buffer;
	GtkTextIter           iter;
	const gchar          *smiley;

	priv = GET_PRIV (window);

	chat = priv->current_chat;

	smiley = g_object_get_data (G_OBJECT (menuitem), "smiley_text");

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (chat->input_text_view));
	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert (buffer, &iter,
				smiley, -1);
}

static void
chat_window_clear_activate_cb (GtkWidget        *menuitem,
			       GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	gossip_chat_clear (priv->current_chat);
}

static void
chat_window_add_contact_activate_cb (GtkWidget        *menuitem,
				     GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipContact        *contact;

	priv = GET_PRIV (window);

	contact = gossip_chat_get_contact (priv->current_chat);

	// FIXME: gossip_add_contact_dialog_show (NULL, contact);
}

static void
chat_window_log_activate_cb (GtkWidget        *menuitem,
			     GossipChatWindow *window)
{
/* FIXME:
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	if (gossip_chat_is_group_chat (priv->current_chat)) {
		GossipGroupChat *group_chat;
		GossipChatroom  *chatroom;

		group_chat = GOSSIP_GROUP_CHAT (priv->current_chat);
		chatroom = gossip_group_chat_get_chatroom (group_chat);
		gossip_log_window_show (NULL, chatroom);
	} else {
		GossipContact *contact;

		contact = gossip_chat_get_contact (priv->current_chat);
		gossip_log_window_show (contact, NULL);
	}
*/
}

static void
chat_window_info_activate_cb (GtkWidget        *menuitem,
			      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipContact        *contact;

	priv = GET_PRIV (window);

	contact = gossip_chat_get_contact (priv->current_chat);

/*FIXME:	gossip_contact_info_dialog_show (contact,
					 GTK_WINDOW (priv->dialog));*/
}

static gboolean
chat_window_save_geometry_timeout_cb (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	gint                  x, y, w, h;

	priv = GET_PRIV (window);

	gtk_window_get_size (GTK_WINDOW (priv->dialog), &w, &h);
	gtk_window_get_position (GTK_WINDOW (priv->dialog), &x, &y);

	gossip_chat_save_geometry (priv->current_chat, x, y, w, h);

	priv->save_geometry_id = 0;

	return FALSE;
}

static gboolean
chat_window_configure_event_cb (GtkWidget         *widget,
				GdkEventConfigure *event,
				GossipChatWindow  *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	/* Only save geometry information if there is ONE chat visible. */
	if (g_list_length (priv->chats) > 1) {
		return FALSE;
	}

	if (priv->save_geometry_id != 0) {
		g_source_remove (priv->save_geometry_id);
	}

	priv->save_geometry_id =
		g_timeout_add (500,
			       (GSourceFunc) chat_window_save_geometry_timeout_cb,
			       window);

	return FALSE;
}

static void
chat_window_conv_activate_cb (GtkWidget        *menuitem,
			      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	gboolean              log_exists = FALSE;

	priv = GET_PRIV (window);
/* FIXME:
	if (gossip_chat_is_group_chat (priv->current_chat)) {
		GossipGroupChat *group_chat;
		GossipChatroom  *chatroom;

		group_chat = GOSSIP_GROUP_CHAT (priv->current_chat);
		chatroom = gossip_group_chat_get_chatroom (group_chat);
		if (chatroom) {
			log_exists = gossip_log_exists_for_chatroom (chatroom);
		}
	} else {
		GossipContact *contact;

		contact = gossip_chat_get_contact (priv->current_chat);
		if (contact) {
			log_exists = gossip_log_exists_for_contact (contact);
		}
	}
*/
	gtk_widget_set_sensitive (priv->menu_conv_log, log_exists);
}

static void
chat_window_show_contacts_toggled_cb (GtkWidget        *menuitem,
				      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	gboolean              show;

	priv = GET_PRIV (window);

	g_return_if_fail (priv->current_chat != NULL);

	show = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (priv->menu_room_show_contacts));
	gossip_chat_set_show_contacts (priv->current_chat, show);
}

static void
chat_window_close_activate_cb (GtkWidget        *menuitem,
			       GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	g_return_if_fail (priv->current_chat != NULL);

	gossip_chat_window_remove_chat (window, priv->current_chat);
}

static void
chat_window_room_set_topic_activate_cb (GtkWidget        *menuitem,
					GossipChatWindow *window)
{
/*FIXME
	GossipChatWindowPriv *priv;
	
	priv = GET_PRIV (window);

	if (gossip_chat_is_group_chat (priv->current_chat)) {
		GossipGroupChat *group_chat;

		group_chat = GOSSIP_GROUP_CHAT (priv->current_chat);
		gossip_group_chat_set_topic (group_chat);
	}*/
}

static void
chat_window_room_join_new_activate_cb (GtkWidget        *menuitem,
				       GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	// FIXME: gossip_new_chatroom_dialog_show (GTK_WINDOW (priv->dialog));
}

static void
chat_window_room_invite_activate_cb (GtkWidget        *menuitem,
				     GossipChatWindow *window)
{
/* FIXME:
	GossipChatWindowPriv *priv;
	GossipContact        *own_contact;
	GossipChatroomId      id = 0;

	priv = GET_PRIV (window);
	own_contact = gossip_chat_get_own_contact (priv->current_chat);

	if (gossip_chat_is_group_chat (priv->current_chat)) {
		GossipGroupChat *group_chat;

		group_chat = GOSSIP_GROUP_CHAT (priv->current_chat);
		id = gossip_group_chat_get_chatroom_id (group_chat);
	}

	gossip_chat_invite_dialog_show (own_contact, id);
*/
}

static void
chat_window_room_add_activate_cb (GtkWidget        *menuitem,
				  GossipChatWindow *window)
{
/* FIXME:
	GossipChatWindowPriv  *priv;
	GossipGroupChat       *group_chat;
	GossipChatroomManager *manager;
	GossipChatroom        *chatroom;

	priv = GET_PRIV (window);

	g_return_if_fail (priv->current_chat != NULL);

	if (!gossip_chat_is_group_chat (priv->current_chat)) {
		return;
	}

	group_chat = GOSSIP_GROUP_CHAT (priv->current_chat);
	chatroom = gossip_group_chat_get_chatroom (group_chat);
	gossip_chatroom_set_favourite (chatroom, TRUE);

	manager = gossip_app_get_chatroom_manager ();
	gossip_chatroom_manager_add (manager, chatroom);
	gossip_chatroom_manager_store (manager);

	chat_window_update_menu (window);
*/
}

static void
chat_window_edit_activate_cb (GtkWidget        *menuitem,
			      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GtkClipboard         *clipboard;
	GtkTextBuffer        *buffer;
	gboolean              text_available;

	priv = GET_PRIV (window);

	g_return_if_fail (priv->current_chat != NULL);

	if (!gossip_chat_is_connected (priv->current_chat)) {
		gtk_widget_set_sensitive (priv->menu_edit_copy, FALSE);
		gtk_widget_set_sensitive (priv->menu_edit_cut, FALSE);
		gtk_widget_set_sensitive (priv->menu_edit_paste, FALSE);
		return;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->current_chat->input_text_view));
	if (gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL)) {
		gtk_widget_set_sensitive (priv->menu_edit_copy, TRUE);
		gtk_widget_set_sensitive (priv->menu_edit_cut, TRUE);
	} else {
		gboolean selection;

		selection = gossip_chat_view_get_selection_bounds (priv->current_chat->view, 
								   NULL, NULL);

		gtk_widget_set_sensitive (priv->menu_edit_cut, FALSE);
		gtk_widget_set_sensitive (priv->menu_edit_copy, selection);
	}

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	text_available = gtk_clipboard_wait_is_text_available (clipboard);
	gtk_widget_set_sensitive (priv->menu_edit_paste, text_available);
}

static void
chat_window_cut_activate_cb (GtkWidget        *menuitem,
			     GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT_WINDOW (window));

	priv = GET_PRIV (window);

	gossip_chat_cut (priv->current_chat);
}

static void
chat_window_copy_activate_cb (GtkWidget        *menuitem,
			      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT_WINDOW (window));

	priv = GET_PRIV (window);

	gossip_chat_copy (priv->current_chat);
}

static void
chat_window_paste_activate_cb (GtkWidget        *menuitem,
			       GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	g_return_if_fail (GOSSIP_IS_CHAT_WINDOW (window));

	priv = GET_PRIV (window);

	gossip_chat_paste (priv->current_chat);
}

static void
chat_window_tabs_left_activate_cb (GtkWidget        *menuitem,
				   GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat           *chat;
	gint                  index;

	priv = GET_PRIV (window);

	chat = priv->current_chat;
	index = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
	if (index <= 0) {
		return;
	}

	gtk_notebook_reorder_child (GTK_NOTEBOOK (priv->notebook),
				    gossip_chat_get_widget (chat),
				    index - 1);

	chat_window_update_menu (window);
	chat_window_update_status (window, chat);
}

static void
chat_window_tabs_right_activate_cb (GtkWidget        *menuitem,
				    GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat           *chat;
	gint                  index;

	priv = GET_PRIV (window);

	chat = priv->current_chat;
	index = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));

	gtk_notebook_reorder_child (GTK_NOTEBOOK (priv->notebook),
				    gossip_chat_get_widget (chat),
				    index + 1);

	chat_window_update_menu (window);
	chat_window_update_status (window, chat);
}

static void
chat_window_detach_activate_cb (GtkWidget        *menuitem,
				GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChatWindow     *new_window;
	GossipChat           *chat;

	priv = GET_PRIV (window);

	chat = priv->current_chat;
	new_window = gossip_chat_window_new ();

	gossip_chat_window_move_chat (window, new_window, chat);

	priv = GET_PRIV (new_window);
	gtk_widget_show (priv->dialog);
}

static gboolean
chat_window_delete_event_cb (GtkWidget        *dialog,
			     GdkEvent         *event,
			     GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GList                *list;
	GList                *l;

	priv = GET_PRIV (window);

	gossip_debug (DEBUG_DOMAIN, "Delete event received");

	list = g_list_copy (priv->chats);

	for (l = list; l; l = l->next) {
		gossip_chat_window_remove_chat (window, l->data);
	}

	g_list_free (list);

	return TRUE;
}

static void
chat_window_status_changed_cb (GossipChat       *chat,
			       GossipChatWindow *window)
{
	chat_window_update_menu (window);
	chat_window_update_status (window, chat);
}

static void
chat_window_update_tooltip (GossipChatWindow *window,
			    GossipChat       *chat)
{
	GossipChatWindowPriv *priv;
	GtkWidget            *widget;
	gchar                *current_tooltip;
	gchar                *str;

	priv = GET_PRIV (window);

	current_tooltip = gossip_chat_get_tooltip (chat);

	if (g_list_find (priv->chats_composing, chat)) {
		str = g_strconcat (current_tooltip, "\n", _("Typing a message."), NULL);
		g_free (current_tooltip);
	} else {
		str = current_tooltip;
	}

	widget = g_object_get_data (G_OBJECT (chat), "chat-window-tab-tooltip-widget");
	gtk_tooltips_set_tip (priv->tooltips,
			      widget,
			      str,
			      NULL);

	g_free (str);
}

static void
chat_window_name_changed_cb (GossipChat       *chat,
			     const gchar      *name,
			     GossipChatWindow *window)
{
	GtkLabel *label;

	label = g_object_get_data (G_OBJECT (chat), "chat-window-tab-label");

	gtk_label_set_text (label, name);
}

static void
chat_window_composing_cb (GossipChat       *chat,
			  gboolean          is_composing,
			  GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	if (is_composing && !g_list_find (priv->chats_composing, chat)) {
		priv->chats_composing = g_list_prepend (priv->chats_composing, chat);
	} else {
		priv->chats_composing = g_list_remove (priv->chats_composing, chat);
	}

	chat_window_update_status (window, chat);
}

static void
chat_window_new_message_cb (GossipChat       *chat,
			    GossipMessage    *message,
			    GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	gboolean              has_focus;
	gboolean              needs_urgency;

	priv = GET_PRIV (window);

	has_focus = gossip_chat_window_has_focus (window);
	
	if (has_focus && priv->current_chat == chat) {
		gossip_debug (DEBUG_DOMAIN, "New message, we have focus");
		return;
	}
	
	gossip_debug (DEBUG_DOMAIN, "New message, no focus");

	needs_urgency = FALSE;
	if (gossip_chat_is_group_chat (chat)) {		
		if (gossip_chat_should_highlight_nick (message)) {
			gossip_debug (DEBUG_DOMAIN, "Highlight this nick");
			needs_urgency = TRUE;
		}
	} else {
		needs_urgency = TRUE;
	}

	if (needs_urgency && !has_focus) {
		chat_window_set_urgency_hint (window, TRUE);
	}

	if (!g_list_find (priv->chats_new_msg, chat)) {
		priv->chats_new_msg = g_list_prepend (priv->chats_new_msg, chat);
		chat_window_update_status (window, chat);
	}
}

static GtkNotebook *
chat_window_detach_hook (GtkNotebook *source,
			 GtkWidget   *page,
			 gint         x,
			 gint         y,
			 gpointer     user_data)
{
	GossipChatWindowPriv *priv;
	GossipChatWindow     *window, *new_window;
	GossipChat           *chat;

	chat = g_object_get_data (G_OBJECT (page), "chat");
	window = gossip_chat_get_window (chat);

	new_window = gossip_chat_window_new ();
	priv = GET_PRIV (new_window);

	gossip_debug (DEBUG_DOMAIN, "Detach hook called");

	gossip_chat_window_move_chat (window, new_window, chat);

	gtk_window_move (GTK_WINDOW (priv->dialog), x, y);
	gtk_widget_show (priv->dialog);

	return NULL;
}

static void
chat_window_page_switched_cb (GtkNotebook      *notebook,
			      GtkNotebookPage  *page,
			      gint	        page_num,
			      GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat           *chat;
	GtkWidget            *child;

	gossip_debug (DEBUG_DOMAIN, "Page switched");

	priv = GET_PRIV (window);

	child = gtk_notebook_get_nth_page (notebook, page_num);
	chat = g_object_get_data (G_OBJECT (child), "chat");

	if (priv->page_added) {
		priv->page_added = FALSE;
		gossip_chat_scroll_down (chat);
	}
	else if (priv->current_chat == chat) {
		return;
	}

	priv->current_chat = chat;
	priv->chats_new_msg = g_list_remove (priv->chats_new_msg, chat);

	chat_window_update_menu (window);
	chat_window_update_status (window, chat);
}

static void
chat_window_page_reordered_cb (GtkNotebook      *notebook,
			       GtkWidget        *widget,
			       guint             page_num,
			       GossipChatWindow *window)
{
	gossip_debug (DEBUG_DOMAIN, "Page reordered");
	
	chat_window_update_menu (window);
}

static void
chat_window_page_added_cb (GtkNotebook      *notebook,
			   GtkWidget	    *child,
			   guint             page_num,
			   GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat           *chat;

	priv = GET_PRIV (window);

	/* If we just received DND to the same window, we don't want
	 * to do anything here like removing the tab and then readding
	 * it, so we return here and in "page-added".
	 */
	if (priv->dnd_same_window) {
		gossip_debug (DEBUG_DOMAIN, "Page added (back to the same window)");
		priv->dnd_same_window = FALSE;
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Page added");

	/* Get chat object */
	chat = g_object_get_data (G_OBJECT (child), "chat");

	/* Set the chat window */
	gossip_chat_set_window (chat, window);

	/* Connect chat signals for this window */
	g_signal_connect (chat, "status_changed",
			  G_CALLBACK (chat_window_status_changed_cb),
			  window);
	g_signal_connect (chat, "name_changed",
			  G_CALLBACK (chat_window_name_changed_cb),
			  window);
	g_signal_connect (chat, "composing",
			  G_CALLBACK (chat_window_composing_cb),
			  window);
	g_signal_connect (chat, "new_message",
			  G_CALLBACK (chat_window_new_message_cb),
			  window);

	/* Set flag so we know to perform some special operations on
	 * switch page due to the new page being added.
	 */
	priv->page_added = TRUE;

	/* Get list of chats up to date */
	priv->chats = g_list_append (priv->chats, chat);
}

static void
chat_window_page_removed_cb (GtkNotebook      *notebook,
			     GtkWidget	      *child,
			     guint             page_num,
			     GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	GossipChat           *chat;

	priv = GET_PRIV (window);

	/* If we just received DND to the same window, we don't want
	 * to do anything here like removing the tab and then readding
	 * it, so we return here and in "page-added".
	 */
	if (priv->dnd_same_window) {
		gossip_debug (DEBUG_DOMAIN, "Page removed (and will be readded to same window)");
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Page removed");

	/* Get chat object */
	chat = g_object_get_data (G_OBJECT (child), "chat");

	/* Unset the window associated with a chat */
	gossip_chat_set_window (chat, NULL);

	/* Disconnect all signal handlers for this chat and this window */
	g_signal_handlers_disconnect_by_func (chat,
					      G_CALLBACK (chat_window_status_changed_cb),
					      window);
	g_signal_handlers_disconnect_by_func (chat,
					      G_CALLBACK (chat_window_name_changed_cb),
					      window);
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
		chat_window_update_menu (window);
		chat_window_update_title (window, NULL);
	}
}

static gboolean
chat_window_focus_in_event_cb (GtkWidget        *widget,
			       GdkEvent         *event,
			       GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	gossip_debug (DEBUG_DOMAIN, "Focus in event, updating title");

	priv = GET_PRIV (window);

	priv->chats_new_msg = g_list_remove (priv->chats_new_msg, priv->current_chat);

	chat_window_set_urgency_hint (window, FALSE);
	
	/* Update the title, since we now mark all unread messages as read. */
	chat_window_update_status (window, priv->current_chat);

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
				GossipChatWindow *window)
{
	if (info == DND_DRAG_TYPE_CONTACT_ID) {
#if 0
FIXME:
		GossipChatManager *manager;
		GossipContact     *contact;
		GossipChat        *chat;
		GossipChatWindow  *old_window;
		const gchar       *id = NULL;

		if (selection) {
			id = (const gchar*) selection->data;
		}

		gossip_debug (DEBUG_DOMAIN, "DND contact from roster with id:'%s'", id);
		
		contact = gossip_session_find_contact (gossip_app_get_session (), id);
		if (!contact) {
			gossip_debug (DEBUG_DOMAIN, "DND contact from roster not found");
			return;
		}
		
		manager = gossip_app_get_chat_manager ();
		chat = GOSSIP_CHAT (gossip_chat_manager_get_chat (manager, contact));
		old_window = gossip_chat_get_window (chat);
		
		if (old_window) {
			if (old_window == window) {
				gtk_drag_finish (context, TRUE, FALSE, time);
				return;
			}
			
			gossip_chat_window_move_chat (old_window, window, chat);
		} else {
			gossip_chat_window_add_chat (window, chat);
		}
		
		/* Added to take care of any outstanding chat events */
		gossip_chat_manager_show_chat (manager, contact);

		/* We should return TRUE to remove the data when doing
		 * GDK_ACTION_MOVE, but we don't here otherwise it has
		 * weird consequences, and we handle that internally
		 * anyway with add_chat() and remove_chat().
		 */
		gtk_drag_finish (context, TRUE, FALSE, time);
#endif
	}
	else if (info == DND_DRAG_TYPE_TAB) {
		GossipChat        *chat = NULL;
		GossipChatWindow  *old_window;
		GtkWidget        **child = NULL;

		gossip_debug (DEBUG_DOMAIN, "DND tab");

		if (selection) {
			child = (void*) selection->data;
		}

		if (child) {
			chat = g_object_get_data (G_OBJECT (*child), "chat");
		}

		old_window = gossip_chat_get_window (chat);
		if (old_window) {
			GossipChatWindowPriv *priv;

			priv = GET_PRIV (window);

			if (old_window == window) {
				gossip_debug (DEBUG_DOMAIN, "DND tab (within same window)");
				priv->dnd_same_window = TRUE;
				gtk_drag_finish (context, TRUE, FALSE, time);
				return;
			}
			
			priv->dnd_same_window = FALSE;
		}

		/* We should return TRUE to remove the data when doing
		 * GDK_ACTION_MOVE, but we don't here otherwise it has
		 * weird consequences, and we handle that internally
		 * anyway with add_chat() and remove_chat().
		 */
		gtk_drag_finish (context, TRUE, FALSE, time);
	} else {
		gossip_debug (DEBUG_DOMAIN, "DND from unknown source");
		gtk_drag_finish (context, FALSE, FALSE, time);
	}
}

static gboolean
chat_window_urgency_timeout_func (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	gossip_debug (DEBUG_DOMAIN, "Turning off urgency hint");
	gtk_window_set_urgency_hint (GTK_WINDOW (priv->dialog), FALSE);

	priv->urgency_timeout_id = 0;

	return FALSE;
}

static void
chat_window_set_urgency_hint (GossipChatWindow *window,
			      gboolean          urgent)
{
	GossipChatWindowPriv *priv;

	priv = GET_PRIV (window);

	if (!urgent) {
		/* Remove any existing hint and timeout. */
		if (priv->urgency_timeout_id) {
			gossip_debug (DEBUG_DOMAIN, "Turning off urgency hint");
			gtk_window_set_urgency_hint (GTK_WINDOW (priv->dialog), FALSE);
			g_source_remove (priv->urgency_timeout_id);
			priv->urgency_timeout_id = 0;
		}
		return;
	}

	/* Add a new hint and renew any exising timeout or add a new one. */
	if (priv->urgency_timeout_id) {
		g_source_remove (priv->urgency_timeout_id);
	} else {
		gossip_debug (DEBUG_DOMAIN, "Turning on urgency hint");
		gtk_window_set_urgency_hint (GTK_WINDOW (priv->dialog), TRUE);
	}

	priv->urgency_timeout_id = g_timeout_add (
		URGENCY_TIMEOUT,
		(GSourceFunc) chat_window_urgency_timeout_func,
		window);
}

GossipChatWindow *
gossip_chat_window_new (void)
{
	return GOSSIP_CHAT_WINDOW (g_object_new (GOSSIP_TYPE_CHAT_WINDOW, NULL));
}

GtkWidget *
gossip_chat_window_get_dialog (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;

	g_return_val_if_fail (window != NULL, NULL);

	priv = GET_PRIV (window);

	return priv->dialog;
}

void
gossip_chat_window_add_chat (GossipChatWindow *window,
			     GossipChat	      *chat)
{
	GossipChatWindowPriv *priv;
	GtkWidget            *label;
	GtkWidget            *child;

	priv = GET_PRIV (window);

	/* Reference the chat object */
	g_object_ref (chat);

	/* Set the chat window */
	gossip_chat_set_window (chat, window);

	if (g_list_length (priv->chats) == 0) {
		gint x, y, w, h;

		gossip_chat_load_geometry (chat, &x, &y, &w, &h);

		if (x >= 0 && y >= 0) {
			/* Let the window manager position it if we don't have
			 * good x, y coordinates.
			 */
			gtk_window_move (GTK_WINDOW (priv->dialog), x, y);
		}

		if (w > 0 && h > 0) {
			/* Use the defaults from the glade file if we don't have
			 * good w, h geometry.
			 */
			gtk_window_resize (GTK_WINDOW (priv->dialog), w, h);
		}
	}

	child = gossip_chat_get_widget (chat);
	label = chat_window_create_label (window, chat); 

	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), child, label);
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (priv->notebook), child, TRUE);
	gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (priv->notebook), child, TRUE);
	gtk_notebook_set_tab_label_packing (GTK_NOTEBOOK (priv->notebook), child,
					    TRUE, TRUE, GTK_PACK_START); 

	gossip_debug (DEBUG_DOMAIN, 
		      "Chat added (%d references)",
		      G_OBJECT (chat)->ref_count);
}

void
gossip_chat_window_remove_chat (GossipChatWindow *window,
				GossipChat	 *chat)
{
	GossipChatWindowPriv *priv;
	gint                  position;

	priv = GET_PRIV (window);

	position = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook),
					  gossip_chat_get_widget (chat));
	gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook), position);

	gossip_debug (DEBUG_DOMAIN, 
		      "Chat removed (%d references)", 
		      G_OBJECT (chat)->ref_count - 1);

	g_object_unref (chat);
}

void
gossip_chat_window_move_chat (GossipChatWindow *old_window,
			      GossipChatWindow *new_window,
			      GossipChat       *chat)
{
	GtkWidget *widget;

	g_return_if_fail (GOSSIP_IS_CHAT_WINDOW (old_window));
	g_return_if_fail (GOSSIP_IS_CHAT_WINDOW (new_window));
	g_return_if_fail (GOSSIP_IS_CHAT (chat));

	widget = gossip_chat_get_widget (chat);

	gossip_debug (DEBUG_DOMAIN,
		      "Chat moving with widget:%p (%d references)", 
		      widget,
		      G_OBJECT (widget)->ref_count);

	/* We reference here to make sure we don't loose the widget
	 * and the GossipChat object during the move.
	 */
	g_object_ref (chat);
	g_object_ref (widget);

	gossip_chat_window_remove_chat (old_window, chat);
	gossip_chat_window_add_chat (new_window, chat);

	g_object_unref (widget);
	g_object_unref (chat);
}

void
gossip_chat_window_switch_to_chat (GossipChatWindow *window,
				   GossipChat	    *chat)
{
	GossipChatWindowPriv *priv;
	gint                  page_num;

	priv = GET_PRIV (window);

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook),
					  gossip_chat_get_widget (chat));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
				       page_num);
}

gboolean
gossip_chat_window_has_focus (GossipChatWindow *window)
{
	GossipChatWindowPriv *priv;
	gboolean              has_focus;

	g_return_val_if_fail (GOSSIP_IS_CHAT_WINDOW (window), FALSE);

	priv = GET_PRIV (window);

	g_object_get (priv->dialog, "has-toplevel-focus", &has_focus, NULL);

	return has_focus;
}
