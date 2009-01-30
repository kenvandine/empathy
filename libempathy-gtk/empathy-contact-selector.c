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
#include <libempathy/empathy-utils.h>

#include "empathy-contact-selector.h"

G_DEFINE_TYPE (EmpathyContactSelector, empathy_contact_selector,
    GTK_TYPE_COMBO_BOX)

enum
{
  PROP_0,
  PROP_CONTACT_LIST
};

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyContactSelector)
typedef struct
{
  EmpathyContactList *contact_list;
  EmpathyContactListStore *store;
  gboolean dispose_run;
} EmpathyContactSelectorPriv;

static void contact_selector_manage_blank_contact (
    EmpathyContactSelector *selector);

static guint
contact_selector_get_number_online_contacts (GtkTreeStore *store)
{
  GtkTreeIter tmp_iter;
  gboolean is_online;
  guint number_online_contacts = 0;
  gboolean ok;

  for (ok = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &tmp_iter);
      ok; ok = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &tmp_iter))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (store),
          &tmp_iter, EMPATHY_CONTACT_LIST_STORE_COL_IS_ONLINE,
          &is_online, -1);
      if (is_online)
        number_online_contacts++;
    }

  return number_online_contacts;
}

static gboolean
contact_selector_get_iter_for_blank_contact (GtkTreeStore *store,
                                             GtkTreeIter *blank_iter)
{
  GtkTreeIter tmp_iter;
  EmpathyContact *tmp_contact;
  gboolean is_present = FALSE;
  gboolean ok;

  for (ok = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &tmp_iter);
      ok; ok = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &tmp_iter))
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
      g_object_unref (tmp_contact);
    }

  return is_present;
}

static void
contact_selector_add_blank_contact (EmpathyContactSelector *selector)
{
  EmpathyContactSelectorPriv *priv = GET_PRIV (selector);
  GtkTreeIter blank_iter;

  gtk_tree_store_insert_with_values (
      GTK_TREE_STORE (priv->store),&blank_iter, NULL, 0,
      EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, NULL,
      EMPATHY_CONTACT_LIST_STORE_COL_NAME, (_("Select a contact")),
      EMPATHY_CONTACT_LIST_STORE_COL_IS_ONLINE, FALSE, -1);
  g_signal_handlers_block_by_func (selector,
      contact_selector_manage_blank_contact, selector);
  gtk_combo_box_set_active_iter (GTK_COMBO_BOX (selector), &blank_iter);
  g_signal_handlers_unblock_by_func (selector,
      contact_selector_manage_blank_contact, selector);
}

static void
contact_selector_remove_blank_contact (EmpathyContactSelector *selector)
{
  EmpathyContactSelectorPriv *priv = GET_PRIV (selector);
  GtkTreeIter blank_iter;

  if (contact_selector_get_iter_for_blank_contact (
        GTK_TREE_STORE (priv->store), &blank_iter))
    gtk_tree_store_remove (GTK_TREE_STORE (priv->store), &blank_iter);
}

static void
contact_selector_manage_sensitivity (EmpathyContactSelector *selector)
{
  EmpathyContactSelectorPriv *priv = GET_PRIV (selector);
  guint number_online_contacts;

  number_online_contacts = contact_selector_get_number_online_contacts (
      GTK_TREE_STORE (priv->store));

  if (number_online_contacts != 0)
    gtk_widget_set_sensitive (GTK_WIDGET (selector), TRUE);
  else
    gtk_widget_set_sensitive (GTK_WIDGET (selector), FALSE);
}

static void
contact_selector_manage_blank_contact (EmpathyContactSelector *selector)
{
  gboolean is_popup_shown;

  g_object_get (selector, "popup-shown", &is_popup_shown, NULL);

  if (is_popup_shown)
    {
      contact_selector_remove_blank_contact (selector);
    }
  else
    {
      if (gtk_combo_box_get_active (GTK_COMBO_BOX (selector)) == -1)
        {
          contact_selector_add_blank_contact (selector);
        }
      else
        {
          contact_selector_remove_blank_contact (selector);
        }
    }

  contact_selector_manage_sensitivity (selector);
}

static GObject *
contact_selector_constructor (GType type,
                              guint n_construct_params,
                              GObjectConstructParam *construct_params)
{
  GObject *object;
  EmpathyContactSelector *contact_selector;
  EmpathyContactSelectorPriv *priv;
  GtkCellLayout *cell_layout;
  GtkCellRenderer *renderer;

  object = G_OBJECT_CLASS (empathy_contact_selector_parent_class)->constructor 
    (type, n_construct_params, construct_params);
  priv = GET_PRIV (object);
  contact_selector = EMPATHY_CONTACT_SELECTOR (object);
  cell_layout = GTK_CELL_LAYOUT (object);

  priv->store = empathy_contact_list_store_new (priv->contact_list);

  g_object_set (priv->store, "is-compact", TRUE, "show-avatars", FALSE,
      "show-offline", FALSE, "show-groups", FALSE,
      "sort-criterium", EMPATHY_CONTACT_LIST_STORE_SORT_NAME, NULL);

  g_signal_connect_swapped (priv->store, "row-changed",
      G_CALLBACK (contact_selector_manage_sensitivity),
      contact_selector);
  g_signal_connect_swapped (contact_selector, "changed",
      G_CALLBACK (contact_selector_manage_blank_contact),
      contact_selector);
  g_signal_connect_swapped (contact_selector, "notify::popup-shown",
      G_CALLBACK (contact_selector_manage_blank_contact),
      contact_selector);

  gtk_combo_box_set_model (GTK_COMBO_BOX (contact_selector),
      GTK_TREE_MODEL (priv->store));
  gtk_widget_set_sensitive (GTK_WIDGET (contact_selector), FALSE);

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (cell_layout, renderer, FALSE);
  gtk_cell_layout_set_attributes (cell_layout, renderer,
      "icon-name", EMPATHY_CONTACT_LIST_STORE_COL_ICON_STATUS, NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (cell_layout, renderer, TRUE);
  gtk_cell_layout_set_attributes (cell_layout, renderer,
      "text", EMPATHY_CONTACT_LIST_STORE_COL_NAME, NULL);

  contact_selector_manage_blank_contact (contact_selector);
  contact_selector_manage_sensitivity (contact_selector);

  return object;
}

static void
empathy_contact_selector_init (EmpathyContactSelector *empathy_contact_selector)
{
  EmpathyContactSelectorPriv *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (empathy_contact_selector,
      EMPATHY_TYPE_CONTACT_SELECTOR, EmpathyContactSelectorPriv);

  empathy_contact_selector->priv = priv;

  priv->dispose_run = FALSE;
}

static void
contact_selector_set_property (GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
  EmpathyContactSelectorPriv *priv = GET_PRIV (object);

  switch (prop_id)
    {
      case PROP_CONTACT_LIST:
        priv->contact_list = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
contact_selector_get_property (GObject *object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
  EmpathyContactSelectorPriv *priv = GET_PRIV (object);

  switch (prop_id)
    {
      case PROP_CONTACT_LIST:
        g_value_set_object (value, priv->contact_list);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
contact_selector_dispose (GObject *object)
{
  EmpathyContactSelector *selector = EMPATHY_CONTACT_SELECTOR (object);
  EmpathyContactSelectorPriv *priv = GET_PRIV (selector);

  if (priv->dispose_run)
    return;

  priv->dispose_run = TRUE;

  if (priv->contact_list)
    {
      g_object_unref (priv->contact_list);
      priv->contact_list = NULL;
    }

  if (priv->store)
    {
      g_object_unref (priv->store);
      priv->store = NULL;
    }

  (G_OBJECT_CLASS (empathy_contact_selector_parent_class)->dispose) (object);
}

static void
empathy_contact_selector_class_init (EmpathyContactSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->constructor = contact_selector_constructor;
  object_class->dispose = contact_selector_dispose;
  object_class->set_property = contact_selector_set_property;
  object_class->get_property = contact_selector_get_property;
  g_type_class_add_private (klass, sizeof (EmpathyContactSelectorPriv));

  g_object_class_install_property (object_class, PROP_CONTACT_LIST,
      g_param_spec_object ("contact-list", "contact list", "contact list",
      EMPATHY_TYPE_CONTACT_LIST, G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_READWRITE | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

/* public methods */

GtkWidget *
empathy_contact_selector_new (EmpathyContactList *contact_list)
{
  g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (contact_list), NULL);

  return GTK_WIDGET (g_object_new (EMPATHY_TYPE_CONTACT_SELECTOR,
      "contact-list", contact_list, NULL));
}

EmpathyContact *
empathy_contact_selector_dup_selected (EmpathyContactSelector *selector)
{
  EmpathyContactSelectorPriv *priv = GET_PRIV (selector);
  EmpathyContact *contact = NULL;
  GtkTreeIter iter;

  g_return_val_if_fail (EMPATHY_IS_CONTACT_SELECTOR (selector), NULL);

  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (selector), &iter))
    return NULL;

  gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
      EMPATHY_CONTACT_LIST_STORE_COL_CONTACT, &contact, -1);

  return contact;
}
