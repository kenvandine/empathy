/*
 *  Copyright (C) 2008 Collabora Ltd.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Authors: Elliot Fairweather <elliot.fairweather@collabora.co.uk>
 */

#ifndef __EMPATHY_TUBES_H__
#define __EMPATHY_TUBES_H__

#include <glib-object.h>

#include <telepathy-glib/channel.h>
#include "empathy-tube.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_TUBES (empathy_tubes_get_type ())
#define EMPATHY_TUBES(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), \
    EMPATHY_TYPE_TUBES, EmpathyTubes))
#define EMPATHY_TUBES_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_TUBES, EmpathyTubesClass))
#define EMPATHY_IS_TUBES(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), \
    EMPATHY_TYPE_TUBES))
#define EMPATHY_IS_TUBES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
    EMPATHY_TYPE_TUBES))
#define EMPATHY_TUBES_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), \
    EMPATHY_TYPE_TUBES, EmpathyTubesClass))

typedef struct _EmpathyTubes EmpathyTubes;
typedef struct _EmpathyTubesClass EmpathyTubesClass;

struct _EmpathyTubes {
    GObject parent;
};

struct _EmpathyTubesClass {
    GObjectClass parent_class;
};

GType empathy_tubes_get_type (void) G_GNUC_CONST;
EmpathyTubes *empathy_tubes_new (TpChannel *channel);

guint empathy_tubes_offer_stream_tube_ipv4 (EmpathyTubes *tubes, gchar *host,
    guint port, gchar *service);
void empathy_tubes_close (EmpathyTubes *tubes);
GSList *empathy_tubes_list_tubes (EmpathyTubes *tubes);
EmpathyTube *empathy_tubes_get_tube (EmpathyTubes *tubes, guint tube_id);

G_END_DECLS

#endif /* __EMPATHY_TUBES_H__ */
