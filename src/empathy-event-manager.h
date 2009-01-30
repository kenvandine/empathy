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

#ifndef __EMPATHY_EVENT_MANAGER_H__
#define __EMPATHY_EVENT_MANAGER_H__

#include <glib.h>
#include <glib-object.h>

#include <libempathy/empathy-contact.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_EVENT_MANAGER         (empathy_event_manager_get_type ())
#define EMPATHY_EVENT_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_EVENT_MANAGER, EmpathyEventManager))
#define EMPATHY_EVENT_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_EVENT_MANAGER, EmpathyEventManagerClass))
#define EMPATHY_IS_EVENT_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_EVENT_MANAGER))
#define EMPATHY_IS_EVENT_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_EVENT_MANAGER))
#define EMPATHY_EVENT_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_EVENT_MANAGER, EmpathyEventManagerClass))

typedef struct _EmpathyEventManager      EmpathyEventManager;
typedef struct _EmpathyEventManagerClass EmpathyEventManagerClass;

struct _EmpathyEventManager {
	GObject parent;
	gpointer priv;
};

struct _EmpathyEventManagerClass {
	GObjectClass parent_class;
};

typedef struct {
	EmpathyContact *contact;
	gchar          *icon_name;
	gchar          *header;
	gchar          *message;
} EmpathyEvent;

GType                empathy_event_manager_get_type      (void) G_GNUC_CONST;
EmpathyEventManager *empathy_event_manager_dup_singleton (void);
EmpathyEvent *       empathy_event_manager_get_top_event (EmpathyEventManager *manager);
GSList *             empathy_event_manager_get_events    (EmpathyEventManager *manager);
void                 empathy_event_activate              (EmpathyEvent        *event);
void                 empathy_event_inhibit_updates       (EmpathyEvent        *event);

G_END_DECLS

#endif /* __EMPATHY_EVENT_MANAGER_H__ */
