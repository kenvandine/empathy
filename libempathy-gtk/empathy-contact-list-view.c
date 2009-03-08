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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libmissioncontrol/mc-account.h>

#include <libempathy/empathy-call-factory.h>
#include <libempathy/empathy-contact-factory.h>
#include <libempathy/empathy-contact-list.h>
#include <libempathy/empathy-contact-groups.h>
#include <libempathy/empathy-dispatcher.h>
#include <libempathy/empathy-utils.h>

#include "empathy-contact-list-view.h"
#include "empathy-contact-list-store.h"
#include "empathy-images.h"
#include "empathy-cell-renderer-expander.h"
#include "empathy-cell-renderer-text.h"
#include "empathy-cell-renderer-activatable.h"
#include "empathy-ui-utils.h"
#include "empathy-gtk-enum-types.h"
#include "empathy-gtk-marshal.h"

#define DEBUG_FLAG EMPATHY_DEBUG_CONTACT
#include <libempathy/empathy-debug.h>

/* Active users are those which have recently changed state
 * (e.g. online, offline or from normal to a busy state).
 */

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyContactListView)
typedef struct {
	EmpathyContactListStore        *store;
	GtkTreeRowReference            *drag_row;
	EmpathyContactListFeatureFlags  list_features;
	EmpathyContactFeatureFlags      contact_features;
	GtkWidget                      *tooltip_widget;
} EmpathyContactListViewPriv;

typedef struct {
	EmpathyContactListView *view;
	GtkTreePath           *path;
	guint                  timeout_id;
} DragMotionData;

typedef struct {
	EmpathyContactListView *view;
	EmpathyContact         *contact;
	gboolean               remove;
} ShowActiveData;

enum {
	PROP_0,
	PROP_STORE,
	PROP_LIST_FEATURES,
	PROP_CONTACT_FEATURES,
};

enum DndDragType {
	DND_DRAG_TYPE_CONTACT_ID,
	DND_DRAG_TYPE_URL,
	DND_DRAG_TYPE_STRING,
};

static const GtkTargetEntry drag_types_dest[] = {
	{ "text/contact-id", 0, DND_DRAG_TYPE_CONTACT_ID },
	{ "text/uri-list",   0, DND_DRAG_TYPE_URL },
	{ "text/plain",      0, DND_DRAG_TYPE_STRING },
	{ "STRING",          0, DND_DRAG_TYPE_STRING },
};

static const GtkTargetEntry drag_types_source[] = {
	{ "text/contact-id", 0, DND_DRAG_TYPE_CONTACT_ID },
};

static GdkAtom drag_atoms_dest[G_N_ELEMENTS (drag_types_dest)];
static GdkAtom drag_atoms_source[G_N_ELEMENTS (drag_types_source)];

enum {
	DRAG_CONTACT_RECEIVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyContactListView, empathy_contact_list_view, GTK_TYPE_TREE_VIEW);

static void
contact_list_view_tooltip_destroy_cb (GtkWidget              *widget,
				      EmpathyContactListView *view)
{
	EmpathyContactListViewPriv *priv = GET_PRIV (view);
	
	if (priv->tooltip_widget) {
		DEBUG ("Tooltip destroyed");
		priv->tooltip_widget = NULL;
		g_object_unref (widget);
	}
}

static gboolean
contact_list_view_query_tooltip_cb (EmpathyContactListView *view,
				    gint                    x,
				    gint                    y,
				    gboolean                keyboard_mode,
				    GtkTooltip             *tooltip,
				    gpointer                user_data)
{
	EmpathyContactListViewPriv *priv = GET_PRIV (view);
	EmpathyContact             *contact;
	GtkTreeModel               *model;
	GtkTreeIter                 iter;
	GtkTreePath                *path;
	static gint                 running = 0;
	gboolean                    ret = FALSE;

	/* Avoid an infinite loop. See GNOME bug #574377 */
	if (running > 0) {
		return FALSE;
	}
	running++;

	if (!gtk_tree_view_get_tooltip_context (GTK_TREE_VIEW (view), &x, &y,
						keyboard_mode,
						&model, &path, &iter)) {
		goto OUT;
	}

	gtk_tree_view_set_tooltip_row (GTK_TREE_VIEW (view), tooltip, path);
	gtk_tree_path_free (path);

	gtk_tree_model_get (model, &iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, &contact,
			    -1);
	if (!contact) {
		goto OUT;
	}

	if (!priv->tooltip_widget) {
		priv->tooltip_widget = empathy_contact_widget_new (contact,
			EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP);
		g_object_ref (priv->tooltip_widget);
		g_signal_connect (priv->tooltip_widget, "destroy",
				  G_CALLBACK (contact_list_view_tooltip_destroy_cb),
				  view);
	} else {
		empathy_contact_widget_set_contact (priv->tooltip_widget,
						    contact);
	}

	gtk_tooltip_set_custom (tooltip, priv->tooltip_widget);
	ret = TRUE;

	g_object_unref (contact);
OUT:
	running--;

	return ret;
}

static void
contact_list_view_drag_data_received (GtkWidget         *widget,
				      GdkDragContext    *context,
				      gint               x,
				      gint               y,
				      GtkSelectionData  *selection,
				      guint              info,
				      guint              time)
{
	EmpathyContactListViewPriv *priv;
	EmpathyContactList         *list;
	EmpathyContactFactory      *factory;
	McAccount                  *account;
	GtkTreeModel               *model;
	GtkTreePath                *path;
	GtkTreeViewDropPosition     position;
	EmpathyContact             *contact = NULL;
	const gchar                *id;
	gchar                     **strv;
	gchar                      *new_group = NULL;
	gchar                      *old_group = NULL;
	gboolean                    is_row;

	priv = GET_PRIV (widget);

	id = (const gchar*) selection->data;
	DEBUG ("Received %s%s drag & drop contact from roster with id:'%s'",
		context->action == GDK_ACTION_MOVE ? "move" : "",
		context->action == GDK_ACTION_COPY ? "copy" : "",
		id);

	strv = g_strsplit (id, "/", 2);
	factory = empathy_contact_factory_dup_singleton ();
	account = mc_account_lookup (strv[0]);
	if (account) {
		contact = empathy_contact_factory_get_from_id (factory,
							       account,
							       strv[1]);
		g_object_unref (account);
	}
	g_object_unref (factory);
	g_strfreev (strv);

	if (!contact) {
		DEBUG ("No contact found associated with drag & drop");
		return;
	}

	empathy_contact_run_until_ready (contact,
					 EMPATHY_CONTACT_READY_HANDLE,
					 NULL);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	/* Get source group information. */
	if (priv->drag_row) {
		path = gtk_tree_row_reference_get_path (priv->drag_row);
		if (path) {
			old_group = empathy_contact_list_store_get_parent_group (model, path, NULL);
			gtk_tree_path_free (path);
		}
	}

	/* Get destination group information. */
	is_row = gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
						    x,
						    y,
						    &path,
						    &position);

	if (is_row) {
		new_group = empathy_contact_list_store_get_parent_group (model, path, NULL);
		gtk_tree_path_free (path);
	}

	DEBUG ("contact %s (%d) dragged from '%s' to '%s'",
		empathy_contact_get_id (contact),
		empathy_contact_get_handle (contact),
		old_group, new_group);

	list = empathy_contact_list_store_get_list_iface (priv->store);
	if (new_group) {
		empathy_contact_list_add_to_group (list, contact, new_group);
	}
	if (old_group && context->action == GDK_ACTION_MOVE) {	
		empathy_contact_list_remove_from_group (list, contact, old_group);
	}

	g_free (old_group);
	g_free (new_group);

	gtk_drag_finish (context, TRUE, FALSE, GDK_CURRENT_TIME);
}

static gboolean
contact_list_view_drag_motion_cb (DragMotionData *data)
{
	gtk_tree_view_expand_row (GTK_TREE_VIEW (data->view),
				  data->path,
				  FALSE);

	data->timeout_id = 0;

	return FALSE;
}

static gboolean
contact_list_view_drag_motion (GtkWidget      *widget,
			       GdkDragContext *context,
			       gint            x,
			       gint            y,
			       guint           time)
{
	static DragMotionData *dm = NULL;
	GtkTreePath           *path;
	gboolean               is_row;
	gboolean               is_different = FALSE;
	gboolean               cleanup = TRUE;

	is_row = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
						x,
						y,
						&path,
						NULL,
						NULL,
						NULL);

	cleanup &= (!dm);

	if (is_row) {
		cleanup &= (dm && gtk_tree_path_compare (dm->path, path) != 0);
		is_different = (!dm || (dm && gtk_tree_path_compare (dm->path, path) != 0));
	} else {
		cleanup &= FALSE;
	}

	if (!is_different && !cleanup) {
		return TRUE;
	}

	if (dm) {
		gtk_tree_path_free (dm->path);
		if (dm->timeout_id) {
			g_source_remove (dm->timeout_id);
		}

		g_free (dm);

		dm = NULL;
	}

	if (!gtk_tree_view_row_expanded (GTK_TREE_VIEW (widget), path)) {
		dm = g_new0 (DragMotionData, 1);

		dm->view = EMPATHY_CONTACT_LIST_VIEW (widget);
		dm->path = gtk_tree_path_copy (path);

		dm->timeout_id = g_timeout_add_seconds (1,
			(GSourceFunc) contact_list_view_drag_motion_cb,
			dm);
	}

	return TRUE;
}

static void
contact_list_view_drag_begin (GtkWidget      *widget,
			      GdkDragContext *context)
{
	EmpathyContactListViewPriv *priv;
	GtkTreeSelection          *selection;
	GtkTreeModel              *model;
	GtkTreePath               *path;
	GtkTreeIter                iter;

	priv = GET_PRIV (widget);

	GTK_WIDGET_CLASS (empathy_contact_list_view_parent_class)->drag_begin (widget,
									      context);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return;
	}

	path = gtk_tree_model_get_path (model, &iter);
	priv->drag_row = gtk_tree_row_reference_new (model, path);
	gtk_tree_path_free (path);
}

static void
contact_list_view_drag_data_get (GtkWidget        *widget,
				 GdkDragContext   *context,
				 GtkSelectionData *selection,
				 guint             info,
				 guint             time)
{
	EmpathyContactListViewPriv *priv;
	GtkTreePath                *src_path;
	GtkTreeIter                 iter;
	GtkTreeModel               *model;
	EmpathyContact             *contact;
	McAccount                  *account;
	const gchar                *contact_id;
	const gchar                *account_id;
	gchar                      *str;

	priv = GET_PRIV (widget);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	if (!priv->drag_row) {
		return;
	}

	src_path = gtk_tree_row_reference_get_path (priv->drag_row);
	if (!src_path) {
		return;
	}

	if (!gtk_tree_model_get_iter (model, &iter, src_path)) {
		gtk_tree_path_free (src_path);
		return;
	}

	gtk_tree_path_free (src_path);

	contact = empathy_contact_list_view_get_selected (EMPATHY_CONTACT_LIST_VIEW (widget));
	if (!contact) {
		return;
	}

	account = empathy_contact_get_account (contact);
	account_id = mc_account_get_unique_name (account);
	contact_id = empathy_contact_get_id (contact);
	g_object_unref (contact);
	str = g_strconcat (account_id, "/", contact_id, NULL);

	switch (info) {
	case DND_DRAG_TYPE_CONTACT_ID:
		gtk_selection_data_set (selection, drag_atoms_source[info], 8,
					(guchar*)str, strlen (str) + 1);
		break;
	}

	g_free (str);
}

static void
contact_list_view_drag_end (GtkWidget      *widget,
			    GdkDragContext *context)
{
	EmpathyContactListViewPriv *priv;

	priv = GET_PRIV (widget);

	GTK_WIDGET_CLASS (empathy_contact_list_view_parent_class)->drag_end (widget,
									    context);

	if (priv->drag_row) {
		gtk_tree_row_reference_free (priv->drag_row);
		priv->drag_row = NULL;
	}
}

static gboolean
contact_list_view_drag_drop (GtkWidget      *widget,
			     GdkDragContext *drag_context,
			     gint            x,
			     gint            y,
			     guint           time)
{
	return FALSE;
}

typedef struct {
	EmpathyContactListView *view;
	guint                   button;
	guint32                 time;
} MenuPopupData;

static gboolean
contact_list_view_popup_menu_idle_cb (gpointer user_data)
{
	MenuPopupData *data = user_data;
	GtkWidget     *menu;

	menu = empathy_contact_list_view_get_contact_menu (data->view);
	if (!menu) {
		menu = empathy_contact_list_view_get_group_menu (data->view);
	}

	if (menu) {
		gtk_widget_show (menu);
		gtk_menu_popup (GTK_MENU (menu),
				NULL, NULL, NULL, NULL,
				data->button, data->time);
	}

	g_slice_free (MenuPopupData, data);

	return FALSE;
}

static gboolean
contact_list_view_button_press_event_cb (EmpathyContactListView *view,
					 GdkEventButton         *event,
					 gpointer                user_data)
{
	if (event->button == 3) {
		MenuPopupData *data;

		data = g_slice_new (MenuPopupData);
		data->view = view;
		data->button = event->button;
		data->time = event->time;
		g_idle_add (contact_list_view_popup_menu_idle_cb, data);
	}

	return FALSE;
}

static gboolean
contact_list_view_key_press_event_cb (EmpathyContactListView *view,
				      GdkEventKey	     *event,
				      gpointer		      user_data)
{
	if (event->keyval == GDK_Menu) {
		MenuPopupData *data;

		data = g_slice_new (MenuPopupData);
		data->view = view;
		data->button = 0;
		data->time = event->time;
		g_idle_add (contact_list_view_popup_menu_idle_cb, data);
	}

	return FALSE;
}

static void
contact_list_view_row_activated (GtkTreeView       *view,
				 GtkTreePath       *path,
				 GtkTreeViewColumn *column)
{
	EmpathyContactListViewPriv *priv = GET_PRIV (view);
	EmpathyContact             *contact;
	GtkTreeModel               *model;
	GtkTreeIter                 iter;

	if (!(priv->contact_features & EMPATHY_CONTACT_FEATURE_CHAT)) {
		return;
	}

	model = GTK_TREE_MODEL (priv->store);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, &contact,
			    -1);

	if (contact) {
		DEBUG ("Starting a chat");
		empathy_dispatcher_chat_with_contact (contact, NULL, NULL);
		g_object_unref (contact);
	}
}

static void
contact_list_view_voip_activated_cb (EmpathyCellRendererActivatable *cell,
				     const gchar                    *path_string,
				     EmpathyContactListView         *view)
{
	EmpathyContactListViewPriv *priv = GET_PRIV (view);
	GtkTreeModel               *model;
	GtkTreeIter                 iter;
	EmpathyContact             *contact;

	if (!(priv->contact_features & EMPATHY_CONTACT_FEATURE_CALL)) {
		return;
	}

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
	if (!gtk_tree_model_get_iter_from_string (model, &iter, path_string)) {
		return;
	}

	gtk_tree_model_get (model, &iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, &contact,
			    -1);

	if (contact) {
		EmpathyCallFactory *factory;

		factory = empathy_call_factory_get ();
		empathy_call_factory_new_call (factory, contact);

		g_object_unref (contact);
	}
}

static void
contact_list_view_cell_set_background (EmpathyContactListView *view,
				       GtkCellRenderer       *cell,
				       gboolean               is_group,
				       gboolean               is_active)
{
	GdkColor  color;
	GtkStyle *style;

	style = gtk_widget_get_style (GTK_WIDGET (view));

	if (!is_group && is_active) {
		color = style->bg[GTK_STATE_SELECTED];

		/* Here we take the current theme colour and add it to
		 * the colour for white and average the two. This
		 * gives a colour which is inline with the theme but
		 * slightly whiter.
		 */
		color.red = (color.red + (style->white).red) / 2;
		color.green = (color.green + (style->white).green) / 2;
		color.blue = (color.blue + (style->white).blue) / 2;

		g_object_set (cell,
			      "cell-background-gdk", &color,
			      NULL);
	} else {
		g_object_set (cell,
			      "cell-background-gdk", NULL,
			      NULL);
	}
}

static void
contact_list_view_pixbuf_cell_data_func (GtkTreeViewColumn     *tree_column,
					 GtkCellRenderer       *cell,
					 GtkTreeModel          *model,
					 GtkTreeIter           *iter,
					 EmpathyContactListView *view)
{
	gchar    *icon_name;
	gboolean  is_group;
	gboolean  is_active;

	gtk_tree_model_get (model, iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP, &is_group,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_ACTIVE, &is_active,
			    EMPATHY_CONTACT_LIST_STORE_COL_ICON_STATUS, &icon_name,
			    -1);

	g_object_set (cell,
		      "visible", !is_group,
		      "icon-name", icon_name,
		      NULL);

	g_free (icon_name);

	contact_list_view_cell_set_background (view, cell, is_group, is_active);
}

static void
contact_list_view_voip_cell_data_func (GtkTreeViewColumn      *tree_column,
				       GtkCellRenderer        *cell,
				       GtkTreeModel           *model,
				       GtkTreeIter            *iter,
				       EmpathyContactListView *view)
{
	gboolean is_group;
	gboolean is_active;
	gboolean can_voip;

	gtk_tree_model_get (model, iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP, &is_group,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_ACTIVE, &is_active,
			    EMPATHY_CONTACT_LIST_STORE_COL_CAN_VOIP, &can_voip,
			    -1);

	g_object_set (cell,
		      "visible", !is_group && can_voip,
		      "icon-name", EMPATHY_IMAGE_VOIP,
		      NULL);

	contact_list_view_cell_set_background (view, cell, is_group, is_active);
}

static void
contact_list_view_avatar_cell_data_func (GtkTreeViewColumn     *tree_column,
					 GtkCellRenderer       *cell,
					 GtkTreeModel          *model,
					 GtkTreeIter           *iter,
					 EmpathyContactListView *view)
{
	GdkPixbuf *pixbuf;
	gboolean   show_avatar;
	gboolean   is_group;
	gboolean   is_active;

	gtk_tree_model_get (model, iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_PIXBUF_AVATAR, &pixbuf,
			    EMPATHY_CONTACT_LIST_STORE_COL_PIXBUF_AVATAR_VISIBLE, &show_avatar,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP, &is_group,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_ACTIVE, &is_active,
			    -1);

	g_object_set (cell,
		      "visible", !is_group && show_avatar,
		      "pixbuf", pixbuf,
		      NULL);

	if (pixbuf) {
		g_object_unref (pixbuf);
	}

	contact_list_view_cell_set_background (view, cell, is_group, is_active);
}

static void
contact_list_view_text_cell_data_func (GtkTreeViewColumn     *tree_column,
				       GtkCellRenderer       *cell,
				       GtkTreeModel          *model,
				       GtkTreeIter           *iter,
				       EmpathyContactListView *view)
{
	gboolean is_group;
	gboolean is_active;
	gboolean show_status;

	gtk_tree_model_get (model, iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP, &is_group,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_ACTIVE, &is_active,
			    EMPATHY_CONTACT_LIST_STORE_COL_STATUS_VISIBLE, &show_status,
			    -1);

	g_object_set (cell,
		      "show-status", show_status,
		      NULL);

	contact_list_view_cell_set_background (view, cell, is_group, is_active);
}

static void
contact_list_view_expander_cell_data_func (GtkTreeViewColumn     *column,
					   GtkCellRenderer       *cell,
					   GtkTreeModel          *model,
					   GtkTreeIter           *iter,
					   EmpathyContactListView *view)
{
	gboolean is_group;
	gboolean is_active;

	gtk_tree_model_get (model, iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP, &is_group,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_ACTIVE, &is_active,
			    -1);

	if (gtk_tree_model_iter_has_child (model, iter)) {
		GtkTreePath *path;
		gboolean     row_expanded;

		path = gtk_tree_model_get_path (model, iter);
		row_expanded = gtk_tree_view_row_expanded (GTK_TREE_VIEW (column->tree_view), path);
		gtk_tree_path_free (path);

		g_object_set (cell,
			      "visible", TRUE,
			      "expander-style", row_expanded ? GTK_EXPANDER_EXPANDED : GTK_EXPANDER_COLLAPSED,
			      NULL);
	} else {
		g_object_set (cell, "visible", FALSE, NULL);
	}

	contact_list_view_cell_set_background (view, cell, is_group, is_active);
}

static void
contact_list_view_row_expand_or_collapse_cb (EmpathyContactListView *view,
					     GtkTreeIter           *iter,
					     GtkTreePath           *path,
					     gpointer               user_data)
{
	EmpathyContactListViewPriv *priv = GET_PRIV (view);
	GtkTreeModel               *model;
	gchar                      *name;
	gboolean                    expanded;

	if (!(priv->list_features & EMPATHY_CONTACT_LIST_FEATURE_GROUPS_SAVE)) {
		return;
	}

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

	gtk_tree_model_get (model, iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_NAME, &name,
			    -1);

	expanded = GPOINTER_TO_INT (user_data);
	empathy_contact_group_set_expanded (name, expanded);

	g_free (name);
}

static void
contact_list_view_row_has_child_toggled_cb (GtkTreeModel          *model,
					    GtkTreePath           *path,
					    GtkTreeIter           *iter,
					    EmpathyContactListView *view)
{
	EmpathyContactListViewPriv *priv = GET_PRIV (view);
	gboolean  is_group = FALSE;
	gchar    *name = NULL;

	gtk_tree_model_get (model, iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP, &is_group,
			    EMPATHY_CONTACT_LIST_STORE_COL_NAME, &name,
			    -1);

	if (!is_group || EMP_STR_EMPTY (name)) {
		g_free (name);
		return;
	}

	if (!(priv->list_features & EMPATHY_CONTACT_LIST_FEATURE_GROUPS_SAVE) ||
	    empathy_contact_group_get_expanded (name)) {
		g_signal_handlers_block_by_func (view,
						 contact_list_view_row_expand_or_collapse_cb,
						 GINT_TO_POINTER (TRUE));
		gtk_tree_view_expand_row (GTK_TREE_VIEW (view), path, TRUE);
		g_signal_handlers_unblock_by_func (view,
						   contact_list_view_row_expand_or_collapse_cb,
						   GINT_TO_POINTER (TRUE));
	} else {
		g_signal_handlers_block_by_func (view,
						 contact_list_view_row_expand_or_collapse_cb,
						 GINT_TO_POINTER (FALSE));
		gtk_tree_view_collapse_row (GTK_TREE_VIEW (view), path);
		g_signal_handlers_unblock_by_func (view,
						   contact_list_view_row_expand_or_collapse_cb,
						   GINT_TO_POINTER (FALSE));
	}

	g_free (name);
}

static void
contact_list_view_setup (EmpathyContactListView *view)
{
	EmpathyContactListViewPriv *priv;
	GtkCellRenderer           *cell;
	GtkTreeViewColumn         *col;
	gint                       i;

	priv = GET_PRIV (view);

	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (view),
					     empathy_contact_list_store_search_equal_func,
					     NULL, NULL);

	g_signal_connect (priv->store, "row-has-child-toggled",
			  G_CALLBACK (contact_list_view_row_has_child_toggled_cb),
			  view);
	gtk_tree_view_set_model (GTK_TREE_VIEW (view),
				 GTK_TREE_MODEL (priv->store));

	/* Setup view */
	g_object_set (view,
		      "headers-visible", FALSE,
		      "reorderable", TRUE,
		      "show-expanders", FALSE,
		      NULL);

	col = gtk_tree_view_column_new ();

	/* State */
	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (
		col, cell,
		(GtkTreeCellDataFunc) contact_list_view_pixbuf_cell_data_func,
		view, NULL);

	g_object_set (cell,
		      "xpad", 5,
		      "ypad", 1,
		      "visible", FALSE,
		      NULL);

	/* Name */
	cell = empathy_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (
		col, cell,
		(GtkTreeCellDataFunc) contact_list_view_text_cell_data_func,
		view, NULL);

	gtk_tree_view_column_add_attribute (col, cell,
					    "name", EMPATHY_CONTACT_LIST_STORE_COL_NAME);
	gtk_tree_view_column_add_attribute (col, cell,
					    "status", EMPATHY_CONTACT_LIST_STORE_COL_STATUS);
	gtk_tree_view_column_add_attribute (col, cell,
					    "is_group", EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP);

	/* Voip Capability Icon */
	cell = empathy_cell_renderer_activatable_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (
		col, cell,
		(GtkTreeCellDataFunc) contact_list_view_voip_cell_data_func,
		view, NULL);

	g_object_set (cell,
		      "visible", FALSE,
		      NULL);

	g_signal_connect (cell, "path-activated",
			  G_CALLBACK (contact_list_view_voip_activated_cb),
			  view);

	/* Avatar */
	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (
		col, cell,
		(GtkTreeCellDataFunc) contact_list_view_avatar_cell_data_func,
		view, NULL);

	g_object_set (cell,
		      "xpad", 0,
		      "ypad", 0,
		      "visible", FALSE,
		      "width", 32,
		      "height", 32,
		      NULL);

	/* Expander */
	cell = empathy_cell_renderer_expander_new ();
	gtk_tree_view_column_pack_end (col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (
		col, cell,
		(GtkTreeCellDataFunc) contact_list_view_expander_cell_data_func,
		view, NULL);

	/* Actually add the column now we have added all cell renderers */
	gtk_tree_view_append_column (GTK_TREE_VIEW (view), col);

	/* Drag & Drop. */
	for (i = 0; i < G_N_ELEMENTS (drag_types_dest); ++i) {
		drag_atoms_dest[i] = gdk_atom_intern (drag_types_dest[i].target,
						      FALSE);
	}

	for (i = 0; i < G_N_ELEMENTS (drag_types_source); ++i) {
		drag_atoms_source[i] = gdk_atom_intern (drag_types_source[i].target,
							FALSE);
	}
}

static void
contact_list_view_set_list_features (EmpathyContactListView         *view,
				     EmpathyContactListFeatureFlags  features)
{
	EmpathyContactListViewPriv *priv = GET_PRIV (view);
	gboolean                    has_tooltip;

	g_return_if_fail (EMPATHY_IS_CONTACT_LIST_VIEW (view));

	priv->list_features = features;

	/* Update DnD source/dest */
	if (features & EMPATHY_CONTACT_LIST_FEATURE_CONTACT_DRAG) {
		gtk_drag_source_set (GTK_WIDGET (view),
				     GDK_BUTTON1_MASK,
				     drag_types_source,
				     G_N_ELEMENTS (drag_types_source),
				     GDK_ACTION_MOVE | GDK_ACTION_COPY);
	} else {
		gtk_drag_source_unset (GTK_WIDGET (view));

	}

	if (features & EMPATHY_CONTACT_LIST_FEATURE_CONTACT_DROP) {
		gtk_drag_dest_set (GTK_WIDGET (view),
				   GTK_DEST_DEFAULT_ALL,
				   drag_types_dest,
				   G_N_ELEMENTS (drag_types_dest),
				   GDK_ACTION_MOVE | GDK_ACTION_COPY);
	} else {
		/* FIXME: URI could still be droped depending on FT feature */
		gtk_drag_dest_unset (GTK_WIDGET (view));
	}

	/* Update has-tooltip */
	has_tooltip = (features & EMPATHY_CONTACT_LIST_FEATURE_CONTACT_TOOLTIP) != 0;
	gtk_widget_set_has_tooltip (GTK_WIDGET (view), has_tooltip);
}

static void
contact_list_view_finalize (GObject *object)
{
	EmpathyContactListViewPriv *priv;

	priv = GET_PRIV (object);

	if (priv->store) {
		g_object_unref (priv->store);
	}
	if (priv->tooltip_widget) {
		gtk_widget_destroy (priv->tooltip_widget);
	}

	G_OBJECT_CLASS (empathy_contact_list_view_parent_class)->finalize (object);
}

static void
contact_list_view_get_property (GObject    *object,
				guint       param_id,
				GValue     *value,
				GParamSpec *pspec)
{
	EmpathyContactListViewPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_STORE:
		g_value_set_object (value, priv->store);
		break;
	case PROP_LIST_FEATURES:
		g_value_set_flags (value, priv->list_features);
		break;
	case PROP_CONTACT_FEATURES:
		g_value_set_flags (value, priv->contact_features);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
contact_list_view_set_property (GObject      *object,
				guint         param_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	EmpathyContactListView     *view = EMPATHY_CONTACT_LIST_VIEW (object);
	EmpathyContactListViewPriv *priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_STORE:
		priv->store = g_value_dup_object (value);
		contact_list_view_setup (view);
		break;
	case PROP_LIST_FEATURES:
		contact_list_view_set_list_features (view, g_value_get_flags (value));
		break;
	case PROP_CONTACT_FEATURES:
		priv->contact_features = g_value_get_flags (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
empathy_contact_list_view_class_init (EmpathyContactListViewClass *klass)
{
	GObjectClass     *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass   *widget_class = GTK_WIDGET_CLASS (klass);
	GtkTreeViewClass *tree_view_class = GTK_TREE_VIEW_CLASS (klass);

	object_class->finalize = contact_list_view_finalize;
	object_class->get_property = contact_list_view_get_property;
	object_class->set_property = contact_list_view_set_property;

	widget_class->drag_data_received = contact_list_view_drag_data_received;
	widget_class->drag_drop          = contact_list_view_drag_drop;
	widget_class->drag_begin         = contact_list_view_drag_begin;
	widget_class->drag_data_get      = contact_list_view_drag_data_get;
	widget_class->drag_end           = contact_list_view_drag_end;
	widget_class->drag_motion        = contact_list_view_drag_motion;

	/* We use the class method to let user of this widget to connect to
	 * the signal and stop emission of the signal so the default handler
	 * won't be called. */
	tree_view_class->row_activated = contact_list_view_row_activated;

	signals[DRAG_CONTACT_RECEIVED] =
		g_signal_new ("drag-contact-received",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      _empathy_gtk_marshal_VOID__OBJECT_STRING_STRING,
			      G_TYPE_NONE,
			      3, EMPATHY_TYPE_CONTACT, G_TYPE_STRING, G_TYPE_STRING);

	g_object_class_install_property (object_class,
					 PROP_STORE,
					 g_param_spec_object ("store",
							     "The store of the view",
							     "The store of the view",
							      EMPATHY_TYPE_CONTACT_LIST_STORE,
							      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_LIST_FEATURES,
					 g_param_spec_flags ("list-features",
							     "Features of the view",
							     "Falgs for all enabled features",
							      EMPATHY_TYPE_CONTACT_LIST_FEATURE_FLAGS,
							      EMPATHY_CONTACT_LIST_FEATURE_NONE,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_CONTACT_FEATURES,
					 g_param_spec_flags ("contact-features",
							     "Features of the contact menu",
							     "Falgs for all enabled features for the menu",
							      EMPATHY_TYPE_CONTACT_FEATURE_FLAGS,
							      EMPATHY_CONTACT_FEATURE_NONE,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EmpathyContactListViewPriv));
}

static void
empathy_contact_list_view_init (EmpathyContactListView *view)
{
	EmpathyContactListViewPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (view,
		EMPATHY_TYPE_CONTACT_LIST_VIEW, EmpathyContactListViewPriv);

	view->priv = priv;
	/* Get saved group states. */
	empathy_contact_groups_get_all ();

	gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (view), 
					      empathy_contact_list_store_row_separator_func,
					      NULL, NULL);

	/* Connect to tree view signals rather than override. */
	g_signal_connect (view, "button-press-event",
			  G_CALLBACK (contact_list_view_button_press_event_cb),
			  NULL);
	g_signal_connect (view, "key-press-event",
			  G_CALLBACK (contact_list_view_key_press_event_cb),
			  NULL);
	g_signal_connect (view, "row-expanded",
			  G_CALLBACK (contact_list_view_row_expand_or_collapse_cb),
			  GINT_TO_POINTER (TRUE));
	g_signal_connect (view, "row-collapsed",
			  G_CALLBACK (contact_list_view_row_expand_or_collapse_cb),
			  GINT_TO_POINTER (FALSE));
	g_signal_connect (view, "query-tooltip",
			  G_CALLBACK (contact_list_view_query_tooltip_cb),
			  NULL);
}

EmpathyContactListView *
empathy_contact_list_view_new (EmpathyContactListStore        *store,
			       EmpathyContactListFeatureFlags  list_features,
			       EmpathyContactFeatureFlags      contact_features)
{
	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST_STORE (store), NULL);
	
	return g_object_new (EMPATHY_TYPE_CONTACT_LIST_VIEW,
			     "store", store,
			     "contact-features", contact_features,
			     "list-features", list_features,
			     NULL);
}

EmpathyContact *
empathy_contact_list_view_get_selected (EmpathyContactListView *view)
{
	EmpathyContactListViewPriv *priv;
	GtkTreeSelection          *selection;
	GtkTreeIter                iter;
	GtkTreeModel              *model;
	EmpathyContact             *contact;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST_VIEW (view), NULL);

	priv = GET_PRIV (view);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, &contact,
			    -1);

	return contact;
}

gchar *
empathy_contact_list_view_get_selected_group (EmpathyContactListView *view)
{
	EmpathyContactListViewPriv *priv;
	GtkTreeSelection          *selection;
	GtkTreeIter                iter;
	GtkTreeModel              *model;
	gboolean                   is_group;
	gchar                     *name;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST_VIEW (view), NULL);

	priv = GET_PRIV (view);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter,
			    EMPATHY_CONTACT_LIST_STORE_COL_IS_GROUP, &is_group,
			    EMPATHY_CONTACT_LIST_STORE_COL_NAME, &name,
			    -1);

	if (!is_group) {
		g_free (name);
		return NULL;
	}

	return name;
}

static gboolean
contact_list_view_remove_dialog_show (GtkWindow   *parent, 
				      const gchar *message, 
				      const gchar *secondary_text)
{
	GtkWidget *dialog;
	gboolean res;
	
	dialog = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 "%s", message);
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_NO,
				GTK_STOCK_DELETE, GTK_RESPONSE_YES,
				NULL);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  "%s", secondary_text);
	 
	gtk_widget_show (dialog);
	 
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return (res == GTK_RESPONSE_YES);
}

static void
contact_list_view_group_remove_activate_cb (GtkMenuItem            *menuitem,
					    EmpathyContactListView *view)
{
	EmpathyContactListViewPriv *priv = GET_PRIV (view);
	gchar                      *group;

	group = empathy_contact_list_view_get_selected_group (view);
	if (group) {
		gchar     *text;
		GtkWindow *parent;

		text = g_strdup_printf (_("Do you really want to remove the group '%s'?"), group);
		parent = empathy_get_toplevel_window (GTK_WIDGET (view));
		if (contact_list_view_remove_dialog_show (parent, _("Removing group"), text)) {
			EmpathyContactList *list;

			list = empathy_contact_list_store_get_list_iface (priv->store);
			empathy_contact_list_remove_group (list, group);
		}

		g_free (text);
	}

	g_free (group);
}

GtkWidget *
empathy_contact_list_view_get_group_menu (EmpathyContactListView *view)
{
	EmpathyContactListViewPriv *priv = GET_PRIV (view);
	gchar                      *group;
	GtkWidget                  *menu;
	GtkWidget                  *item;
	GtkWidget                  *image;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST_VIEW (view), NULL);

	if (!(priv->list_features & (EMPATHY_CONTACT_LIST_FEATURE_GROUPS_RENAME |
				     EMPATHY_CONTACT_LIST_FEATURE_GROUPS_REMOVE))) {
		return NULL;
	}

	group = empathy_contact_list_view_get_selected_group (view);
	if (!group) {
		return NULL;
	}

	menu = gtk_menu_new ();

	/* FIXME: Not implemented yet
	if (priv->features & EMPATHY_CONTACT_LIST_FEATURE_GROUPS_RENAME) {
		item = gtk_menu_item_new_with_mnemonic (_("Re_name"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
		g_signal_connect (item, "activate",
				  G_CALLBACK (contact_list_view_group_rename_activate_cb),
				  view);
	}*/

	if (priv->list_features & EMPATHY_CONTACT_LIST_FEATURE_GROUPS_REMOVE) {
		item = gtk_image_menu_item_new_with_mnemonic (_("_Remove"));
		image = gtk_image_new_from_icon_name (GTK_STOCK_REMOVE,
						      GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
		g_signal_connect (item, "activate",
				  G_CALLBACK (contact_list_view_group_remove_activate_cb),
				  view);
	}

	g_free (group);

	return menu;
}

static void
contact_list_view_remove_activate_cb (GtkMenuItem            *menuitem,
				      EmpathyContactListView *view)
{
	EmpathyContactListViewPriv *priv = GET_PRIV (view);
	EmpathyContact             *contact;
		
	contact = empathy_contact_list_view_get_selected (view);

	if (contact) {
		gchar     *text; 
		GtkWindow *parent;

		parent = empathy_get_toplevel_window (GTK_WIDGET (view));
		text = g_strdup_printf (_("Do you really want to remove the contact '%s'?"),
					empathy_contact_get_name (contact));						
		if (contact_list_view_remove_dialog_show (parent, _("Removing contact"), text)) {
			EmpathyContactList *list;

			list = empathy_contact_list_store_get_list_iface (priv->store);
			empathy_contact_list_remove (list, contact, 
				_("Sorry, I don't want you in my contact list anymore."));
		}

		g_free (text);
		g_object_unref (contact);
	}
}

GtkWidget *
empathy_contact_list_view_get_contact_menu (EmpathyContactListView *view)
{
	EmpathyContactListViewPriv *priv = GET_PRIV (view);
	EmpathyContact             *contact;
	GtkWidget                  *menu;
	GtkWidget                  *item;
	GtkWidget                  *image;

	g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST_VIEW (view), NULL);

	contact = empathy_contact_list_view_get_selected (view);
	if (!contact) {
		return NULL;
	}

	menu = empathy_contact_menu_new (contact, priv->contact_features);

	if (!(priv->list_features & EMPATHY_CONTACT_LIST_FEATURE_CONTACT_REMOVE)) {
		g_object_unref (contact);
		return menu;
	}

	if (menu) {
		/* Separator */
		item = gtk_separator_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
	} else {
		menu = gtk_menu_new ();
	}

	/* Remove contact */
	if (priv->list_features & EMPATHY_CONTACT_LIST_FEATURE_CONTACT_REMOVE) {
		item = gtk_image_menu_item_new_with_mnemonic (_("_Remove"));
		image = gtk_image_new_from_icon_name (GTK_STOCK_REMOVE,
						      GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
		g_signal_connect (item, "activate",
				  G_CALLBACK (contact_list_view_remove_activate_cb),
				  view);
	}

	g_object_unref (contact);

	return menu;
}

