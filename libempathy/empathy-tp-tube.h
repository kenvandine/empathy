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

#ifndef __EMPATHY_TP_TUBE_H__
#define __EMPATHY_TP_TUBE_H__

#include <glib-object.h>

#include <telepathy-glib/channel.h>

#include "empathy-contact.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_TP_TUBE (empathy_tp_tube_get_type ())
#define EMPATHY_TP_TUBE(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), \
    EMPATHY_TYPE_TP_TUBE, EmpathyTpTube))
#define EMPATHY_TP_TUBE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_TP_TUBE, EmpathyTpTubeClass))
#define EMPATHY_IS_TP_TUBE(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), \
    EMPATHY_TYPE_TP_TUBE))
#define EMPATHY_IS_TP_TUBE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
    EMPATHY_TYPE_TP_TUBE))
#define EMPATHY_TP_TUBE_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), \
    EMPATHY_TYPE_TP_TUBE, EmpathyTpTubeClass))

typedef struct _EmpathyTpTube EmpathyTpTube;
typedef struct _EmpathyTpTubeClass EmpathyTpTubeClass;

typedef struct {
  TpSocketAddressType type;
  union {
    struct socket_address_t {
      GArray *path;
    } socket;
    struct inet_address_t {
      gchar *hostname;
      guint port;
    } inet;
  } a;
} EmpathyTpTubeAddress;

struct _EmpathyTpTube {
  GObject parent;
  gpointer priv;
};

struct _EmpathyTpTubeClass {
  GObjectClass parent_class;
};

GType empathy_tp_tube_get_type (void) G_GNUC_CONST;
EmpathyTpTube *empathy_tp_tube_new (TpChannel *channel);
EmpathyTpTube *empathy_tp_tube_new_stream_tube (EmpathyContact *contact,
    TpSocketAddressType type, const gchar *hostname, guint port,
    const gchar *service, GHashTable *parameters);

typedef void (EmpatyTpTubeAcceptStreamTubeCb) (EmpathyTpTube *tube,
  const EmpathyTpTubeAddress *address, const GError *error,
  gpointer user_data);

void empathy_tp_tube_accept_stream_tube (EmpathyTpTube *tube,
  TpSocketAddressType type, EmpatyTpTubeAcceptStreamTubeCb *callback,
  gpointer user_data);

G_END_DECLS

#endif /* __EMPATHY_TP_TUBE_H__ */
