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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Davyd Madeley <davyd.madeley@collabora.co.uk>
 */

#ifndef __EMPATHY_STATUS_PRESET_DIALOG_H__
#define __EMPATHY_STATUS_PRESET_DIALOG_H__

#include <glib.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_STATUS_PRESET_DIALOG	(empathy_status_preset_dialog_get_type ())
#define EMPATHY_STATUS_PRESET_DIALOG(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), EMPATHY_TYPE_STATUS_PRESET_DIALOG, EmpathyStatusPresetDialog))
#define EMPATHY_STATUS_PRESET_DIALOG_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), EMPATHY_TYPE_STATUS_PRESET_DIALOG, EmpathyStatusPresetDialogClass))
#define EMPATHY_IS_STATUS_PRESET_DIALOG(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EMPATHY_TYPE_STATUS_PRESET_DIALOG))
#define EMPATHY_IS_STATUS_PRESET_DIALOG_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EMPATHY_TYPE_STATUS_PRESET_DIALOG))
#define EMPATHY_STATUS_PRESET_DIALOG_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), EMPATHY_TYPE_STATUS_PRESET_DIALOG, EmpathyStatusPresetDialogClass))

typedef struct _EmpathyStatusPresetDialog EmpathyStatusPresetDialog;
typedef struct _EmpathyStatusPresetDialogClass EmpathyStatusPresetDialogClass;

struct _EmpathyStatusPresetDialog
{
	GtkDialog parent;

	/*< private >*/
	gpointer priv;
};

struct _EmpathyStatusPresetDialogClass
{
	GtkDialogClass parent_class;
};

GType empathy_status_preset_dialog_get_type (void);
GtkWidget *empathy_status_preset_dialog_new (GtkWindow *parent);

G_END_DECLS

#endif
