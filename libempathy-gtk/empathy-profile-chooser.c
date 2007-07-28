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

#include <gtk/gtk.h>
#include <libmissioncontrol/mc-profile.h>

#include "empathy-profile-chooser.h"
#include "empathy-ui-utils.h"

enum {
	COL_ICON,
	COL_LABEL,
	COL_PROFILE,
	COL_COUNT
};

McProfile*
empathy_profile_chooser_get_selected (GtkWidget *widget)
{
	GtkTreeModel *model;
	GtkTreeIter   iter;
	McProfile    *profile = NULL;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter)) {
		gtk_tree_model_get (model, &iter,
				    COL_PROFILE, &profile,
				    -1);
	}

	return profile;
}

static gint
profile_chooser_sort_profile_value (McProfile *profile)
{
	guint        i;
	const gchar *profile_name;
	const gchar *names[] = {"jabber",
				"salut",
				"gtalk"};

	profile_name = mc_profile_get_unique_name (profile);

	for (i = 0 ; i < G_N_ELEMENTS (names); i++) {
		if (strcmp (profile_name, names[i]) == 0) {
			return i;
		}
	}

	return i;
}

static gint
profile_chooser_sort_func (GtkTreeModel *model,
			   GtkTreeIter  *iter_a,
			   GtkTreeIter  *iter_b,
			   gpointer      user_data)
{
	McProfile   *profile_a;
	McProfile   *profile_b;
	gint         cmp;

	gtk_tree_model_get (model, iter_a,
			    COL_PROFILE, &profile_a,
			    -1);
	gtk_tree_model_get (model, iter_b,
			    COL_PROFILE, &profile_b,
			    -1);

	cmp = profile_chooser_sort_profile_value (profile_a);
	cmp -= profile_chooser_sort_profile_value (profile_b);
	if (cmp == 0) {
		cmp = strcmp (mc_profile_get_display_name (profile_a),
			      mc_profile_get_display_name (profile_b));
	}

	g_object_unref (profile_a);
	g_object_unref (profile_b);

	return cmp;
}

GtkWidget *
empathy_profile_chooser_new (void)
{
	GList           *profiles, *l;
	GtkListStore    *store;
	GtkCellRenderer *renderer;
	GtkWidget       *combo_box;
	GtkTreeIter      iter;

	/* set up combo box with new store */
	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_STRING,    /* Icon name */
				    G_TYPE_STRING,    /* Label     */
				    MC_TYPE_PROFILE); /* Profile   */
	combo_box = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));


	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"icon-name", COL_ICON,
					NULL);
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_BUTTON, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"text", COL_LABEL,
					NULL);

	/* Set the profile sort function */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      COL_PROFILE,
					      GTK_SORT_ASCENDING);
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store),
					 COL_PROFILE,
					 profile_chooser_sort_func,
					 NULL, NULL);

	profiles = mc_profiles_list ();
	for (l = profiles; l; l = l->next) {
		McProfile *profile;

		profile = l->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_ICON, mc_profile_get_icon_name (profile),
				    COL_LABEL, mc_profile_get_display_name (profile),
				    COL_PROFILE, profile,
				    -1);
	}

	gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo_box), &iter);

	mc_profiles_free_list (profiles);
	g_object_unref (store);

	return combo_box;
}

