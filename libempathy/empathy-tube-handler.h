/*
 * Copyright (C) 2008 Collabora Ltd.
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
 *          Elliot Fairweather <elliot.fairweather@collabora.co.uk>
 */

#ifndef __EMPATHY_TUBE_HANDLER_H__
#define __EMPATHY_TUBE_HANDLER_H__

#include <glib.h>

#include <telepathy-glib/enums.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_TUBE_HANDLER (empathy_tube_handler_get_type ())
#define EMPATHY_TUBE_HANDLER(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), \
    EMPATHY_TYPE_TUBE_HANDLER, EmpathyTubeHandler))
#define EMPATHY_TUBE_HANDLER_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), \
    EMPATHY_TYPE_TUBE_HANDLER, EmpathyTubeHandlerClass))
#define EMPATHY_IS_TUBE_HANDLER(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
    EMPATHY_TYPE_TUBE_HANDLER))
#define EMPATHY_IS_TUBE_HANDLER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), \
    EMPATHY_TYPE_TUBE_HANDLER))
#define EMPATHY_TUBE_HANDLER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), \
    EMPATHY_TYPE_TUBE_HANDLER, EmpathyTubeHandlerClass))

typedef struct _EmpathyTubeHandler EmpathyTubeHandler;
typedef struct _EmpathyTubeHandlerClass EmpathyTubeHandlerClass;

struct _EmpathyTubeHandler {
  GObject parent;
};

struct _EmpathyTubeHandlerClass {
  GObjectClass parent_class;
};

GType empathy_tube_handler_get_type (void) G_GNUC_CONST;
EmpathyTubeHandler *empathy_tube_handler_new (TpTubeType type,
    const gchar *service);
gchar *empathy_tube_handler_build_bus_name (TpTubeType type,
    const gchar *service);
gchar *empathy_tube_handler_build_object_path (TpTubeType type,
    const gchar *service);

G_END_DECLS

#endif /* __EMPATHY_TUBE_HANDLER_H__ */
