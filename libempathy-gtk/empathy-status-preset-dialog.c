/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * empathy-status-preset-dialog.c
 *
 * EmpathyStatusPresetDialog - a dialog for adding and removing preset status
 * messages.
 *
 * Copyright (C) 2009 Collabora Ltd.
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
 * Authors: Davyd Madeley <davyd.madeley@collabora.co.uk>
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <libmissioncontrol/mc-enum-types.h>

#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-status-presets.h>

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

#include "empathy-ui-utils.h"
#include "empathy-status-preset-dialog.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyStatusPresetDialog)

G_DEFINE_TYPE (EmpathyStatusPresetDialog, empathy_status_preset_dialog, GTK_TYPE_DIALOG);

static McPresence states[] = {
	MC_PRESENCE_AVAILABLE,
	MC_PRESENCE_DO_NOT_DISTURB,
	MC_PRESENCE_AWAY
};

typedef struct _EmpathyStatusPresetDialogPriv EmpathyStatusPresetDialogPriv;
struct _EmpathyStatusPresetDialogPriv
{
	GtkWidget *presets_treeview;
};

enum
{
	PRESETS_STORE_STATE,
	PRESETS_STORE_ICON_NAME,
	PRESETS_STORE_STATUS,
	PRESETS_STORE_N_COLS
};

static void
empathy_status_preset_dialog_class_init (EmpathyStatusPresetDialogClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (class);

	g_type_class_add_private (gobject_class,
			sizeof (EmpathyStatusPresetDialogPriv));
}

static void
status_preset_dialog_setup_presets_update (EmpathyStatusPresetDialog *self)
{
	EmpathyStatusPresetDialogPriv *priv = GET_PRIV (self);
	GtkListStore *store;
	int i;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (
				GTK_TREE_VIEW (priv->presets_treeview)));

	for (i = 0; i < G_N_ELEMENTS (states); i++)
	{
		GList *presets, *l;
		const char *icon_name;

		presets = empathy_status_presets_get (states[i], -1);
		icon_name = empathy_icon_name_for_presence (states[i]);

		for (l = presets; l; l = l->next) {
			char *preset = (char *) l->data;

			gtk_list_store_insert_with_values (store,
					NULL, -1,
					PRESETS_STORE_STATE, states[i],
					PRESETS_STORE_ICON_NAME, icon_name,
					PRESETS_STORE_STATUS, preset,
					-1);
		}

		g_list_free (presets);
	}
}

static void
status_preset_dialog_setup_presets_treeview (EmpathyStatusPresetDialog *self)
{
	EmpathyStatusPresetDialogPriv *priv = GET_PRIV (self);
	GtkWidget *treeview = priv->presets_treeview;
	GtkListStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	store = gtk_list_store_new (PRESETS_STORE_N_COLS,
			MC_TYPE_PRESENCE,	/* PRESETS_STORE_STATE */
			G_TYPE_STRING,		/* PRESETS_STORE_ICON_NAME */
			G_TYPE_STRING);		/* PRESETS_STORE_STATUS */

	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
				 GTK_TREE_MODEL (store));
	g_object_unref (store);

	status_preset_dialog_setup_presets_update (self);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer,
			"icon-name", PRESETS_STORE_ICON_NAME);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer,
			"text", PRESETS_STORE_STATUS);
}

static void
status_preset_dialog_preset_selection_changed (GtkTreeSelection *selection,
					       GtkWidget *remove_button)
{
	/* update the sensitivity of the Remove button */
	gtk_widget_set_sensitive (remove_button,
			gtk_tree_selection_get_selected (selection, NULL, NULL));
}

static void
empathy_status_preset_dialog_init (EmpathyStatusPresetDialog *self)
{
	EmpathyStatusPresetDialogPriv *priv = self->priv =
		G_TYPE_INSTANCE_GET_PRIVATE (self,
			EMPATHY_TYPE_STATUS_PRESET_DIALOG,
			EmpathyStatusPresetDialogPriv);
	GtkBuilder *gui;
	GtkWidget *toplevel_vbox, *remove_button;
	char *filename;

	(void) priv; /* hack */

	gtk_dialog_set_has_separator (GTK_DIALOG (self), FALSE);
	gtk_dialog_add_button (GTK_DIALOG (self),
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

	filename = empathy_file_lookup ("empathy-status-preset-dialog.ui",
			"libempathy-gtk");
	gui = empathy_builder_get_file (filename,
			"toplevel-vbox", &toplevel_vbox,
			"presets-treeview", &priv->presets_treeview,
			"remove-button", &remove_button,
			NULL);
	g_free (filename);

	status_preset_dialog_setup_presets_treeview (self);
	g_signal_connect (gtk_tree_view_get_selection (
				GTK_TREE_VIEW (priv->presets_treeview)),
			"changed",
			G_CALLBACK (status_preset_dialog_preset_selection_changed),
			remove_button);

	gtk_box_pack_start(GTK_BOX (GTK_DIALOG (self)->vbox), toplevel_vbox,
			TRUE, TRUE, 0);

	g_object_unref (gui);
}

GtkWidget *
empathy_status_preset_dialog_new (GtkWindow *parent)
{
	GtkWidget *self = g_object_new (EMPATHY_TYPE_STATUS_PRESET_DIALOG,
			NULL);

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (self), parent);
	}

	return self;
}
