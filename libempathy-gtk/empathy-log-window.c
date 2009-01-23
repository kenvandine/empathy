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

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libempathy/empathy-log-manager.h>
#include <libempathy/empathy-chatroom-manager.h>
#include <libempathy/empathy-chatroom.h>
#include <libempathy/empathy-message.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-time.h>

#include "empathy-log-window.h"
#include "empathy-account-chooser.h"
#include "empathy-chat-view.h"
#include "empathy-theme-manager.h"
#include "empathy-ui-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

typedef struct {
	GtkWidget         *window;

	GtkWidget         *notebook;

	GtkWidget         *entry_find;
	GtkWidget         *button_find;
	GtkWidget         *treeview_find;
	GtkWidget         *scrolledwindow_find;
	EmpathyChatView    *chatview_find;
	GtkWidget         *button_previous;
	GtkWidget         *button_next;

	GtkWidget         *vbox_chats;
	GtkWidget         *account_chooser_chats;
	GtkWidget         *entry_chats;
	GtkWidget         *calendar_chats;
	GtkWidget         *treeview_chats;
	GtkWidget         *scrolledwindow_chats;
	EmpathyChatView    *chatview_chats;

	gchar             *last_find;

	EmpathyLogManager *log_manager;
} EmpathyLogWindow;

static void     log_window_destroy_cb                      (GtkWidget        *widget,
							    EmpathyLogWindow *window);
static void     log_window_entry_find_changed_cb           (GtkWidget        *entry,
							    EmpathyLogWindow *window);
static void     log_window_find_changed_cb                 (GtkTreeSelection *selection,
							    EmpathyLogWindow *window);
static void     log_window_find_populate                   (EmpathyLogWindow *window,
							    const gchar      *search_criteria);
static void     log_window_find_setup                      (EmpathyLogWindow *window);
static void     log_window_button_find_clicked_cb          (GtkWidget        *widget,
							    EmpathyLogWindow *window);
static void     log_window_button_next_clicked_cb          (GtkWidget        *widget,
							    EmpathyLogWindow *window);
static void     log_window_button_previous_clicked_cb      (GtkWidget        *widget,
							    EmpathyLogWindow *window);
static void     log_window_chats_changed_cb                (GtkTreeSelection *selection,
							    EmpathyLogWindow *window);
static void     log_window_chats_populate                  (EmpathyLogWindow *window);
static void     log_window_chats_setup                     (EmpathyLogWindow *window);
static void     log_window_chats_accounts_changed_cb       (GtkWidget        *combobox,
							    EmpathyLogWindow *window);
static void     log_window_chats_set_selected              (EmpathyLogWindow *window,
							    McAccount        *account,
							    const gchar      *chat_id,
							    gboolean          is_chatroom);
static gboolean log_window_chats_get_selected              (EmpathyLogWindow *window,
							    McAccount       **account,
							    gchar           **chat_id,
							    gboolean         *is_chatroom);
static void     log_window_chats_get_messages              (EmpathyLogWindow *window,
							    const gchar      *date_to_show);
static void     log_window_calendar_chats_day_selected_cb  (GtkWidget        *calendar,
							    EmpathyLogWindow *window);
static void     log_window_calendar_chats_month_changed_cb (GtkWidget        *calendar,
							    EmpathyLogWindow *window);
static void     log_window_entry_chats_changed_cb          (GtkWidget        *entry,
							    EmpathyLogWindow *window);
static void     log_window_entry_chats_activate_cb         (GtkWidget        *entry,
							    EmpathyLogWindow *window);

enum {
	COL_FIND_ACCOUNT_ICON,
	COL_FIND_ACCOUNT_NAME,
	COL_FIND_ACCOUNT,
	COL_FIND_CHAT_NAME,
	COL_FIND_CHAT_ID,
	COL_FIND_IS_CHATROOM,
	COL_FIND_DATE,
	COL_FIND_DATE_READABLE,
	COL_FIND_COUNT
};

enum {
	COL_CHAT_ICON,
	COL_CHAT_NAME,
	COL_CHAT_ACCOUNT,
	COL_CHAT_ID,
	COL_CHAT_IS_CHATROOM,
	COL_CHAT_COUNT
};

GtkWidget *
empathy_log_window_show (McAccount   *account,
			const gchar *chat_id,
			gboolean     is_chatroom,
			GtkWindow   *parent)
{
	static EmpathyLogWindow *window = NULL;
	EmpathyAccountChooser   *account_chooser;
	GList                  *accounts;
	gint                    account_num;
	GladeXML               *glade;
	gchar                  *filename;

	if (window) {
		gtk_window_present (GTK_WINDOW (window->window));

		if (account && chat_id) {
			gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), 1);
			log_window_chats_set_selected (window, account,
						       chat_id, is_chatroom);
		}

		return window->window;
	}

	window = g_new0 (EmpathyLogWindow, 1);
	window->log_manager = empathy_log_manager_dup_singleton ();

	filename = empathy_file_lookup ("empathy-log-window.glade",
					"libempathy-gtk");
	glade = empathy_glade_get_file (filename,
				       "log_window",
				       NULL,
				       "log_window", &window->window,
				       "notebook", &window->notebook,
				       "entry_find", &window->entry_find,
				       "button_find", &window->button_find,
				       "treeview_find", &window->treeview_find,
				       "scrolledwindow_find", &window->scrolledwindow_find,
				       "button_previous", &window->button_previous,
				       "button_next", &window->button_next,
				       "entry_chats", &window->entry_chats,
				       "calendar_chats", &window->calendar_chats,
				       "vbox_chats", &window->vbox_chats,
				       "treeview_chats", &window->treeview_chats,
				       "scrolledwindow_chats", &window->scrolledwindow_chats,
				       NULL);
	g_free (filename);

	empathy_glade_connect (glade,
			      window,
			      "log_window", "destroy", log_window_destroy_cb,
			      "entry_find", "changed", log_window_entry_find_changed_cb,
			      "button_previous", "clicked", log_window_button_previous_clicked_cb,
			      "button_next", "clicked", log_window_button_next_clicked_cb,
			      "button_find", "clicked", log_window_button_find_clicked_cb,
			      "entry_chats", "changed", log_window_entry_chats_changed_cb,
			      "entry_chats", "activate", log_window_entry_chats_activate_cb,
			      NULL);

	g_object_unref (glade);

	g_object_add_weak_pointer (G_OBJECT (window->window),
				   (gpointer) &window);

	/* We set this up here so we can block it when needed. */
	g_signal_connect (window->calendar_chats, "day-selected",
			  G_CALLBACK (log_window_calendar_chats_day_selected_cb),
			  window);
	g_signal_connect (window->calendar_chats, "month-changed",
			  G_CALLBACK (log_window_calendar_chats_month_changed_cb),
			  window);

	/* Configure Search EmpathyChatView */
	window->chatview_find = empathy_theme_manager_create_view (empathy_theme_manager_get ());
	gtk_container_add (GTK_CONTAINER (window->scrolledwindow_find),
			   GTK_WIDGET (window->chatview_find));
	gtk_widget_show (GTK_WIDGET (window->chatview_find));

	/* Configure Contacts EmpathyChatView */
	window->chatview_chats = empathy_theme_manager_create_view (empathy_theme_manager_get ());
	gtk_container_add (GTK_CONTAINER (window->scrolledwindow_chats),
			   GTK_WIDGET (window->chatview_chats));
	gtk_widget_show (GTK_WIDGET (window->chatview_chats));

	/* Account chooser for chats */
	window->account_chooser_chats = empathy_account_chooser_new ();
	account_chooser = EMPATHY_ACCOUNT_CHOOSER (window->account_chooser_chats);

	gtk_box_pack_start (GTK_BOX (window->vbox_chats),
			    window->account_chooser_chats,
			    FALSE, TRUE, 0);

	g_signal_connect (window->account_chooser_chats, "changed",
			  G_CALLBACK (log_window_chats_accounts_changed_cb),
			  window);

	/* Populate */
	accounts = mc_accounts_list ();
	account_num = g_list_length (accounts);
	mc_accounts_list_free (accounts);

	if (account_num > 1) {
		gtk_widget_show (window->vbox_chats);
		gtk_widget_show (window->account_chooser_chats);
	} else {
		gtk_widget_hide (window->vbox_chats);
		gtk_widget_hide (window->account_chooser_chats);
	}

	/* Search List */
	log_window_find_setup (window);

	/* Contacts */
	log_window_chats_setup (window);
	log_window_chats_populate (window);

	/* Select chat */
	if (account && chat_id) {
		gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), 1);
		log_window_chats_set_selected (window, account,
					       chat_id, is_chatroom);
	}

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (window->window),
					      GTK_WINDOW (parent));
	}

	gtk_widget_show (window->window);

	return window->window;
}

static void
log_window_destroy_cb (GtkWidget       *widget,
		       EmpathyLogWindow *window)
{
	g_free (window->last_find);
	g_object_unref (window->log_manager);

	g_free (window);
}

/*
 * Search code.
 */
static void
log_window_entry_find_changed_cb (GtkWidget       *entry,
				  EmpathyLogWindow *window)
{
	const gchar *str;
	gboolean     is_sensitive = TRUE;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_find));

	is_sensitive &= !EMP_STR_EMPTY (str);
	is_sensitive &= 
		!window->last_find || 
		(window->last_find && strcmp (window->last_find, str) != 0);

	gtk_widget_set_sensitive (window->button_find, is_sensitive);
}

static void
log_window_find_changed_cb (GtkTreeSelection *selection,
			    EmpathyLogWindow  *window)
{
	GtkTreeView   *view;
	GtkTreeModel  *model;
	GtkTreeIter    iter;
	McAccount     *account;
	gchar         *chat_id;
	gboolean       is_chatroom;
	gchar         *date;
	EmpathyMessage *message;
	GList         *messages;
	GList         *l;
	gboolean       can_do_previous;
	gboolean       can_do_next;

	/* Get selected information */
	view = GTK_TREE_VIEW (window->treeview_find);
	model = gtk_tree_view_get_model (view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_widget_set_sensitive (window->button_previous, FALSE);
		gtk_widget_set_sensitive (window->button_next, FALSE);

		empathy_chat_view_clear (window->chatview_find);
	
		return;
	}

	gtk_widget_set_sensitive (window->button_previous, TRUE);
	gtk_widget_set_sensitive (window->button_next, TRUE);

	gtk_tree_model_get (model, &iter,
			    COL_FIND_ACCOUNT, &account,
			    COL_FIND_CHAT_ID, &chat_id,
			    COL_FIND_IS_CHATROOM, &is_chatroom,
			    COL_FIND_DATE, &date,
			    -1);

	/* Clear all current messages shown in the textview */
	empathy_chat_view_clear (window->chatview_find);

	/* Turn off scrolling temporarily */
	empathy_chat_view_scroll (window->chatview_find, FALSE);

	/* Get messages */
	messages = empathy_log_manager_get_messages_for_date (window->log_manager,
							      account,
							      chat_id,
							      is_chatroom,
							      date);
	g_object_unref (account);
	g_free (date);
	g_free (chat_id);

	for (l = messages; l; l = l->next) {
		message = l->data;
		empathy_chat_view_append_message (window->chatview_find, message);
		g_object_unref (message);
	}
	g_list_free (messages);

	/* Scroll to the most recent messages */
	empathy_chat_view_scroll (window->chatview_find, TRUE);

	/* Highlight and find messages */
	empathy_chat_view_highlight (window->chatview_find,
				    window->last_find);
	empathy_chat_view_find_next (window->chatview_find,
				    window->last_find,
				    TRUE);
	empathy_chat_view_find_abilities (window->chatview_find,
					 window->last_find,
					 &can_do_previous,
					 &can_do_next);
	gtk_widget_set_sensitive (window->button_previous, can_do_previous);
	gtk_widget_set_sensitive (window->button_next, can_do_next);
	gtk_widget_set_sensitive (window->button_find, FALSE);
}

static void
log_window_find_populate (EmpathyLogWindow *window,
			  const gchar     *search_criteria)
{
	GList              *hits, *l;

	GtkTreeView        *view;
	GtkTreeModel       *model;
	GtkTreeSelection   *selection;
	GtkListStore       *store;
	GtkTreeIter         iter;

	view = GTK_TREE_VIEW (window->treeview_find);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);
	store = GTK_LIST_STORE (model);

	empathy_chat_view_clear (window->chatview_find);

	gtk_list_store_clear (store);

	if (EMP_STR_EMPTY (search_criteria)) {
		/* Just clear the search. */
		return;
	}

	hits = empathy_log_manager_search_new (window->log_manager, search_criteria);

	for (l = hits; l; l = l->next) {
		EmpathyLogSearchHit *hit;
		const gchar         *account_name;
		const gchar         *account_icon;
		gchar               *date_readable;

		hit = l->data;

		/* Protect against invalid data (corrupt or old log files. */
		if (!hit->account || !hit->chat_id) {
			continue;
		}

		date_readable = empathy_log_manager_get_date_readable (hit->date);
		account_name = mc_account_get_display_name (hit->account);
		account_icon = empathy_icon_name_from_account (hit->account);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_FIND_ACCOUNT_ICON, account_icon,
				    COL_FIND_ACCOUNT_NAME, account_name,
				    COL_FIND_ACCOUNT, hit->account,
				    COL_FIND_CHAT_NAME, hit->chat_id, /* FIXME */
				    COL_FIND_CHAT_ID, hit->chat_id,
				    COL_FIND_IS_CHATROOM, hit->is_chatroom,
				    COL_FIND_DATE, hit->date,
				    COL_FIND_DATE_READABLE, date_readable,
				    -1);

		g_free (date_readable);

		/* FIXME: Update COL_FIND_CHAT_NAME */
		if (hit->is_chatroom) {
		} else {
		}
	}

	if (hits) {
		empathy_log_manager_search_free (hits);
	}
}

static void
log_window_find_setup (EmpathyLogWindow *window)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;
	GtkTreeSelection  *selection;
	GtkTreeSortable   *sortable;
	GtkTreeViewColumn *column;
	GtkListStore      *store;
	GtkCellRenderer   *cell;
	gint               offset;

	view = GTK_TREE_VIEW (window->treeview_find);
	selection = gtk_tree_view_get_selection (view);

	/* New store */
	store = gtk_list_store_new (COL_FIND_COUNT,
				    G_TYPE_STRING,          /* account icon name */
				    G_TYPE_STRING,          /* account name */
				    MC_TYPE_ACCOUNT,        /* account */
				    G_TYPE_STRING,          /* chat name */
				    G_TYPE_STRING,          /* chat id */
				    G_TYPE_BOOLEAN,         /* is chatroom */
				    G_TYPE_STRING,          /* date */
				    G_TYPE_STRING);         /* date_readable */

	model = GTK_TREE_MODEL (store);
	sortable = GTK_TREE_SORTABLE (store);

	gtk_tree_view_set_model (view, model);

	/* New column */
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_add_attribute (column, cell,
					    "icon-name",
					    COL_FIND_ACCOUNT_ICON);

	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_add_attribute (column, cell,
					    "text",
					    COL_FIND_ACCOUNT_NAME);

	gtk_tree_view_column_set_title (column, _("Account"));
	gtk_tree_view_append_column (view, column);

	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_clickable (column, TRUE);

	cell = gtk_cell_renderer_text_new ();
	offset = gtk_tree_view_insert_column_with_attributes (view, -1, _("Conversation"),
							      cell, "text", COL_FIND_CHAT_NAME,
							      NULL);

	column = gtk_tree_view_get_column (view, offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_FIND_CHAT_NAME);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_clickable (column, TRUE);

	cell = gtk_cell_renderer_text_new ();
	offset = gtk_tree_view_insert_column_with_attributes (view, -1, _("Date"),
							      cell, "text", COL_FIND_DATE_READABLE,
							      NULL);

	column = gtk_tree_view_get_column (view, offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_FIND_DATE);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_clickable (column, TRUE);

	/* Set up treeview properties */
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_tree_sortable_set_sort_column_id (sortable,
					      COL_FIND_DATE,
					      GTK_SORT_ASCENDING);

	/* Set up signals */
	g_signal_connect (selection, "changed",
			  G_CALLBACK (log_window_find_changed_cb),
			  window);

	g_object_unref (store);
}

static void
log_window_button_find_clicked_cb (GtkWidget       *widget,
				   EmpathyLogWindow *window)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_find));

	/* Don't find the same crap again */
	if (window->last_find && strcmp (window->last_find, str) == 0) {
		return;
	}

	g_free (window->last_find);
	window->last_find = g_strdup (str);

	log_window_find_populate (window, str);
}

static void
log_window_button_next_clicked_cb (GtkWidget       *widget,
				   EmpathyLogWindow *window)
{
	if (window->last_find) {
		gboolean can_do_previous;
		gboolean can_do_next;

		empathy_chat_view_find_next (window->chatview_find,
					    window->last_find,
					    FALSE);
		empathy_chat_view_find_abilities (window->chatview_find,
						 window->last_find,
						 &can_do_previous,
						 &can_do_next);
		gtk_widget_set_sensitive (window->button_previous, can_do_previous);
		gtk_widget_set_sensitive (window->button_next, can_do_next);
	}
}

static void
log_window_button_previous_clicked_cb (GtkWidget       *widget,
				       EmpathyLogWindow *window)
{
	if (window->last_find) {
		gboolean can_do_previous;
		gboolean can_do_next;

		empathy_chat_view_find_previous (window->chatview_find,
						window->last_find,
						FALSE);
		empathy_chat_view_find_abilities (window->chatview_find,
						 window->last_find,
						 &can_do_previous,
						 &can_do_next);
		gtk_widget_set_sensitive (window->button_previous, can_do_previous);
		gtk_widget_set_sensitive (window->button_next, can_do_next);
	}
}

/*
 * Chats Code
 */

static void
log_window_chats_changed_cb (GtkTreeSelection *selection,
			     EmpathyLogWindow  *window)
{
	/* Use last date by default */
	gtk_calendar_clear_marks (GTK_CALENDAR (window->calendar_chats));

	log_window_chats_get_messages (window, NULL);
}

static void
log_window_chats_populate (EmpathyLogWindow *window)
{
	EmpathyAccountChooser *account_chooser;
	McAccount            *account;
	GList                *chats, *l;

	GtkTreeView          *view;
	GtkTreeModel         *model;
	GtkTreeSelection     *selection;
	GtkListStore         *store;
	GtkTreeIter           iter;

	account_chooser = EMPATHY_ACCOUNT_CHOOSER (window->account_chooser_chats);
	account = empathy_account_chooser_get_account (account_chooser);

	view = GTK_TREE_VIEW (window->treeview_chats);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);
	store = GTK_LIST_STORE (model);

	if (account == NULL) {
		gtk_list_store_clear (store);
		return;
	}

	/* Block signals to stop the logs being retrieved prematurely */
	g_signal_handlers_block_by_func (selection,
					 log_window_chats_changed_cb,
					 window);

	gtk_list_store_clear (store);

	chats = empathy_log_manager_get_chats (window->log_manager, account);
	for (l = chats; l; l = l->next) {
		EmpathyLogSearchHit *hit;

		hit = l->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_CHAT_ICON, "empathy-available", /* FIXME */
				    COL_CHAT_NAME, hit->chat_id,				
				    COL_CHAT_ACCOUNT, account,
				    COL_CHAT_ID, hit->chat_id,
				    COL_CHAT_IS_CHATROOM, hit->is_chatroom,
				    -1);

		/* FIXME: Update COL_CHAT_ICON/NAME */
		if (hit->is_chatroom) {
		} else {
		}
	}
	empathy_log_manager_search_free (chats);

	/* Unblock signals */
	g_signal_handlers_unblock_by_func (selection,
					   log_window_chats_changed_cb,
					   window);


	g_object_unref (account);
}

static void
log_window_chats_setup (EmpathyLogWindow *window)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;
	GtkTreeSelection  *selection;
	GtkTreeSortable   *sortable;
	GtkTreeViewColumn *column;
	GtkListStore      *store;
	GtkCellRenderer   *cell;

	view = GTK_TREE_VIEW (window->treeview_chats);
	selection = gtk_tree_view_get_selection (view);

	/* new store */
	store = gtk_list_store_new (COL_CHAT_COUNT,
				    G_TYPE_STRING,    /* icon */
				    G_TYPE_STRING,    /* name */
				    MC_TYPE_ACCOUNT,  /* account */
				    G_TYPE_STRING,    /* id */
				    G_TYPE_BOOLEAN);  /* is chatroom */

	model = GTK_TREE_MODEL (store);
	sortable = GTK_TREE_SORTABLE (store);

	gtk_tree_view_set_model (view, model);

	/* new column */
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_add_attribute (column, cell,
					    "icon-name",
					    COL_CHAT_ICON);

	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_add_attribute (column, cell,
					    "text",
					    COL_CHAT_NAME);

	gtk_tree_view_append_column (view, column);

	/* set up treeview properties */
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_tree_sortable_set_sort_column_id (sortable,
					      COL_CHAT_NAME,
					      GTK_SORT_ASCENDING);

	/* set up signals */
	g_signal_connect (selection, "changed",
			  G_CALLBACK (log_window_chats_changed_cb),
			  window);

	g_object_unref (store);
}

static void
log_window_chats_accounts_changed_cb (GtkWidget       *combobox,
				      EmpathyLogWindow *window)
{
	/* Clear all current messages shown in the textview */
	empathy_chat_view_clear (window->chatview_chats);

	log_window_chats_populate (window);
}

static void
log_window_chats_set_selected  (EmpathyLogWindow *window,
				McAccount       *account,
				const gchar     *chat_id,
				gboolean         is_chatroom)
{
	EmpathyAccountChooser *account_chooser;
	GtkTreeView          *view;
	GtkTreeModel         *model;
	GtkTreeSelection     *selection;
	GtkTreeIter           iter;
	GtkTreePath          *path;
	gboolean              ok;

	account_chooser = EMPATHY_ACCOUNT_CHOOSER (window->account_chooser_chats);
	empathy_account_chooser_set_account (account_chooser, account);

	view = GTK_TREE_VIEW (window->treeview_chats);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_model_get_iter_first (model, &iter)) {
		return;
	}

	for (ok = TRUE; ok; ok = gtk_tree_model_iter_next (model, &iter)) {
		McAccount *this_account;
		gchar     *this_chat_id;
		gboolean   this_is_chatroom;

		gtk_tree_model_get (model, &iter,
				    COL_CHAT_ACCOUNT, &this_account,
				    COL_CHAT_ID, &this_chat_id,
				    COL_CHAT_IS_CHATROOM, &this_is_chatroom,
				    -1);

		if (empathy_account_equal (this_account, account) &&
		    strcmp (this_chat_id, chat_id) == 0 &&
		    this_is_chatroom == is_chatroom) {
			gtk_tree_selection_select_iter (selection, &iter);
			path = gtk_tree_model_get_path (model, &iter);
			gtk_tree_view_scroll_to_cell (view, path, NULL, TRUE, 0.5, 0.0);
			gtk_tree_path_free (path);
			g_object_unref (this_account);
			g_free (this_chat_id);
			break;
		}

		g_object_unref (this_account);
		g_free (this_chat_id);
	}
}

static gboolean
log_window_chats_get_selected (EmpathyLogWindow  *window,
			       McAccount       **account,
			       gchar           **chat_id,
			       gboolean         *is_chatroom)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	gchar            *id = NULL;
	McAccount        *acc = NULL;
	gboolean          room = FALSE;

	view = GTK_TREE_VIEW (window->treeview_chats);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return FALSE;
	}

	gtk_tree_model_get (model, &iter,
			    COL_CHAT_ACCOUNT, &acc,
			    COL_CHAT_ID, &id,
			    COL_CHAT_IS_CHATROOM, &room,
			    -1);

	if (chat_id) {
		*chat_id = id;
	} else {
		g_free (id);
	}
	if (account) {
		*account = acc;
	} else {
		g_object_unref (acc);
	}
	if (is_chatroom) {
		*is_chatroom = room;
	}

	return TRUE;
}

static void
log_window_chats_get_messages (EmpathyLogWindow *window,
			       const gchar     *date_to_show)
{
	McAccount     *account;
	gchar         *chat_id;
	gboolean       is_chatroom;
	EmpathyMessage *message;
	GList         *messages;
	GList         *dates = NULL;
	GList         *l;
	const gchar   *date;
	guint          year_selected;
	guint          year;
	guint          month;
	guint          month_selected;
	guint          day;

	if (!log_window_chats_get_selected (window, &account,
					    &chat_id, &is_chatroom)) {
		return;
	}

	g_signal_handlers_block_by_func (window->calendar_chats,
					 log_window_calendar_chats_day_selected_cb,
					 window);

	/* Either use the supplied date or get the last */
	date = date_to_show;
	if (!date) {
		gboolean day_selected = FALSE;

		/* Get a list of dates and show them on the calendar */
		dates = empathy_log_manager_get_dates (window->log_manager,
						       account, chat_id,
						       is_chatroom);

		for (l = dates; l; l = l->next) {
			const gchar *str;

			str = l->data;
			if (!str) {
				continue;
			}

			sscanf (str, "%4d%2d%2d", &year, &month, &day);
			gtk_calendar_get_date (GTK_CALENDAR (window->calendar_chats),
					       &year_selected,
					       &month_selected,
					       NULL);

			month_selected++;

			if (!l->next) {
				date = str;
			}

			if (year != year_selected || month != month_selected) {
				continue;
			}


			DEBUG ("Marking date:'%s'", str);
			gtk_calendar_mark_day (GTK_CALENDAR (window->calendar_chats), day);

			if (l->next) {
				continue;
			}

			day_selected = TRUE;

			gtk_calendar_select_day (GTK_CALENDAR (window->calendar_chats), day);
		}

		if (!day_selected) {
			/* Unselect the day in the calendar */
			gtk_calendar_select_day (GTK_CALENDAR (window->calendar_chats), 0);
		}
	} else {
		sscanf (date, "%4d%2d%2d", &year, &month, &day);
		gtk_calendar_get_date (GTK_CALENDAR (window->calendar_chats),
				       &year_selected,
				       &month_selected,
				       NULL);

		month_selected++;

		if (year != year_selected && month != month_selected) {
			day = 0;
		}

		gtk_calendar_select_day (GTK_CALENDAR (window->calendar_chats), day);
	}

	g_signal_handlers_unblock_by_func (window->calendar_chats,
					   log_window_calendar_chats_day_selected_cb,
					   window);

	if (!date) {
		goto OUT;
	}

	/* Clear all current messages shown in the textview */
	empathy_chat_view_clear (window->chatview_chats);

	/* Turn off scrolling temporarily */
	empathy_chat_view_scroll (window->chatview_find, FALSE);

	/* Get messages */
	messages = empathy_log_manager_get_messages_for_date (window->log_manager,
							      account, chat_id,
							      is_chatroom,
							      date);

	for (l = messages; l; l = l->next) {
		message = l->data;

		empathy_chat_view_append_message (window->chatview_chats,
						 message);
		g_object_unref (message);
	}
	g_list_free (messages);

	/* Turn back on scrolling */
	empathy_chat_view_scroll (window->chatview_find, TRUE);

	/* Give the search entry main focus */
	gtk_widget_grab_focus (window->entry_chats);

OUT:
	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);
	g_object_unref (account);
	g_free (chat_id);
}

static void
log_window_calendar_chats_day_selected_cb (GtkWidget       *calendar,
					   EmpathyLogWindow *window)
{
	guint  year;
	guint  month;
	guint  day;

	gchar *date;

	gtk_calendar_get_date (GTK_CALENDAR (calendar), &year, &month, &day);

	/* We need this hear because it appears that the months start from 0 */
	month++;

	date = g_strdup_printf ("%4.4d%2.2d%2.2d", year, month, day);

	DEBUG ("Currently selected date is:'%s'", date);

	log_window_chats_get_messages (window, date);

	g_free (date);
}

static void
log_window_calendar_chats_month_changed_cb (GtkWidget       *calendar,
					    EmpathyLogWindow *window)
{
	McAccount     *account;
	gchar         *chat_id;
	gboolean       is_chatroom;
	guint          year_selected;
	guint          month_selected;

	GList         *dates;
	GList         *l;

	gtk_calendar_clear_marks (GTK_CALENDAR (calendar));

	if (!log_window_chats_get_selected (window, &account,
					    &chat_id, &is_chatroom)) {
		DEBUG ("No chat selected to get dates for...");
		return;
	}

	g_object_get (calendar,
		      "month", &month_selected,
		      "year", &year_selected,
		      NULL);

	/* We need this hear because it appears that the months start from 0 */
	month_selected++;

	/* Get the log object for this contact */
	dates = empathy_log_manager_get_dates (window->log_manager, account,
					       chat_id, is_chatroom);
	g_object_unref (account);
	g_free (chat_id);

	for (l = dates; l; l = l->next) {
		const gchar *str;
		guint        year;
		guint        month;
		guint        day;

		str = l->data;
		if (!str) {
			continue;
		}

		sscanf (str, "%4d%2d%2d", &year, &month, &day);

		if (year == year_selected && month == month_selected) {
			DEBUG ("Marking date:'%s'", str);
			gtk_calendar_mark_day (GTK_CALENDAR (window->calendar_chats), day);
		}
	}

	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);

	DEBUG ("Currently showing month %d and year %d", month_selected,
		year_selected);
}

static void
log_window_entry_chats_changed_cb (GtkWidget       *entry,
				   EmpathyLogWindow *window)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_chats));
	empathy_chat_view_highlight (window->chatview_chats, str);

	if (str) {
		empathy_chat_view_find_next (window->chatview_chats,
					    str,
					    TRUE);
	}
}

static void
log_window_entry_chats_activate_cb (GtkWidget       *entry,
				    EmpathyLogWindow *window)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (window->entry_chats));

	if (str) {
		empathy_chat_view_find_next (window->chatview_chats,
					    str,
					    FALSE);
	}
}

