/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
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
 */

#ifndef __EMPATHY_CHANDLER_H__
#define __EMPATHY_CHANDLER_H__

#include <glib.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_CHANDLER         (empathy_chandler_get_type ())
#define EMPATHY_CHANDLER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CHANDLER, EmpathyChandler))
#define EMPATHY_CHANDLER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_CHANDLER, EmpathyChandlerClass))
#define EMPATHY_IS_CHANDLER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CHANDLER))
#define EMPATHY_IS_CHANDLER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CHANDLER))
#define EMPATHY_CHANDLER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CHANDLER, EmpathyChandlerClass))

typedef struct _EmpathyChandler      EmpathyChandler;
typedef struct _EmpathyChandlerClass EmpathyChandlerClass;

struct _EmpathyChandler {
	GObject parent;
};

struct _EmpathyChandlerClass {
	GObjectClass parent_class;
};

GType            empathy_chandler_get_type (void) G_GNUC_CONST;
EmpathyChandler *empathy_chandler_new      (const gchar *bus_name,
					    const gchar *object_path);

G_END_DECLS

#endif /* __EMPATHY_CHANDLER_H__ */
