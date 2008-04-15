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

#ifndef __EMPATHY_CELL_RENDERER_ACTIVATABLE_H__
#define __EMPATHY_CELL_RENDERER_ACTIVATABLE_H__

#include <gtk/gtkcellrendererpixbuf.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_CELL_RENDERER_ACTIVATABLE            (empathy_cell_renderer_activatable_get_type ())
#define EMPATHY_CELL_RENDERER_ACTIVATABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EMPATHY_TYPE_CELL_RENDERER_ACTIVATABLE, EmpathyCellRendererActivatable))
#define EMPATHY_CELL_RENDERER_ACTIVATABLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EMPATHY_TYPE_CELL_RENDERER_ACTIVATABLE, EmpathyCellRendererActivatableClass))
#define EMPATHY_IS_CELL_RENDERER_ACTIVATABLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EMPATHY_TYPE_CELL_RENDERER_ACTIVATABLE))
#define EMPATHY_IS_CELL_RENDERER_ACTIVATABLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EMPATHY_TYPE_CELL_RENDERER_ACTIVATABLE))
#define EMPATHY_CELL_RENDERER_ACTIVATABLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EMPATHY_TYPE_CELL_RENDERER_ACTIVATABLE, EmpathyCellRendererActivatableClass))

typedef struct _EmpathyCellRendererActivatable      EmpathyCellRendererActivatable;
typedef struct _EmpathyCellRendererActivatableClass EmpathyCellRendererActivatableClass;

struct _EmpathyCellRendererActivatable {
  GtkCellRendererPixbuf parent;
};

struct _EmpathyCellRendererActivatableClass {
  GtkCellRendererPixbufClass parent_class;
};

GType            empathy_cell_renderer_activatable_get_type (void) G_GNUC_CONST;
GtkCellRenderer *empathy_cell_renderer_activatable_new      (void);

G_END_DECLS

#endif /* __EMPATHY_CELL_RENDERER_ACTIVATABLE_H__ */

