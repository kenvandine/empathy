/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 * Copyright (C) 2007-2009 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 *          Jonny Lamb <jonny.lamb@collabora.co.uk>
 */

#include <config.h>

#include <string.h>

#include <gtk/gtk.h>
#include <libmissioncontrol/mc-profile.h>
#include <libmissioncontrol/mc-protocol.h>

#include <libempathy/empathy-utils.h>

#include "empathy-profile-chooser.h"
#include "empathy-ui-utils.h"

/**
 * SECTION:empathy-profile-chooser
 * @title: EmpathyProfileChooser
 * @short_description: A widget used to choose from a list of profiles
 * @include: libempathy-gtk/empathy-account-chooser.h
 *
 * #EmpathyProfileChooser is a widget which extends #GtkComboBox to provides a
 * chooser of available profiles.
 */

/**
 * EmpathyProfileChooser:
 * @parent: parent object
 *
 * Widget which extends #GtkComboBox to provide a chooser of available
 * profiles.
 */

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyProfileChooser)
typedef struct
{
  GtkListStore *store;
  gboolean dispose_run;
} EmpathyProfileChooserPriv;

enum
{
  COL_ICON,
  COL_LABEL,
  COL_PROFILE,
  COL_COUNT
};

G_DEFINE_TYPE (EmpathyProfileChooser, empathy_profile_chooser,
    GTK_TYPE_COMBO_BOX);

static gint
profile_chooser_sort_profile_value (McProfile *profile)
{
  guint i;
  const gchar *profile_name;
  const gchar *names[] = {
    "jabber",
    "salut",
    "gtalk",
    NULL
  };

  profile_name = mc_profile_get_unique_name (profile);

  for (i = 0 ; names[i]; i++)
    {
      if (strcmp (profile_name, names[i]) == 0)
        return i;
    }

  return i;
}

static gint
profile_chooser_sort_func (GtkTreeModel *model,
    GtkTreeIter  *iter_a,
    GtkTreeIter  *iter_b,
    gpointer      user_data)
{
  McProfile *profile_a;
  McProfile *profile_b;
  gint cmp;

  gtk_tree_model_get (model, iter_a,
      COL_PROFILE, &profile_a,
      -1);
  gtk_tree_model_get (model, iter_b,
      COL_PROFILE, &profile_b,
      -1);

  cmp = profile_chooser_sort_profile_value (profile_a);
  cmp -= profile_chooser_sort_profile_value (profile_b);
  if (cmp == 0)
    {
      cmp = strcmp (mc_profile_get_display_name (profile_a),
          mc_profile_get_display_name (profile_b));
    }

  g_object_unref (profile_a);
  g_object_unref (profile_b);

  return cmp;
}

static GObject *
profile_chooser_constructor (GType type,
    guint n_construct_params,
    GObjectConstructParam *construct_params)
{
  GObject *object;
  EmpathyProfileChooser *profile_chooser;
  EmpathyProfileChooserPriv *priv;

  GList *profiles, *l, *seen;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  gboolean iter_set = FALSE;
  McManager *btf_cm;

  object = G_OBJECT_CLASS (empathy_profile_chooser_parent_class)->constructor (
      type, n_construct_params, construct_params);
  priv = GET_PRIV (object);
  profile_chooser = EMPATHY_PROFILE_CHOOSER (object);

  /* set up combo box with new store */
  priv->store = gtk_list_store_new (COL_COUNT,
          G_TYPE_STRING,    /* Icon name */
          G_TYPE_STRING,    /* Label     */
          MC_TYPE_PROFILE); /* Profile   */

  gtk_combo_box_set_model (GTK_COMBO_BOX (object),
      GTK_TREE_MODEL (priv->store));

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (object), renderer, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (object), renderer,
      "icon-name", COL_ICON,
      NULL);
  g_object_set (renderer, "stock-size", GTK_ICON_SIZE_BUTTON, NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (object), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (object), renderer,
      "text", COL_LABEL,
      NULL);

  btf_cm = mc_manager_lookup ("butterfly");
  profiles = mc_profiles_list ();
  seen = NULL;
  for (l = profiles; l; l = g_list_next (l))
    {
      McProfile   *profile;
      McProtocol  *protocol;
      const gchar *unique_name;

      profile = l->data;

      /* Check if the CM is installed, otherwise skip that profile.
       * Workaround SF bug #1688779 */
      protocol = mc_profile_get_protocol (profile);
      if (!protocol)
        continue;

      g_object_unref (protocol);

      /* Skip MSN-Haze if we have butterfly */
      unique_name = mc_profile_get_unique_name (profile);
      if (btf_cm && strcmp (unique_name, "msn-haze") == 0)
        continue;

      if (g_list_find_custom (seen, unique_name, (GCompareFunc) strcmp))
        continue;

      seen = g_list_append (seen, (char *) unique_name);

      gtk_list_store_insert_with_values (priv->store, &iter, 0,
          COL_ICON, mc_profile_get_icon_name (profile),
          COL_LABEL, mc_profile_get_display_name (profile),
          COL_PROFILE, profile,
          -1);
      iter_set = TRUE;
    }

  g_list_free (seen);

  if (btf_cm)
    g_object_unref (btf_cm);

  /* Set the profile sort function */
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (priv->store),
      COL_PROFILE,
      profile_chooser_sort_func,
      NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->store),
      COL_PROFILE,
      GTK_SORT_ASCENDING);

  if (iter_set)
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (object), &iter);

  mc_profiles_free_list (profiles);

  return object;
}

static void
empathy_profile_chooser_init (EmpathyProfileChooser *profile_chooser)
{
  EmpathyProfileChooserPriv *priv =
    G_TYPE_INSTANCE_GET_PRIVATE (profile_chooser,
        EMPATHY_TYPE_PROFILE_CHOOSER, EmpathyProfileChooserPriv);

  priv->dispose_run = FALSE;

  profile_chooser->priv = priv;
}

static void
profile_chooser_dispose (GObject *object)
{
  EmpathyProfileChooser *profile_chooser = EMPATHY_PROFILE_CHOOSER (object);
  EmpathyProfileChooserPriv *priv = GET_PRIV (profile_chooser);

  if (priv->dispose_run)
    return;

  priv->dispose_run = TRUE;

  if (priv->store)
    {
      g_object_unref (priv->store);
      priv->store = NULL;
    }

  (G_OBJECT_CLASS (empathy_profile_chooser_parent_class)->dispose) (object);
}

static void
empathy_profile_chooser_class_init (EmpathyProfileChooserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = profile_chooser_constructor;
  object_class->dispose = profile_chooser_dispose;

  g_type_class_add_private (object_class, sizeof (EmpathyProfileChooserPriv));
}

/**
 * empathy_profile_chooser_dup_selected:
 * @profile_chooser: an #EmpathyProfileChooser
 *
 * Returns a new reference to the selected #McProfile in @profile_chooser. The
 * returned #McProfile should be unrefed with g_object_unref() when finished
 * with.
 *
 * Return value: a new reference to the selected #McProfile
 */
McProfile *
empathy_profile_chooser_dup_selected (EmpathyProfileChooser *profile_chooser)
{
  EmpathyProfileChooserPriv *priv = GET_PRIV (profile_chooser);
  GtkTreeIter iter;
  McProfile *profile = NULL;

  g_return_val_if_fail (EMPATHY_IS_PROFILE_CHOOSER (profile_chooser), NULL);

  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (profile_chooser), &iter))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
          COL_PROFILE, &profile,
          -1);
    }

  return profile;
}

/**
 * empathy_profile_chooser_n_profiles:
 * @profile_chooser: an #EmpathyProfileChooser
 *
 * Returns the number of profiles in @profile_chooser.
 *
 * Return value: the number of profiles in @profile_chooser
 */
gint
empathy_profile_chooser_n_profiles (EmpathyProfileChooser *profile_chooser)
{
  EmpathyProfileChooserPriv *priv = GET_PRIV (profile_chooser);

  g_return_val_if_fail (EMPATHY_IS_PROFILE_CHOOSER (profile_chooser), 0);

  return gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->store), NULL);
}

/**
 * empathy_profile_chooser_new:
 *
 * Creates a new #EmpathyProfileChooser widget.
 *
 * Return value: a new #EmpathyProfileChooser widget
 */
GtkWidget *
empathy_profile_chooser_new (void)
{
  return GTK_WIDGET (g_object_new (EMPATHY_TYPE_PROFILE_CHOOSER, NULL));
}
