/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Raphael Slinckx <raphael@slinckx.net>
 * Copyright (C) 2007-2008 Collabora Ltd.
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
 * Authors: Raphael Slinckx <raphael@slinckx.net>
 */

#include <gtk/gtktreeview.h>

#include "empathy-cell-renderer-activatable.h"

static void     empathy_cell_renderer_activatable_init       (EmpathyCellRendererActivatable      *cell);
static void     empathy_cell_renderer_activatable_class_init (EmpathyCellRendererActivatableClass *klass);
static gboolean cell_renderer_activatable_activate           (GtkCellRenderer                     *cell,
							      GdkEvent                            *event,
							      GtkWidget                           *widget,
							      const gchar                         *path,
							      GdkRectangle                        *background_area,
							      GdkRectangle                        *cell_area,
							      GtkCellRendererState                 flags);

enum {
	PATH_ACTIVATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyCellRendererActivatable, empathy_cell_renderer_activatable, GTK_TYPE_CELL_RENDERER_PIXBUF)

static void
empathy_cell_renderer_activatable_init (EmpathyCellRendererActivatable *cell)
{
	g_object_set (cell,
		      "xpad", 0,
		      "ypad", 0,
		      "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
		      "follow-state", TRUE,
		      NULL);
}

static void
empathy_cell_renderer_activatable_class_init (EmpathyCellRendererActivatableClass *klass)
{
	GtkCellRendererClass *cell_class;

	cell_class = GTK_CELL_RENDERER_CLASS (klass);
	cell_class->activate = cell_renderer_activatable_activate;

	signals[PATH_ACTIVATED] =
		g_signal_new ("path-activated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);
}

GtkCellRenderer *
empathy_cell_renderer_activatable_new (void)
{
	return g_object_new (EMPATHY_TYPE_CELL_RENDERER_ACTIVATABLE, NULL);
}

static gboolean
cell_renderer_activatable_activate (GtkCellRenderer      *cell,
				    GdkEvent             *event,
				    GtkWidget            *widget,
				    const gchar          *path_string,
				    GdkRectangle         *background_area,
				    GdkRectangle         *cell_area,
				    GtkCellRendererState  flags)
{
	EmpathyCellRendererActivatable *activatable;
	gint                            ex, ey, bx, by, bw, bh;

	activatable = EMPATHY_CELL_RENDERER_ACTIVATABLE (cell);

	if (!GTK_IS_TREE_VIEW (widget) || event == NULL || 
	    event->type != GDK_BUTTON_PRESS) {
		return FALSE;
	}

	ex  = (gint) ((GdkEventButton *) event)->x;
	ey  = (gint) ((GdkEventButton *) event)->y;
	bx = background_area->x;
	by = background_area->y;
	bw = background_area->width;
	bh = background_area->height;

	if (ex < bx || ex > (bx+bw) || ey < by || ey > (by+bh)){
		/* Click wasn't on the icon */
		return FALSE;
	}

	g_signal_emit (activatable, signals[PATH_ACTIVATED], 0, path_string);

	return TRUE;
}

