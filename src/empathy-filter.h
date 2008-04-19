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

#ifndef __EMPATHY_FILTER_H__
#define __EMPATHY_FILTER_H__

#include <glib.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_FILTER         (empathy_filter_get_type ())
#define EMPATHY_FILTER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_FILTER, EmpathyFilter))
#define EMPATHY_FILTER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_FILTER, EmpathyFilterClass))
#define EMPATHY_IS_FILTER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_FILTER))
#define EMPATHY_IS_FILTER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_FILTER))
#define EMPATHY_FILTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_FILTER, EmpathyFilterClass))

typedef struct _EmpathyFilter      EmpathyFilter;
typedef struct _EmpathyFilterClass EmpathyFilterClass;
typedef struct _EmpathyFilterPriv  EmpathyFilterPriv;

struct _EmpathyFilter {
	GObject      parent;
};

struct _EmpathyFilterClass {
	GObjectClass parent_class;
};

typedef struct {
	gchar *icon_name;
	gchar *message;
} EmpathyFilterEvent;

GType               empathy_filter_get_type       (void) G_GNUC_CONST;
EmpathyFilter *     empathy_filter_new            (void);
void                empathy_filter_activate_event (EmpathyFilter      *filter,
						   EmpathyFilterEvent *event);
EmpathyFilterEvent *empathy_filter_get_top_event  (EmpathyFilter      *filter);

G_END_DECLS

#endif /* __EMPATHY_FILTER_H__ */
