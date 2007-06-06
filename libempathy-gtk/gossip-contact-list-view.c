/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-contact-list.h>
#include <libempathy/gossip-debug.h>
#include <libempathy/gossip-utils.h>
#include <libempathy/empathy-marshal.h>

#include "gossip-contact-list-view.h"
#include "gossip-contact-list-store.h"
#include "empathy-images.h"
#include "gossip-contact-groups.h"
#include "gossip-cell-renderer-expander.h"
#include "gossip-cell-renderer-text.h"
#include "gossip-ui-utils.h"
#include "empathy-contact-dialogs.h"
//#include "gossip-chat-invite.h"
//#include "gossip-ft-window.h"
//#include "gossip-log-window.h"

#define DEBUG_DOMAIN "ContactListView"

/* Flashing delay for icons (milliseconds). */
#define FLASH_TIMEOUT 500

/* Active users are those which have recently changed state
 * (e.g. online, offline or from normal to a busy state).
 */

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CONTACT_LIST_VIEW, GossipContactListViewPriv))

struct _GossipContactListViewPriv {
	GossipContactListStore *store;
	GtkUIManager           *ui;
	GtkTreeRowReference    *drag_row;
	GtkTreeModel           *filter;
	gchar                  *filter_text;

	GossipContactListViewDragReceivedFunc drag_received;
	gpointer                              drag_received_data;
};

typedef struct {
	GossipContactListView *view;
	GtkTreePath           *path;
	guint                  timeout_id;
} DragMotionData;

typedef struct {
	GossipContactListView *view;
	GossipContact         *contact;
	gboolean               remove;
} ShowActiveData;

static void        gossip_contact_list_view_class_init         (GossipContactListViewClass *klass);
static void        gossip_contact_list_view_init               (GossipContactListView      *list);
static void        contact_list_view_finalize                  (GObject                    *object);
static void        contact_list_view_get_property              (GObject                    *object,
								guint                       param_id,
								GValue                     *value,
								GParamSpec                 *pspec);
static void        contact_list_view_set_property              (GObject                    *object,
								guint                       param_id,
								const GValue               *value,
								GParamSpec                 *pspec);
static void        contact_list_view_setup                     (GossipContactListView      *view);
static void        contact_list_view_row_has_child_toggled_cb  (GtkTreeModel               *model,
								GtkTreePath                *path,
								GtkTreeIter                *iter,
								GossipContactListView      *view);
static void        contact_list_view_contact_received          (GossipContactListView      *view,
								GossipContact              *contact,
								GdkDragAction               action,
								const gchar                *old_group,
								const gchar                *new_group);
static void        contact_list_view_drag_data_received        (GtkWidget                  *widget,
								GdkDragContext             *context,
								gint                        x,
								gint                        y,
								GtkSelectionData           *selection,
								guint                       info,
								guint                       time,
								gpointer                    user_data);
static gboolean    contact_list_view_drag_motion               (GtkWidget                  *widget,
								GdkDragContext             *context,
								gint                        x,
								gint                        y,
								guint                       time,
								gpointer                    data);
static gboolean    contact_list_view_drag_motion_cb            (DragMotionData             *data);
static void        contact_list_view_drag_begin                (GtkWidget                  *widget,
								GdkDragContext             *context,
								gpointer                    user_data);
static void        contact_list_view_drag_data_get             (GtkWidget                  *widget,
								GdkDragContext             *context,
								GtkSelectionData           *selection,
								guint                       info,
								guint                       time,
								gpointer                    user_data);
static void        contact_list_view_drag_end                  (GtkWidget                  *widget,
								GdkDragContext             *context,
								gpointer                    user_data);
static void        contact_list_view_cell_set_background       (GossipContactListView      *view,
								GtkCellRenderer            *cell,
								gboolean                    is_group,
								gboolean                    is_active);
static void        contact_list_view_pixbuf_cell_data_func     (GtkTreeViewColumn          *tree_column,
								GtkCellRenderer            *cell,
								GtkTreeModel               *model,
								GtkTreeIter                *iter,
								GossipContactListView      *view);
static void        contact_list_view_avatar_cell_data_func     (GtkTreeViewColumn          *tree_column,
								GtkCellRenderer            *cell,
								GtkTreeModel               *model,
								GtkTreeIter                *iter,
								GossipContactListView      *view);
static void        contact_list_view_text_cell_data_func       (GtkTreeViewColumn          *tree_column,
								GtkCellRenderer            *cell,
								GtkTreeModel               *model,
								GtkTreeIter                *iter,
								GossipContactListView      *view);
static void        contact_list_view_expander_cell_data_func   (GtkTreeViewColumn          *column,
								GtkCellRenderer            *cell,
								GtkTreeModel               *model,
								GtkTreeIter                *iter,
								GossipContactListView      *view);
static GtkWidget * contact_list_view_get_contact_menu          (GossipContactListView      *view,
								gboolean                    can_send_file,
								gboolean                    can_show_log);
static gboolean    contact_list_view_button_press_event_cb     (GossipContactListView      *view,
								GdkEventButton             *event,
								gpointer                    user_data);
static void        contact_list_view_row_activated_cb          (GossipContactListView      *view,
								GtkTreePath                *path,
								GtkTreeViewColumn          *col,
								gpointer                    user_data);
static void        contact_list_view_row_expand_or_collapse_cb (GossipContactListView      *view,
								GtkTreeIter                *iter,
								GtkTreePath                *path,
								gpointer                    user_data);
static gboolean    contact_list_view_filter_show_contact       (GossipContact              *contact,
								const gchar                *filter);
static gboolean    contact_list_view_filter_show_group         (GossipContactListView      *view,
								const gchar                *group,
								const gchar                *filter);
static gboolean    contact_list_view_filter_func               (GtkTreeModel               *model,
								GtkTreeIter                *iter,
								GossipContactListView      *view);
static void        contact_list_view_action_cb                 (GtkAction                  *action,
								GossipContactListView      *view);
static void        contact_list_view_action_activated          (GossipContactListView      *view,
								GossipContact              *contact);

enum {
	PROP_0,
	PROP_FILTER,
};

static const GtkActionEntry entries[] = {
	{ "ContactMenu", NULL,
	  N_("_Contact"), NULL, NULL,
	  NULL
	},
	{ "GroupMenu", NULL,
	  N_("_Group"),NULL, NULL,
	  NULL
	},
	{ "Chat", EMPATHY_IMAGE_MESSAGE,
	  N_("_Chat"), NULL, N_("Chat with contact"),
	  G_CALLBACK (contact_list_view_action_cb)
	},
	{ "Information", EMPATHY_IMAGE_CONTACT_INFORMATION,
	  N_("Infor_mation"), "<control>I", N_("View contact information"),
	  G_CALLBACK (contact_list_view_action_cb)
	},
	{ "Rename", NULL,
	  N_("Re_name"), NULL, N_("Rename"),
	  G_CALLBACK (contact_list_view_action_cb)
	},
	{ "Edit", GTK_STOCK_EDIT,
	  N_("_Edit"), NULL, N_("Edit the groups and name for this contact"),
	  G_CALLBACK (contact_list_view_action_cb)
	},
	{ "Remove", GTK_STOCK_REMOVE,
	  N_("_Remove"), NULL, N_("Remove contact"),
	  G_CALLBACK (contact_list_view_action_cb)
	},
	{ "Invite", EMPATHY_IMAGE_GROUP_MESSAGE,
	  N_("_Invite to Chat Room"), NULL, N_("Invite to a currently open chat room"),
	  G_CALLBACK (contact_list_view_action_cb)
	},
	{ "SendFile", NULL,
	  N_("_Send File..."), NULL, N_("Send a file"),
	  G_CALLBACK (contact_list_view_action_cb)
	},
	{ "Log", GTK_STOCK_JUSTIFY_LEFT,
	  N_("_View Previous Conversations"), NULL, N_("View previous conversations with this contact"),
	  G_CALLBACK (contact_list_view_action_cb)
	},
};

static guint n_entries = G_N_ELEMENTS (entries);

static const gchar *ui_info =
	"<ui>"
	"  <popup name='Contact'>"
	"    <menuitem action='Chat'/>"
	"    <menuitem action='Log'/>"
	"    <menuitem action='SendFile'/>"
	"    <separator/>"
	"    <menuitem action='Invite'/>"
	"    <separator/>"
	"    <menuitem action='Edit'/>"
	"    <menuitem action='Remove'/>"
	"    <separator/>"
	"    <menuitem action='Information'/>"
	"  </popup>"
	"  <popup name='Group'>"
	"    <menuitem action='Rename'/>"
	"  </popup>"
	"</ui>";

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

G_DEFINE_TYPE (GossipContactListView, gossip_contact_list_view, GTK_TYPE_TREE_VIEW);

static void
gossip_contact_list_view_class_init (GossipContactListViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = contact_list_view_finalize;
	object_class->get_property = contact_list_view_get_property;
	object_class->set_property = contact_list_view_set_property;

	signals[DRAG_CONTACT_RECEIVED] =
		g_signal_new ("drag-contact-received",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      empathy_marshal_VOID__OBJECT_STRING_STRING,
			      G_TYPE_NONE,
			      3, GOSSIP_TYPE_CONTACT, G_TYPE_STRING, G_TYPE_STRING);

	g_object_class_install_property (object_class,
					 PROP_FILTER,
					 g_param_spec_string ("filter",
							      "Filter",
							      "The text to use to filter the contact list",
							      NULL,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipContactListViewPriv));
}

static void
gossip_contact_list_view_init (GossipContactListView *view)
{
	GossipContactListViewPriv *priv;
	GtkActionGroup            *action_group;
	GError                    *error = NULL;

	priv = GET_PRIV (view);

	/* Get saved group states. */
	gossip_contact_groups_get_all ();

	/* Set up UI Manager */
	priv->ui = gtk_ui_manager_new ();

	action_group = gtk_action_group_new ("Actions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group, entries, n_entries, view);
	gtk_ui_manager_insert_action_group (priv->ui, action_group, 0);

	if (!gtk_ui_manager_add_ui_from_string (priv->ui, ui_info, -1, &error)) {
		g_warning ("Could not build contact menus from string:'%s'", error->message);
		g_error_free (error);
	}

	g_object_unref (action_group);

	gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (view), 
					      gossip_contact_list_store_row_separator_func,
					      NULL, NULL);

	/* Connect to tree view signals rather than override. */
	g_signal_connect (view,
			  "button-press-event",
			  G_CALLBACK (contact_list_view_button_press_event_cb),
			  NULL);
	g_signal_connect (view,
			  "row-activated",
			  G_CALLBACK (contact_list_view_row_activated_cb),
			  NULL);
	g_signal_connect (view,
			  "row-expanded",
			  G_CALLBACK (contact_list_view_row_expand_or_collapse_cb),
			  GINT_TO_POINTER (TRUE));
	g_signal_connect (view,
			  "row-collapsed",
			  G_CALLBACK (contact_list_view_row_expand_or_collapse_cb),
			  GINT_TO_POINTER (FALSE));
}

static void
contact_list_view_finalize (GObject *object)
{
	GossipContactListViewPriv *priv;

	priv = GET_PRIV (object);

	if (priv->ui) {
		g_object_unref (priv->ui);
	}
	if (priv->store) {
		g_object_unref (priv->store);
	}
	if (priv->filter) {
		g_object_unref (priv->filter);
	}
	g_free (priv->filter_text);

	G_OBJECT_CLASS (gossip_contact_list_view_parent_class)->finalize (object);
}

static void
contact_list_view_get_property (GObject    *object,
				guint       param_id,
				GValue     *value,
				GParamSpec *pspec)
{
	GossipContactListViewPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_FILTER:
		g_value_set_string (value, priv->filter_text);
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
	GossipContactListViewPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_FILTER:
		gossip_contact_list_view_set_filter (GOSSIP_CONTACT_LIST_VIEW (object),
						g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

GossipContactListView *
gossip_contact_list_view_new (GossipContactListStore *store)
{
	GossipContactListViewPriv *priv;
	GossipContactListView     *view;
	
	view = g_object_new (GOSSIP_TYPE_CONTACT_LIST_VIEW, NULL);
	priv = GET_PRIV (view);

	priv->store = g_object_ref (store);
	contact_list_view_setup (view);

	return view;
}

GossipContact *
gossip_contact_list_view_get_selected (GossipContactListView *view)
{
	GossipContactListViewPriv *priv;
	GtkTreeSelection          *selection;
	GtkTreeIter                iter;
	GtkTreeModel              *model;
	GossipContact             *contact;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST_VIEW (view), NULL);

	priv = GET_PRIV (view);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter, COL_CONTACT, &contact, -1);

	return contact;
}

gchar *
gossip_contact_list_view_get_selected_group (GossipContactListView *view)
{
	GossipContactListViewPriv *priv;
	GtkTreeSelection          *selection;
	GtkTreeIter                iter;
	GtkTreeModel              *model;
	gboolean                   is_group;
	gchar                     *name;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST_VIEW (view), NULL);

	priv = GET_PRIV (view);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter,
			    COL_IS_GROUP, &is_group,
			    COL_NAME, &name,
			    -1);

	if (!is_group) {
		g_free (name);
		return NULL;
	}

	return name;
}

GtkWidget *
gossip_contact_list_view_get_group_menu (GossipContactListView *view)
{
	GossipContactListViewPriv *priv;
	GtkWidget                 *widget;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST_VIEW (view), NULL);

	priv = GET_PRIV (view);

	widget = gtk_ui_manager_get_widget (priv->ui, "/Group");

	return widget;
}

GtkWidget *
gossip_contact_list_view_get_contact_menu (GossipContactListView *view,
					   GossipContact         *contact)
{
	gboolean can_show_log;
	gboolean can_send_file;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST_VIEW (view), NULL);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	can_show_log = FALSE; /* FIXME: gossip_log_exists_for_contact (contact); */
	can_send_file = FALSE;

	return contact_list_view_get_contact_menu (view,
						   can_send_file,
						   can_show_log);
}

void
gossip_contact_list_view_set_filter (GossipContactListView *view,
				     const gchar           *filter)
{
	GossipContactListViewPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT_LIST_VIEW (view));

	priv = GET_PRIV (view);

	g_free (priv->filter_text);
	if (filter) {
		priv->filter_text = g_utf8_casefold (filter, -1);
	} else {
		priv->filter_text = NULL;
	}

	gossip_debug (DEBUG_DOMAIN, "Refiltering with filter:'%s' (case folded)", filter);
	gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter));
}

void
gossip_contact_list_view_set_drag_received_func (GossipContactListView                 *view,
						 GossipContactListViewDragReceivedFunc  func,
						 gpointer                               user_data)
{
	GossipContactListViewPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT_LIST_VIEW (view));

	priv = GET_PRIV (view);

	if (func) {
		priv->drag_received = func;
		priv->drag_received_data = user_data;
	} else {
		priv->drag_received = NULL;
		priv->drag_received_data = NULL;
	}
}

static void
contact_list_view_setup (GossipContactListView *view)
{
	GossipContactListViewPriv *priv;
	GtkCellRenderer           *cell;
	GtkTreeViewColumn         *col;
	gint                       i;

	priv = GET_PRIV (view);

	/* Create filter */
	priv->filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (priv->store), NULL);

	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (priv->filter),
						(GtkTreeModelFilterVisibleFunc)
						contact_list_view_filter_func,
						view, NULL);
	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (view),
					     gossip_contact_list_store_search_equal_func,
					     view, NULL);
	g_signal_connect (priv->filter, "row-has-child-toggled",
			  G_CALLBACK (contact_list_view_row_has_child_toggled_cb),
			  view);
	gtk_tree_view_set_model (GTK_TREE_VIEW (view), priv->filter);


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
	cell = gossip_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (
		col, cell,
		(GtkTreeCellDataFunc) contact_list_view_text_cell_data_func,
		view, NULL);

	gtk_tree_view_column_add_attribute (col, cell,
					    "name", COL_NAME);
	gtk_tree_view_column_add_attribute (col, cell,
					    "status", COL_STATUS);
	gtk_tree_view_column_add_attribute (col, cell,
					    "is_group", COL_IS_GROUP);

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
	cell = gossip_cell_renderer_expander_new ();
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

	/* Note: We support the COPY action too, but need to make the
	 * MOVE action the default.
	 */
	gtk_drag_source_set (GTK_WIDGET (view),
			     GDK_BUTTON1_MASK,
			     drag_types_source,
			     G_N_ELEMENTS (drag_types_source),
			     GDK_ACTION_MOVE);

	gtk_drag_dest_set (GTK_WIDGET (view),
			   GTK_DEST_DEFAULT_ALL,
			   drag_types_dest,
			   G_N_ELEMENTS (drag_types_dest),
			   GDK_ACTION_MOVE | GDK_ACTION_LINK);

	g_signal_connect (GTK_WIDGET (view),
			  "drag-data-received",
			  G_CALLBACK (contact_list_view_drag_data_received),
			  NULL);

	/* FIXME: noticed but when you drag the row over the treeview
	 * fast, it seems to stop redrawing itself, if we don't
	 * connect this signal, all is fine.
	 */
	g_signal_connect (view,
			  "drag-motion",
			  G_CALLBACK (contact_list_view_drag_motion),
			  NULL);

	g_signal_connect (view,
			  "drag-begin",
			  G_CALLBACK (contact_list_view_drag_begin),
			  NULL);
	g_signal_connect (view,
			  "drag-data-get",
			  G_CALLBACK (contact_list_view_drag_data_get),
			  NULL);
	g_signal_connect (view,
			  "drag-end",
			  G_CALLBACK (contact_list_view_drag_end),
			  NULL);
}

static void
contact_list_view_row_has_child_toggled_cb (GtkTreeModel          *model,
					    GtkTreePath           *path,
					    GtkTreeIter           *iter,
					    GossipContactListView *view)
{
	gboolean  is_group = FALSE;
	gchar    *name = NULL;

	gtk_tree_model_get (model, iter,
			    COL_IS_GROUP, &is_group,
			    COL_NAME, &name,
			    -1);

	if (!is_group || G_STR_EMPTY (name)) {
		g_free (name);
		return;
	}

	if (gossip_contact_group_get_expanded (name)) {
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
contact_list_view_contact_received (GossipContactListView *view,
				    GossipContact         *contact,
				    GdkDragAction          action,
				    const gchar           *old_group,
				    const gchar           *new_group)
{
	GossipContactListViewPriv *priv;
	GList                     *groups, *l;
	GList                     *new_groups_list = NULL;

	priv = GET_PRIV (view);

	groups = gossip_contact_get_groups (contact);
	for (l = groups; l; l = l->next) {
		gchar *str;

		str = l->data;

		if (action == GDK_ACTION_MOVE &&
		    old_group != NULL &&
		    strcmp (str, old_group) == 0) {
			continue;
		}

		if (new_group && strcmp (str, new_group) == 0) {
			/* Otherwise we set it twice */
			continue;
		}

		new_groups_list = g_list_prepend (new_groups_list, g_strdup (str));
	}

	if (new_group) {
		new_groups_list = g_list_prepend (new_groups_list, g_strdup (new_group));
	}

	gossip_contact_set_groups (contact, new_groups_list);
}

static void
contact_list_view_drag_data_received (GtkWidget         *widget,
				      GdkDragContext    *context,
				      gint               x,
				      gint               y,
				      GtkSelectionData  *selection,
				      guint              info,
				      guint              time,
				      gpointer           user_data)
{
	GossipContactListViewPriv *priv;
	EmpathyContactList        *list;
	GtkTreeModel              *model;
	GtkTreePath               *path;
	GtkTreeViewDropPosition    position;
	GossipContact             *contact;
	const gchar               *id;
	gchar                     *new_group = NULL;
	gchar                     *old_group = NULL;
	gboolean                   is_row;

	priv = GET_PRIV (widget);

	id = (const gchar*) selection->data;
	gossip_debug (DEBUG_DOMAIN, "Received %s%s drag & drop contact from roster with id:'%s'",
		      context->action == GDK_ACTION_MOVE ? "move" : "",
		      context->action == GDK_ACTION_COPY ? "copy" : "",
		      id);

	/* FIXME: This is ambigous, an id can come from multiple accounts */
	list = gossip_contact_list_store_get_list_iface (priv->store);
	contact = empathy_contact_list_find (list, id);

	if (!contact) {
		gossip_debug (DEBUG_DOMAIN, "No contact found associated with drag & drop");
		return;
	}

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	/* Get source group information. */
	if (priv->drag_row) {
		path = gtk_tree_row_reference_get_path (priv->drag_row);
		if (path) {
			old_group = gossip_contact_list_store_get_parent_group (model, path, NULL);
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
		new_group = gossip_contact_list_store_get_parent_group (model, path, NULL);
		gtk_tree_path_free (path);
	}

	gossip_debug (DEBUG_DOMAIN,
		      "contact '%s' dragged from '%s' to '%s'",
		      gossip_contact_get_name (contact),
		      old_group, new_group);

	if (priv->drag_received) {
		priv->drag_received (contact,
				     context->action,
				     old_group,
				     new_group,
				     priv->drag_received_data);
	} else {
		contact_list_view_contact_received (GOSSIP_CONTACT_LIST_VIEW (widget),
						    contact,
						    context->action,
						    old_group,
						    new_group);
	}

	g_free (old_group);
	g_free (new_group);

	gtk_drag_finish (context, TRUE, FALSE, GDK_CURRENT_TIME);
}

static gboolean
contact_list_view_drag_motion (GtkWidget      *widget,
			       GdkDragContext *context,
			       gint            x,
			       gint            y,
			       guint           time,
			       gpointer        data)
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

		dm->view = GOSSIP_CONTACT_LIST_VIEW (widget);
		dm->path = gtk_tree_path_copy (path);

		dm->timeout_id = g_timeout_add (
			1500,
			(GSourceFunc) contact_list_view_drag_motion_cb,
			dm);
	}

	return TRUE;
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

static void
contact_list_view_drag_begin (GtkWidget      *widget,
			      GdkDragContext *context,
			      gpointer        user_data)
{
	GossipContactListViewPriv *priv;
	GtkTreeSelection          *selection;
	GtkTreeModel              *model;
	GtkTreePath               *path;
	GtkTreeIter                iter;

	priv = GET_PRIV (widget);

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
				 guint             time,
				 gpointer          user_data)
{
	GossipContactListViewPriv *priv;
	GtkTreePath               *src_path;
	GtkTreeIter                iter;
	GtkTreeModel              *model;
	GossipContact             *contact;
	const gchar               *id;

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

	contact = gossip_contact_list_view_get_selected (GOSSIP_CONTACT_LIST_VIEW (widget));
	if (!contact) {
		return;
	}

	id = gossip_contact_get_id (contact);
	g_object_unref (contact);

	switch (info) {
	case DND_DRAG_TYPE_CONTACT_ID:
		gtk_selection_data_set (selection, drag_atoms_source[info], 8,
					(guchar*)id, strlen (id) + 1);
		break;

	default:
		return;
	}
}

static void
contact_list_view_drag_end (GtkWidget      *widget,
			    GdkDragContext *context,
			    gpointer        user_data)
{
	GossipContactListViewPriv *priv;

	priv = GET_PRIV (widget);

	if (priv->drag_row) {
		gtk_tree_row_reference_free (priv->drag_row);
		priv->drag_row = NULL;
	}
}

static void
contact_list_view_cell_set_background (GossipContactListView *view,
				       GtkCellRenderer       *cell,
				       gboolean               is_group,
				       gboolean               is_active)
{
	GdkColor  color;
	GtkStyle *style;

	style = gtk_widget_get_style (GTK_WIDGET (view));

	if (!is_group) {
		if (is_active) {
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
					 GossipContactListView *view)
{
	gchar    *icon_name;
	gboolean  is_group;
	gboolean  is_active;

	gtk_tree_model_get (model, iter,
			    COL_IS_GROUP, &is_group,
			    COL_IS_ACTIVE, &is_active,
			    COL_ICON_STATUS, &icon_name,
			    -1);

	g_object_set (cell,
		      "visible", !is_group,
		      "icon-name", icon_name,
		      NULL);

	g_free (icon_name);

	contact_list_view_cell_set_background (view, cell, is_group, is_active);
}

static void
contact_list_view_avatar_cell_data_func (GtkTreeViewColumn     *tree_column,
					 GtkCellRenderer       *cell,
					 GtkTreeModel          *model,
					 GtkTreeIter           *iter,
					 GossipContactListView *view)
{
	GdkPixbuf *pixbuf;
	gboolean   show_avatar;
	gboolean   is_group;
	gboolean   is_active;

	gtk_tree_model_get (model, iter,
			    COL_PIXBUF_AVATAR, &pixbuf,
			    COL_PIXBUF_AVATAR_VISIBLE, &show_avatar,
			    COL_IS_GROUP, &is_group,
			    COL_IS_ACTIVE, &is_active,
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
				       GossipContactListView *view)
{
	gboolean is_group;
	gboolean is_active;
	gboolean show_status;

	gtk_tree_model_get (model, iter,
			    COL_IS_GROUP, &is_group,
			    COL_IS_ACTIVE, &is_active,
			    COL_STATUS_VISIBLE, &show_status,
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
					   GossipContactListView *view)
{
	gboolean is_group;
	gboolean is_active;

	gtk_tree_model_get (model, iter,
			    COL_IS_GROUP, &is_group,
			    COL_IS_ACTIVE, &is_active,
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

static GtkWidget *
contact_list_view_get_contact_menu (GossipContactListView *view,
				    gboolean               can_send_file,
				    gboolean               can_show_log)
{
	GossipContactListViewPriv *priv;
	GtkAction                 *action;
	GtkWidget                 *widget;

	priv = GET_PRIV (view);

	/* Sort out sensitive items */
	action = gtk_ui_manager_get_action (priv->ui, "/Contact/Log");
	gtk_action_set_sensitive (action, can_show_log);

	action = gtk_ui_manager_get_action (priv->ui, "/Contact/SendFile");
	gtk_action_set_visible (action, can_send_file);

	widget = gtk_ui_manager_get_widget (priv->ui, "/Contact");

	return widget;
}

static gboolean
contact_list_view_button_press_event_cb (GossipContactListView *view,
					 GdkEventButton        *event,
					 gpointer               user_data)
{
	GossipContactListViewPriv *priv;
	GossipContact             *contact;
	GtkTreePath               *path;
	GtkTreeSelection          *selection;
	GtkTreeModel              *model;
	GtkTreeIter                iter;
	gboolean                   row_exists;
	GtkWidget                 *menu;

	if (event->button != 3) {
		return FALSE;
	}

	priv = GET_PRIV (view);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

	gtk_widget_grab_focus (GTK_WIDGET (view));

	row_exists = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (view),
						    event->x, event->y,
						    &path,
						    NULL, NULL, NULL);
	if (!row_exists) {
		return FALSE;
	}

	gtk_tree_selection_unselect_all (selection);
	gtk_tree_selection_select_path (selection, path);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	gtk_tree_model_get (model, &iter, COL_CONTACT, &contact, -1);

	if (contact) {
		menu = gossip_contact_list_view_get_contact_menu (view, contact);
		g_object_unref (contact);
	} else {
		menu = gossip_contact_list_view_get_group_menu (view);
	}

	if (!menu) {
		return FALSE;
	}

	gtk_widget_show (menu);

	gtk_menu_popup (GTK_MENU (menu),
			NULL, NULL, NULL, NULL,
			event->button, event->time);

	return TRUE;
}

static void
contact_list_view_row_activated_cb (GossipContactListView *view,
				    GtkTreePath           *path,
				    GtkTreeViewColumn     *col,
				    gpointer               user_data)
{
	GossipContact *contact;
	GtkTreeModel  *model;
	GtkTreeIter    iter;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COL_CONTACT, &contact, -1);

	if (contact) {
		contact_list_view_action_activated (view, contact);
		g_object_unref (contact);
	}
}

static void
contact_list_view_row_expand_or_collapse_cb (GossipContactListView *view,
					     GtkTreeIter           *iter,
					     GtkTreePath           *path,
					     gpointer               user_data)
{
	GtkTreeModel *model;
	gchar        *name;
	gboolean      expanded;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));

	gtk_tree_model_get (model, iter,
			    COL_NAME, &name,
			    -1);

	expanded = GPOINTER_TO_INT (user_data);
	gossip_contact_group_set_expanded (name, expanded);

	g_free (name);
}

static gboolean 
contact_list_view_filter_show_contact (GossipContact *contact,
				       const gchar   *filter)
{
	gchar    *str;
	gboolean  visible;

	/* Check contact id */
	str = g_utf8_casefold (gossip_contact_get_id (contact), -1);
	visible = G_STR_EMPTY (str) || strstr (str, filter);
	g_free (str);

	if (visible) {
		return TRUE;
	}

	/* Check contact name */
	str = g_utf8_casefold (gossip_contact_get_name (contact), -1);
	visible = G_STR_EMPTY (str) || strstr (str, filter);
	g_free (str);
	
	return visible;
}

static gboolean
contact_list_view_filter_show_group (GossipContactListView *view,
				     const gchar           *group,
				     const gchar           *filter)
{
	GossipContactListViewPriv *priv;
	EmpathyContactList        *list;
	GList                     *contacts, *l;
	gchar                     *str;
	gboolean                   show_group = FALSE;

	priv = GET_PRIV (view);
	
	str = g_utf8_casefold (group, -1);
	if (!str) {
		return FALSE;
	}

	/* If the filter is the partially the group name, we show the
	 * whole group.
	 */
	if (strstr (str, filter)) {
		g_free (str);
		return TRUE;
	}

	/* At this point, we need to check in advance if this
	 * group should be shown because a contact we want to
	 * show exists in it.
	 */
	list = gossip_contact_list_store_get_list_iface (priv->store);
	contacts = empathy_contact_list_get_members (list);
	for (l = contacts; l && !show_group; l = l->next) {
		if (!gossip_contact_is_in_group (l->data, group)) {
			continue;
		}

		if (contact_list_view_filter_show_contact (l->data, filter)) {
			show_group = TRUE;
		}
	}
	g_list_foreach (contacts, (GFunc) g_object_unref, NULL);
	g_list_free (contacts);
	g_free (str);

	return show_group;
}

static gboolean
contact_list_view_filter_func (GtkTreeModel          *model,
			       GtkTreeIter           *iter,
			       GossipContactListView *view)
{
	GossipContactListViewPriv *priv;
	gboolean                   is_group;
	gboolean                   is_separator;
	gboolean                   visible = TRUE;

	priv = GET_PRIV (view);

	if (G_STR_EMPTY (priv->filter_text)) {
		return TRUE;
	}
	
	/* Check to see if iter matches any group names */
	gtk_tree_model_get (model, iter,
			    COL_IS_GROUP, &is_group,
			    COL_IS_SEPARATOR, &is_separator,
			    -1);

	if (is_group) {
		gchar *name;

		gtk_tree_model_get (model, iter, COL_NAME, &name, -1);
		visible &= contact_list_view_filter_show_group (view,
								name,
								priv->filter_text);
		g_free (name);
	} else if (is_separator) {
		/* Do nothing here */
	} else {
		GossipContact *contact;

		/* Check contact id */
		gtk_tree_model_get (model, iter, COL_CONTACT, &contact, -1);
		visible &= contact_list_view_filter_show_contact (contact, 
								  priv->filter_text);
		g_object_unref (contact);
	}

	return visible;
}

static void
contact_list_view_action_cb (GtkAction             *action,
			     GossipContactListView *view)
{
	GossipContactListViewPriv *priv;
	GossipContact             *contact;
	const gchar               *name;
	gchar                     *group;
	GtkWindow                 *parent;

	priv = GET_PRIV (view);

	name = gtk_action_get_name (action);
	if (!name) {
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Action:'%s' activated", name);

	contact = gossip_contact_list_view_get_selected (view);
	group = gossip_contact_list_view_get_selected_group (view);
	parent = gossip_get_toplevel_window (GTK_WIDGET (view));

	if (contact && strcmp (name, "Chat") == 0) {
		contact_list_view_action_activated (view, contact);
	}
	else if (contact && strcmp (name, "Information") == 0) {
		empathy_contact_information_dialog_show (contact, parent, FALSE);
	}
	else if (contact && strcmp (name, "Edit") == 0) {
		empathy_contact_information_dialog_show (contact, parent, TRUE);
	}
	else if (contact && strcmp (name, "Remove") == 0) {
		/* FIXME: Ask for confirmation */
		EmpathyContactList *list;

		list = gossip_contact_list_store_get_list_iface (priv->store);
		empathy_contact_list_remove (list, contact,
					     _("Sorry, I don't want you in my contact list anymore."));
	}
	else if (contact && strcmp (name, "Invite") == 0) {
	}
	else if (contact && strcmp (name, "SendFile") == 0) {
	}
	else if (contact && strcmp (name, "Log") == 0) {
	}
	else if (group && strcmp (name, "Rename") == 0) {
	}

	g_free (group);
	if (contact) {
		g_object_unref (contact);
	}
}

static void
contact_list_view_action_activated (GossipContactListView *view,
				    GossipContact         *contact)
{
	MissionControl *mc;

	mc = gossip_mission_control_new ();
	mission_control_request_channel (mc,
					 gossip_contact_get_account (contact),
					 TP_IFACE_CHANNEL_TYPE_TEXT,
					 gossip_contact_get_handle (contact),
					 TP_HANDLE_TYPE_CONTACT,
					 NULL, NULL);
	g_object_unref (mc);
}

