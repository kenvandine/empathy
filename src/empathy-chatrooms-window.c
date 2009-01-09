/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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
 *          Martyn Russell <martyn@imendio.com>
 *          Mikael Hallendal <micke@imendio.com>
 */

#include <config.h>

#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <libempathy/empathy-chatroom-manager.h>
#include <libempathy/empathy-utils.h>

#include <libempathy-gtk/empathy-account-chooser.h>
#include <libempathy-gtk/empathy-ui-utils.h>

#include "empathy-chatrooms-window.h"
#include "empathy-new-chatroom-dialog.h"

typedef struct {
	EmpathyChatroomManager *manager;

	GtkWidget             *window;
	GtkWidget             *hbox_account;
	GtkWidget             *label_account;
	GtkWidget             *account_chooser;
	GtkWidget             *treeview;
	GtkWidget             *button_remove;
	GtkWidget             *button_edit;
	GtkWidget             *button_close;

	gint                   room_column;
} EmpathyChatroomsWindow;

static void             chatrooms_window_destroy_cb                      (GtkWidget             *widget,
									  EmpathyChatroomsWindow *window);
static void             chatrooms_window_model_setup                     (EmpathyChatroomsWindow *window);
static void             chatrooms_window_model_add_columns               (EmpathyChatroomsWindow *window);
static void             chatrooms_window_model_refresh_data              (EmpathyChatroomsWindow *window,
									  gboolean               first_time);
static void             chatrooms_window_model_add                       (EmpathyChatroomsWindow *window,
									  EmpathyChatroom        *chatroom,
									  gboolean               set_active);
static void             chatrooms_window_model_cell_auto_connect_toggled (GtkCellRendererToggle  *cell,
									  gchar                  *path_string,
									  EmpathyChatroomsWindow  *window);
static EmpathyChatroom * chatrooms_window_model_get_selected              (EmpathyChatroomsWindow *window);
static void             chatrooms_window_model_action_selected           (EmpathyChatroomsWindow *window);
static void             chatrooms_window_row_activated_cb                (GtkTreeView           *tree_view,
									  GtkTreePath           *path,
									  GtkTreeViewColumn     *column,
									  EmpathyChatroomsWindow *window);
static void             chatrooms_window_button_remove_clicked_cb        (GtkWidget             *widget,
									  EmpathyChatroomsWindow *window);
static void             chatrooms_window_button_edit_clicked_cb          (GtkWidget             *widget,
									  EmpathyChatroomsWindow *window);
static void             chatrooms_window_button_close_clicked_cb         (GtkWidget             *widget,
									  EmpathyChatroomsWindow *window);
static void             chatrooms_window_chatroom_added_cb               (EmpathyChatroomManager *manager,
									  EmpathyChatroom        *chatroom,
									  EmpathyChatroomsWindow *window);
static void             chatrooms_window_chatroom_removed_cb             (EmpathyChatroomManager *manager,
									  EmpathyChatroom        *chatroom,
									  EmpathyChatroomsWindow *window);
static gboolean         chatrooms_window_remove_chatroom_foreach         (GtkTreeModel          *model,
									  GtkTreePath           *path,
									  GtkTreeIter           *iter,
									  EmpathyChatroom        *chatroom);
static void             chatrooms_window_account_changed_cb              (GtkWidget             *combo_box,
									  EmpathyChatroomsWindow *window);

enum {
	COL_IMAGE,
	COL_NAME,
	COL_ROOM,
	COL_AUTO_CONNECT,
	COL_POINTER,
	COL_COUNT
};

void
empathy_chatrooms_window_show (GtkWindow *parent)
{
	static EmpathyChatroomsWindow *window = NULL;
	GladeXML                     *glade;
	gchar                        *filename;

	if (window) {
		gtk_window_present (GTK_WINDOW (window->window));
		return;
	}

	window = g_new0 (EmpathyChatroomsWindow, 1);

	filename = empathy_file_lookup ("empathy-chatrooms-window.glade", "src");
	glade = empathy_glade_get_file (filename,
				       "chatrooms_window",
				       NULL,
				       "chatrooms_window", &window->window,
				       "hbox_account", &window->hbox_account,
				       "label_account", &window->label_account,
				       "treeview", &window->treeview,
				       "button_edit", &window->button_edit,
				       "button_remove", &window->button_remove,
				       "button_close", &window->button_close,
				       NULL);
	g_free (filename);

	empathy_glade_connect (glade,
			      window,
			      "chatrooms_window", "destroy", chatrooms_window_destroy_cb,
			      "button_remove", "clicked", chatrooms_window_button_remove_clicked_cb,
			      "button_edit", "clicked", chatrooms_window_button_edit_clicked_cb,
			      "button_close", "clicked", chatrooms_window_button_close_clicked_cb,
			      NULL);

	g_object_unref (glade);

	g_object_add_weak_pointer (G_OBJECT (window->window), (gpointer) &window);

	/* Get the session and chat room manager */
	window->manager = empathy_chatroom_manager_dup_singleton (NULL);

	g_signal_connect (window->manager, "chatroom-added",
			  G_CALLBACK (chatrooms_window_chatroom_added_cb),
			  window);
	g_signal_connect (window->manager, "chatroom-removed",
			  G_CALLBACK (chatrooms_window_chatroom_removed_cb),
			  window);

	/* Account chooser for chat rooms */
	window->account_chooser = empathy_account_chooser_new ();
	empathy_account_chooser_set_filter (EMPATHY_ACCOUNT_CHOOSER (window->account_chooser),
					    empathy_account_chooser_filter_is_connected,
					    NULL);
	g_object_set (window->account_chooser, 
		      "has-all-option", TRUE,
		      NULL);
	empathy_account_chooser_set_account (EMPATHY_ACCOUNT_CHOOSER (window->account_chooser), NULL);

	gtk_box_pack_start (GTK_BOX (window->hbox_account),
			    window->account_chooser,
			    TRUE, TRUE, 0);

	g_signal_connect (window->account_chooser, "changed",
			  G_CALLBACK (chatrooms_window_account_changed_cb),
			  window);

	gtk_widget_show (window->account_chooser);

	/* Set up chatrooms */
	chatrooms_window_model_setup (window);

	/* Set focus */
	gtk_widget_grab_focus (window->treeview);

	/* Last touches */
	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (window->window),
					      GTK_WINDOW (parent));
	}

	gtk_widget_show (window->window);
}

static void
chatrooms_window_destroy_cb (GtkWidget             *widget,
			     EmpathyChatroomsWindow *window)
{
	g_signal_handlers_disconnect_by_func (window->manager,
					      chatrooms_window_chatroom_added_cb,
					      window);
	g_signal_handlers_disconnect_by_func (window->manager,
					      chatrooms_window_chatroom_removed_cb,
					      window);
	g_object_unref (window->manager);
	g_free (window);
}

static void
chatrooms_window_model_setup (EmpathyChatroomsWindow *window)
{
	GtkTreeView      *view;
	GtkListStore     *store;
	GtkTreeSelection *selection;

	/* View */
	view = GTK_TREE_VIEW (window->treeview);

	g_signal_connect (view, "row-activated",
			  G_CALLBACK (chatrooms_window_row_activated_cb),
			  window);

	/* Store */
	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_STRING,         /* Image */
				    G_TYPE_STRING,         /* Name */
				    G_TYPE_STRING,         /* Room */
				    G_TYPE_BOOLEAN,        /* Auto start */
				    EMPATHY_TYPE_CHATROOM); /* Chatroom */

	gtk_tree_view_set_model (view, GTK_TREE_MODEL (store));

	/* Selection */ 
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	/* Columns */
	chatrooms_window_model_add_columns (window);

	/* Add data */
	chatrooms_window_model_refresh_data (window, TRUE);

	/* Clean up */
	g_object_unref (store);
}

static void
chatrooms_window_model_add_columns (EmpathyChatroomsWindow *window)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;
	GtkTreeViewColumn *column;
	GtkCellRenderer   *cell;
	gint               count;

	view = GTK_TREE_VIEW (window->treeview);
	model = gtk_tree_view_get_model (view);

	gtk_tree_view_set_headers_visible (view, TRUE);
	gtk_tree_view_set_headers_clickable (view, TRUE);

	/* Name & Status */
	column = gtk_tree_view_column_new ();
	count = gtk_tree_view_append_column (view, column);

	gtk_tree_view_column_set_title (column, _("Name"));
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_sort_column_id (column, count - 1);

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_add_attribute (column, cell, "icon-name", COL_IMAGE);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell,
		      "xpad", 4,
		      "ypad", 1,
		      NULL);
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_add_attribute (column, cell, "text", COL_NAME);

	/* Room */
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Room"), cell, 
							   "text", COL_ROOM, 
							   NULL);
	count = gtk_tree_view_append_column (view, column);
	gtk_tree_view_column_set_sort_column_id (column, count - 1);
	window->room_column = count - 1;

	/* Chatroom auto connect */
	cell = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Auto-Connect"), cell,
							   "active", COL_AUTO_CONNECT,
							   NULL);
	count = gtk_tree_view_append_column (view, column);
	gtk_tree_view_column_set_sort_column_id (column, count - 1);

	g_signal_connect (cell, "toggled",
			  G_CALLBACK (chatrooms_window_model_cell_auto_connect_toggled),
			  window);

	/* Sort model */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model), 0, 
					      GTK_SORT_ASCENDING);
}

static void
chatrooms_window_model_refresh_data (EmpathyChatroomsWindow *window,
				     gboolean               first_time)
{
	GtkTreeView           *view;
	GtkTreeSelection      *selection;
	GtkTreeModel          *model;
	GtkListStore          *store;
	GtkTreeIter            iter;
	GtkTreeViewColumn     *column;
	EmpathyAccountChooser  *account_chooser;
	McAccount             *account;
	GList                 *chatrooms, *l;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);

	/* Look up chatrooms */
	account_chooser = EMPATHY_ACCOUNT_CHOOSER (window->account_chooser);
	account = empathy_account_chooser_get_account (account_chooser);

	chatrooms = empathy_chatroom_manager_get_chatrooms (window->manager, account);

	/* Sort out columns, we only show the server column for
	 * selected protocol types, such as Jabber. 
	 */
	if (account) {
		column = gtk_tree_view_get_column (view, window->room_column);
		gtk_tree_view_column_set_visible (column, TRUE);
	} else {
		column = gtk_tree_view_get_column (view, window->room_column);
		gtk_tree_view_column_set_visible (column, FALSE);
	}

	/* Clean out the store */
	gtk_list_store_clear (store);

	/* Populate with chatroom list. */
	for (l = chatrooms; l; l = l->next) {
		chatrooms_window_model_add (window, l->data,  FALSE);
	}

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		gtk_tree_selection_select_iter (selection, &iter);
	}

	if (account) {
		g_object_unref (account);
	}

	g_list_free (chatrooms);
}

static void
chatrooms_window_model_add (EmpathyChatroomsWindow *window,
			    EmpathyChatroom        *chatroom,
			    gboolean               set_active)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkListStore     *store;
	GtkTreeIter       iter;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_NAME, empathy_chatroom_get_name (chatroom),
			    COL_ROOM, empathy_chatroom_get_room (chatroom),
			    COL_AUTO_CONNECT, empathy_chatroom_get_auto_connect (chatroom),
			    COL_POINTER, chatroom,
			    -1);

	if (set_active) {
		gtk_tree_selection_select_iter (selection, &iter);
	}
}

static void
chatrooms_window_model_cell_auto_connect_toggled (GtkCellRendererToggle  *cell,
						  gchar                  *path_string,
						  EmpathyChatroomsWindow  *window)
{
	EmpathyChatroom *chatroom;
	gboolean        enabled;
	GtkTreeView    *view;
	GtkTreeModel   *model;
	GtkListStore   *store;
	GtkTreePath    *path;
	GtkTreeIter     iter;

	view = GTK_TREE_VIEW (window->treeview);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);

	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    COL_AUTO_CONNECT, &enabled,
			    COL_POINTER, &chatroom,
			    -1);

	enabled = !enabled;

	empathy_chatroom_set_auto_connect (chatroom, enabled);

	gtk_list_store_set (store, &iter, COL_AUTO_CONNECT, enabled, -1);
	gtk_tree_path_free (path);
	g_object_unref (chatroom);
}

static EmpathyChatroom *
chatrooms_window_model_get_selected (EmpathyChatroomsWindow *window)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	EmpathyChatroom   *chatroom = NULL;

	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, COL_POINTER, &chatroom, -1);
	}

	return chatroom;
}

static void
chatrooms_window_model_action_selected (EmpathyChatroomsWindow *window)
{
	EmpathyChatroom *chatroom;
	GtkTreeView    *view;
	GtkTreeModel   *model;

	view = GTK_TREE_VIEW (window->treeview);
	model = gtk_tree_view_get_model (view);

	chatroom = chatrooms_window_model_get_selected (window);
	if (!chatroom) {
		return;
	}

	//empathy_edit_chatroom_dialog_show (GTK_WINDOW (window->window), chatroom);

	g_object_unref (chatroom);
}

static void
chatrooms_window_row_activated_cb (GtkTreeView           *tree_view,
				   GtkTreePath           *path,
				   GtkTreeViewColumn     *column,
				   EmpathyChatroomsWindow *window)
{
	if (GTK_WIDGET_IS_SENSITIVE (window->button_edit)) {
		chatrooms_window_model_action_selected (window);
	}
}

static void
chatrooms_window_button_remove_clicked_cb (GtkWidget             *widget,
					   EmpathyChatroomsWindow *window)
{
	EmpathyChatroom        *chatroom;
	GtkTreeView           *view;
	GtkTreeModel          *model;
	GtkTreeSelection      *selection;
	GtkTreeIter            iter;

	/* Remove from treeview */
	view = GTK_TREE_VIEW (window->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
	}

	gtk_tree_model_get (model, &iter, COL_POINTER, &chatroom, -1);
	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	/* Remove from config */
	empathy_chatroom_manager_remove (window->manager, chatroom);

	g_object_unref (chatroom);
}

static void
chatrooms_window_button_edit_clicked_cb (GtkWidget             *widget,
					 EmpathyChatroomsWindow *window)
{
	EmpathyChatroom *chatroom;

	chatroom = chatrooms_window_model_get_selected (window);
	if (!chatroom) {
		return;
	}

	//empathy_edit_chatroom_dialog_show (GTK_WINDOW (window->window), chatroom);

	g_object_unref (chatroom);
}

static void
chatrooms_window_button_close_clicked_cb (GtkWidget             *widget,
					  EmpathyChatroomsWindow *window)
{
	gtk_widget_destroy (window->window);
}

static void
chatrooms_window_chatroom_added_cb (EmpathyChatroomManager *manager,
				    EmpathyChatroom        *chatroom,
				    EmpathyChatroomsWindow *window)
{
	EmpathyAccountChooser *account_chooser;
	McAccount            *account;

	account_chooser = EMPATHY_ACCOUNT_CHOOSER (window->account_chooser);
	account = empathy_account_chooser_get_account (account_chooser);

	if (!account) {
		chatrooms_window_model_add (window, chatroom, FALSE);
	} else {
		if (empathy_account_equal (account, empathy_chatroom_get_account (chatroom))) {
			chatrooms_window_model_add (window, chatroom, FALSE);
		}

		g_object_unref (account);
	}
}

static void
chatrooms_window_chatroom_removed_cb (EmpathyChatroomManager *manager,
				      EmpathyChatroom        *chatroom,
				      EmpathyChatroomsWindow *window)
{
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (window->treeview));

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) chatrooms_window_remove_chatroom_foreach,
				chatroom);
}

static gboolean
chatrooms_window_remove_chatroom_foreach (GtkTreeModel   *model,
					  GtkTreePath    *path,
					  GtkTreeIter    *iter,
					  EmpathyChatroom *chatroom)
{
	EmpathyChatroom *this_chatroom;

	gtk_tree_model_get (model, iter, COL_POINTER, &this_chatroom, -1);

	if (empathy_chatroom_equal (chatroom, this_chatroom)) {
		gtk_list_store_remove (GTK_LIST_STORE (model), iter);
		g_object_unref (this_chatroom);
		return TRUE;
	}

	g_object_unref (this_chatroom);

	return FALSE;
}

static void
chatrooms_window_account_changed_cb (GtkWidget             *combo_box,
				     EmpathyChatroomsWindow *window)
{
	chatrooms_window_model_refresh_data (window, FALSE);
}

