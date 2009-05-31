/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 * Authors: Pierre-Luc Beaudoin <pierre-luc.beaudoin@collabora.co.uk>
 */

#ifndef __EMPATHY_LOCATION_MANAGER_H__
#define __EMPATHY_LOCATION_MANAGER_H__

#include <glib-object.h>


G_BEGIN_DECLS

#define EMPATHY_TYPE_LOCATION_MANAGER         (empathy_location_manager_get_type ())
#define EMPATHY_LOCATION_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_LOCATION_MANAGER, EmpathyLocationManager))
#define EMPATHY_LOCATION_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_LOCATION_MANAGER, EmpathyLocationManagerClass))
#define EMPATHY_IS_LOCATION_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_LOCATION_MANAGER))
#define EMPATHY_IS_LOCATION_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_LOCATION_MANAGER))
#define EMPATHY_LOCATION_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_LOCATION_MANAGER, EmpathyLocationManagerClass))

typedef struct _EmpathyLocationManager      EmpathyLocationManager;
typedef struct _EmpathyLocationManagerClass EmpathyLocationManagerClass;

struct _EmpathyLocationManager
{
  GObject parent;
  gpointer priv;
};

struct _EmpathyLocationManagerClass
{
  GObjectClass parent_class;
};

GType empathy_location_manager_get_type (void) G_GNUC_CONST;
EmpathyLocationManager * empathy_location_manager_dup_singleton (void);

G_END_DECLS

#endif /* __EMPATHY_LOCATION_MANAGER_H__ */
