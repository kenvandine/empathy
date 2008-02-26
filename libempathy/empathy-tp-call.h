/*
 *  Copyright (C) 2007 Elliot Fairweather
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

#ifndef __EMPATHY_TP_CALL_H__
#define __EMPATHY_TP_CALL_H__

#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-conn.h>

#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-chandler.h>
#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-tp-group.h>

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
#define EMPATHY_TP_CALL_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), \
    EMPATHY_TYPE_TP_CALL, EmpathyTpCallClass))

typedef struct _EmpathyTpCall EmpathyTpCall;
typedef struct _EmpathyTpCallClass EmpathyTpCallClass;

struct _EmpathyTpCall {
    GObject parent;
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
EmpathyTpCall *empathy_tp_call_new (TpConn *connection, TpChan *channel);

void empathy_tp_call_accept_incoming_call (EmpathyTpCall *call);
void empathy_tp_call_close_channel (EmpathyTpCall *call);
void empathy_tp_call_request_video_stream_direction (EmpathyTpCall *call,
    gboolean is_sending);
void empathy_tp_call_add_preview_video (EmpathyTpCall *call,
    guint preview_video_socket_id);
void empathy_tp_call_remove_preview_video (EmpathyTpCall *call,
    guint preview_video_socket_id);
void empathy_tp_call_add_output_video (EmpathyTpCall *call,
    guint output_video_socket_id);
void empathy_tp_call_set_output_volume (EmpathyTpCall *call, guint volume);
void empathy_tp_call_mute_output (EmpathyTpCall *call, gboolean is_muted);
void empathy_tp_call_mute_input (EmpathyTpCall *call, gboolean is_muted);

G_END_DECLS

#endif /* __EMPATHY_TP_CALL_H__ */
