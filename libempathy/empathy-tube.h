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
 * Authors: Guillaume Desmottes <guillaume.desmottes@collabora.co.uk>
 *          Elliot Fairweather <elliot.fairweather@collabora.co.uk>
 */

#ifndef __EMPATHY_TUBE_H__
#define __EMPATHY_TUBE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_TUBE (empathy_tube_get_type ())
#define EMPATHY_TUBE(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), \
    EMPATHY_TYPE_TUBE, EmpathyTube))
#define EMPATHY_TUBE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_TUBE, EmpathyTubeClass))
#define EMPATHY_IS_TUBE(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), \
    EMPATHY_TYPE_TUBE))
#define EMPATHY_IS_TUBE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
    EMPATHY_TYPE_TUBE))
#define EMPATHY_TUBE_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), \
    EMPATHY_TYPE_TUBE, EmpathyTubeClass))

typedef struct _EmpathyTube EmpathyTube;
typedef struct _EmpathyTubeClass EmpathyTubeClass;

struct _EmpathyTube {
  GObject parent;
};

struct _EmpathyTubeClass {
  GObjectClass parent_class;
};

GType empathy_tube_get_type (void) G_GNUC_CONST;

void empathy_tube_close (EmpathyTube *tube);
void empathy_tube_accept_stream_tube_ipv4 (EmpathyTube *tube);
void empathy_tube_accept_stream_tube_unix (EmpathyTube *tube);
void empathy_tube_get_stream_tube_socket_ipv4 (EmpathyTube *tube,
    gchar **hostname, guint *port);
gchar * empathy_tube_get_stream_tube_socket_unix (EmpathyTube *tube);

G_END_DECLS

#endif /* __EMPATHY_TUBE_H__ */
