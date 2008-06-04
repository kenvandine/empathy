/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_STATUS_ICON_H__
#define __EMPATHY_STATUS_ICON_H__

#include <glib.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_STATUS_ICON         (empathy_status_icon_get_type ())
#define EMPATHY_STATUS_ICON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_STATUS_ICON, EmpathyStatusIcon))
#define EMPATHY_STATUS_ICON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_STATUS_ICON, EmpathyStatusIconClass))
#define EMPATHY_IS_STATUS_ICON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_STATUS_ICON))
#define EMPATHY_IS_STATUS_ICON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_STATUS_ICON))
#define EMPATHY_STATUS_ICON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_STATUS_ICON, EmpathyStatusIconClass))

typedef struct _EmpathyStatusIcon      EmpathyStatusIcon;
typedef struct _EmpathyStatusIconClass EmpathyStatusIconClass;

struct _EmpathyStatusIcon {
	GObject parent;
	gpointer priv;
};

struct _EmpathyStatusIconClass {
	GObjectClass parent_class;
};

GType              empathy_status_icon_get_type (void) G_GNUC_CONST;
EmpathyStatusIcon *empathy_status_icon_new      (GtkWindow *window,
						 gboolean   hide_contact_list);

G_END_DECLS

#endif /* __EMPATHY_STATUS_ICON_H__ */
