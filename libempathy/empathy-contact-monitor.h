/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
 * Authors: Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
 */

#ifndef __EMPATHY_CONTACT_MONITOR_H__
#define __EMPATHY_CONTACT_MONITOR_H__

#include <glib-object.h>

#include "empathy-types.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_CONTACT_MONITOR         (empathy_contact_monitor_get_type ())
#define EMPATHY_CONTACT_MONITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CONTACT_MONITOR, EmpathyContactMonitor))
#define EMPATHY_CONTACT_MONITOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_CONTACT_MONITOR, EmpathyContactMonitorClass))
#define EMPATHY_IS_CONTACT_MONITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CONTACT_MONITOR))
#define EMPATHY_IS_CONTACT_MONITOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CONTACT_MONITOR))
#define EMPATHY_CONTACT_MONITOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CONTACT_MONITOR, EmpathyContactMonitorClass))

typedef struct _EmpathyContactMonitorClass EmpathyContactMonitorClass;

struct _EmpathyContactMonitor {
  GObject parent;
  gpointer priv;
};

struct _EmpathyContactMonitorClass {
  GObjectClass parent_class;
};

GType empathy_contact_monitor_get_type (void);

/* public methods */

void
empathy_contact_monitor_set_iface (EmpathyContactMonitor *self,
                                   EmpathyContactList *iface);

EmpathyContactMonitor *
empathy_contact_monitor_new_for_iface (EmpathyContactList *iface);

G_END_DECLS

#endif /* __EMPATHY_CONTACT_MONITOR_H__ */

