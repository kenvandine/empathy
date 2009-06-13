/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 */

#ifndef __EMPATHY_CELL_RENDERER_TEXT_H__
#define __EMPATHY_CELL_RENDERER_TEXT_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_CELL_RENDERER_TEXT         (empathy_cell_renderer_text_get_type ())
#define EMPATHY_CELL_RENDERER_TEXT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CELL_RENDERER_TEXT, EmpathyCellRendererText))
#define EMPATHY_CELL_RENDERER_TEXT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_CELL_RENDERER_TEXT, EmpathyCellRendererTextClass))
#define EMPATHY_IS_CELL_RENDERER_TEXT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CELL_RENDERER_TEXT))
#define EMPATHY_IS_CELL_RENDERER_TEXT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CELL_RENDERER_TEXT))
#define EMPATHY_CELL_RENDERER_TEXT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CELL_RENDERER_TEXT, EmpathyCellRendererTextClass))

typedef struct _EmpathyCellRendererText      EmpathyCellRendererText;
typedef struct _EmpathyCellRendererTextClass EmpathyCellRendererTextClass;

struct _EmpathyCellRendererText {
	GtkCellRendererText parent;
	gpointer priv;
};

struct _EmpathyCellRendererTextClass {
	GtkCellRendererTextClass    parent_class;
};

GType             empathy_cell_renderer_text_get_type (void) G_GNUC_CONST;
GtkCellRenderer * empathy_cell_renderer_text_new      (void);

G_END_DECLS

#endif /* __EMPATHY_CELL_RENDERER_TEXT_H__ */
