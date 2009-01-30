/*
*  Copyright (C) 2007 Marco Barisione <marco@barisione.org>
*  Copyright (C) 2008 Collabora Ltd.
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
*  Authors: Marco Barisione <marco@barisione.org>
*           Elliot Fairweather <elliot.fairweather@collabora.co.uk>
*/

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libempathy/empathy-contact.h>
#include <libempathy-gtk/empathy-contact-list-store.h>
#include "empathy-contact-selector.h"

G_DEFINE_TYPE (EmpathyContactSelector, empathy_contact_selector,
    GTK_TYPE_COMBO_BOX)

#define GET_PRIV(object) (G_TYPE_INSTANCE_GET_PRIVATE \
    ((object), EMPATHY_TYPE_CONTACT_SELECTOR, EmpathyContactSelectorPriv))

enum
{
  PROP_0,
  PROP_STORE
};

typedef struct _EmpathyContactSelectorPriv EmpathyContactSelectorPriv;

struct _EmpathyContactSelectorPriv
{
  EmpathyContactListStore *store;
  gboolean is_blank_set;
};

static void changed_cb (GtkComboBox *widget, gpointer data);


EmpathyContact *
empathy_contact_selector_get_selected (EmpathyContactSelector *selector)
{
  EmpathyContactSelectorPriv *priv = GET_PRIV (selector);
  EmpathyContact *contact = NULL;
  GtkTreeIter iter;

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (selector), &iter))
    return NULL;

  gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
      EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, &contact, -1);

  return contact;
}


static gboolean
get_iter_for_blank_contact (GtkTreeStore *store,
                            GtkTreeIter *blank_iter)
{
  GtkTreePath *path;
  GtkTreeIter tmp_iter;
  EmpathyContact *tmp_contact;
  gboolean is_present = FALSE;

  path = gtk_tree_path_new_first ();
  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &tmp_iter, path))
    {
      do
        {
          gtk_tree_model_get (GTK_TREE_MODEL (store),
              &tmp_iter, EMPATHY_CONTACT_LIST_STORE_COL_CONTACT,
              &tmp_contact, -1);
          if (tmp_contact == NULL)
            {
              *blank_iter = tmp_iter;
              is_present = TRUE;
              break;
            }
        } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &tmp_iter));
    }

  gtk_tree_path_free (path);

  return is_present;
}


static void
set_blank_contact (EmpathyContactSelector *selector)
{
  EmpathyContactSelectorPriv *priv = GET_PRIV (selector);
  GtkTreeIter blank_iter;

  gtk_tree_store_insert (GTK_TREE_STORE (priv->store), &blank_iter, NULL, 0);
  gtk_tree_store_set (GTK_TREE_STORE (priv->store), &blank_iter,
      EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, NULL,
      EMPATHY_CONTACT_LIST_STORE_COL_NAME, ("Select a contact"), -1);
  g_signal_handlers_block_by_func(selector, changed_cb, NULL);
  gtk_combo_box_set_active_iter (GTK_COMBO_BOX (selector), &blank_iter);
  g_signal_handlers_unblock_by_func(selector, changed_cb, NULL);
  priv->is_blank_set = TRUE;
}


static void
unset_blank_contact (EmpathyContactSelector *selector)
{
  EmpathyContactSelectorPriv *priv = GET_PRIV (selector);
  GtkTreeIter blank_iter;

  if (get_iter_for_blank_contact (GTK_TREE_STORE (priv->store), &blank_iter))
    {
      gtk_tree_store_remove (GTK_TREE_STORE (priv->store), &blank_iter);
      priv->is_blank_set = FALSE;
    }
}


static void
manage_sensitivity (EmpathyContactSelector *selector)
{
  EmpathyContactSelectorPriv *priv = GET_PRIV (selector);
  gint children;

  children = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->store), NULL);
  g_print ("Number of children %d\n", children);

  if (children == 1 && priv->is_blank_set)
      gtk_widget_set_sensitive (GTK_WIDGET (selector), FALSE);
  else if (children)
      gtk_widget_set_sensitive (GTK_WIDGET (selector), TRUE);
  else
      gtk_widget_set_sensitive (GTK_WIDGET (selector), FALSE);
}


static void
manage_blank_contact (EmpathyContactSelector *selector)
{
  gboolean is_popup_shown;

  g_object_get (selector, "popup-shown", &is_popup_shown, NULL);

  if (is_popup_shown)
    {
      unset_blank_contact (selector);
    }
  else
    {
      if (gtk_combo_box_get_active (GTK_COMBO_BOX (selector)) == -1)
        {
          set_blank_contact (selector);
        }
      else
        {
          unset_blank_contact (selector);
        }
    }

    manage_sensitivity (selector);
}


static void
notify_popup_shown_cb (GtkComboBox *widget,
                       GParamSpec *property,
                       gpointer data)
{
  EmpathyContactSelector *selector = EMPATHY_CONTACT_SELECTOR (widget);

  manage_blank_contact (selector);
}


static void
changed_cb (GtkComboBox *widget,
            gpointer data)
{
  EmpathyContactSelector *selector = EMPATHY_CONTACT_SELECTOR (widget);

  manage_blank_contact (selector);
}


static void
empathy_store_row_changed_cb (EmpathyContactListStore *empathy_store,
                              GtkTreePath *empathy_path,
                              GtkTreeIter *empathy_iter,
                              gpointer data)
{
  EmpathyContactSelector *selector = EMPATHY_CONTACT_SELECTOR (data);

  g_print ("Row changed\n");
  manage_sensitivity (selector);
}


static GObject *
empathy_contact_selector_constructor (GType type,
                                      guint n_construct_params,
                                      GObjectConstructParam *construct_params)
{
  GObject *object =
      G_OBJECT_CLASS (empathy_contact_selector_parent_class)->constructor (type,
      n_construct_params, construct_params);
  EmpathyContactSelector *contact_selector = EMPATHY_CONTACT_SELECTOR (object);
  EmpathyContactSelectorPriv *priv = GET_PRIV (contact_selector);
  GtkCellRenderer *renderer;

  g_object_set (priv->store, "is-compact", TRUE, "show-avatars", FALSE,
      "show-offline", FALSE, "show-groups", FALSE,
      "sort-criterium", EMPATHY_CONTACT_LIST_STORE_SORT_NAME, NULL);

  g_signal_connect (priv->store, "row-changed",
      G_CALLBACK (empathy_store_row_changed_cb), (gpointer) contact_selector);
  g_signal_connect (GTK_COMBO_BOX (contact_selector), "changed",
      G_CALLBACK (changed_cb), NULL);
  g_signal_connect (GTK_COMBO_BOX (contact_selector), "notify::popup-shown",
      G_CALLBACK (notify_popup_shown_cb), NULL);

  gtk_combo_box_set_model (GTK_COMBO_BOX (contact_selector),
      GTK_TREE_MODEL (priv->store));
  gtk_widget_set_sensitive (GTK_WIDGET (contact_selector), FALSE);

  /* Status icon */
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (contact_selector),
      renderer, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (contact_selector), renderer,
      "icon-name", EMPATHY_CONTACT_LIST_STORE_COL_ICON_STATUS, NULL);

  /* Contact name */
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (contact_selector),
      renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (contact_selector), renderer,
      "text", EMPATHY_CONTACT_LIST_STORE_COL_NAME, NULL);

  manage_blank_contact (contact_selector);
  manage_sensitivity (contact_selector);

  object = G_OBJECT (contact_selector);
  return object;
}


static void
empathy_contact_selector_init (EmpathyContactSelector *empathy_contact_selector)
{
}


static void
empathy_contact_selector_set_property (GObject *object,
                                       guint prop_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
  EmpathyContactSelector *contact_selector = EMPATHY_CONTACT_SELECTOR (object);
  EmpathyContactSelectorPriv *priv = GET_PRIV (contact_selector);

  switch (prop_id)
    {
      case PROP_STORE:
        priv->store = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}


static void
empathy_contact_selector_get_property (GObject *object,
                                       guint prop_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
  EmpathyContactSelector *contact_selector = EMPATHY_CONTACT_SELECTOR (object);
  EmpathyContactSelectorPriv *priv = GET_PRIV (contact_selector);

  switch (prop_id)
    {
      case PROP_STORE:
        g_value_set_object (value, priv->store);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}


static void
empathy_contact_selector_dispose (GObject *object)
{
  g_debug ("EmpathyContactSelector - dispose: %p",  object);

  (G_OBJECT_CLASS (empathy_contact_selector_parent_class)->dispose) (object);
}


static void
empathy_contact_selector_finalize (GObject *object)
{
  g_debug ("EmpathyContactSelector - finalize: %p",  object);

  (G_OBJECT_CLASS (empathy_contact_selector_parent_class)->finalize) (object);
}


static void
empathy_contact_selector_class_init (EmpathyContactSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->constructor = empathy_contact_selector_constructor;
  object_class->dispose = empathy_contact_selector_dispose;
  object_class->finalize = empathy_contact_selector_finalize;
  object_class->set_property = empathy_contact_selector_set_property;
  object_class->get_property = empathy_contact_selector_get_property;
  g_type_class_add_private (klass, sizeof (EmpathyContactSelectorPriv));

  g_object_class_install_property (object_class, PROP_STORE,
      g_param_spec_object ("store", "store", "store",
      EMPATHY_TYPE_CONTACT_LIST_STORE, G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_READWRITE | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}


EmpathyContactSelector *
empathy_contact_selector_new (EmpathyContactListStore *store)
{
  return g_object_new (EMPATHY_TYPE_CONTACT_SELECTOR, "store", store, NULL);
}
