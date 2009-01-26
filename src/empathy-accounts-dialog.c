/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
#include <stdlib.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>

#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mc-profile.h>
#include <telepathy-glib/util.h>

#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-account-manager.h>
#include <libempathy-gtk/empathy-ui-utils.h>
#include <libempathy-gtk/empathy-profile-chooser.h>
#include <libempathy-gtk/empathy-account-widget.h>
#include <libempathy-gtk/empathy-account-widget-irc.h>
#include <libempathy-gtk/empathy-account-widget-sip.h>
#include <libempathy-gtk/empathy-conf.h>

#include "empathy-accounts-dialog.h"
#include "empathy-import-dialog.h"

#define DEBUG_FLAG EMPATHY_DEBUG_ACCOUNT
#include <libempathy/empathy-debug.h>

/* Flashing delay for icons (milliseconds). */
#define FLASH_TIMEOUT 500

typedef struct {
	GtkWidget        *window;

	GtkWidget        *alignment_settings;

	GtkWidget        *vbox_details;
	GtkWidget        *frame_no_profile;

	GtkWidget        *treeview;

	GtkWidget        *button_add;
	GtkWidget        *button_remove;
	GtkWidget        *button_import;

	GtkWidget        *frame_new_account;
	GtkWidget        *combobox_profile;
	GtkWidget        *hbox_type;
	GtkWidget        *button_create;
	GtkWidget        *button_back;
	GtkWidget        *checkbutton_register;

	GtkWidget        *image_type;
	GtkWidget        *label_name;
	GtkWidget        *label_type;
	GtkWidget        *settings_widget;

	gboolean          connecting_show;
	guint             connecting_id;

	EmpathyAccountManager *account_manager;
	MissionControl    *mc;
} EmpathyAccountsDialog;

enum {
	COL_ENABLED,
	COL_NAME,
	COL_STATUS,
	COL_ACCOUNT_POINTER,
	COL_COUNT
};

static void       accounts_dialog_update_account            (EmpathyAccountsDialog    *dialog,
							     McAccount                *account);
static void       accounts_dialog_model_setup               (EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_model_add_columns         (EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_name_editing_started_cb   (GtkCellRenderer          *renderer,
							     GtkCellEditable          *editable,
							     gchar                    *path,
							     EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_model_select_first        (EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_model_pixbuf_data_func    (GtkTreeViewColumn        *tree_column,
							     GtkCellRenderer          *cell,
							     GtkTreeModel             *model,
							     GtkTreeIter              *iter,
							     EmpathyAccountsDialog    *dialog);
static McAccount *accounts_dialog_model_get_selected        (EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_model_set_selected        (EmpathyAccountsDialog    *dialog,
							     McAccount                *account);
static gboolean   accounts_dialog_model_remove_selected     (EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_model_selection_changed   (GtkTreeSelection         *selection,
							     EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_add_or_update_account     (EmpathyAccountsDialog    *dialog,
							     McAccount                *account);
static void       accounts_dialog_account_added_cb          (EmpathyAccountManager    *manager,
							     McAccount                *account,
							     EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_account_removed_cb        (EmpathyAccountManager    *manager,
							     McAccount                *account,
							     EmpathyAccountsDialog    *dialog);
static gboolean   accounts_dialog_row_changed_foreach       (GtkTreeModel             *model,
							     GtkTreePath              *path,
							     GtkTreeIter              *iter,
							     gpointer                  user_data);
static gboolean   accounts_dialog_flash_connecting_cb       (EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_connection_changed_cb     (EmpathyAccountManager    *manager,
							     McAccount                *account,
							     TpConnectionStatusReason  reason,
							     TpConnectionStatus        current,
							     TpConnectionStatus        previous,
							     EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_button_create_clicked_cb  (GtkWidget                *button,
							     EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_button_back_clicked_cb    (GtkWidget                *button,
							     EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_button_add_clicked_cb     (GtkWidget                *button,
							     EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_button_help_clicked_cb    (GtkWidget                *button,
							     EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_button_remove_clicked_cb  (GtkWidget                *button,
							     EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_button_import_clicked_cb  (GtkWidget                *button,
							     EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_response_cb               (GtkWidget                *widget,
							     gint                      response,
							     EmpathyAccountsDialog    *dialog);
static void       accounts_dialog_destroy_cb                (GtkWidget                *widget,
							     EmpathyAccountsDialog    *dialog);

static void
accounts_dialog_update_name_label (EmpathyAccountsDialog *dialog,
				   McAccount             *account)
{
	gchar *text;

	text = g_markup_printf_escaped ("<big><b>%s</b></big>",
			mc_account_get_display_name (account));
	gtk_label_set_markup (GTK_LABEL (dialog->label_name), text);

	g_free (text);
}

static void
accounts_dialog_update_account (EmpathyAccountsDialog *dialog,
				McAccount            *account)
{
	McProfile   *profile;
	const gchar *config_ui;

	if (!account) {
		GtkTreeView  *view;
		GtkTreeModel *model;

		view = GTK_TREE_VIEW (dialog->treeview);
		model = gtk_tree_view_get_model (view);

		if (gtk_tree_model_iter_n_children (model, NULL) > 0) {
			/* We have configured accounts, select the first one */
			accounts_dialog_model_select_first (dialog);
			return;
		}
		if (empathy_profile_chooser_n_profiles (dialog->combobox_profile) > 0) {
			/* We have no account configured but we have some
			 * profiles instsalled. The user obviously wants to add
			 * an account. Click on the Add button for him. */
			accounts_dialog_button_add_clicked_cb (dialog->button_add,
							       dialog);
			return;
		}

		/* No account and no profile, warn the user */
		gtk_widget_hide (dialog->vbox_details);
		gtk_widget_hide (dialog->frame_new_account);
		gtk_widget_show (dialog->frame_no_profile);
		gtk_widget_set_sensitive (dialog->button_add, FALSE);
		gtk_widget_set_sensitive (dialog->button_remove, FALSE);
		return;
	}

	/* We have an account selected, destroy old settings and create a new
	 * one for the account selected */
	gtk_widget_hide (dialog->frame_new_account);
	gtk_widget_hide (dialog->frame_no_profile);
	gtk_widget_show (dialog->vbox_details);
	gtk_widget_set_sensitive (dialog->button_add, TRUE);
	gtk_widget_set_sensitive (dialog->button_remove, TRUE);

	if (dialog->settings_widget) {
		gtk_widget_destroy (dialog->settings_widget);
		dialog->settings_widget = NULL;
	}

	profile = mc_account_get_profile (account);
	config_ui = mc_profile_get_configuration_ui (profile);
	if (!tp_strdiff (config_ui, "jabber")) {
		dialog->settings_widget = 
			empathy_account_widget_jabber_new (account);
	} 
	else if (!tp_strdiff (config_ui, "msn")) {
		dialog ->settings_widget =
			empathy_account_widget_msn_new (account);
	}
	else if (!tp_strdiff (config_ui, "local-xmpp")) {
		dialog->settings_widget =
			empathy_account_widget_salut_new (account);
	}
	else if (!tp_strdiff (config_ui, "irc")) {
		dialog->settings_widget =
			empathy_account_widget_irc_new (account);
	}
	else if (!tp_strdiff(config_ui, "icq")) {
		dialog->settings_widget =
			empathy_account_widget_icq_new (account);
	}
	else if (!tp_strdiff(config_ui, "aim")) {
		dialog->settings_widget =
			empathy_account_widget_aim_new (account);
	}
	else if (!tp_strdiff (config_ui, "yahoo")) {
		dialog->settings_widget =
			empathy_account_widget_yahoo_new (account);
	}
	else if  (!tp_strdiff (config_ui, "sofiasip")) {
		dialog->settings_widget =
			empathy_account_widget_sip_new (account);
	}
	else if  (!tp_strdiff (config_ui, "groupwise")) {
		dialog->settings_widget =
			empathy_account_widget_groupwise_new (account);
	}
	else {
		dialog->settings_widget = 
			empathy_account_widget_generic_new (account);
	}

	gtk_container_add (GTK_CONTAINER (dialog->alignment_settings),
			   dialog->settings_widget);


	gtk_image_set_from_icon_name (GTK_IMAGE (dialog->image_type),
				      mc_profile_get_icon_name (profile),
				      GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_tooltip_text (dialog->image_type,
				     mc_profile_get_display_name (profile));

	accounts_dialog_update_name_label (dialog, account);

	g_object_unref (profile);
}

static void
accounts_dialog_model_setup (EmpathyAccountsDialog *dialog)
{
	GtkListStore     *store;
	GtkTreeSelection *selection;

	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_BOOLEAN,    /* enabled */
				    G_TYPE_STRING,     /* name */
				    G_TYPE_UINT,       /* status */
				    MC_TYPE_ACCOUNT);  /* account */

	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->treeview),
				 GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->treeview));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (accounts_dialog_model_selection_changed),
			  dialog);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      COL_NAME, GTK_SORT_ASCENDING);

	accounts_dialog_model_add_columns (dialog);

	g_object_unref (store);
}

static void
accounts_dialog_name_edited_cb (GtkCellRendererText   *renderer,
				gchar                 *path,
				gchar                 *new_text,
				EmpathyAccountsDialog *dialog)
{
	McAccount    *account;
	GtkTreeModel *model;
	GtkTreePath  *treepath;
	GtkTreeIter   iter;

	if (empathy_account_manager_get_connecting_accounts (dialog->account_manager) > 0) {
		dialog->connecting_id = g_timeout_add (FLASH_TIMEOUT,
						       (GSourceFunc) accounts_dialog_flash_connecting_cb,
						       dialog);
	}

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->treeview));
	treepath = gtk_tree_path_new_from_string (path);
	gtk_tree_model_get_iter (model, &iter, treepath);
	gtk_tree_model_get (model, &iter,
			    COL_ACCOUNT_POINTER, &account,
			    -1);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    COL_NAME, new_text,
			    -1);
	gtk_tree_path_free (treepath);

	mc_account_set_display_name (account, new_text);
	g_object_unref (account);
}

static void
accounts_dialog_enable_toggled_cb (GtkCellRendererToggle *cell_renderer,
				   gchar                 *path,
				   EmpathyAccountsDialog *dialog)
{
	McAccount    *account;
	GtkTreeModel *model;
	GtkTreePath  *treepath;
	GtkTreeIter   iter;
	gboolean      enabled;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->treeview));
	treepath = gtk_tree_path_new_from_string (path);
	gtk_tree_model_get_iter (model, &iter, treepath);
	gtk_tree_model_get (model, &iter,
			    COL_ACCOUNT_POINTER, &account,
			    -1);
	gtk_tree_path_free (treepath);

	enabled = mc_account_is_enabled (account);
	mc_account_set_enabled (account, !enabled);

	DEBUG ("%s account %s", enabled ? "Disabled" : "Enable",
		mc_account_get_display_name(account));

	g_object_unref (account);
}

static void
accounts_dialog_name_editing_started_cb (GtkCellRenderer       *renderer,
					 GtkCellEditable       *editable,
					 gchar                 *path,
					 EmpathyAccountsDialog *dialog)
{
	if (dialog->connecting_id) {
		g_source_remove (dialog->connecting_id);
	}
	DEBUG ("Editing account name started; stopping flashing");
}

static void
accounts_dialog_model_add_columns (EmpathyAccountsDialog *dialog)
{
	GtkTreeView       *view;
	GtkTreeViewColumn *column;
	GtkCellRenderer   *cell;

	view = GTK_TREE_VIEW (dialog->treeview);
	gtk_tree_view_set_headers_visible (view, TRUE);

	/* Enabled column */
	cell = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_insert_column_with_attributes (view, -1,
						     _("Enabled"),
						     cell,
						     "active", COL_ENABLED,
						     NULL);
	g_signal_connect (cell, "toggled",
			  G_CALLBACK (accounts_dialog_enable_toggled_cb),
			  dialog);
	
	/* Account column */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Accounts"));
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_append_column (view, column);

	/* Icon renderer */
	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, cell,
						 (GtkTreeCellDataFunc)
						 accounts_dialog_model_pixbuf_data_func,
						 dialog,
						 NULL);

	/* Name renderer */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "editable", TRUE,
		      NULL);
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_add_attribute (column, cell, "text", COL_NAME);
	g_signal_connect (cell, "edited",
			  G_CALLBACK (accounts_dialog_name_edited_cb),
			  dialog);
	g_signal_connect (cell, "editing-started",
			  G_CALLBACK (accounts_dialog_name_editing_started_cb),
			  dialog);
}

static void
accounts_dialog_model_select_first (EmpathyAccountsDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;

	/* select first */
	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);
	
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		selection = gtk_tree_view_get_selection (view);
		gtk_tree_selection_select_iter (selection, &iter);
	} else {
		accounts_dialog_update_account (dialog, NULL);
	}
}

static void
accounts_dialog_model_pixbuf_data_func (GtkTreeViewColumn    *tree_column,
					GtkCellRenderer      *cell,
					GtkTreeModel         *model,
					GtkTreeIter          *iter,
					EmpathyAccountsDialog *dialog)
{
	McAccount          *account;
	const gchar        *icon_name;
	GdkPixbuf          *pixbuf;
	TpConnectionStatus  status;

	gtk_tree_model_get (model, iter,
			    COL_STATUS, &status,
			    COL_ACCOUNT_POINTER, &account,
			    -1);

	icon_name = empathy_icon_name_from_account (account);
	pixbuf = empathy_pixbuf_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);

	if (pixbuf) {
		if (status == TP_CONNECTION_STATUS_DISCONNECTED ||
		    (status == TP_CONNECTION_STATUS_CONNECTING && 
		     !dialog->connecting_show)) {
			GdkPixbuf *modded_pixbuf;

			modded_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
							TRUE,
							8,
							gdk_pixbuf_get_width (pixbuf),
							gdk_pixbuf_get_height (pixbuf));

			gdk_pixbuf_saturate_and_pixelate (pixbuf,
							  modded_pixbuf,
							  1.0,
							  TRUE);
			g_object_unref (pixbuf);
			pixbuf = modded_pixbuf;
		}
	}

	g_object_set (cell,
		      "visible", TRUE,
		      "pixbuf", pixbuf,
		      NULL);

	g_object_unref (account);
	if (pixbuf) {
		g_object_unref (pixbuf);
	}
}

static gboolean
accounts_dialog_get_account_iter (EmpathyAccountsDialog *dialog,
				 McAccount             *account,
				 GtkTreeIter           *iter)
{
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	gboolean          ok;
	
	/* Update the status in the model */
	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);
	model = gtk_tree_view_get_model (view);

	for (ok = gtk_tree_model_get_iter_first (model, iter);
	     ok;
	     ok = gtk_tree_model_iter_next (model, iter)) {
		McAccount *this_account;
		gboolean   equal;

		gtk_tree_model_get (model, iter,
				    COL_ACCOUNT_POINTER, &this_account,
				    -1);

		equal = empathy_account_equal (this_account, account);
		g_object_unref (this_account);

		if (equal) {
			return TRUE;
		}
	}

	return FALSE;
}

static McAccount *
accounts_dialog_model_get_selected (EmpathyAccountsDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;
	McAccount        *account;

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter, COL_ACCOUNT_POINTER, &account, -1);

	return account;
}

static void
accounts_dialog_model_set_selected (EmpathyAccountsDialog *dialog,
				    McAccount             *account)
{
	GtkTreeSelection *selection;
	GtkTreeIter       iter;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->treeview));
	if (accounts_dialog_get_account_iter (dialog, account, &iter)) {
		gtk_tree_selection_select_iter (selection, &iter);
	}
}

static gboolean
accounts_dialog_model_remove_selected (EmpathyAccountsDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeModel     *model;
	GtkTreeSelection *selection;
	GtkTreeIter       iter;

	view = GTK_TREE_VIEW (dialog->treeview);
	selection = gtk_tree_view_get_selection (view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return FALSE;
	}

	return gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
accounts_dialog_model_selection_changed (GtkTreeSelection     *selection,
					 EmpathyAccountsDialog *dialog)
{
	McAccount    *account;
	GtkTreeModel *model;
	GtkTreeIter   iter;
	gboolean      is_selection;

	is_selection = gtk_tree_selection_get_selected (selection, &model, &iter);

	account = accounts_dialog_model_get_selected (dialog);
	accounts_dialog_update_account (dialog, account);

	if (account) {
		g_object_unref (account);
	}
}

static void
accounts_dialog_add_or_update_account (EmpathyAccountsDialog *dialog,
				       McAccount             *account)
{
	GtkTreeModel       *model;
	GtkTreeIter         iter;
	TpConnectionStatus  status;
	const gchar        *name;
	gboolean            enabled;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->treeview));
	status = mission_control_get_connection_status (dialog->mc, account, NULL);
	name = mc_account_get_display_name (account);
	enabled = mc_account_is_enabled (account);

	if (!accounts_dialog_get_account_iter (dialog, account, &iter)) {
		DEBUG ("Adding new account");
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	}

	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    COL_ENABLED, enabled,
			    COL_NAME, name,
			    COL_STATUS, status,
			    COL_ACCOUNT_POINTER, account,
			    -1);

	accounts_dialog_connection_changed_cb (dialog->account_manager,
					       account,
					       TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED,
					       status,
					       TP_CONNECTION_STATUS_DISCONNECTED,
					       dialog);
}

static void
accounts_dialog_account_added_cb (EmpathyAccountManager *manager,
				  McAccount *account,
				  EmpathyAccountsDialog *dialog)
{
	const gchar *current_name;
	gchar       *account_param = NULL;

	accounts_dialog_add_or_update_account (dialog, account);

	/* Change the display name to "%s (%s)" % (protocol, account).
	 *  - The protocol is the display name of the profile.
	 *  - The account should be the normalized name of the McAccount but
	 *    it's not set until first connection, so we get the "account"
	 *    parameter for CM that have it. */
	current_name = mc_account_get_display_name (account);
	mc_account_get_param_string (account, "account", &account_param);
	if (!EMP_STR_EMPTY (account_param)) {
		McProfile   *profile;
		const gchar *profile_name;
		gchar       *new_name;

		profile = mc_account_get_profile (account);
		profile_name = mc_profile_get_display_name (profile);
		new_name = g_strdup_printf ("%s (%s)", profile_name,
					    account_param);

		DEBUG ("Setting new display name for account %s: '%s'",
		       mc_account_get_unique_name (account), new_name);

		mc_account_set_display_name (account, new_name);
		g_free (new_name);
		g_object_unref (profile);
	} else {
		/* FIXME: This CM has no account parameter, what can be done? */
	}
	g_free (account_param);
}

static void
accounts_dialog_account_removed_cb (EmpathyAccountManager *manager,
				    McAccount            *account,
				    EmpathyAccountsDialog *dialog)
{

	accounts_dialog_model_set_selected (dialog, account);
	accounts_dialog_model_remove_selected (dialog);
}

static gboolean
accounts_dialog_row_changed_foreach (GtkTreeModel *model,
				     GtkTreePath  *path,
				     GtkTreeIter  *iter,
				     gpointer      user_data)
{
	gtk_tree_model_row_changed (model, path, iter);

	return FALSE;
}

static gboolean
accounts_dialog_flash_connecting_cb (EmpathyAccountsDialog *dialog)
{
	GtkTreeView  *view;
	GtkTreeModel *model;

	dialog->connecting_show = !dialog->connecting_show;

	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);

	gtk_tree_model_foreach (model, accounts_dialog_row_changed_foreach, NULL);

	return TRUE;
}

static void
accounts_dialog_connection_changed_cb     (EmpathyAccountManager    *manager,
					   McAccount                *account,
					   TpConnectionStatusReason  reason,
					   TpConnectionStatus        current,
					   TpConnectionStatus        previous,
					   EmpathyAccountsDialog    *dialog)
{
	GtkTreeModel *model;
	GtkTreeIter   iter;
	gboolean      found;
	
	/* Update the status in the model */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->treeview));

	if (accounts_dialog_get_account_iter (dialog, account, &iter)) {
		GtkTreePath *path;

		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    COL_STATUS, current,
				    -1);

		path = gtk_tree_model_get_path (model, &iter);
		gtk_tree_model_row_changed (model, path, &iter);
		gtk_tree_path_free (path);
	}

	found = (empathy_account_manager_get_connecting_accounts (manager) > 0);

	if (!found && dialog->connecting_id) {
		g_source_remove (dialog->connecting_id);
		dialog->connecting_id = 0;
	}

	if (found && !dialog->connecting_id) {
		dialog->connecting_id = g_timeout_add (FLASH_TIMEOUT,
						       (GSourceFunc) accounts_dialog_flash_connecting_cb,
						       dialog);
	}
}

static void
enable_or_disable_account (EmpathyAccountsDialog *dialog,
			   McAccount *account,
			   gboolean enabled)
{
	GtkTreeModel *model;
	GtkTreeIter   iter;

	/* Update the status in the model */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->treeview));

	DEBUG ("Account %s is now %s",
		mc_account_get_display_name (account),
		enabled ? "enabled" : "disabled");

	if (accounts_dialog_get_account_iter (dialog, account, &iter)) {
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    COL_ENABLED, enabled,
				    -1);
	}
}

static void
accounts_dialog_account_disabled_cb (EmpathyAccountManager *manager,
				     McAccount             *account,
				     EmpathyAccountsDialog *dialog)
{
	enable_or_disable_account (dialog, account, FALSE);
}

static void
accounts_dialog_account_enabled_cb (EmpathyAccountManager *manager,
				    McAccount             *account,
				    EmpathyAccountsDialog *dialog)
{
	enable_or_disable_account (dialog, account, TRUE);
}

static void
accounts_dialog_account_changed_cb (EmpathyAccountManager *manager,
				    McAccount             *account,
				    EmpathyAccountsDialog  *dialog)
{
	McAccount *selected_account;

	accounts_dialog_add_or_update_account (dialog, account);
	selected_account = accounts_dialog_model_get_selected (dialog);
	if (empathy_account_equal (account, selected_account)) {
		accounts_dialog_update_name_label (dialog, account);
	}
}

static void
accounts_dialog_button_create_clicked_cb (GtkWidget             *button,
					  EmpathyAccountsDialog  *dialog)
{
	McProfile *profile;
	McAccount *account;
	gchar     *str;
	McProfileCapabilityFlags cap;

	profile = empathy_profile_chooser_get_selected (dialog->combobox_profile);

	/* Create account */
	account = mc_account_create (profile);
	if (account == NULL) {
		/* We can't display an error to the user as MC doesn't give us
		 * any clue about the reason of the failure... */
		return;
	}

	/* To translator: %s is the protocol name */
	str = g_strdup_printf (_("New %s account"),
			       mc_profile_get_display_name (profile));
	mc_account_set_display_name (account, str);
	g_free (str);

	cap = mc_profile_get_capabilities (profile);
	if (cap & MC_PROFILE_CAPABILITY_REGISTRATION_UI) {
		gboolean active;

		active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_register));
		if (!active) {
			mc_account_set_param_boolean (account, "register", TRUE);
		}
	}

	accounts_dialog_add_or_update_account (dialog, account);
	accounts_dialog_model_set_selected (dialog, account);

	g_object_unref (account);
	g_object_unref (profile);
}

static void
accounts_dialog_button_back_clicked_cb (GtkWidget             *button,
					EmpathyAccountsDialog  *dialog)
{
	McAccount *account;

	account = accounts_dialog_model_get_selected (dialog);
	accounts_dialog_update_account (dialog, account);
}

static void
accounts_dialog_profile_changed_cb (GtkWidget             *widget,
				    EmpathyAccountsDialog *dialog)
{
	McProfile *profile;
	McProfileCapabilityFlags cap;

	profile = empathy_profile_chooser_get_selected (dialog->combobox_profile);
	cap = mc_profile_get_capabilities (profile);

	if (cap & MC_PROFILE_CAPABILITY_REGISTRATION_UI) {
		gtk_widget_show (dialog->checkbutton_register);
	} else {
		gtk_widget_hide (dialog->checkbutton_register);
	}
	g_object_unref (profile);
}

static void
accounts_dialog_button_add_clicked_cb (GtkWidget             *button,
				       EmpathyAccountsDialog *dialog)
{
	GtkTreeView      *view;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;

	view = GTK_TREE_VIEW (dialog->treeview);
	model = gtk_tree_view_get_model (view);
	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_unselect_all (selection);

	gtk_widget_set_sensitive (dialog->button_add, FALSE);
	gtk_widget_set_sensitive (dialog->button_remove, FALSE);
	gtk_widget_hide (dialog->vbox_details);
	gtk_widget_hide (dialog->frame_no_profile);
	gtk_widget_show (dialog->frame_new_account);

	/* If we have no account, no need of a back button */
	if (gtk_tree_model_iter_n_children (model, NULL) > 0) {
		gtk_widget_show (dialog->button_back);
	} else {
		gtk_widget_hide (dialog->button_back);
	}

	accounts_dialog_profile_changed_cb (dialog->checkbutton_register, dialog);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_register),
				      TRUE);
	gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->combobox_profile), 0);
	gtk_widget_grab_focus (dialog->combobox_profile);
}

static void
accounts_dialog_button_help_clicked_cb (GtkWidget             *button,
					EmpathyAccountsDialog *dialog)
{
	empathy_url_show (button, "ghelp:empathy?empathy-create-account");
}

static void
accounts_dialog_button_remove_clicked_cb (GtkWidget            *button,
					  EmpathyAccountsDialog *dialog)
{
	McAccount *account;
	GtkWidget *message_dialog;
	gint       res;

	account = accounts_dialog_model_get_selected (dialog);

	if (!mc_account_is_complete (account)) {
		accounts_dialog_model_remove_selected (dialog);
		accounts_dialog_model_select_first (dialog);
		return;
	}
	message_dialog = gtk_message_dialog_new
		(GTK_WINDOW (dialog->window),
		 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		 GTK_MESSAGE_QUESTION,
		 GTK_BUTTONS_NONE,
		 _("You are about to remove your %s account!\n"
		   "Are you sure you want to proceed?"),
		 mc_account_get_display_name (account));

	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (message_dialog),
		 _("Any associated conversations and chat rooms will NOT be "
		   "removed if you decide to proceed.\n"
		   "\n"
		   "Should you decide to add the account back at a later time, "
		   "they will still be available."));

	gtk_dialog_add_button (GTK_DIALOG (message_dialog),
			       GTK_STOCK_CANCEL, 
			       GTK_RESPONSE_NO);
	gtk_dialog_add_button (GTK_DIALOG (message_dialog),
			       GTK_STOCK_REMOVE, 
			       GTK_RESPONSE_YES);

	gtk_widget_show (message_dialog);
	res = gtk_dialog_run (GTK_DIALOG (message_dialog));

	if (res == GTK_RESPONSE_YES) {
		mc_account_delete (account);
		accounts_dialog_model_select_first (dialog);
	}
	gtk_widget_destroy (message_dialog);
}

static void
accounts_dialog_button_import_clicked_cb (GtkWidget             *button,
					  EmpathyAccountsDialog *dialog)
{
	empathy_import_dialog_show (GTK_WINDOW (dialog->window), TRUE);
}

static void
accounts_dialog_response_cb (GtkWidget            *widget,
			     gint                  response,
			     EmpathyAccountsDialog *dialog)
{
	if (response == GTK_RESPONSE_CLOSE) {
		gtk_widget_destroy (widget);
	}
}

static void
accounts_dialog_destroy_cb (GtkWidget            *widget,
			    EmpathyAccountsDialog *dialog)
{
	GList *accounts, *l;

	/* Disconnect signals */
	g_signal_handlers_disconnect_by_func (dialog->account_manager,
					      accounts_dialog_account_added_cb,
					      dialog);
	g_signal_handlers_disconnect_by_func (dialog->account_manager,
					      accounts_dialog_account_removed_cb,
					      dialog);
	g_signal_handlers_disconnect_by_func (dialog->account_manager,
					      accounts_dialog_account_enabled_cb,
					      dialog);
	g_signal_handlers_disconnect_by_func (dialog->account_manager,
					      accounts_dialog_account_disabled_cb,
					      dialog);
	g_signal_handlers_disconnect_by_func (dialog->account_manager,
					      accounts_dialog_account_changed_cb,
					      dialog);
	g_signal_handlers_disconnect_by_func (dialog->account_manager,
					      accounts_dialog_connection_changed_cb,
					      dialog);

	/* Delete incomplete accounts */
	accounts = mc_accounts_list ();
	for (l = accounts; l; l = l->next) {
		McAccount *account;

		account = l->data;
		if (!mc_account_is_complete (account)) {
			/* FIXME: Warn the user the account is not complete
			 *        and is going to be removed. */
			mc_account_delete (account);
		}

		g_object_unref (account);
	}
	g_list_free (accounts);

	if (dialog->connecting_id) {
		g_source_remove (dialog->connecting_id);
	}

	g_object_unref (dialog->account_manager);
	g_object_unref (dialog->mc);
	
	g_free (dialog);
}

GtkWidget *
empathy_accounts_dialog_show (GtkWindow *parent,
			      McAccount *selected_account)
{
	static EmpathyAccountsDialog *dialog = NULL;
	GladeXML                     *glade;
	gchar                        *filename;
	GList                        *accounts, *l;
	gboolean                      import_asked;

	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog->window));
		return dialog->window;
	}

	dialog = g_new0 (EmpathyAccountsDialog, 1);

	filename = empathy_file_lookup ("empathy-accounts-dialog.glade",
					"src");
	glade = empathy_glade_get_file (filename,
				       "accounts_dialog",
				       NULL,
				       "accounts_dialog", &dialog->window,
				       "vbox_details", &dialog->vbox_details,
				       "frame_no_profile", &dialog->frame_no_profile,
				       "alignment_settings", &dialog->alignment_settings,
				       "treeview", &dialog->treeview,
				       "frame_new_account", &dialog->frame_new_account,
				       "hbox_type", &dialog->hbox_type,
				       "button_create", &dialog->button_create,
				       "button_back", &dialog->button_back,
				       "checkbutton_register", &dialog->checkbutton_register,
				       "image_type", &dialog->image_type,
				       "label_name", &dialog->label_name,
				       "button_add", &dialog->button_add,
				       "button_remove", &dialog->button_remove,
				       "button_import", &dialog->button_import,
				       NULL);
	g_free (filename);

	empathy_glade_connect (glade,
			      dialog,
			      "accounts_dialog", "destroy", accounts_dialog_destroy_cb,
			      "accounts_dialog", "response", accounts_dialog_response_cb,
			      "button_create", "clicked", accounts_dialog_button_create_clicked_cb,
			      "button_back", "clicked", accounts_dialog_button_back_clicked_cb,
			      "button_add", "clicked", accounts_dialog_button_add_clicked_cb,
			      "button_remove", "clicked", accounts_dialog_button_remove_clicked_cb,
			      "button_import", "clicked", accounts_dialog_button_import_clicked_cb,
			      "button_help", "clicked", accounts_dialog_button_help_clicked_cb,
			      NULL);

	g_object_add_weak_pointer (G_OBJECT (dialog->window), (gpointer) &dialog);

	g_object_unref (glade);

	/* Create profile chooser */
	dialog->combobox_profile = empathy_profile_chooser_new ();
	gtk_box_pack_end (GTK_BOX (dialog->hbox_type),
			  dialog->combobox_profile,
			  TRUE, TRUE, 0);
	gtk_widget_show (dialog->combobox_profile);
	g_signal_connect (dialog->combobox_profile, "changed",
			  G_CALLBACK (accounts_dialog_profile_changed_cb),
			  dialog);

	/* Set up signalling */
	dialog->account_manager = empathy_account_manager_dup_singleton ();
	dialog->mc = empathy_mission_control_dup_singleton ();

	g_signal_connect (dialog->account_manager, "account-created",
			  G_CALLBACK (accounts_dialog_account_added_cb),
			  dialog);
	g_signal_connect (dialog->account_manager, "account-deleted",
			  G_CALLBACK (accounts_dialog_account_removed_cb),
			  dialog);
	g_signal_connect (dialog->account_manager, "account-enabled",
			  G_CALLBACK (accounts_dialog_account_enabled_cb),
			  dialog);
	g_signal_connect (dialog->account_manager, "account-disabled",
			  G_CALLBACK (accounts_dialog_account_disabled_cb),
			  dialog);
	g_signal_connect (dialog->account_manager, "account-changed",
			  G_CALLBACK (accounts_dialog_account_changed_cb),
			  dialog);
	g_signal_connect (dialog->account_manager, "account-connection-changed",
			  G_CALLBACK (accounts_dialog_connection_changed_cb),
			  dialog);

	accounts_dialog_model_setup (dialog);

	/* Add existing accounts */
	accounts = mc_accounts_list ();
	for (l = accounts; l; l = l->next) {
		accounts_dialog_add_or_update_account (dialog, l->data);
		g_object_unref (l->data);
	}
	g_list_free (accounts);

	if (selected_account) {
		accounts_dialog_model_set_selected (dialog, selected_account);
	} else {
		accounts_dialog_model_select_first (dialog);
	}

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog->window),
					      GTK_WINDOW (parent));
	}

	gtk_widget_show (dialog->window);

	empathy_conf_get_bool (empathy_conf_get (),
			       EMPATHY_PREFS_IMPORT_ASKED, &import_asked);

	if (!import_asked) {
		empathy_conf_set_bool (empathy_conf_get (),
				       EMPATHY_PREFS_IMPORT_ASKED, TRUE);
		empathy_import_dialog_show (GTK_WINDOW (dialog->window),
					    FALSE);
	}

	return dialog->window;
}

