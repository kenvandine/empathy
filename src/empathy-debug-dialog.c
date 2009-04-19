/*
*  Copyright (C) 2009 Collabora Ltd.
*
*  This library is free software; you can redistribute it and/or
*  modify it under the terms of the GNU Lesser General Public
*  License as published by the Free Software Foundation; either
*  version 2.1 of the License, or (at your option) any later version.
*
*  This library is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  Lesser General Public License for more details.
*
*  You should have received a copy of the GNU Lesser General Public
*  License along with this library; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
*  Authors: Jonny Lamb <jonny.lamb@collabora.co.uk>
*/

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libempathy/empathy-utils.h>

#include "empathy-debug-dialog.h"

G_DEFINE_TYPE (EmpathyDebugDialog, empathy_debug_dialog,
    GTK_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_PARENT
};

enum
{
  COL_TIMESTAMP = 0,
  COL_DOMAIN,
  COL_CATEGORY,
  COL_LEVEL,
  COL_MESSAGE,
  NUM_COLS
};

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyDebugDialog)
typedef struct
{
  GtkWidget *filter;
  GtkWindow *parent;
  GtkWidget *view;
  GtkListStore *store;
  gboolean dispose_run;
} EmpathyDebugDialogPriv;

static GObject *
debug_dialog_constructor (GType type,
                          guint n_construct_params,
                          GObjectConstructParam *construct_params)
{
  GObject *object;
  EmpathyDebugDialogPriv *priv;
  GtkWidget *vbox;
  GtkWidget *toolbar;
  GtkWidget *image;
  GtkToolItem *item;
  GtkCellRenderer *renderer;
  GtkWidget *scrolled_win;

  /* tmp */
  GtkTreeIter iter;

  object = G_OBJECT_CLASS (empathy_debug_dialog_parent_class)->constructor
    (type, n_construct_params, construct_params);
  priv = GET_PRIV (object);

  gtk_window_set_title (GTK_WINDOW (object), _("Debug Window"));
  gtk_window_set_default_size (GTK_WINDOW (object), 800, 400);
  gtk_window_set_transient_for (GTK_WINDOW (object), priv->parent);

  vbox = GTK_DIALOG (object)->vbox;

  toolbar = gtk_toolbar_new ();
  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
  gtk_toolbar_set_show_arrow (GTK_TOOLBAR (toolbar), TRUE);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_widget_show (toolbar);

  gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);

  /* Save */
  item = gtk_tool_button_new_from_stock (GTK_STOCK_SAVE);
  gtk_widget_show (GTK_WIDGET (item));
  gtk_tool_item_set_is_important (GTK_TOOL_ITEM (item), TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  /* Clear */
  item = gtk_tool_button_new_from_stock (GTK_STOCK_CLEAR);
  gtk_widget_show (GTK_WIDGET (item));
  gtk_tool_item_set_is_important (GTK_TOOL_ITEM (item), TRUE);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  item = gtk_separator_tool_item_new ();
  gtk_widget_show (GTK_WIDGET (item));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  /* Pause */
  image = gtk_image_new_from_stock (GTK_STOCK_MEDIA_PAUSE, GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  item = gtk_toggle_tool_button_new ();
  gtk_widget_show (GTK_WIDGET (item));
  gtk_tool_item_set_is_important (GTK_TOOL_ITEM (item), TRUE);
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), _("Pause"));
  gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (item), image);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  item = gtk_separator_tool_item_new ();
  gtk_widget_show (GTK_WIDGET (item));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  /* Level */
  item = gtk_tool_item_new ();
  gtk_widget_show (GTK_WIDGET (item));
  gtk_container_add (GTK_CONTAINER (item), gtk_label_new (_("Level ")));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  priv->filter = gtk_combo_box_new_text ();
  gtk_widget_show (priv->filter);

  item = gtk_tool_item_new ();
  gtk_widget_show (GTK_WIDGET (item));
  gtk_container_add (GTK_CONTAINER (item), priv->filter);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->filter), _("All"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->filter), _("Debug"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->filter), _("Info"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->filter), _("Message"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->filter), _("Warning"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->filter), _("Critical"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (priv->filter), _("Error"));

  gtk_combo_box_set_active (GTK_COMBO_BOX (priv->filter), 0);
  gtk_widget_show (GTK_WIDGET (priv->filter));

  /* Debug treeview */
  priv->view = gtk_tree_view_new ();

  renderer = gtk_cell_renderer_text_new ();

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->view),
      -1, _("Time"), renderer, "text", COL_TIMESTAMP, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->view),
      -1, _("Domain"), renderer, "text", COL_DOMAIN, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->view),
      -1, _("Category"), renderer, "text", COL_CATEGORY, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->view),
      -1, _("Level"), renderer, "text", COL_LEVEL, NULL);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->view),
      -1, _("Message"), renderer, "text", COL_MESSAGE, NULL);

  priv->store = gtk_list_store_new (NUM_COLS, G_TYPE_DOUBLE,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  gtk_list_store_append (priv->store, &iter);
  gtk_list_store_set (priv->store, &iter,
      COL_TIMESTAMP, 2.0,
      COL_DOMAIN, "domain",
      COL_CATEGORY, "category",
      COL_LEVEL, "level",
      COL_MESSAGE, "message",
      -1);

  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->view),
      GTK_TREE_MODEL (priv->store));

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_widget_show (priv->view);
  gtk_container_add (GTK_CONTAINER (scrolled_win), priv->view);

  gtk_widget_show (scrolled_win);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);

  gtk_widget_show (GTK_WIDGET (object));

  return object;
}

static void
empathy_debug_dialog_init (EmpathyDebugDialog *empathy_debug_dialog)
{
  EmpathyDebugDialogPriv *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (empathy_debug_dialog,
      EMPATHY_TYPE_DEBUG_DIALOG, EmpathyDebugDialogPriv);

  empathy_debug_dialog->priv = priv;

  priv->dispose_run = FALSE;
}

static void
debug_dialog_set_property (GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
  EmpathyDebugDialogPriv *priv = GET_PRIV (object);

  switch (prop_id)
    {
      case PROP_PARENT:
	priv->parent = GTK_WINDOW (g_value_dup_object (value));
	break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
debug_dialog_get_property (GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
  EmpathyDebugDialogPriv *priv = GET_PRIV (object);

  switch (prop_id)
    {
      case PROP_PARENT:
	g_value_set_object (value, priv->parent);
	break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
debug_dialog_dispose (GObject *object)
{
  EmpathyDebugDialog *selector = EMPATHY_DEBUG_DIALOG (object);
  EmpathyDebugDialogPriv *priv = GET_PRIV (selector);

  if (priv->dispose_run)
    return;

  priv->dispose_run = TRUE;

  if (priv->parent)
    g_object_unref (priv->parent);

  if (priv->store)
    g_object_unref (priv->store);

  (G_OBJECT_CLASS (empathy_debug_dialog_parent_class)->dispose) (object);
}

static void
empathy_debug_dialog_class_init (EmpathyDebugDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->constructor = debug_dialog_constructor;
  object_class->dispose = debug_dialog_dispose;
  object_class->set_property = debug_dialog_set_property;
  object_class->get_property = debug_dialog_get_property;
  g_type_class_add_private (klass, sizeof (EmpathyDebugDialogPriv));

  g_object_class_install_property (object_class, PROP_PARENT,
      g_param_spec_object ("parent", "parent", "parent",
      GTK_TYPE_WINDOW, G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_READWRITE | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

/* public methods */

GtkWidget *
empathy_debug_dialog_new (GtkWindow *parent)
{
  return GTK_WIDGET (g_object_new (EMPATHY_TYPE_DEBUG_DIALOG,
      "parent", parent, NULL));
}
