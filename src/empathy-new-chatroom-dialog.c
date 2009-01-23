/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2007 Imendio AB
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
 * Authors: Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <libmissioncontrol/mission-control.h>
#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mc-profile.h>

#include <libempathy/empathy-tp-roomlist.h>
#include <libempathy/empathy-chatroom.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-dispatcher.h>

#include <libempathy-gtk/empathy-account-chooser.h>
#include <libempathy-gtk/empathy-ui-utils.h>

#include "empathy-new-chatroom-dialog.h"
#include "ephy-spinner.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

typedef struct {
	EmpathyTpRoomlist *room_list;

	GtkWidget         *window;
	GtkWidget         *vbox_widgets;
	GtkWidget         *table_info;
	GtkWidget         *label_account;
	GtkWidget         *account_chooser;
	GtkWidget         *label_server;
	GtkWidget         *entry_server;
	GtkWidget         *togglebutton_refresh;
	GtkWidget         *label_room;
	GtkWidget         *entry_room;
	GtkWidget         *vbox_browse;
	GtkWidget         *image_status;
	GtkWidget         *label_status;
	GtkWidget         *hbox_status;
	GtkWidget         *throbber;
	GtkWidget         *treeview;
	GtkTreeModel      *model;
	GtkWidget         *button_join;
	GtkWidget         *button_close;
} EmpathyNewChatroomDialog;

enum {
	COL_NAME,
	COL_ROOM,
	COL_COUNT
};

static void     new_chatroom_dialog_response_cb                     (GtkWidget               *widget,
								     gint                     response,
								     EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_destroy_cb                      (GtkWidget               *widget,
								     EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_setup                     (EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_add_columns               (EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_update_widgets                  (EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_account_changed_cb              (GtkComboBox              *combobox,
								     EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_roomlist_destroy_cb             (EmpathyTpRoomlist        *room_list,
								     EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_new_room_cb                     (EmpathyTpRoomlist        *room_list,
								     EmpathyChatroom          *chatroom,
								     EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_listing_cb                      (EmpathyTpRoomlist        *room_list,
								     gpointer                  unused,
								     EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_clear                     (EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_row_activated_cb          (GtkTreeView             *tree_view,
								     GtkTreePath             *path,
								     GtkTreeViewColumn       *column,
								     EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_selection_changed         (GtkTreeSelection        *selection,
								     EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_join                            (EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_entry_changed_cb                (GtkWidget               *entry,
								     EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_browse_start                    (EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_browse_stop                     (EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_entry_server_activate_cb        (GtkWidget               *widget,
								     EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_togglebutton_refresh_toggled_cb (GtkWidget               *widget,
								     EmpathyNewChatroomDialog *dialog);

static EmpathyNewChatroomDialog *dialog_p = NULL;

void
empathy_new_chatroom_dialog_show (GtkWindow *parent)
{
	EmpathyNewChatroomDialog *dialog;
	GladeXML                 *glade;
	GtkSizeGroup             *size_group;
	gchar                    *filename;

	if (dialog_p) {
		gtk_window_present (GTK_WINDOW (dialog_p->window));
		return;
	}

	dialog_p = dialog = g_new0 (EmpathyNewChatroomDialog, 1);

	filename = empathy_file_lookup ("empathy-new-chatroom-dialog.glade", "src");
	glade = empathy_glade_get_file (filename,
				       "new_chatroom_dialog",
				       NULL,
				       "new_chatroom_dialog", &dialog->window,
				       "table_info", &dialog->table_info,
				       "label_account", &dialog->label_account,
				       "label_server", &dialog->label_server,
				       "label_room", &dialog->label_room,
				       "entry_server", &dialog->entry_server,
				       "entry_room", &dialog->entry_room,
				       "togglebutton_refresh", &dialog->togglebutton_refresh,
				       "vbox_browse", &dialog->vbox_browse,
				       "image_status", &dialog->image_status,
				       "label_status", &dialog->label_status,
				       "hbox_status", &dialog->hbox_status,
				       "treeview", &dialog->treeview,
				       "button_join", &dialog->button_join,
				       NULL);
	g_free (filename);

	empathy_glade_connect (glade,
			      dialog,
			      "new_chatroom_dialog", "response", new_chatroom_dialog_response_cb,
			      "new_chatroom_dialog", "destroy", new_chatroom_dialog_destroy_cb,
			      "entry_server", "changed", new_chatroom_dialog_entry_changed_cb,
			      "entry_server", "activate", new_chatroom_dialog_entry_server_activate_cb,
			      "entry_room", "changed", new_chatroom_dialog_entry_changed_cb,
			      "togglebutton_refresh", "toggled", new_chatroom_dialog_togglebutton_refresh_toggled_cb,
			      NULL);

	g_object_unref (glade);

	g_object_add_weak_pointer (G_OBJECT (dialog->window), (gpointer) &dialog_p);

	/* Label alignment */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, dialog->label_account);
	gtk_size_group_add_widget (size_group, dialog->label_server);
	gtk_size_group_add_widget (size_group, dialog->label_room);

	g_object_unref (size_group);

	/* Set up chatrooms treeview */
	new_chatroom_dialog_model_setup (dialog);

	/* Add throbber */
	dialog->throbber = ephy_spinner_new ();
	ephy_spinner_set_size (EPHY_SPINNER (dialog->throbber), GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_widget_show (dialog->throbber);

	gtk_box_pack_start (GTK_BOX (dialog->hbox_status), dialog->throbber, 
			    FALSE, FALSE, 0);

	/* Account chooser for custom */
	dialog->account_chooser = empathy_account_chooser_new ();
	empathy_account_chooser_set_filter (EMPATHY_ACCOUNT_CHOOSER (dialog->account_chooser),
					    empathy_account_chooser_filter_is_connected,
					    NULL);
	gtk_table_attach_defaults (GTK_TABLE (dialog->table_info),
				   dialog->account_chooser,
				   1, 3, 0, 1);
	gtk_widget_show (dialog->account_chooser);

	g_signal_connect (GTK_COMBO_BOX (dialog->account_chooser), "changed",
			  G_CALLBACK (new_chatroom_dialog_account_changed_cb),
			  dialog);
	new_chatroom_dialog_account_changed_cb (GTK_COMBO_BOX (dialog->account_chooser),
						dialog);

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog->window),
					      GTK_WINDOW (parent));
	}

	gtk_widget_show (dialog->window);
}

static void
new_chatroom_dialog_response_cb (GtkWidget               *widget,
				 gint                     response,
				 EmpathyNewChatroomDialog *dialog)
{
	if (response == GTK_RESPONSE_OK) {
		new_chatroom_dialog_join (dialog);
	}

	gtk_widget_destroy (widget);
}

static void
new_chatroom_dialog_destroy_cb (GtkWidget               *widget,
				EmpathyNewChatroomDialog *dialog)
{
	if (dialog->room_list) {
		g_object_unref (dialog->room_list);
	}
  	g_object_unref (dialog->model);  

	g_free (dialog);
}

static void
new_chatroom_dialog_model_setup (EmpathyNewChatroomDialog *dialog)
{
	GtkTreeView      *view;
	GtkListStore     *store;
	GtkTreeSelection *selection;

	/* View */
	view = GTK_TREE_VIEW (dialog->treeview);

	g_signal_connect (view, "row-activated",
			  G_CALLBACK (new_chatroom_dialog_model_row_activated_cb),
			  dialog);

	/* Store/Model */
	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_STRING,       /* Image */
				    G_TYPE_STRING,       /* Text */
				    G_TYPE_STRING);      /* Room */

	dialog->model = GTK_TREE_MODEL (store);
	gtk_tree_view_set_model (view, dialog->model);

	/* Selection */
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      COL_NAME, GTK_SORT_ASCENDING);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (new_chatroom_dialog_model_selection_changed),
			  dialog);

	/* Columns */
	new_chatroom_dialog_model_add_columns (dialog);
}

static void
new_chatroom_dialog_model_add_columns (EmpathyNewChatroomDialog *dialog)
{
	GtkTreeView       *view;
	GtkTreeViewColumn *column;
	GtkCellRenderer   *cell;

	view = GTK_TREE_VIEW (dialog->treeview);
	gtk_tree_view_set_headers_visible (view, FALSE);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell,
		      "xpad", (guint) 4,
		      "ypad", (guint) 1,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	column = gtk_tree_view_column_new_with_attributes (_("Chat Rooms"),
		                                           cell,
		                                           "text", COL_NAME,
		                                           NULL);

	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_append_column (view, column);
}

static void
new_chatroom_dialog_update_widgets (EmpathyNewChatroomDialog *dialog)
{
	EmpathyAccountChooser *account_chooser;
	McAccount             *account;
	McProfile             *profile;
	const gchar           *protocol;
	const gchar           *room;
	
	account_chooser = EMPATHY_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = empathy_account_chooser_get_account (account_chooser);
	profile = mc_account_get_profile (account);
	protocol = mc_profile_get_protocol_name (profile);

	gtk_entry_set_text (GTK_ENTRY (dialog->entry_server), "");

	/* hardcode here known protocols */
	if (strcmp (protocol, "jabber") == 0) {
		gtk_widget_set_sensitive (dialog->entry_server, TRUE);
		gtk_widget_show (dialog->vbox_browse);

	}
	else if (strcmp (protocol, "local-xmpp") == 0) {
		gtk_widget_set_sensitive (dialog->entry_server, FALSE);
		gtk_widget_show (dialog->vbox_browse);		
	}
	else if (strcmp (protocol, "irc") == 0) {
		gtk_widget_set_sensitive (dialog->entry_server, FALSE);
		gtk_widget_show (dialog->vbox_browse);		
	} else {
		gtk_widget_set_sensitive (dialog->entry_server, TRUE);
		gtk_widget_show (dialog->vbox_browse);
	}

	room = gtk_entry_get_text (GTK_ENTRY (dialog->entry_room));
	gtk_widget_set_sensitive (dialog->button_join, !EMP_STR_EMPTY (room));

	/* Final set up of the dialog */
	gtk_widget_grab_focus (dialog->entry_room);

	g_object_unref (account);
	g_object_unref (profile);
}

static void
new_chatroom_dialog_account_changed_cb (GtkComboBox             *combobox,
					EmpathyNewChatroomDialog *dialog)
{
	EmpathyAccountChooser *account_chooser;
	McAccount             *account;
	gboolean               listing = FALSE;

	if (dialog->room_list) {
		g_object_unref (dialog->room_list);
	}

	ephy_spinner_stop (EPHY_SPINNER (dialog->throbber));
	new_chatroom_dialog_model_clear (dialog);

	account_chooser = EMPATHY_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = empathy_account_chooser_get_account (account_chooser);
	dialog->room_list = empathy_tp_roomlist_new (account);

	if (dialog->room_list)	{
		g_signal_connect (dialog->room_list, "destroy",
				  G_CALLBACK (new_chatroom_dialog_roomlist_destroy_cb),
				  dialog);
		g_signal_connect (dialog->room_list, "new-room",
				  G_CALLBACK (new_chatroom_dialog_new_room_cb),
				  dialog);
		g_signal_connect (dialog->room_list, "notify::is-listing",
				  G_CALLBACK (new_chatroom_dialog_listing_cb),
				  dialog);

		listing = empathy_tp_roomlist_is_listing (dialog->room_list);
		if (listing) {
			ephy_spinner_start (EPHY_SPINNER (dialog->throbber));
		}
	}

	new_chatroom_dialog_update_widgets (dialog);
}

static void
new_chatroom_dialog_roomlist_destroy_cb (EmpathyTpRoomlist        *room_list,
					 EmpathyNewChatroomDialog *dialog)
{
	g_object_unref (dialog->room_list);
	dialog->room_list = NULL;
}

static void
new_chatroom_dialog_new_room_cb (EmpathyTpRoomlist        *room_list,
				 EmpathyChatroom          *chatroom,
				 EmpathyNewChatroomDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkListStore     *store;
	GtkTreeIter       iter;

	DEBUG ("New chatroom listed: %s (%s)",
		empathy_chatroom_get_name (chatroom),
		empathy_chatroom_get_room (chatroom));

	/* Add to model */
	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);
	store = GTK_LIST_STORE (dialog->model);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_NAME, empathy_chatroom_get_name (chatroom),
			    COL_ROOM, empathy_chatroom_get_room (chatroom),
			    -1);
}

static void
new_chatroom_dialog_listing_cb (EmpathyTpRoomlist        *room_list,
				gpointer                  unused,
				EmpathyNewChatroomDialog *dialog)
{
	gboolean listing;

	listing = empathy_tp_roomlist_is_listing (room_list);

	/* Update the throbber */
	if (listing) {
		ephy_spinner_start (EPHY_SPINNER (dialog->throbber));		
	} else {
		ephy_spinner_stop (EPHY_SPINNER (dialog->throbber));
	}

	/* Update the refresh toggle button */
	g_signal_handlers_block_by_func (dialog->togglebutton_refresh,
					 new_chatroom_dialog_togglebutton_refresh_toggled_cb,
					 dialog);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->togglebutton_refresh),
				      listing);
	g_signal_handlers_unblock_by_func (dialog->togglebutton_refresh,
					   new_chatroom_dialog_togglebutton_refresh_toggled_cb,
					   dialog);
}

static void
new_chatroom_dialog_model_clear (EmpathyNewChatroomDialog *dialog)
{
	GtkListStore *store;

	store = GTK_LIST_STORE (dialog->model);
	gtk_list_store_clear (store);
}

static void
new_chatroom_dialog_model_row_activated_cb (GtkTreeView             *tree_view,
					    GtkTreePath             *path,
					    GtkTreeViewColumn       *column,
					    EmpathyNewChatroomDialog *dialog)
{
	gtk_widget_activate (dialog->button_join);
}

static void
new_chatroom_dialog_model_selection_changed (GtkTreeSelection         *selection,
					     EmpathyNewChatroomDialog *dialog)
{	
	GtkTreeModel *model;
	GtkTreeIter   iter;
	gchar        *room = NULL;
	gchar        *server = NULL;

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
	}

	gtk_tree_model_get (model, &iter, COL_ROOM, &room, -1);
	server = strstr (room, "@");
	if (server) {
		*server = '\0';
		server++;
	}

	gtk_entry_set_text (GTK_ENTRY (dialog->entry_server), server ? server : "");
	gtk_entry_set_text (GTK_ENTRY (dialog->entry_room), room ? room : "");

	g_free (room);
}

static void
new_chatroom_dialog_join (EmpathyNewChatroomDialog *dialog)
{
	EmpathyAccountChooser *account_chooser;
	McAccount             *account;
	const gchar           *room;
	const gchar           *server = NULL;
	gchar                 *room_name = NULL;

	room = gtk_entry_get_text (GTK_ENTRY (dialog->entry_room));
	server = gtk_entry_get_text (GTK_ENTRY (dialog->entry_server));

	account_chooser = EMPATHY_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = empathy_account_chooser_get_account (account_chooser);

	if (!EMP_STR_EMPTY (server)) {
		room_name = g_strconcat (room, "@", server, NULL);
	} else {
		room_name = g_strdup (room);
	}

	DEBUG ("Requesting channel for '%s'", room_name);
	empathy_dispatcher_join_muc (account, room_name, NULL, NULL);

	g_free (room_name);
}

static void
new_chatroom_dialog_entry_changed_cb (GtkWidget                *entry,
				      EmpathyNewChatroomDialog *dialog)
{
	if (entry == dialog->entry_room) {
		const gchar *room;

		room = gtk_entry_get_text (GTK_ENTRY (dialog->entry_room));
		gtk_widget_set_sensitive (dialog->button_join, !EMP_STR_EMPTY (room));
		/* FIXME: Select the room in the list */
	}
}

static void
new_chatroom_dialog_browse_start (EmpathyNewChatroomDialog *dialog)
{
	new_chatroom_dialog_model_clear (dialog);
	if (dialog->room_list) {
		empathy_tp_roomlist_start (dialog->room_list);
	}
}

static void
new_chatroom_dialog_browse_stop (EmpathyNewChatroomDialog *dialog)
{
	if (dialog->room_list) {
		empathy_tp_roomlist_stop (dialog->room_list);
	}
}

static void
new_chatroom_dialog_entry_server_activate_cb (GtkWidget                *widget,
					      EmpathyNewChatroomDialog  *dialog)
{
	new_chatroom_dialog_togglebutton_refresh_toggled_cb (dialog->togglebutton_refresh, 
							     dialog);
}

static void
new_chatroom_dialog_togglebutton_refresh_toggled_cb (GtkWidget               *widget,
						     EmpathyNewChatroomDialog *dialog)
{
	gboolean toggled;

	toggled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	
	if (toggled) {
		new_chatroom_dialog_browse_start (dialog);
	} else {
		new_chatroom_dialog_browse_stop (dialog);
	}
}

