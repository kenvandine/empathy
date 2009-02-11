/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Elliot Fairweather
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
 * Authors: Elliot Fairweather <elliot.fairweather@collabora.co.uk>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_TP_CALL_H__
#define __EMPATHY_TP_CALL_H__

#include <glib.h>
#include <telepathy-glib/channel.h>

#include "empathy-contact.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_TP_CALL (empathy_tp_call_get_type ())
#define EMPATHY_TP_CALL(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), \
    EMPATHY_TYPE_TP_CALL, EmpathyTpCall))
#define EMPATHY_TP_CALL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
    EMPATHY_TYPE_TP_CALL, EmpathyTpCallClass))
#define EMPATHY_IS_TP_CALL(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), \
    EMPATHY_TYPE_TP_CALL))
#define EMPATHY_IS_TP_CALL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
    EMPATHY_TYPE_TP_CALL))
#define EMPATHY_TP_CALL_GET_CLASS(object) \
  (G_TYPE_INSTANCE_GET_CLASS ((object), \
    EMPATHY_TYPE_TP_CALL, EmpathyTpCallClass))

typedef struct _EmpathyTpCall EmpathyTpCall;
typedef struct _EmpathyTpCallClass EmpathyTpCallClass;

struct _EmpathyTpCall {
  GObject parent;
  gpointer priv;
};

struct _EmpathyTpCallClass {
  GObjectClass parent_class;
};

typedef enum
{
  EMPATHY_TP_CALL_STATUS_READYING,
  EMPATHY_TP_CALL_STATUS_PENDING,
  EMPATHY_TP_CALL_STATUS_ACCEPTED,
  EMPATHY_TP_CALL_STATUS_CLOSED
} EmpathyTpCallStatus;

typedef struct
{
  gboolean exists;
  guint id;
  guint state;
  guint direction;
} EmpathyTpCallStream;

GType empathy_tp_call_get_type (void) G_GNUC_CONST;
EmpathyTpCall *empathy_tp_call_new (TpChannel *channel);
void empathy_tp_call_close (EmpathyTpCall *call);

void empathy_tp_call_to (EmpathyTpCall *call, EmpathyContact *contact);

void empathy_tp_call_accept_incoming_call (EmpathyTpCall *call);
void empathy_tp_call_request_video_stream_direction (EmpathyTpCall *call,
    gboolean is_sending);
void empathy_tp_call_start_tone (EmpathyTpCall *call, TpDTMFEvent event);
void empathy_tp_call_stop_tone (EmpathyTpCall *call);
gboolean empathy_tp_call_has_dtmf (EmpathyTpCall *call);

G_END_DECLS

#endif /* __EMPATHY_TP_CALL_H__ */
