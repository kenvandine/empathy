/*
 * empathy-dispatch-operation.h - Header for EmpathyDispatchOperation
 * Copyright (C) 2008 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
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
 */

#ifndef __EMPATHY_DISPATCH_OPERATION_H__
#define __EMPATHY_DISPATCH_OPERATION_H__

#include <glib-object.h>

#include <libempathy/empathy-contact.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/channel.h>

G_BEGIN_DECLS

typedef struct _EmpathyDispatchOperation EmpathyDispatchOperation;
typedef struct _EmpathyDispatchOperationClass EmpathyDispatchOperationClass;

struct _EmpathyDispatchOperationClass {
    GObjectClass parent_class;
};

struct _EmpathyDispatchOperation {
    GObject parent;
};

typedef enum {
    /* waiting for the channel information to be ready */
    EMPATHY_DISPATCHER_OPERATION_STATE_PREPARING = 0,
    /* Information gathered ready to be dispatched */
    EMPATHY_DISPATCHER_OPERATION_STATE_PENDING,
    /* Send to approving bits for approval */
    EMPATHY_DISPATCHER_OPERATION_STATE_APPROVING,
    /* Send to handlers for dispatching */
    EMPATHY_DISPATCHER_OPERATION_STATE_DISPATCHING,
    /* somebody claimed the channel */
    EMPATHY_DISPATCHER_OPERATION_STATE_CLAIMED,
    /* dispatch operation invalidated, underlying channel died */
    EMPATHY_DISPATCHER_OPERATION_STATE_INVALIDATED,
} EmpathyDispatchOperationState;

GType empathy_dispatch_operation_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_DISPATCH_OPERATION \
  (empathy_dispatch_operation_get_type ())
#define EMPATHY_DISPATCH_OPERATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), EMPATHY_TYPE_DISPATCH_OPERATION, \
    EmpathyDispatchOperation))
#define EMPATHY_DISPATCH_OPERATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EMPATHY_TYPE_DISPATCH_OPERATION, \
  EmpathyDispatchOperationClass))
#define EMPATHY_IS_DISPATCH_OPERATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), EMPATHY_TYPE_DISPATCH_OPERATION))
#define EMPATHY_IS_DISPATCH_OPERATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EMPATHY_TYPE_DISPATCH_OPERATION))
#define EMPATHY_DISPATCH_OPERATION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EMPATHY_TYPE_DISPATCH_OPERATION, \
  EmpathyDispatchOperationClass))

EmpathyDispatchOperation *empathy_dispatch_operation_new (
  TpConnection *connection, TpChannel *channel, EmpathyContact *contact,
  gboolean incoming);

EmpathyDispatchOperation *empathy_dispatch_operation_new_with_wrapper (
  TpConnection *connection, TpChannel *channel, EmpathyContact *contact,
  gboolean incoming, GObject *channel_wrapper);

/* Start the dispatching process, goes to the APPROVING state for incoming
 * channels and DISPATCHING for outgoing ones */
void empathy_dispatch_operation_start (EmpathyDispatchOperation *operation);

void empathy_dispatch_operation_approve (EmpathyDispatchOperation *operation);

/* Returns whether or not the operation was successfully claimed */
gboolean empathy_dispatch_operation_claim (EmpathyDispatchOperation *operation);

TpChannel *empathy_dispatch_operation_get_channel (
  EmpathyDispatchOperation *operation);

GObject *empathy_dispatch_operation_get_channel_wrapper (
  EmpathyDispatchOperation *operation);

TpConnection *empathy_dispatch_operation_get_tp_connection (
  EmpathyDispatchOperation *operation);

const gchar *empathy_dispatch_operation_get_channel_type (
  EmpathyDispatchOperation *operation);

GQuark empathy_dispatch_operation_get_channel_type_id (
  EmpathyDispatchOperation *operation);

const gchar * empathy_dispatch_operation_get_object_path (
  EmpathyDispatchOperation *operation);

EmpathyDispatchOperationState empathy_dispatch_operation_get_status (
  EmpathyDispatchOperation *operation);

gboolean empathy_dispatch_operation_is_incoming (
  EmpathyDispatchOperation *operation);

G_END_DECLS

#endif /* #ifndef __EMPATHY_DISPATCH_OPERATION_H__*/
