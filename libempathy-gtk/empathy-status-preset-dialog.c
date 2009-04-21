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

#include <libempathy/empathy-status-presets.h>

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

#include "empathy-status-preset-dialog.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyStatusPresetDialog)

G_DEFINE_TYPE (EmpathyStatusPresetDialog, empathy_status_preset_dialog, GTK_TYPE_DIALOG)

typedef struct _EmpathyStatusPresetDialogPriv EmpathyStatusPresetDialogPriv;
struct _EmpathyStatusPresetDialogPriv
{
	int dum; /*dummy*/
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
empathy_status_preset_dialog_init (EmpathyStatusPresetDialog *self)
{
	EmpathyStatusPresetDialogPriv *priv = self->priv =
		G_TYPE_INSTANCE_GET_PRIVATE (self,
			EMPATHY_TYPE_STATUS_PRESET_DIALOG,
			EmpathyStatusPresetDialogPriv);

	(void) priv;
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
