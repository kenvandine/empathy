/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libtelepathy/tp-helpers.h>

#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-contact-manager.h>
#include <libempathy/gossip-debug.h>

#include "gossip-contact-list.h"
#include "gossip-contact-groups.h"
#include "gossip-cell-renderer-expander.h"
#include "gossip-cell-renderer-text.h"
#include "gossip-stock.h"
#include "gossip-ui-utils.h"
//#include "gossip-chat-invite.h"
//#include "gossip-contact-info-dialog.h"
//#include "gossip-edit-contact-dialog.h"
//#include "gossip-ft-window.h"
//#include "gossip-log-window.h"

#define DEBUG_DOMAIN "ContactListUI"

/* Flashing delay for icons (milliseconds). */
#define FLASH_TIMEOUT 500

/* Active users are those which have recently changed state
 * (e.g. online, offline or from normal to a busy state).
 */

/* Time user is shown as active */
#define ACTIVE_USER_SHOW_TIME 7000

/* Time after connecting which we wait before active users are enabled */
#define ACTIVE_USER_WAIT_TO_ENABLE_TIME 5000

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CONTACT_LIST, GossipContactListPriv))

struct _GossipContactListPriv {
	EmpathyContactManager *manager;

	GHashTable            *groups;

	GtkUIManager          *ui;
	GtkTreeRowReference   *drag_row;

	GtkTreeStore          *store;
	GtkTreeModel          *filter;
	gchar                 *filter_text;

	gboolean               show_offline;
	gboolean               show_avatars;
	gboolean               is_compact;
	gboolean               show_active;
};

typedef struct {
	GtkTreeIter  iter;
	const gchar *name;
	gboolean     found;
} FindGroup;

typedef struct {
	GossipContact *contact;
	gboolean       found;
	GList         *iters;
} FindContact;

typedef struct {
	GossipContactList *list;
	GtkTreePath       *path;
	guint              timeout_id;
} DragMotionData;

typedef struct {
	GossipContactList *list;
	GossipContact     *contact;
	gboolean           remove;
} ShowActiveData;

static void     gossip_contact_list_class_init               (GossipContactListClass *klass);
static void     gossip_contact_list_init                     (GossipContactList      *list);
static void     contact_list_finalize                        (GObject                *object);
static void     contact_list_get_property                    (GObject                *object,
							      guint                   param_id,
							      GValue                 *value,
							      GParamSpec             *pspec);
static void     contact_list_set_property                    (GObject                *object,
							      guint                   param_id,
							      const GValue           *value,
							      GParamSpec             *pspec);
static gboolean contact_list_row_separator_func              (GtkTreeModel           *model,
							      GtkTreeIter            *iter,
							      gpointer                data);
static void     contact_list_contact_update                  (GossipContactList      *list,
							      GossipContact          *contact);
static void     contact_list_contact_added_cb                (EmpathyContactManager  *manager,
							      GossipContact          *contact,
							      GossipContactList      *list);
static void     contact_list_contact_updated_cb              (GossipContact          *contact,
							      GParamSpec             *param,
							      GossipContactList      *list);
static void     contact_list_contact_groups_updated_cb       (GossipContact          *contact,
							      GParamSpec             *param,
							      GossipContactList      *list);
static void     contact_list_contact_removed_cb              (EmpathyContactManager  *manager,
							      GossipContact          *contact,
							      GossipContactList      *list);
static void     contact_list_contact_set_active              (GossipContactList      *list,
							      GossipContact          *contact,
							      gboolean                active,
							      gboolean                set_changed);
static ShowActiveData *
		contact_list_contact_active_new              (GossipContactList      *list,
							      GossipContact          *contact,
							      gboolean                remove);
static void     contact_list_contact_active_free             (ShowActiveData         *data);
static gboolean contact_list_contact_active_cb               (ShowActiveData         *data);
static gchar *  contact_list_get_parent_group                (GtkTreeModel           *model,
							      GtkTreePath            *path,
							      gboolean               *path_is_group);
static void     contact_list_get_group                       (GossipContactList      *list,
							      const gchar            *name,
							      GtkTreeIter            *iter_group_to_set,
							      GtkTreeIter            *iter_separator_to_set,
							      gboolean               *created);
static gboolean contact_list_get_group_foreach               (GtkTreeModel           *model,
							      GtkTreePath            *path,
							      GtkTreeIter            *iter,
							      FindGroup              *fg);
static void     contact_list_add_contact                     (GossipContactList      *list,
							      GossipContact          *contact);
static void     contact_list_remove_contact                  (GossipContactList      *list,
							      GossipContact          *contact);
static void     contact_list_create_model                    (GossipContactList      *list);
static gboolean contact_list_search_equal_func               (GtkTreeModel           *model,
							      gint                    column,
							      const gchar            *key,
							      GtkTreeIter            *iter,
							      gpointer                search_data);
static void     contact_list_setup_view                      (GossipContactList      *list);
static void     contact_list_drag_data_received              (GtkWidget              *widget,
							      GdkDragContext         *context,
							      gint                    x,
							      gint                    y,
							      GtkSelectionData       *selection,
							      guint                   info,
							      guint                   time,
							      gpointer                user_data);
static gboolean contact_list_drag_motion                     (GtkWidget              *widget,
							      GdkDragContext         *context,
							      gint                    x,
							      gint                    y,
							      guint                   time,
							      gpointer                data);
static gboolean contact_list_drag_motion_cb                  (DragMotionData         *data);
static void     contact_list_drag_begin                      (GtkWidget              *widget,
							      GdkDragContext         *context,
							      gpointer                user_data);
static void     contact_list_drag_data_get                   (GtkWidget              *widget,
							      GdkDragContext         *contact,
							      GtkSelectionData       *selection,
							      guint                   info,
							      guint                   time,
							      gpointer                user_data);
static void     contact_list_drag_end                        (GtkWidget              *widget,
							      GdkDragContext         *context,
							      gpointer                user_data);
static void     contact_list_cell_set_background             (GossipContactList      *list,
							      GtkCellRenderer        *cell,
							      gboolean                is_group,
							      gboolean                is_active);
static void     contact_list_pixbuf_cell_data_func           (GtkTreeViewColumn      *tree_column,
							      GtkCellRenderer        *cell,
							      GtkTreeModel           *model,
							      GtkTreeIter            *iter,
							      GossipContactList      *list);
static void     contact_list_avatar_cell_data_func           (GtkTreeViewColumn      *tree_column,
							      GtkCellRenderer        *cell,
							      GtkTreeModel           *model,
							      GtkTreeIter            *iter,
							      GossipContactList      *list);
static void     contact_list_text_cell_data_func             (GtkTreeViewColumn      *tree_column,
							      GtkCellRenderer        *cell,
							      GtkTreeModel           *model,
							      GtkTreeIter            *iter,
							      GossipContactList      *list);
static void     contact_list_expander_cell_data_func         (GtkTreeViewColumn      *column,
							      GtkCellRenderer        *cell,
							      GtkTreeModel           *model,
							      GtkTreeIter            *iter,
							      GossipContactList      *list);
static GtkWidget *contact_list_get_contact_menu              (GossipContactList      *list,
							      gboolean                can_send_file,
							      gboolean                can_show_log);
static gboolean contact_list_button_press_event_cb           (GossipContactList      *list,
							      GdkEventButton         *event,
							      gpointer                user_data);
static void     contact_list_row_activated_cb                (GossipContactList      *list,
							      GtkTreePath            *path,
							      GtkTreeViewColumn      *col,
							      gpointer                user_data);
static void     contact_list_row_expand_or_collapse_cb       (GossipContactList      *list,
							      GtkTreeIter            *iter,
							      GtkTreePath            *path,
							      gpointer                user_data);
static gint     contact_list_sort_func                       (GtkTreeModel           *model,
							      GtkTreeIter            *iter_a,
							      GtkTreeIter            *iter_b,
							      gpointer                user_data);
static gboolean contact_list_filter_func                     (GtkTreeModel           *model,
							      GtkTreeIter            *iter,
							      GossipContactList      *list);
static GList *  contact_list_find_contact                    (GossipContactList      *list,
							      GossipContact          *contact);
static gboolean contact_list_find_contact_foreach            (GtkTreeModel           *model,
							      GtkTreePath            *path,
							      GtkTreeIter            *iter,
							      FindContact            *fc);
static void     contact_list_action_cb                       (GtkAction              *action,
							      GossipContactList      *list);
static void     contact_list_action_activated                (GossipContactList      *list,
							      GossipContact          *contact);
static gboolean contact_list_update_list_mode_foreach        (GtkTreeModel           *model,
							      GtkTreePath            *path,
							      GtkTreeIter            *iter,
							      GossipContactList      *list);

enum {
	COL_PIXBUF_STATUS,
	COL_PIXBUF_AVATAR,
	COL_PIXBUF_AVATAR_VISIBLE,
	COL_NAME,
	COL_STATUS,
	COL_STATUS_VISIBLE,
	COL_CONTACT,
	COL_IS_GROUP,
	COL_IS_ACTIVE,
	COL_IS_ONLINE,
	COL_IS_SEPARATOR,
	COL_COUNT
};

enum {
	PROP_0,
	PROP_SHOW_OFFLINE,
	PROP_SHOW_AVATARS,
	PROP_IS_COMPACT,
	PROP_FILTER
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
	{ "Chat", GOSSIP_STOCK_MESSAGE,
	  N_("_Chat"), NULL, N_("Chat with contact"),
	  G_CALLBACK (contact_list_action_cb)
	},
	{ "Information", GOSSIP_STOCK_CONTACT_INFORMATION,
	  N_("Infor_mation"), "<control>I", N_("View contact information"),
	  G_CALLBACK (contact_list_action_cb)
	},
	{ "Rename", NULL,
	  N_("Re_name"), NULL, N_("Rename"),
	  G_CALLBACK (contact_list_action_cb)
	},
	{ "Edit", GTK_STOCK_EDIT,
	  N_("_Edit"), NULL, N_("Edit the groups and name for this contact"),
	  G_CALLBACK (contact_list_action_cb)
	},
	{ "Remove", GTK_STOCK_REMOVE,
	  N_("_Remove"), NULL, N_("Remove contact"),
	  G_CALLBACK (contact_list_action_cb)
	},
	{ "Invite", GOSSIP_STOCK_GROUP_MESSAGE,
	  N_("_Invite to Chat Room"), NULL, N_("Invite to a currently open chat room"),
	  G_CALLBACK (contact_list_action_cb)
	},
	{ "SendFile", NULL,
	  N_("_Send File..."), NULL, N_("Send a file"),
	  G_CALLBACK (contact_list_action_cb)
	},
	{ "Log", GTK_STOCK_JUSTIFY_LEFT,
	  N_("_View Previous Conversations"), NULL, N_("View previous conversations with this contact"),
	  G_CALLBACK (contact_list_action_cb)
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

G_DEFINE_TYPE (GossipContactList, gossip_contact_list, GTK_TYPE_TREE_VIEW);

static void
gossip_contact_list_class_init (GossipContactListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = contact_list_finalize;
	object_class->get_property = contact_list_get_property;
	object_class->set_property = contact_list_set_property;

	g_object_class_install_property (object_class,
					 PROP_SHOW_OFFLINE,
					 g_param_spec_boolean ("show-offline",
							       "Show Offline",
							       "Whether contact list should display "
							       "offline contacts",
							       FALSE,
							       G_PARAM_READWRITE));
	 g_object_class_install_property (object_class,
					  PROP_SHOW_AVATARS,
					  g_param_spec_boolean ("show-avatars",
								"Show Avatars",
								"Whether contact list should display "
								"avatars for contacts",
								TRUE,
								G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_IS_COMPACT,
					 g_param_spec_boolean ("is-compact",
							       "Is Compact",
							       "Whether the contact list is in compact mode or not",
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_FILTER,
					 g_param_spec_string ("filter",
							      "Filter",
							      "The text to use to filter the contact list",
							      NULL,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GossipContactListPriv));
}

static void
gossip_contact_list_init (GossipContactList *list)
{
	GossipContactListPriv *priv;
	GtkActionGroup        *action_group;
	GList                 *contacts, *l;
	GError                *error = NULL;

	priv = GET_PRIV (list);

	priv->manager = empathy_contact_manager_new ();
	priv->is_compact = FALSE;
	priv->show_active = TRUE;
	priv->show_avatars = TRUE;

	contact_list_create_model (list);
	contact_list_setup_view (list);
	empathy_contact_manager_setup (priv->manager);

	/* Get saved group states. */
	gossip_contact_groups_get_all ();

	/* Set up UI Manager */
	priv->ui = gtk_ui_manager_new ();

	action_group = gtk_action_group_new ("Actions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group, entries, n_entries, list);
	gtk_ui_manager_insert_action_group (priv->ui, action_group, 0);

	if (!gtk_ui_manager_add_ui_from_string (priv->ui, ui_info, -1, &error)) {
		g_warning ("Could not build contact menus from string:'%s'", error->message);
		g_error_free (error);
	}

	g_object_unref (action_group);

	gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (list), 
					      contact_list_row_separator_func,
					      NULL, NULL);

	/* Signal connection. */
	g_signal_connect (priv->manager,
			  "contact-added",
			  G_CALLBACK (contact_list_contact_added_cb),
			  list);
	g_signal_connect (priv->manager,
			  "contact-removed",
			  G_CALLBACK (contact_list_contact_removed_cb),
			  list);

	/* Connect to tree view signals rather than override. */
	g_signal_connect (list,
			  "button-press-event",
			  G_CALLBACK (contact_list_button_press_event_cb),
			  NULL);
	g_signal_connect (list,
			  "row-activated",
			  G_CALLBACK (contact_list_row_activated_cb),
			  NULL);
	g_signal_connect (list,
			  "row-expanded",
			  G_CALLBACK (contact_list_row_expand_or_collapse_cb),
			  GINT_TO_POINTER (TRUE));
	g_signal_connect (list,
			  "row-collapsed",
			  G_CALLBACK (contact_list_row_expand_or_collapse_cb),
			  GINT_TO_POINTER (FALSE));

	/* Add contacts already created */
	contacts = empathy_contact_manager_get_contacts (priv->manager);
	for (l = contacts; l; l = l->next) {
		GossipContact *contact;

		contact = l->data;

		contact_list_contact_added_cb (priv->manager, contact, list);

		g_object_unref (contact);
	}
	g_list_free (contacts);
}

static void
contact_list_finalize (GObject *object)
{
	GossipContactListPriv *priv;

	priv = GET_PRIV (object);

	/* FIXME: disconnect all signals on the manager and contacts */

	g_object_unref (priv->manager);
	g_object_unref (priv->ui);
	g_object_unref (priv->store);
	g_object_unref (priv->filter);
	g_free (priv->filter_text);

	G_OBJECT_CLASS (gossip_contact_list_parent_class)->finalize (object);
}

static void
contact_list_get_property (GObject    *object,
			   guint       param_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
	GossipContactListPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_SHOW_OFFLINE:
		g_value_set_boolean (value, priv->show_offline);
		break;
	case PROP_SHOW_AVATARS:
		g_value_set_boolean (value, priv->show_avatars);
		break;
	case PROP_IS_COMPACT:
		g_value_set_boolean (value, priv->is_compact);
		break;
	case PROP_FILTER:
		g_value_set_string (value, priv->filter_text);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
contact_list_set_property (GObject      *object,
			   guint         param_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
	GossipContactListPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_SHOW_OFFLINE:
		gossip_contact_list_set_show_offline (GOSSIP_CONTACT_LIST (object),
						      g_value_get_boolean (value));
		break;
	case PROP_SHOW_AVATARS:
		gossip_contact_list_set_show_avatars (GOSSIP_CONTACT_LIST (object),
						      g_value_get_boolean (value));
		break;
	case PROP_IS_COMPACT:
		gossip_contact_list_set_is_compact (GOSSIP_CONTACT_LIST (object),
						    g_value_get_boolean (value));
		break;
	case PROP_FILTER:
		gossip_contact_list_set_filter (GOSSIP_CONTACT_LIST (object),
						g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static gboolean
contact_list_row_separator_func (GtkTreeModel *model,
				 GtkTreeIter  *iter,
				 gpointer      data)
{
	gboolean is_separator = FALSE;

	gtk_tree_model_get (model, iter,
			    COL_IS_SEPARATOR, &is_separator,
			    -1);

	return is_separator;
}

static void
contact_list_contact_update (GossipContactList *list,
			     GossipContact     *contact)
{
	GossipContactListPriv *priv;
	ShowActiveData        *data;
	GtkTreeModel          *model;
	GList                 *iters, *l;
	gboolean               in_list;
	gboolean               should_be_in_list;
	gboolean               was_online = TRUE;
	gboolean               now_online = FALSE;
	gboolean               set_model = FALSE;
	gboolean               do_remove = FALSE;
	gboolean               do_set_active = FALSE;
	gboolean               do_set_refresh = FALSE;
	GdkPixbuf             *pixbuf_presence;
	GdkPixbuf             *pixbuf_avatar;

	priv = GET_PRIV (list);

	model = GTK_TREE_MODEL (priv->store);

	iters = contact_list_find_contact (list, contact);
	if (!iters) {
		in_list = FALSE;
	} else {
		in_list = TRUE;
	}

	/* Get online state now. */
	now_online = gossip_contact_is_online (contact);

	if (priv->show_offline || now_online) {
		should_be_in_list = TRUE;
	} else {
		should_be_in_list = FALSE;
	}

	if (!in_list && !should_be_in_list) {
		/* Nothing to do. */
		gossip_debug (DEBUG_DOMAIN,
			      "Contact:'%s' in list:NO, should be:NO",
			      gossip_contact_get_name (contact));

		g_list_foreach (iters, (GFunc) gtk_tree_iter_free, NULL);
		g_list_free (iters);
		return;
	}
	else if (in_list && !should_be_in_list) {
		gossip_debug (DEBUG_DOMAIN,
			      "Contact:'%s' in list:YES, should be:NO",
			      gossip_contact_get_name (contact));

		if (priv->show_active) {
			do_remove = TRUE;
			do_set_active = TRUE;
			do_set_refresh = TRUE;

			set_model = TRUE;
			gossip_debug (DEBUG_DOMAIN, "Remove item (after timeout)");
		} else {
			gossip_debug (DEBUG_DOMAIN, "Remove item (now)!");
			contact_list_remove_contact (list, contact);
		}
	}
	else if (!in_list && should_be_in_list) {
		gossip_debug (DEBUG_DOMAIN,
			      "Contact:'%s' in list:NO, should be:YES",
			      gossip_contact_get_name (contact));

		contact_list_add_contact (list, contact);

		if (priv->show_active) {
			do_set_active = TRUE;

			gossip_debug (DEBUG_DOMAIN, "Set active (contact added)");
		}
	} else {
		gossip_debug (DEBUG_DOMAIN,
			      "Contact:'%s' in list:YES, should be:YES",
			      gossip_contact_get_name (contact));

		/* Get online state before. */
		if (iters && g_list_length (iters) > 0) {
			gtk_tree_model_get (model, iters->data, COL_IS_ONLINE, &was_online, -1);
		}

		/* Is this really an update or an online/offline. */
		if (priv->show_active) {
			if (was_online != now_online) {
				gchar *str;

				do_set_active = TRUE;
				do_set_refresh = TRUE;

				if (was_online) {
					str = "online  -> offline";
				} else {
					str = "offline -> online";
				}

				gossip_debug (DEBUG_DOMAIN, "Set active (contact updated %s)", str);
			} else {
				/* Was TRUE for presence updates. */
				/* do_set_active = FALSE;  */
				do_set_refresh = TRUE;

				gossip_debug (DEBUG_DOMAIN, "Set active (contact updated)");
			}
		}

		set_model = TRUE;
	}

	pixbuf_presence = gossip_pixbuf_for_contact (contact);
	pixbuf_avatar = gossip_pixbuf_avatar_from_contact_scaled (contact, 32, 32);
	for (l = iters; l && set_model; l = l->next) {
		gtk_tree_store_set (priv->store, l->data,
				    COL_PIXBUF_STATUS, pixbuf_presence,
				    COL_STATUS, gossip_contact_get_status (contact),
				    COL_IS_ONLINE, now_online,
				    COL_NAME, gossip_contact_get_name (contact),
				    COL_PIXBUF_AVATAR, pixbuf_avatar,
				    -1);
	}

	if (pixbuf_presence) {
		g_object_unref (pixbuf_presence);
	}
	if (pixbuf_avatar) {
		g_object_unref (pixbuf_avatar);
	}

	if (priv->show_active && do_set_active) {
		contact_list_contact_set_active (list, contact, do_set_active, do_set_refresh);

		if (do_set_active) {
			data = contact_list_contact_active_new (list, contact, do_remove);
			g_timeout_add (ACTIVE_USER_SHOW_TIME,
				       (GSourceFunc) contact_list_contact_active_cb,
				       data);
		}
	}

	/* FIXME: when someone goes online then offline quickly, the
	 * first timeout sets the user to be inactive and the second
	 * timeout removes the user from the contact list, really we
	 * should remove the first timeout.
	 */
	g_list_foreach (iters, (GFunc) gtk_tree_iter_free, NULL);
	g_list_free (iters);
}

static void
contact_list_contact_added_cb (EmpathyContactManager *manager,
			       GossipContact         *contact,
			       GossipContactList     *list)
{
	GossipContactListPriv *priv;

	priv = GET_PRIV (list);

	gossip_debug (DEBUG_DOMAIN, 
		      "Contact:'%s' added",
		      gossip_contact_get_name (contact));

	g_signal_connect (contact, "notify::groups",
			  G_CALLBACK (contact_list_contact_groups_updated_cb),
			  list);
	g_signal_connect (contact, "notify::presence",
			  G_CALLBACK (contact_list_contact_updated_cb),
			  list);
	g_signal_connect (contact, "notify::name",
			  G_CALLBACK (contact_list_contact_updated_cb),
			  list);
	g_signal_connect (contact, "notify::avatar",
			  G_CALLBACK (contact_list_contact_updated_cb),
			  list);
	g_signal_connect (contact, "notify::type",
			  G_CALLBACK (contact_list_contact_updated_cb),
			  list);

	contact_list_add_contact (list, contact);
}

static void
contact_list_contact_groups_updated_cb (GossipContact     *contact,
					GParamSpec        *param,
					GossipContactList *list)
{
	GossipContactListPriv *priv;

	priv = GET_PRIV (list);

	if (priv->show_offline || gossip_contact_is_online (contact)) {
	}


	gossip_debug (DEBUG_DOMAIN, "Contact:'%s' groups updated",
		      gossip_contact_get_name (contact));

	/* We do this to make sure the groups are correct, if not, we
	 * would have to check the groups already set up for each
	 * contact and then see what has been updated.
	 */
	contact_list_remove_contact (list, contact);
	contact_list_add_contact (list, contact);
}

static void
contact_list_contact_updated_cb (GossipContact     *contact,
				 GParamSpec        *param,
				 GossipContactList *list)
{
	gossip_debug (DEBUG_DOMAIN,
		      "Contact:'%s' updated, checking roster is in sync...",
		      gossip_contact_get_name (contact));

	contact_list_contact_update (list, contact);
}

static void
contact_list_contact_removed_cb (EmpathyContactManager *manager,
				 GossipContact         *contact,
				 GossipContactList     *list)
{
	gossip_debug (DEBUG_DOMAIN, "Contact:'%s' removed",
		      gossip_contact_get_name (contact));

	/* Disconnect signals */
	g_signal_handlers_disconnect_by_func (contact, 
					      G_CALLBACK (contact_list_contact_groups_updated_cb),
					      list);
	g_signal_handlers_disconnect_by_func (contact,
					      G_CALLBACK (contact_list_contact_updated_cb),
					      list);

	contact_list_remove_contact (list, contact);
}

static void
contact_list_contact_set_active (GossipContactList *list,
				 GossipContact     *contact,
				 gboolean           active,
				 gboolean           set_changed)
{
	GossipContactListPriv *priv;
	GtkTreeModel          *model;
	GList                 *iters, *l;

	priv = GET_PRIV (list);

	model = GTK_TREE_MODEL (priv->store);

	iters = contact_list_find_contact (list, contact);
	for (l = iters; l; l = l->next) {
		GtkTreePath *path;

		gtk_tree_store_set (priv->store, l->data,
				    COL_IS_ACTIVE, active,
				    -1);

		gossip_debug (DEBUG_DOMAIN, "Set item %s", active ? "active" : "inactive");

		if (set_changed) {
			path = gtk_tree_model_get_path (model, l->data);
			gtk_tree_model_row_changed (model, path, l->data);
			gtk_tree_path_free (path);
		}
	}

	g_list_foreach (iters, (GFunc)gtk_tree_iter_free, NULL);
	g_list_free (iters);

}

static ShowActiveData *
contact_list_contact_active_new (GossipContactList *list,
				 GossipContact     *contact,
				 gboolean           remove)
{
	ShowActiveData *data;

	g_return_val_if_fail (list != NULL, NULL);
	g_return_val_if_fail (contact != NULL, NULL);

	gossip_debug (DEBUG_DOMAIN, 
		      "Contact:'%s' now active, and %s be removed",
		      gossip_contact_get_name (contact), 
		      remove ? "WILL" : "WILL NOT");
	
	data = g_slice_new0 (ShowActiveData);

	data->list = g_object_ref (list);
	data->contact = g_object_ref (contact);

	data->remove = remove;

	return data;
}

static void
contact_list_contact_active_free (ShowActiveData *data)
{
	g_return_if_fail (data != NULL);

	g_object_unref (data->contact);
	g_object_unref (data->list);

	g_slice_free (ShowActiveData, data);
}

static gboolean
contact_list_contact_active_cb (ShowActiveData *data)
{
	GossipContactListPriv *priv;

	g_return_val_if_fail (data != NULL, FALSE);

	priv = GET_PRIV (data->list);

	if (data->remove &&
	    !priv->show_offline &&
	    !gossip_contact_is_online (data->contact)) {
		gossip_debug (DEBUG_DOMAIN, 
			      "Contact:'%s' active timeout, removing item",
			      gossip_contact_get_name (data->contact));
		contact_list_remove_contact (data->list,
					     data->contact);
	}

	gossip_debug (DEBUG_DOMAIN, 
		      "Contact:'%s' no longer active",
		      gossip_contact_get_name (data->contact));
	contact_list_contact_set_active (data->list,
					 data->contact,
					 FALSE,
					 TRUE);

	contact_list_contact_active_free (data);

	return FALSE;
}

static gchar *
contact_list_get_parent_group (GtkTreeModel *model,
			       GtkTreePath  *path,
			       gboolean     *path_is_group)
{
	GtkTreeIter  parent_iter, iter;
	gchar       *name;
	gboolean     is_group;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (path_is_group != NULL, NULL);

	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter,
			    COL_IS_GROUP, &is_group,
			    -1);

	if (!is_group) {
		if (!gtk_tree_model_iter_parent (model, &parent_iter, &iter)) {
			return NULL;
		}

		iter = parent_iter;

		gtk_tree_model_get (model, &iter,
				    COL_IS_GROUP, &is_group,
				    -1);

		if (!is_group) {
			return NULL;
		}

		*path_is_group = TRUE;
	}

	gtk_tree_model_get (model, &iter,
			    COL_NAME, &name,
			    -1);

	return name;
}

static gboolean
contact_list_get_group_foreach (GtkTreeModel *model,
				GtkTreePath  *path,
				GtkTreeIter  *iter,
				FindGroup    *fg)
{
	gchar    *str;
	gboolean  is_group;

	/* Groups are only at the top level. */
	if (gtk_tree_path_get_depth (path) != 1) {
		return FALSE;
	}

	gtk_tree_model_get (model, iter,
			    COL_NAME, &str,
			    COL_IS_GROUP, &is_group,
			    -1);

	if (is_group && strcmp (str, fg->name) == 0) {
		fg->found = TRUE;
		fg->iter = *iter;
	}

	g_free (str);

	return fg->found;
}

static void
contact_list_get_group (GossipContactList *list,
			const gchar       *name,
			GtkTreeIter       *iter_group_to_set,
			GtkTreeIter       *iter_separator_to_set,
			gboolean          *created)
{
	GossipContactListPriv *priv;
	GtkTreeModel          *model;
	GtkTreeIter            iter_group, iter_separator;
	FindGroup              fg;

	priv = GET_PRIV (list);

	memset (&fg, 0, sizeof (fg));

	fg.name = name;

	model = GTK_TREE_MODEL (priv->store);
	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) contact_list_get_group_foreach,
				&fg);

	if (!fg.found) {
		if (created) {
			*created = TRUE;
		}

		gtk_tree_store_append (priv->store, &iter_group, NULL);
		gtk_tree_store_set (priv->store, &iter_group,
				    COL_PIXBUF_STATUS, NULL,
				    COL_NAME, name,
				    COL_IS_GROUP, TRUE,
				    COL_IS_ACTIVE, FALSE,
				    COL_IS_SEPARATOR, FALSE,
				    -1);

		if (iter_group_to_set) {
			*iter_group_to_set = iter_group;
		}

		gtk_tree_store_append (priv->store,
				       &iter_separator, 
				       &iter_group);
		gtk_tree_store_set (priv->store, &iter_separator,
				    COL_IS_SEPARATOR, TRUE,
				    -1);

		if (iter_separator_to_set) {
			*iter_separator_to_set = iter_separator;
		}
	} else {
		if (created) {
			*created = FALSE;
		}

		if (iter_group_to_set) {
			*iter_group_to_set = fg.iter;
		}

		iter_separator = fg.iter;

		if (gtk_tree_model_iter_next (model, &iter_separator)) {
			gboolean is_separator;

			gtk_tree_model_get (model, &iter_separator,
					    COL_IS_SEPARATOR, &is_separator,
					    -1);

			if (is_separator && iter_separator_to_set) {
				*iter_separator_to_set = iter_separator;
			}
		}
	}
}

static void
contact_list_add_contact (GossipContactList *list,
			  GossipContact     *contact)
{
	GossipContactListPriv *priv;
	GtkTreeIter            iter, iter_group, iter_separator;
	GtkTreeModel          *model;
	GList                 *l, *groups;

	priv = GET_PRIV (list);
	
	if (!priv->show_offline && !gossip_contact_is_online (contact)) {
		return;
	}

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));

	/* If no groups just add it at the top level. */
	groups = gossip_contact_get_groups (contact);
	if (!groups) {
		GdkPixbuf *pixbuf_status;
		GdkPixbuf *pixbuf_avatar;
		gboolean   show_avatar = FALSE;

		pixbuf_status = gossip_pixbuf_for_contact (contact);
		pixbuf_avatar = gossip_pixbuf_avatar_from_contact_scaled (contact, 32, 32);

		if (priv->show_avatars && !priv->is_compact) {
			show_avatar = TRUE;
		}

		gossip_debug (DEBUG_DOMAIN, "");
		gossip_debug (DEBUG_DOMAIN, 
			      "vvvvvvvvvvvvvvvv FIXME: Errors may follow below (since filter work) vvvvvvvvvvvvvvvv");

		gossip_debug (DEBUG_DOMAIN, 
			      "**** GossipContact:%p, is GObject:%s, is GossipContact:%s, ADDING CONTACT #1",
			      contact,
			      G_IS_OBJECT (contact) ? "yes" : "no",
			      GOSSIP_IS_CONTACT (contact) ? "yes" : "no");

		gtk_tree_store_append (priv->store, &iter, NULL);
		gtk_tree_store_set (priv->store, &iter,
				    COL_PIXBUF_STATUS, pixbuf_status,
				    COL_PIXBUF_AVATAR, pixbuf_avatar,
				    COL_PIXBUF_AVATAR_VISIBLE, show_avatar,
				    COL_NAME, gossip_contact_get_name (contact),
				    COL_STATUS, gossip_contact_get_status (contact),
				    COL_STATUS_VISIBLE, !priv->is_compact,
				    COL_CONTACT, contact,
				    COL_IS_GROUP, FALSE,
				    COL_IS_ACTIVE, FALSE,
				    COL_IS_ONLINE, gossip_contact_is_online (contact),
				    COL_IS_SEPARATOR, FALSE,
				    -1);

		gossip_debug (DEBUG_DOMAIN, 
			      "^^^^^^^^^^^^^^^^ FIXME: Errors may occur above  (since filter work) ^^^^^^^^^^^^^^^^");
		gossip_debug (DEBUG_DOMAIN, "");

		if (pixbuf_avatar) {
			g_object_unref (pixbuf_avatar);
		}
		if (pixbuf_status) {
			g_object_unref (pixbuf_status);
		}
	}

	/* Else add to each group. */
	for (l = groups; l; l = l->next) {
		GtkTreePath *path;
		GtkTreeIter  model_iter_group;
		GdkPixbuf   *pixbuf_status;
		GdkPixbuf   *pixbuf_avatar;
		const gchar *name;
		gboolean     created;
		gboolean     found;
		gboolean     show_avatar = FALSE;

		name = l->data;
		if (!name) {
			continue;
		}

		pixbuf_status = gossip_pixbuf_for_contact (contact);
		pixbuf_avatar = gossip_pixbuf_avatar_from_contact_scaled (contact, 32, 32);

		contact_list_get_group (list, name, &iter_group, &iter_separator, &created);

		if (priv->show_avatars && !priv->is_compact) {
			show_avatar = TRUE;
		}

		gossip_debug (DEBUG_DOMAIN, "");
		gossip_debug (DEBUG_DOMAIN, 
			      "vvvvvvvvvvvvvvvv FIXME: Errors may follow below (since filter work) vvvvvvvvvvvvvvvv");

		gossip_debug (DEBUG_DOMAIN, 
			      "**** GossipContact:%p, is GObject:%s, is GossipContact:%s, ADDING CONTACT #2",
			      contact,
			      G_IS_OBJECT (contact) ? "yes" : "no",
			      GOSSIP_IS_CONTACT (contact) ? "yes" : "no");

		gtk_tree_store_insert_after (priv->store, &iter, &iter_group, NULL);
		gtk_tree_store_set (priv->store, &iter,
				    COL_PIXBUF_STATUS, pixbuf_status,
				    COL_PIXBUF_AVATAR, pixbuf_avatar,
				    COL_PIXBUF_AVATAR_VISIBLE, show_avatar,
				    COL_NAME, gossip_contact_get_name (contact),
				    COL_STATUS, gossip_contact_get_status (contact),
				    COL_STATUS_VISIBLE, !priv->is_compact,
				    COL_CONTACT, contact,
				    COL_IS_GROUP, FALSE,
				    COL_IS_ACTIVE, FALSE,
				    COL_IS_ONLINE, gossip_contact_is_online (contact),
				    COL_IS_SEPARATOR, FALSE,
				    -1);

		gossip_debug (DEBUG_DOMAIN, 
			      "^^^^^^^^^^^^^^^^ FIXME: Errors may occur above  (since filter work) ^^^^^^^^^^^^^^^^");
		gossip_debug (DEBUG_DOMAIN, "");

		if (pixbuf_avatar) {
			g_object_unref (pixbuf_avatar);
		}
		if (pixbuf_status) {
			g_object_unref (pixbuf_status);
		}

		if (!created) {
			continue;
		}

		found = gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (priv->filter),  
									  &model_iter_group,  
									  &iter_group); 
		if (!found) {
			continue;
		}
		
		path = gtk_tree_model_get_path (model, &model_iter_group);
		if (!path) {
			continue;
		}

		if (gossip_contact_group_get_expanded (name)) {
			g_signal_handlers_block_by_func (list,
							 contact_list_row_expand_or_collapse_cb,
							 GINT_TO_POINTER (TRUE));
			gtk_tree_view_expand_row (GTK_TREE_VIEW (list), path, TRUE);
			g_signal_handlers_unblock_by_func (list,
							   contact_list_row_expand_or_collapse_cb,
							   GINT_TO_POINTER (TRUE));
		} else {
			g_signal_handlers_block_by_func (list,
							 contact_list_row_expand_or_collapse_cb,
							 GINT_TO_POINTER (FALSE));
			gtk_tree_view_collapse_row (GTK_TREE_VIEW (list), path);
			g_signal_handlers_unblock_by_func (list,
							   contact_list_row_expand_or_collapse_cb,
							   GINT_TO_POINTER (FALSE));
		}

		gtk_tree_path_free (path);
	}
}

static void
contact_list_remove_contact (GossipContactList *list,
			     GossipContact     *contact)
{
	GossipContactListPriv *priv;
	GtkTreeModel          *model;
	GList                 *iters, *l;

	priv = GET_PRIV (list);

	iters = contact_list_find_contact (list, contact);
	if (!iters) {
		return;
	}
	
	/* Clean up model */
	model = GTK_TREE_MODEL (priv->store);

	for (l = iters; l; l = l->next) {
		GtkTreeIter parent;

		/* NOTE: it is only <= 2 here because we have
		 * separators after the group name, otherwise it
		 * should be 1. 
		 */
		if (gtk_tree_model_iter_parent (model, &parent, l->data) &&
		    gtk_tree_model_iter_n_children (model, &parent) <= 2) {
			gtk_tree_store_remove (priv->store, &parent);
		} else {
			gtk_tree_store_remove (priv->store, l->data);
		}
	}

	g_list_foreach (iters, (GFunc) gtk_tree_iter_free, NULL);
	g_list_free (iters);
}

static void
contact_list_create_model (GossipContactList *list)
{
	GossipContactListPriv *priv;
	GtkTreeModel          *model;
	
	priv = GET_PRIV (list);

	if (priv->store) {
		g_object_unref (priv->store);
	}

	if (priv->filter) {
		g_object_unref (priv->filter);
	}

	priv->store = gtk_tree_store_new (COL_COUNT,
					  GDK_TYPE_PIXBUF,     /* Status pixbuf */
					  GDK_TYPE_PIXBUF,     /* Avatar pixbuf */
					  G_TYPE_BOOLEAN,      /* Avatar pixbuf visible */
					  G_TYPE_STRING,       /* Name */
					  G_TYPE_STRING,       /* Status string */
					  G_TYPE_BOOLEAN,      /* Show status */
					  GOSSIP_TYPE_CONTACT, /* Contact type */
					  G_TYPE_BOOLEAN,      /* Is group */
					  G_TYPE_BOOLEAN,      /* Is active */
					  G_TYPE_BOOLEAN,      /* Is online */
					  G_TYPE_BOOLEAN);     /* Is separator */

	/* Save normal model */
	model = GTK_TREE_MODEL (priv->store);

	/* Set up sorting */
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (model),
					 COL_NAME,
					 contact_list_sort_func,
					 list, NULL);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
					      COL_NAME,
					      GTK_SORT_ASCENDING);

	/* Create filter */
	priv->filter = gtk_tree_model_filter_new (model, NULL);

	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (priv->filter),
						(GtkTreeModelFilterVisibleFunc)
						contact_list_filter_func,
						list, NULL);

	gtk_tree_view_set_model (GTK_TREE_VIEW (list), priv->filter);
}

static gboolean
contact_list_search_equal_func (GtkTreeModel *model,
				gint          column,
				const gchar  *key,
				GtkTreeIter  *iter,
				gpointer      search_data)
{
	gchar    *name, *name_folded;
	gchar    *key_folded;
	gboolean  ret;

	if (!key) {
		return FALSE;
	}

	gtk_tree_model_get (model, iter, COL_NAME, &name, -1);

	if (!name) {
		return FALSE;
	}

	name_folded = g_utf8_casefold (name, -1);
	key_folded = g_utf8_casefold (key, -1);

	if (name_folded && key_folded && 
	    strstr (name_folded, key_folded)) {
		ret = FALSE;
	} else {
		ret = TRUE;
	}

	g_free (name);
	g_free (name_folded);
	g_free (key_folded);

	return ret;
}

static void
contact_list_setup_view (GossipContactList *list)
{
	GtkCellRenderer   *cell;
	GtkTreeViewColumn *col;
	gint               i;

	gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (list),
					     contact_list_search_equal_func,
					     list,
					     NULL);

	g_object_set (list,
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
		(GtkTreeCellDataFunc) contact_list_pixbuf_cell_data_func,
		list, NULL);

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
		(GtkTreeCellDataFunc) contact_list_text_cell_data_func,
		list, NULL);

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
		(GtkTreeCellDataFunc) contact_list_avatar_cell_data_func,
		list, NULL);

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
		(GtkTreeCellDataFunc) contact_list_expander_cell_data_func,
		list, NULL);

	/* Actually add the column now we have added all cell renderers */
	gtk_tree_view_append_column (GTK_TREE_VIEW (list), col);

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
	gtk_drag_source_set (GTK_WIDGET (list),
			     GDK_BUTTON1_MASK,
			     drag_types_source,
			     G_N_ELEMENTS (drag_types_source),
			     GDK_ACTION_MOVE);

	gtk_drag_dest_set (GTK_WIDGET (list),
			   GTK_DEST_DEFAULT_ALL,
			   drag_types_dest,
			   G_N_ELEMENTS (drag_types_dest),
			   GDK_ACTION_MOVE | GDK_ACTION_LINK);

	g_signal_connect (GTK_WIDGET (list),
			  "drag-data-received",
			  G_CALLBACK (contact_list_drag_data_received),
			  NULL);

	/* FIXME: noticed but when you drag the row over the treeview
	 * fast, it seems to stop redrawing itself, if we don't
	 * connect this signal, all is fine.
	 */
	g_signal_connect (GTK_WIDGET (list),
			  "drag-motion",
			  G_CALLBACK (contact_list_drag_motion),
			  NULL);

	g_signal_connect (GTK_WIDGET (list),
			  "drag-begin",
			  G_CALLBACK (contact_list_drag_begin),
			  NULL);
	g_signal_connect (GTK_WIDGET (list),
			  "drag-data-get",
			  G_CALLBACK (contact_list_drag_data_get),
			  NULL);
	g_signal_connect (GTK_WIDGET (list),
			  "drag-end",
			  G_CALLBACK (contact_list_drag_end),
			  NULL);
}

static void
contact_list_drag_data_received (GtkWidget         *widget,
				 GdkDragContext    *context,
				 gint               x,
				 gint               y,
				 GtkSelectionData  *selection,
				 guint              info,
				 guint              time,
				 gpointer           user_data)
{
	GossipContactListPriv   *priv;
	GtkTreeModel            *model;
	GtkTreePath             *path;
	GtkTreeViewDropPosition  position;
	GossipContact           *contact;
	GList                   *groups;
	const gchar             *id;
	gchar                   *old_group;
	gboolean                 is_row;
	gboolean                 drag_success = TRUE;
	gboolean                 drag_del = FALSE;

	priv = GET_PRIV (widget);

	id = (const gchar*) selection->data;
	gossip_debug (DEBUG_DOMAIN, "Received %s%s drag & drop contact from roster with id:'%s'",
		      context->action == GDK_ACTION_MOVE ? "move" : "",
		      context->action == GDK_ACTION_COPY ? "copy" : "",
		      id);

	/* FIXME: This is ambigous, an id can come from multiple accounts */
	contact = empathy_contact_manager_find (priv->manager, id);
	if (!contact) {
		gossip_debug (DEBUG_DOMAIN, "No contact found associated with drag & drop");
		return;
	}

	groups = gossip_contact_get_groups (contact);

	is_row = gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
						    x,
						    y,
						    &path,
						    &position);

	if (!is_row) {
		if (g_list_length (groups) != 1) {
			/* if they have dragged a contact out of a
			 * group then we would set the contact to have
			 * NO groups but only if they were ONE group
			 * to begin with - should we do this
			 * regardless to how many groups they are in
			 * already or not at all?
			 */
			return;
		}

		gossip_contact_set_groups (contact, NULL);
	} else {
		GList    *l, *new_groups;
		gchar    *name;
		gboolean  is_group;

		model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
		name = contact_list_get_parent_group (model, path, &is_group);

		if (groups && name &&
		    g_list_find_custom (groups, name, (GCompareFunc)strcmp)) {
			g_free (name);
			return;
		}

		/* Get source group information. */
		priv = GET_PRIV (widget);
		if (!priv->drag_row) {
			g_free (name);
			return;
		}

		path = gtk_tree_row_reference_get_path (priv->drag_row);
		if (!path) {
			g_free (name);
			return;
		}

		old_group = contact_list_get_parent_group (model, path, &is_group);
		gtk_tree_path_free (path);

		if (!name && old_group && GDK_ACTION_MOVE) {
			drag_success = FALSE;
		}

		if (context->action == GDK_ACTION_MOVE) {
			drag_del = TRUE;
		}

		/* Create new groups GList. */
		for (l = groups, new_groups = NULL; l && drag_success; l = l->next) {
			gchar *str;

			str = l->data;
			if (context->action == GDK_ACTION_MOVE &&
			    old_group != NULL &&
			    strcmp (str, old_group) == 0) {
				continue;
			}

			if (str == NULL) {
				continue;
			}

			new_groups = g_list_append (new_groups, g_strdup (str));
		}

		if (drag_success) {
			if (name) {
				new_groups = g_list_append (new_groups, name);
			}
			gossip_contact_set_groups (contact, new_groups);
		} else {
			g_free (name);
		}
	}

	gtk_drag_finish (context, drag_success, drag_del, GDK_CURRENT_TIME);
}

static gboolean
contact_list_drag_motion (GtkWidget      *widget,
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

		dm->list = GOSSIP_CONTACT_LIST (widget);
		dm->path = gtk_tree_path_copy (path);

		dm->timeout_id = g_timeout_add (
			1500,
			(GSourceFunc) contact_list_drag_motion_cb,
			dm);
	}

	return TRUE;
}

static gboolean
contact_list_drag_motion_cb (DragMotionData *data)
{
	gtk_tree_view_expand_row (GTK_TREE_VIEW (data->list),
				  data->path,
				  FALSE);

	data->timeout_id = 0;

	return FALSE;
}

static void
contact_list_drag_begin (GtkWidget      *widget,
			 GdkDragContext *context,
			 gpointer        user_data)
{
	GossipContactListPriv *priv;
	GtkTreeSelection      *selection;
	GtkTreeModel          *model;
	GtkTreePath           *path;
	GtkTreeIter            iter;

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
contact_list_drag_data_get (GtkWidget             *widget,
			    GdkDragContext        *context,
			    GtkSelectionData      *selection,
			    guint                  info,
			    guint                  time,
			    gpointer               user_data)
{
	GossipContactListPriv *priv;
	GtkTreePath           *src_path;
	GtkTreeIter            iter;
	GtkTreeModel          *model;
	GossipContact         *contact;
	const gchar           *id;

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

	contact = gossip_contact_list_get_selected (GOSSIP_CONTACT_LIST (widget));
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
contact_list_drag_end (GtkWidget      *widget,
		       GdkDragContext *context,
		       gpointer        user_data)
{
	GossipContactListPriv *priv;

	priv = GET_PRIV (widget);

	if (priv->drag_row) {
		gtk_tree_row_reference_free (priv->drag_row);
		priv->drag_row = NULL;
	}
}

static void
contact_list_cell_set_background (GossipContactList  *list,
				  GtkCellRenderer    *cell,
				  gboolean            is_group,
				  gboolean            is_active)
{
	GdkColor  color;
	GtkStyle *style;

	g_return_if_fail (list != NULL);
	g_return_if_fail (cell != NULL);

	style = gtk_widget_get_style (GTK_WIDGET (list));

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
#if 0
		gint color_sum_normal;
		gint color_sum_selected;
		
		color = style->base[GTK_STATE_SELECTED];
		color_sum_normal = color.red+color.green+color.blue;
		color = style->base[GTK_STATE_NORMAL];
		color_sum_selected = color.red+color.green+color.blue;
		color = style->text_aa[GTK_STATE_INSENSITIVE];

		if (color_sum_normal < color_sum_selected) { 
			/* Found a light theme */
			color.red = (color.red + (style->white).red) / 2;
			color.green = (color.green + (style->white).green) / 2;
			color.blue = (color.blue + (style->white).blue) / 2;
		} else { 
			/* Found a dark theme */
			color.red = (color.red + (style->black).red) / 2;
			color.green = (color.green + (style->black).green) / 2;
			color.blue = (color.blue + (style->black).blue) / 2;
		}

		g_object_set (cell,
			      "cell-background-gdk", &color,
			      NULL);
#endif
	}
}

static void
contact_list_pixbuf_cell_data_func (GtkTreeViewColumn *tree_column,
				    GtkCellRenderer   *cell,
				    GtkTreeModel      *model,
				    GtkTreeIter       *iter,
				    GossipContactList *list)
{
	GdkPixbuf *pixbuf;
	gboolean   is_group;
	gboolean   is_active;

	gtk_tree_model_get (model, iter,
			    COL_IS_GROUP, &is_group,
			    COL_IS_ACTIVE, &is_active,
			    COL_PIXBUF_STATUS, &pixbuf,
			    -1);

	g_object_set (cell,
		      "visible", !is_group,
		      "pixbuf", pixbuf,
		      NULL);

	if (pixbuf) {
		g_object_unref (pixbuf);
	}

	contact_list_cell_set_background (list, cell, is_group, is_active);
}

static void
contact_list_avatar_cell_data_func (GtkTreeViewColumn *tree_column,
				    GtkCellRenderer   *cell,
				    GtkTreeModel      *model,
				    GtkTreeIter       *iter,
				    GossipContactList *list)
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

	contact_list_cell_set_background (list, cell, is_group, is_active);
}

static void
contact_list_text_cell_data_func (GtkTreeViewColumn *tree_column,
				  GtkCellRenderer   *cell,
				  GtkTreeModel      *model,
				  GtkTreeIter       *iter,
				  GossipContactList *list)
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

	contact_list_cell_set_background (list, cell, is_group, is_active);
}

static void
contact_list_expander_cell_data_func (GtkTreeViewColumn *column,
				      GtkCellRenderer   *cell,
				      GtkTreeModel      *model,
				      GtkTreeIter       *iter,
				      GossipContactList *list)
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

	contact_list_cell_set_background (list, cell, is_group, is_active);
}

static GtkWidget *
contact_list_get_contact_menu (GossipContactList *list,
			       gboolean           can_send_file,
			       gboolean           can_show_log)
{
	GossipContactListPriv *priv;
	GtkAction             *action;
	GtkWidget             *widget;

	priv = GET_PRIV (list);

	/* Sort out sensitive items */
	action = gtk_ui_manager_get_action (priv->ui, "/Contact/Log");
	gtk_action_set_sensitive (action, can_show_log);

	action = gtk_ui_manager_get_action (priv->ui, "/Contact/SendFile");
	gtk_action_set_visible (action, can_send_file);

	widget = gtk_ui_manager_get_widget (priv->ui, "/Contact");

	return widget;
}

GtkWidget *
gossip_contact_list_get_group_menu (GossipContactList *list)
{
	GossipContactListPriv *priv;
	GtkWidget             *widget;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);

	widget = gtk_ui_manager_get_widget (priv->ui, "/Group");

	return widget;
}

GtkWidget *
gossip_contact_list_get_contact_menu (GossipContactList *list,
				      GossipContact     *contact)
{
	GtkWidget *menu;
	gboolean   can_show_log;
	gboolean   can_send_file;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST (list), NULL);
	g_return_val_if_fail (GOSSIP_IS_CONTACT (contact), NULL);

	can_show_log = FALSE; /* FIXME: gossip_log_exists_for_contact (contact); */
	can_send_file = FALSE;

	menu = contact_list_get_contact_menu (list,
					      can_send_file,
					      can_show_log);
	return menu;
}

static gboolean
contact_list_button_press_event_cb (GossipContactList *list,
				    GdkEventButton    *event,
				    gpointer           user_data)
{
	GossipContactListPriv *priv;
	GossipContact         *contact;
	GtkTreePath           *path;
	GtkTreeSelection      *selection;
	GtkTreeModel          *model;
	GtkTreeIter            iter;
	gboolean               row_exists;
	GtkWidget             *menu;

	if (event->button != 3) {
		return FALSE;
	}

	priv = GET_PRIV (list);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));
	model = GTK_TREE_MODEL (priv->store);

	gtk_widget_grab_focus (GTK_WIDGET (list));

	row_exists = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (list),
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
		menu = gossip_contact_list_get_contact_menu (list, contact);
		g_object_unref (contact);
	} else {
		menu = gossip_contact_list_get_group_menu (list);
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
contact_list_row_activated_cb (GossipContactList *list,
			       GtkTreePath       *path,
			       GtkTreeViewColumn *col,
			       gpointer           user_data)
{
	GossipContact *contact;
	GtkTreeView   *view;
	GtkTreeModel  *model;
	GtkTreeIter    iter;

	view = GTK_TREE_VIEW (list);
	model = gtk_tree_view_get_model (view);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COL_CONTACT, &contact, -1);

	if (contact) {
		contact_list_action_activated (list, contact);
		g_object_unref (contact);
	}
}

static void
contact_list_row_expand_or_collapse_cb (GossipContactList *list,
					GtkTreeIter       *iter,
					GtkTreePath       *path,
					gpointer           user_data)
{
	GtkTreeModel *model;
	gchar        *name;
	gboolean      expanded;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));

	gtk_tree_model_get (model, iter,
			    COL_NAME, &name,
			    -1);

	expanded = GPOINTER_TO_INT (user_data);
	gossip_contact_group_set_expanded (name, expanded);

	g_free (name);
}

static gint
contact_list_sort_func (GtkTreeModel *model,
			GtkTreeIter  *iter_a,
			GtkTreeIter  *iter_b,
			gpointer      user_data)
{
	gchar         *name_a, *name_b;
	GossipContact *contact_a, *contact_b;
	gboolean       is_separator_a, is_separator_b;
	gint           ret_val;

	gtk_tree_model_get (model, iter_a,
			    COL_NAME, &name_a,
			    COL_CONTACT, &contact_a,
			    COL_IS_SEPARATOR, &is_separator_a,
			    -1);
	gtk_tree_model_get (model, iter_b,
			    COL_NAME, &name_b,
			    COL_CONTACT, &contact_b,
			    COL_IS_SEPARATOR, &is_separator_b,
			    -1);

	/* If contact is NULL it means it's a group. */

	if (is_separator_a || is_separator_b) {
		if (is_separator_a) {
			ret_val = -1;
		} else if (is_separator_b) {
			ret_val = 1;
		}
	} else if (!contact_a && contact_b) {
		ret_val = 1;
	} else if (contact_a && !contact_b) {
		ret_val = -1;
	} else {
		ret_val = g_utf8_collate (name_a, name_b);
	}

	g_free (name_a);
	g_free (name_b);

	if (contact_a) {
		g_object_unref (contact_a);
	}

	if (contact_b) {
		g_object_unref (contact_b);
	}

	return ret_val;
}

static gboolean 
contact_list_filter_show_contact (GossipContact *contact,
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
contact_list_filter_show_group (GossipContactList *list,
				const gchar       *group,
				const gchar       *filter)
{
	GossipContactListPriv *priv;
	GList                 *contacts, *l;
	gchar                 *str;
	gboolean               show_group = FALSE;

	priv = GET_PRIV (list);
	
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
	contacts = empathy_contact_manager_get_contacts (priv->manager);
	for (l = contacts; l && !show_group; l = l->next) {
		if (!gossip_contact_is_in_group (l->data, group)) {
			continue;
		}

		if (contact_list_filter_show_contact (l->data, filter)) {
			show_group = TRUE;
		}
	}
	g_list_foreach (contacts, (GFunc) g_object_unref, NULL);
	g_list_free (contacts);
	g_free (str);

	return show_group;
}

static gboolean
contact_list_filter_func (GtkTreeModel      *model,
			  GtkTreeIter       *iter,
			  GossipContactList *list)
{
	GossipContactListPriv *priv;
	gboolean               is_group;
	gboolean               is_separator;
	gboolean               visible = TRUE;

	priv = GET_PRIV (list);

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
		visible &= contact_list_filter_show_group (list, 
							   name, 
							   priv->filter_text);
		g_free (name);
	} else if (is_separator) {
		/* Do nothing here */
	} else {
		GossipContact *contact;

		/* Check contact id */
		gtk_tree_model_get (model, iter, COL_CONTACT, &contact, -1);
		visible &= contact_list_filter_show_contact (contact, 
							     priv->filter_text);
		g_object_unref (contact);
	}

	return visible;
}

static gboolean
contact_list_iter_equal_contact (GtkTreeModel  *model,
				 GtkTreeIter   *iter,
				 GossipContact *contact)
{
	GossipContact *c;
	gboolean       equal;

	gtk_tree_model_get (model, iter,
			    COL_CONTACT, &c,
			    -1);

	if (!c) {
		return FALSE;
	}

	equal = (c == contact);
	g_object_unref (c);

	return equal;
}

static gboolean
contact_list_find_contact_foreach (GtkTreeModel *model,
				   GtkTreePath  *path,
				   GtkTreeIter  *iter,
				   FindContact  *fc)
{
	if (contact_list_iter_equal_contact (model, iter, fc->contact)) {
		fc->found = TRUE;
		fc->iters = g_list_append (fc->iters, gtk_tree_iter_copy (iter));
	}

	/* We want to find ALL contacts that match, this means if we
	 * have the same contact in 3 groups, all iters should be
	 * returned.
	 */
	return FALSE;
}

static GList *
contact_list_find_contact (GossipContactList *list,
			   GossipContact     *contact)
{
	GossipContactListPriv *priv;
	GtkTreeModel          *model;
	GList                 *l = NULL;
	FindContact            fc;

	priv = GET_PRIV (list);

	memset (&fc, 0, sizeof (fc));

	fc.contact = contact;

	model = GTK_TREE_MODEL (priv->store);
	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) contact_list_find_contact_foreach,
				&fc);

	if (fc.found) {
		l = fc.iters;
	}

	return l;
}

static void
contact_list_action_cb (GtkAction         *action,
			GossipContactList *list)
{
	GossipContact *contact;
	const gchar   *name;
	gchar         *group;

	name = gtk_action_get_name (action);
	if (!name) {
		return;
	}

	gossip_debug (DEBUG_DOMAIN, "Action:'%s' activated", name);

	contact = gossip_contact_list_get_selected (list);
	group = gossip_contact_list_get_selected_group (list);

	if (contact && strcmp (name, "Chat") == 0) {
		contact_list_action_activated (list, contact);
	}
	else if (contact && strcmp (name, "Information") == 0) {
	}
	else if (contact && strcmp (name, "Edit") == 0) {
	}
	else if (contact && strcmp (name, "Remove") == 0) {
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
contact_list_action_activated (GossipContactList *list,
			       GossipContact     *contact)
{
	MissionControl *mc;

	mc = mission_control_new (tp_get_bus ());
	mission_control_request_channel (mc,
					 gossip_contact_get_account (contact),
					 TP_IFACE_CHANNEL_TYPE_TEXT,
					 gossip_contact_get_handle (contact),
					 TP_HANDLE_TYPE_CONTACT,
					 NULL, NULL);
	g_object_unref (mc);
}

static gboolean
contact_list_update_list_mode_foreach (GtkTreeModel      *model,
				       GtkTreePath       *path,
				       GtkTreeIter       *iter,
				       GossipContactList *list)
{
	GossipContactListPriv *priv;
	gboolean               show_avatar = FALSE;

	priv = GET_PRIV (list);

	if (priv->show_avatars && !priv->is_compact) {
		show_avatar = TRUE;
	}

	gtk_tree_store_set (priv->store, iter,
			    COL_PIXBUF_AVATAR_VISIBLE, show_avatar,
			    COL_STATUS_VISIBLE, !priv->is_compact,
			    -1);

	return FALSE;
}

GossipContactList *
gossip_contact_list_new (void)
{
	return g_object_new (GOSSIP_TYPE_CONTACT_LIST, NULL);
}

GossipContact *
gossip_contact_list_get_selected (GossipContactList *list)
{
	GossipContactListPriv *priv;
	GtkTreeSelection      *selection;
	GtkTreeIter            iter;
	GtkTreeModel          *model;
	GossipContact         *contact;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter, COL_CONTACT, &contact, -1);

	return contact;
}

gchar *
gossip_contact_list_get_selected_group (GossipContactList *list)
{
	GossipContactListPriv *priv;
	GtkTreeSelection      *selection;
	GtkTreeIter            iter;
	GtkTreeModel          *model;
	gboolean               is_group;
	gchar                 *name;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST (list), NULL);

	priv = GET_PRIV (list);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));
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

gboolean
gossip_contact_list_get_show_offline (GossipContactList *list)
{
	GossipContactListPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST (list), FALSE);

	priv = GET_PRIV (list);

	return priv->show_offline;
}

gboolean
gossip_contact_list_get_show_avatars (GossipContactList *list)
{
	GossipContactListPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST (list), TRUE);

	priv = GET_PRIV (list);

	return priv->show_avatars;
}

gboolean
gossip_contact_list_get_is_compact (GossipContactList *list)
{
	GossipContactListPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONTACT_LIST (list), TRUE);

	priv = GET_PRIV (list);

	return priv->is_compact;
}

void
gossip_contact_list_set_show_offline (GossipContactList *list,
				      gboolean           show_offline)
{
	GossipContactListPriv *priv;
	GList                 *contacts, *l;
	gboolean               show_active;

	g_return_if_fail (GOSSIP_IS_CONTACT_LIST (list));

	priv = GET_PRIV (list);

	priv->show_offline = show_offline;
	show_active = priv->show_active;

	/* Disable temporarily. */
	priv->show_active = FALSE;

	contacts = empathy_contact_manager_get_contacts (priv->manager);
	for (l = contacts; l; l = l->next) {
		GossipContact *contact;

		contact = GOSSIP_CONTACT (l->data);

		contact_list_contact_update (list, contact);
		
		g_object_unref (contact);
	}
	g_list_free (contacts);

	/* Restore to original setting. */
	priv->show_active = show_active;
}

void
gossip_contact_list_set_show_avatars (GossipContactList *list,
				      gboolean           show_avatars)
{
	GossipContactListPriv *priv;
	GtkTreeModel          *model;

	g_return_if_fail (GOSSIP_IS_CONTACT_LIST (list));

	priv = GET_PRIV (list);

	priv->show_avatars = show_avatars;

	model = GTK_TREE_MODEL (priv->store);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc)
				contact_list_update_list_mode_foreach,
				list);
}

void
gossip_contact_list_set_is_compact (GossipContactList *list,
				    gboolean           is_compact)
{
	GossipContactListPriv *priv;
	GtkTreeModel          *model;

	g_return_if_fail (GOSSIP_IS_CONTACT_LIST (list));

	priv = GET_PRIV (list);

	priv->is_compact = is_compact;

	model = GTK_TREE_MODEL (priv->store);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc)
				contact_list_update_list_mode_foreach,
				list);
}

void
gossip_contact_list_set_filter (GossipContactList *list,
				const gchar       *filter)
{
	GossipContactListPriv *priv;

	g_return_if_fail (GOSSIP_IS_CONTACT_LIST (list));

	priv = GET_PRIV (list);

	g_free (priv->filter_text);
	if (filter) {
		priv->filter_text = g_utf8_casefold (filter, -1);
	} else {
		priv->filter_text = NULL;
	}

	gossip_debug (DEBUG_DOMAIN, "Refiltering with filter:'%s' (case folded)", filter);
	gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter));
}
