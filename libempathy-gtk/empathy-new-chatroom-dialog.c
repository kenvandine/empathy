/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2007 Imendio AB
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
 * Authors: Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <libmissioncontrol/mission-control.h>
#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mc-profile.h>

#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-debug.h>

#include "empathy-new-chatroom-dialog.h"
#include "empathy-account-chooser.h"
#include "empathy-ui-utils.h"
#include "ephy-spinner.h"

#define DEBUG_DOMAIN "NewChatroomDialog"

typedef struct {
	GtkWidget    *window;

	GtkWidget    *vbox_widgets;
	GtkWidget    *table_info;

	GtkWidget    *label_account;
	GtkWidget    *account_chooser;

	GtkWidget    *label_server;
	GtkWidget    *entry_server;
	GtkWidget    *togglebutton_refresh;
	
	GtkWidget    *label_room;
	GtkWidget    *entry_room;

	GtkWidget    *vbox_browse;
	GtkWidget    *image_status;
	GtkWidget    *label_status;
	GtkWidget    *hbox_status;
	GtkWidget    *throbber;
	GtkWidget    *treeview;
	GtkTreeModel *model;
	GtkTreeModel *filter;

	GtkWidget    *button_join;
	GtkWidget    *button_close;
} EmpathyNewChatroomDialog;

typedef struct {
	guint  handle;
	gchar *channel_type;
	gchar *name;
	gchar *id;
} EmpathyRoomListItem;

enum {
	COL_IMAGE,
	COL_NAME,
	COL_POINTER,
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
static void     new_chatroom_dialog_account_changed_cb              (GtkComboBox             *combobox,
								     EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_add                       (EmpathyNewChatroomDialog *dialog,
								     EmpathyRoomListItem     *item);
static void     new_chatroom_dialog_model_clear                     (EmpathyNewChatroomDialog *dialog);
static GList *  new_chatroom_dialog_model_get_selected              (EmpathyNewChatroomDialog *dialog);
static gboolean new_chatroom_dialog_model_filter_func               (GtkTreeModel            *model,
								     GtkTreeIter             *iter,
								     EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_row_activated_cb          (GtkTreeView             *tree_view,
								     GtkTreePath             *path,
								     GtkTreeViewColumn       *column,
								     EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_row_inserted_cb           (GtkTreeModel            *model,
								     GtkTreePath             *path,
								     GtkTreeIter             *iter,
								     EmpathyNewChatroomDialog *dialog);
static void     new_chatroom_dialog_model_row_deleted_cb            (GtkTreeModel            *model,
								     GtkTreePath             *path,
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

	if (dialog_p) {
		gtk_window_present (GTK_WINDOW (dialog_p->window));
		return;
	}

	dialog_p = dialog = g_new0 (EmpathyNewChatroomDialog, 1);

	glade = empathy_glade_get_file ("empathy-new-chatroom-dialog.glade",
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

	/* Account chooser for custom */
	dialog->account_chooser = empathy_account_chooser_new ();
	gtk_table_attach_defaults (GTK_TABLE (dialog->table_info),
				   dialog->account_chooser,
				   1, 3, 0, 1);
	gtk_widget_show (dialog->account_chooser);

	g_signal_connect (GTK_COMBO_BOX (dialog->account_chooser), "changed",
			  G_CALLBACK (new_chatroom_dialog_account_changed_cb),
			  dialog);

	/* Add throbber */
	dialog->throbber = ephy_spinner_new ();
	ephy_spinner_set_size (EPHY_SPINNER (dialog->throbber), GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_widget_show (dialog->throbber);

	gtk_box_pack_start (GTK_BOX (dialog->hbox_status), dialog->throbber, 
			    FALSE, FALSE, 0);

	/* Set up chatrooms treeview */
	new_chatroom_dialog_model_setup (dialog);

	/* Set things up according to the account type */
	new_chatroom_dialog_update_widgets (dialog);

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
  	g_object_unref (dialog->model);  
 	g_object_unref (dialog->filter); 

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
				    G_TYPE_POINTER);     /* infos */

	dialog->model = GTK_TREE_MODEL (store);

	/* Filter */
	dialog->filter = gtk_tree_model_filter_new (dialog->model, NULL);

	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (dialog->filter),
						(GtkTreeModelFilterVisibleFunc)
						new_chatroom_dialog_model_filter_func,
						dialog,
						NULL);

	gtk_tree_view_set_model (view, dialog->filter);

	g_signal_connect (dialog->filter, "row-inserted",
			  G_CALLBACK (new_chatroom_dialog_model_row_inserted_cb),
			  dialog);
	g_signal_connect (dialog->filter, "row-deleted",
			  G_CALLBACK (new_chatroom_dialog_model_row_deleted_cb),
			  dialog);

	/* Selection */
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      COL_NAME, GTK_SORT_ASCENDING);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (new_chatroom_dialog_model_selection_changed), dialog);

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

	/* Chatroom pointer */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Chat Rooms"));

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell,
		      "xpad", (guint) 4,
		      "ypad", (guint) 1,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	gtk_tree_view_column_pack_start (column, cell, TRUE);

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
	else if (strcmp (protocol, "salut") == 0) {
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
	gtk_widget_set_sensitive (dialog->button_join, !G_STR_EMPTY (room));

	/* Final set up of the dialog */
	gtk_widget_grab_focus (dialog->entry_room);

	g_object_unref (account);
	g_object_unref (profile);
}

static void
new_chatroom_dialog_account_changed_cb (GtkComboBox             *combobox,
					EmpathyNewChatroomDialog *dialog)
{
	new_chatroom_dialog_update_widgets (dialog);
}

static void
new_chatroom_dialog_model_add (EmpathyNewChatroomDialog *dialog,
			       EmpathyRoomListItem     *item)
{
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkListStore     *store;
	GtkTreeIter       iter;

	/* Add to model */
	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);
	store = GTK_LIST_STORE (dialog->model);

	gtk_list_store_append (store, &iter);

	gtk_list_store_set (store, &iter,
			    COL_NAME, item->name,
			    COL_POINTER, item,
			    -1);
}

static void
new_chatroom_dialog_model_clear (EmpathyNewChatroomDialog *dialog)
{
	GtkListStore *store;

	store = GTK_LIST_STORE (dialog->model);
	gtk_list_store_clear (store);
}

static GList *
new_chatroom_dialog_model_get_selected (EmpathyNewChatroomDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GList            *rows, *l;
	GList            *chatrooms = NULL;

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);

	rows = gtk_tree_selection_get_selected_rows (selection, NULL);
	for (l = rows; l; l = l->next) {
		GtkTreeIter          iter;
		EmpathyRoomListItem *chatroom;

		if (!gtk_tree_model_get_iter (model, &iter, l->data)) {
			continue;
		}

		gtk_tree_model_get (model, &iter, COL_POINTER, &chatroom, -1);
		chatrooms = g_list_append (chatrooms, chatroom);
	}

	g_list_foreach (rows, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (rows);

	return chatrooms;
}

static gboolean
new_chatroom_dialog_model_filter_func (GtkTreeModel            *model,
				       GtkTreeIter             *iter,
				       EmpathyNewChatroomDialog *dialog)
{
	EmpathyRoomListItem *chatroom;
	const gchar         *text;
	gchar               *room_nocase;
	gchar               *text_nocase;
	gboolean             found = FALSE;

	gtk_tree_model_get (model, iter, COL_POINTER, &chatroom, -1);

	if (!chatroom) {
		return TRUE;
	}

	text = gtk_entry_get_text (GTK_ENTRY (dialog->entry_room));

	/* Casefold */
	room_nocase = g_utf8_casefold (chatroom->id, -1);
	text_nocase = g_utf8_casefold (text, -1);

	/* Compare */
	if (g_utf8_strlen (text_nocase, -1) < 1 ||
	    strstr (room_nocase, text_nocase)) {
		found = TRUE;
	}

	g_free (room_nocase);
	g_free (text_nocase);

	return found;
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
new_chatroom_dialog_model_row_inserted_cb (GtkTreeModel            *model,
					   GtkTreePath             *path,
					   GtkTreeIter             *iter,
					   EmpathyNewChatroomDialog *dialog)
{
}

static void
new_chatroom_dialog_model_row_deleted_cb (GtkTreeModel            *model,
					  GtkTreePath             *path,
					  EmpathyNewChatroomDialog *dialog)
{
}

static void
new_chatroom_dialog_model_selection_changed (GtkTreeSelection      *selection,
					     EmpathyNewChatroomDialog *dialog)
{
}

static void
new_chatroom_dialog_join (EmpathyNewChatroomDialog *dialog)
{
	McAccount            *account;
	EmpathyAccountChooser *account_chooser;
	MissionControl       *mc;
	GList                *chatrooms, *l;
	const gchar          *room;
	const gchar          *server = NULL;
	gchar                *room_name = NULL;

	chatrooms = new_chatroom_dialog_model_get_selected (dialog);
	if (chatrooms) {
		for (l = chatrooms; l; l = l->next) {
			/* Join it */
		}
		g_list_free (chatrooms);
		return;
	}

	room = gtk_entry_get_text (GTK_ENTRY (dialog->entry_room));
	server = gtk_entry_get_text (GTK_ENTRY (dialog->entry_server));

	account_chooser = EMPATHY_ACCOUNT_CHOOSER (dialog->account_chooser);
	account = empathy_account_chooser_get_account (account_chooser);

	if (!G_STR_EMPTY (server)) {
		room_name = g_strconcat (room, "@", server, NULL);
	} else {
		room_name = g_strdup (room);
	}

	empathy_debug (DEBUG_DOMAIN, "Requesting channel for '%s'", room_name);

	mc = empathy_mission_control_new ();
	mission_control_request_channel_with_string_handle (mc,
							    account,
							    TP_IFACE_CHANNEL_TYPE_TEXT,
							    room_name,
							    TP_HANDLE_TYPE_ROOM,
							    NULL, NULL);	
	g_free (room_name);
	g_object_unref (mc);
}

static void
new_chatroom_dialog_entry_changed_cb (GtkWidget                *entry,
				      EmpathyNewChatroomDialog *dialog)
{
	if (entry == dialog->entry_room) {
		const gchar *room;

		room = gtk_entry_get_text (GTK_ENTRY (dialog->entry_room));
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (dialog->filter));
		gtk_widget_set_sensitive (dialog->button_join, !G_STR_EMPTY (room));
	}
}

static void
new_chatroom_dialog_browse_start (EmpathyNewChatroomDialog *dialog)
{
	if (0) {
		new_chatroom_dialog_model_clear (dialog);
		new_chatroom_dialog_model_add (dialog, NULL);
	}
}

static void
new_chatroom_dialog_browse_stop (EmpathyNewChatroomDialog *dialog)
{
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

