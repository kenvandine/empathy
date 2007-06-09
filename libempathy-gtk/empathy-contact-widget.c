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

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libmissioncontrol/mc-account.h>

#include <libempathy/empathy-contact-manager.h>

#include "empathy-contact-widget.h"
#include "gossip-account-chooser.h"
#include "gossip-ui-utils.h"

typedef struct {
	GossipContact   *contact;
	gboolean         is_user;
	gboolean         editable;
	gboolean         can_change_contact;
	GtkCellRenderer *renderer;

	GtkWidget       *vbox_contact_widget;

	/* Contact */
	GtkWidget       *vbox_contact;
	GtkWidget       *widget_avatar;
	GtkWidget       *widget_account;
	GtkWidget       *widget_id;
	GtkWidget       *widget_alias;
	GtkWidget       *label_alias;
	GtkWidget       *entry_alias;
	GtkWidget       *hbox_presence;
	GtkWidget       *image_state;
	GtkWidget       *label_status;
	GtkWidget       *table_contact;
	GtkWidget       *hbox_contact;

	/* Groups */
	GtkWidget       *vbox_groups;
	GtkWidget       *entry_group;
	GtkWidget       *button_group;
	GtkWidget       *treeview_groups;

	/* Details */
	GtkWidget       *vbox_details;
	GtkWidget       *table_details;
	GtkWidget       *hbox_details_requested;

	/* Client */
	GtkWidget       *vbox_client;
	GtkWidget       *table_client;
	GtkWidget       *hbow_client_requested;

} EmpathyContactWidget;

typedef struct {
	EmpathyContactWidget *information;
	const gchar          *name;
	gboolean              found;
	GtkTreeIter           found_iter;
} FindName;

static void     contact_widget_destroy_cb                 (GtkWidget             *widget,
							   EmpathyContactWidget  *information);
static void     contact_widget_remove_contact             (EmpathyContactWidget  *information);
static void     contact_widget_set_contact                (EmpathyContactWidget  *information,
							   GossipContact         *contact);
static void     contact_widget_contact_setup              (EmpathyContactWidget  *information);
static void     contact_widget_contact_update             (EmpathyContactWidget  *information);
static gboolean contact_widget_update_contact             (EmpathyContactWidget  *information);
static void     contact_widget_account_changed_cb         (GtkComboBox           *widget,
							   EmpathyContactWidget  *information);
static gboolean contact_widget_id_focus_out_cb            (GtkWidget             *widget,
							   GdkEventFocus         *event,
							   EmpathyContactWidget  *information);
static void     contact_widget_entry_alias_focus_event_cb (GtkEditable           *editable,
							   GdkEventFocus         *event,
							   EmpathyContactWidget  *information);
static void     contact_widget_name_notify_cb             (EmpathyContactWidget  *information);
static void     contact_widget_presence_notify_cb         (EmpathyContactWidget  *information);
static void     contact_widget_avatar_notify_cb           (EmpathyContactWidget  *information);
static void     contact_widget_groups_setup               (EmpathyContactWidget  *information);
static void     contact_widget_groups_update              (EmpathyContactWidget  *information);
static void     contact_widget_model_setup                (EmpathyContactWidget  *information);
static void     contact_widget_model_populate_columns     (EmpathyContactWidget  *information);
static void     contact_widget_groups_populate_data       (EmpathyContactWidget  *information);
static void     contact_widget_groups_notify_cb           (EmpathyContactWidget  *information);
static gboolean contact_widget_model_find_name            (EmpathyContactWidget  *information,
							   const gchar           *name,
							   GtkTreeIter           *iter);
static gboolean contact_widget_model_find_name_foreach    (GtkTreeModel          *model,
							   GtkTreePath           *path,
							   GtkTreeIter           *iter,
							   FindName              *data);
static void     contact_widget_cell_toggled               (GtkCellRendererToggle *cell,
							   gchar                 *path_string,
							   EmpathyContactWidget  *information);
static void     contact_widget_entry_group_changed_cb     (GtkEditable           *editable,
							   EmpathyContactWidget  *information);
static void     contact_widget_entry_group_activate_cb    (GtkEntry              *entry,
							   EmpathyContactWidget  *information);
static void     contact_widget_button_group_clicked_cb    (GtkButton             *button,
							   EmpathyContactWidget  *information);
static void     contact_widget_details_setup              (EmpathyContactWidget  *information);
static void     contact_widget_details_update             (EmpathyContactWidget  *information);
static void     contact_widget_client_setup               (EmpathyContactWidget  *information);
static void     contact_widget_client_update              (EmpathyContactWidget  *information);

enum {
	COL_NAME,
	COL_ENABLED,
	COL_EDITABLE,
	COL_COUNT
};

GtkWidget *
empathy_contact_widget_new (GossipContact *contact,
			    gboolean       editable)
{
	EmpathyContactWidget *information;
	GladeXML             *glade;

	information = g_slice_new0 (EmpathyContactWidget);
	information->editable = editable;
	if (contact) {
		GossipContact *user_contact;
	
		user_contact = gossip_contact_get_user (contact);
		information->is_user = gossip_contact_equal (contact, user_contact);
		information->can_change_contact = FALSE;
	} else {
		information->is_user = FALSE;
		information->can_change_contact = TRUE;
	}

	glade = gossip_glade_get_file ("empathy-contact-widget.glade",
				       "vbox_contact_widget",
				       NULL,
				       "vbox_contact_widget", &information->vbox_contact_widget,
				       "vbox_contact", &information->vbox_contact,
				       "hbox_presence", &information->hbox_presence,
				       "label_alias", &information->label_alias,
				       "image_state", &information->image_state,
				       "label_status", &information->label_status,
				       "table_contact", &information->table_contact,
				       "hbox_contact", &information->hbox_contact,
				       "vbox_groups", &information->vbox_groups,
				       "entry_group", &information->entry_group,
				       "button_group", &information->button_group,
				       "treeview_groups", &information->treeview_groups,
				       "vbox_details", &information->vbox_details,
				       "table_details", &information->table_details,
				       "hbox_details_requested", &information->hbox_details_requested,
				       "vbox_client", &information->vbox_client,
				       "table_client", &information->table_client,
				       "hbox_client_requested", &information->hbow_client_requested,
				       NULL);

	gossip_glade_connect (glade,
			      information,
			      "vbox_contact_widget", "destroy", contact_widget_destroy_cb,
			      "entry_group", "changed", contact_widget_entry_group_changed_cb,
			      "entry_group", "activate", contact_widget_entry_group_activate_cb,
			      "button_group", "clicked", contact_widget_button_group_clicked_cb,
			      NULL);

	g_object_unref (glade);

	g_object_set_data (G_OBJECT (information->vbox_contact_widget),
			   "EmpathyContactWidget",
			   information);

	/* Create widgets */
	contact_widget_contact_setup (information);
	contact_widget_groups_setup (information);
	contact_widget_details_setup (information);
	contact_widget_client_setup (information);

	contact_widget_set_contact (information, contact);

	gtk_widget_show (information->vbox_contact_widget);

	return information->vbox_contact_widget;
}

GossipContact *
empathy_contact_widget_get_contact (GtkWidget *widget)
{
	EmpathyContactWidget *information;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	information = g_object_get_data (G_OBJECT (widget), "EmpathyContactWidget");
	if (!information) {
		return NULL;
	}

	return information->contact;
}
	
static void
contact_widget_destroy_cb (GtkWidget            *widget,
			   EmpathyContactWidget *information)
{
	contact_widget_remove_contact (information);
	g_slice_free (EmpathyContactWidget, information);
}

static void
contact_widget_remove_contact (EmpathyContactWidget *information)
{
	if (information->contact) {
		g_signal_handlers_disconnect_by_func (information->contact,
						      contact_widget_name_notify_cb,
						      information);
		g_signal_handlers_disconnect_by_func (information->contact,
						      contact_widget_presence_notify_cb,
						      information);
		g_signal_handlers_disconnect_by_func (information->contact,
						      contact_widget_avatar_notify_cb,
						      information);
		g_signal_handlers_disconnect_by_func (information->contact,
						      contact_widget_groups_notify_cb,
						      information);

		g_object_unref (information->contact);
		information->contact = NULL;
	}
}

static void
contact_widget_set_contact (EmpathyContactWidget *information,
			    GossipContact        *contact)
{
	contact_widget_remove_contact (information);
	if (contact) {
		information->contact = g_object_ref (contact);
	}

	/* Update information for widgets */
	contact_widget_contact_update (information);
	contact_widget_groups_update (information);
	contact_widget_details_update (information);
	contact_widget_client_update (information);
}

static void
contact_widget_contact_setup (EmpathyContactWidget *information)
{
	/* FIXME: Use GossipAvatarImage if (editable && is_user)  */
	information->widget_avatar = gtk_image_new ();
	gtk_box_pack_end (GTK_BOX (information->hbox_contact),
	 		  information->widget_avatar,
	 		  FALSE, FALSE,
	 		  6);

	/* Setup account label/chooser */
	if (information->can_change_contact) {
		information->widget_account = gossip_account_chooser_new ();
		g_signal_connect (information->widget_account, "changed",
				  G_CALLBACK (contact_widget_account_changed_cb),
				  information);
	} else {
		information->widget_account = gtk_label_new (NULL);
		gtk_label_set_selectable (GTK_LABEL (information->widget_account), TRUE);
	}
	gtk_table_attach_defaults (GTK_TABLE (information->table_contact),
				   information->widget_account,
				   1, 2, 0, 1);
	gtk_widget_show (information->widget_account);

	/* Setup id label/entry */
	if (information->can_change_contact) {
		information->widget_id = gtk_entry_new ();
		g_signal_connect (information->widget_id, "focus-out-event",
				  G_CALLBACK (contact_widget_id_focus_out_cb),
				  information);
	} else {
		information->widget_id = gtk_label_new (NULL);
		gtk_label_set_selectable (GTK_LABEL (information->widget_id), TRUE);
	}
	gtk_table_attach_defaults (GTK_TABLE (information->table_contact),
				   information->widget_id,
				   1, 2, 1, 2);
	gtk_widget_show (information->widget_id);

	/* Setup alias label/entry */
	if (information->editable) {
		information->widget_alias = gtk_entry_new ();
		g_signal_connect (information->widget_alias, "focus-out-event",
				  G_CALLBACK (contact_widget_entry_alias_focus_event_cb),
				  information);
	} else {
		information->widget_alias = gtk_label_new (NULL);
		gtk_label_set_selectable (GTK_LABEL (information->widget_alias), TRUE);
	}
	gtk_table_attach_defaults (GTK_TABLE (information->table_contact),
				   information->widget_alias,
				   1, 2, 2, 3);
	gtk_widget_show (information->widget_alias);
}

static void
contact_widget_contact_update (EmpathyContactWidget *information)
{
	McAccount   *account = NULL;
	const gchar *id = NULL;

	/* Connect and get info from new contact */
	if (information->contact) {
		g_signal_connect_swapped (information->contact, "notify::name",
					  G_CALLBACK (contact_widget_name_notify_cb),
					  information);
		g_signal_connect_swapped (information->contact, "notify::presence",
					  G_CALLBACK (contact_widget_presence_notify_cb),
					  information);
		g_signal_connect_swapped (information->contact, "notify::avatar",
					  G_CALLBACK (contact_widget_avatar_notify_cb),
					  information);

		account = gossip_contact_get_account (information->contact);
		id = gossip_contact_get_id (information->contact);
	}

	/* Update account widget */
	if (information->can_change_contact) {
		if (account) {
			g_signal_handlers_block_by_func (information->widget_account,
							 contact_widget_account_changed_cb,
							 information);
			gossip_account_chooser_set_account (GOSSIP_ACCOUNT_CHOOSER (information->widget_account),
							    account);
			g_signal_handlers_unblock_by_func (information->widget_account,
							   contact_widget_account_changed_cb,
							   information);
		}
	}
	else if (account) {
		const gchar *name;

		name = mc_account_get_display_name (account);
		gtk_label_set_label (GTK_LABEL (information->widget_account), name);
	}

	/* Update id widget */
	if (information->can_change_contact) {
		if (!G_STR_EMPTY (id)) {
			gtk_entry_set_text (GTK_ENTRY (information->widget_id), id);
		}
	} else {
		gtk_label_set_label (GTK_LABEL (information->widget_id), id);
	}

	/* Update other widgets */
	if (information->contact) {
		contact_widget_name_notify_cb (information);
		contact_widget_presence_notify_cb (information);
		contact_widget_avatar_notify_cb (information);

		gtk_widget_show (information->label_alias);
		gtk_widget_show (information->widget_alias);
		gtk_widget_show (information->hbox_presence);
	} else {
		gtk_widget_hide (information->label_alias);
		gtk_widget_hide (information->widget_alias);
		gtk_widget_hide (information->hbox_presence);
		gtk_widget_hide (information->widget_avatar);
	}
}

static gboolean
contact_widget_update_contact (EmpathyContactWidget *information)
{
	McAccount   *account;
	const gchar *id;

	account = gossip_account_chooser_get_account (GOSSIP_ACCOUNT_CHOOSER (information->widget_account));
	id = gtk_entry_get_text (GTK_ENTRY (information->widget_id));

	if (account && !G_STR_EMPTY (id)) {
		EmpathyContactManager *manager;
		GossipContact         *contact;

		manager = empathy_contact_manager_new ();
		contact = empathy_contact_manager_create (manager, account, id);
		contact_widget_set_contact (information, contact);

		if (contact) {
			g_object_unref (contact);
		}
		g_object_unref (manager);
	}

	return FALSE;
}

static void
contact_widget_account_changed_cb (GtkComboBox          *widget,
				   EmpathyContactWidget *information)
{
	contact_widget_update_contact (information);
}

static gboolean
contact_widget_id_focus_out_cb (GtkWidget            *widget,
				GdkEventFocus        *event,
				EmpathyContactWidget *information)
{
	contact_widget_update_contact (information);
	return FALSE;
}

static void
contact_widget_entry_alias_focus_event_cb (GtkEditable          *editable,
					   GdkEventFocus        *event,
					   EmpathyContactWidget *information)
{
	if (information->contact) {
		const gchar *name;

		name = gtk_entry_get_text (GTK_ENTRY (editable));
		gossip_contact_set_name (information->contact, name);
	}
}

static void
contact_widget_name_notify_cb (EmpathyContactWidget *information)
{
	if (information->editable) {
		gtk_entry_set_text (GTK_ENTRY (information->widget_alias),
				    gossip_contact_get_name (information->contact));
	} else {
		gtk_label_set_label (GTK_LABEL (information->widget_alias),
				     gossip_contact_get_name (information->contact));
	}
}

static void
contact_widget_presence_notify_cb (EmpathyContactWidget *information)
{
	gtk_label_set_text (GTK_LABEL (information->label_status),
			    gossip_contact_get_status (information->contact));
	gtk_image_set_from_icon_name (GTK_IMAGE (information->image_state),
				      gossip_icon_name_for_contact (information->contact),
				      GTK_ICON_SIZE_BUTTON);

}

static void
contact_widget_avatar_notify_cb (EmpathyContactWidget *information)
{
	GdkPixbuf *avatar_pixbuf;

	avatar_pixbuf = gossip_pixbuf_avatar_from_contact_scaled (information->contact,
								  48, 48);

	if (avatar_pixbuf) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (information->widget_avatar),
					   avatar_pixbuf);
		gtk_widget_show  (information->widget_avatar);
		g_object_unref (avatar_pixbuf);
	} else {
		gtk_widget_hide  (information->widget_avatar);
	}
}

static void
contact_widget_groups_setup (EmpathyContactWidget *information)
{
	if (information->editable) {
		contact_widget_model_setup (information);
	}
}

static void
contact_widget_groups_update (EmpathyContactWidget *information)
{
	if (information->editable && information->contact) {
		g_signal_connect_swapped (information->contact, "notify::groups",
					  G_CALLBACK (contact_widget_groups_notify_cb),
					  information);
		contact_widget_groups_populate_data (information);

		gtk_widget_show (information->vbox_groups);
	} else {
		gtk_widget_hide (information->vbox_groups);
	}
}

static void
contact_widget_model_setup (EmpathyContactWidget *information)
{
	GtkTreeView      *view;
	GtkListStore     *store;
	GtkTreeSelection *selection;

	view = GTK_TREE_VIEW (information->treeview_groups);

	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_STRING,   /* name */
				    G_TYPE_BOOLEAN,  /* enabled */
				    G_TYPE_BOOLEAN); /* editable */

	gtk_tree_view_set_model (view, GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	contact_widget_model_populate_columns (information);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      COL_NAME, GTK_SORT_ASCENDING);

	g_object_unref (store);
}

static void
contact_widget_model_populate_columns (EmpathyContactWidget *information)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;
	GtkTreeViewColumn *column;
	GtkCellRenderer   *renderer;
	guint              col_offset;

	view = GTK_TREE_VIEW (information->treeview_groups);
	model = gtk_tree_view_get_model (view);

	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (contact_widget_cell_toggled),
			  information);

	column = gtk_tree_view_column_new_with_attributes (_("Select"), renderer,
							   "active", COL_ENABLED,
							   NULL);

	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width (column, 50);
	gtk_tree_view_append_column (view, column);

	renderer = gtk_cell_renderer_text_new ();
	col_offset = gtk_tree_view_insert_column_with_attributes (view,
								  -1, _("Group"),
								  renderer,
								  "text", COL_NAME,
								  /* "editable", COL_EDITABLE, */
								  NULL);

	g_object_set_data (G_OBJECT (renderer),
			   "column", GINT_TO_POINTER (COL_NAME));

	column = gtk_tree_view_get_column (view, col_offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_NAME);
	gtk_tree_view_column_set_resizable (column,FALSE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	if (information->renderer) {
		g_object_unref (information->renderer);
	}

	information->renderer = g_object_ref (renderer);
}

static void
contact_widget_groups_populate_data (EmpathyContactWidget *information)
{
	EmpathyContactManager *manager;
	GtkTreeView           *view;
	GtkListStore          *store;
	GtkTreeIter            iter;
	GList                 *my_groups, *l;
	GList                 *all_groups;

	view = GTK_TREE_VIEW (information->treeview_groups);
	store = GTK_LIST_STORE (gtk_tree_view_get_model (view));
	gtk_list_store_clear (store);

	manager = empathy_contact_manager_new ();
	all_groups = empathy_contact_manager_get_groups (manager);
	my_groups = gossip_contact_get_groups (information->contact);
	g_object_unref (manager);

	for (l = all_groups; l; l = l->next) {
		const gchar *group_str;
		gboolean     enabled;

		group_str = l->data;

		enabled = g_list_find_custom (my_groups,
					      group_str,
					      (GCompareFunc) strcmp) != NULL;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_NAME, group_str,
				    COL_EDITABLE, TRUE,
				    COL_ENABLED, enabled,
				    -1);
	}

	g_list_free (all_groups);
}

static void
contact_widget_groups_notify_cb (EmpathyContactWidget *information)
{
	/* FIXME: not implemented */
}

static gboolean
contact_widget_model_find_name (EmpathyContactWidget *information,
				const gchar          *name,
				GtkTreeIter          *iter)
{
	GtkTreeView  *view;
	GtkTreeModel *model;
	FindName      data;

	if (G_STR_EMPTY (name)) {
		return FALSE;
	}

	data.information = information;
	data.name = name;
	data.found = FALSE;

	view = GTK_TREE_VIEW (information->treeview_groups);
	model = gtk_tree_view_get_model (view);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) contact_widget_model_find_name_foreach,
				&data);

	if (data.found == TRUE) {
		*iter = data.found_iter;
		return TRUE;
	}

	return FALSE;
}

static gboolean
contact_widget_model_find_name_foreach (GtkTreeModel *model,
					GtkTreePath  *path,
					GtkTreeIter  *iter,
					FindName     *data)
{
	gchar *name;

	gtk_tree_model_get (model, iter,
			    COL_NAME, &name,
			    -1);

	if (!name) {
		return FALSE;
	}

	if (data->name && strcmp (data->name, name) == 0) {
		data->found = TRUE;
		data->found_iter = *iter;

		g_free (name);

		return TRUE;
	}

	g_free (name);

	return FALSE;
}

static void
contact_widget_cell_toggled (GtkCellRendererToggle *cell,
			     gchar                 *path_string,
			     EmpathyContactWidget  *information)
{
	GtkTreeView  *view;
	GtkTreeModel *model;
	GtkListStore *store;
	GtkTreePath  *path;
	GtkTreeIter   iter;
	gboolean      enabled;
	gchar        *group;

	view = GTK_TREE_VIEW (information->treeview_groups);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);

	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    COL_ENABLED, &enabled,
			    COL_NAME, &group,
			    -1);

	gtk_list_store_set (store, &iter, COL_ENABLED, !enabled, -1);
	gtk_tree_path_free (path);

	if (group) {
		if (enabled) {
			gossip_contact_remove_group (information->contact, group);
		} else {
			gossip_contact_add_group (information->contact, group);	
		}

		g_free (group);
	}
}

static void
contact_widget_entry_group_changed_cb (GtkEditable           *editable,
				       EmpathyContactWidget  *information)
{
	GtkTreeIter  iter;
	const gchar *group;

	group = gtk_entry_get_text (GTK_ENTRY (information->entry_group));

	if (contact_widget_model_find_name (information, group, &iter)) {
		gtk_widget_set_sensitive (GTK_WIDGET (information->button_group), FALSE);

	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (information->button_group),
					  !G_STR_EMPTY (group));
	}
}

static void
contact_widget_entry_group_activate_cb (GtkEntry              *entry,
					EmpathyContactWidget  *information)
{
	gtk_widget_activate (GTK_WIDGET (information->button_group));
}

static void
contact_widget_button_group_clicked_cb (GtkButton             *button,
					EmpathyContactWidget  *information)
{
	GtkTreeView  *view;
	GtkListStore *store;
	GtkTreeIter   iter;
	const gchar  *group;

	view = GTK_TREE_VIEW (information->treeview_groups);
	store = GTK_LIST_STORE (gtk_tree_view_get_model (view));

	group = gtk_entry_get_text (GTK_ENTRY (information->entry_group));

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    COL_NAME, group,
			    COL_ENABLED, TRUE,
			    -1);

	gossip_contact_add_group (information->contact, group);
}

static void
contact_widget_details_setup (EmpathyContactWidget *information)
{
	/* FIXME: Needs new telepathy spec */
	gtk_widget_hide (information->vbox_details);
}

static void
contact_widget_details_update (EmpathyContactWidget *information)
{
	/* FIXME: Needs new telepathy spec */
}

static void
contact_widget_client_setup (EmpathyContactWidget *information)
{
	/* FIXME: Needs new telepathy spec */
	gtk_widget_hide (information->vbox_client);
}

static void
contact_widget_client_update (EmpathyContactWidget *information)
{
	/* FIXME: Needs new telepathy spec */
}

