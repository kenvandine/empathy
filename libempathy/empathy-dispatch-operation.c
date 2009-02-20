/*
 * empathy-dispatch-operation.c - Source for EmpathyDispatchOperation
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


#include <stdio.h>
#include <stdlib.h>

#include "empathy-dispatch-operation.h"
#include <libempathy/empathy-enum-types.h>
#include <libempathy/empathy-tp-chat.h>
#include <libempathy/empathy-tp-call.h>
#include <libempathy/empathy-tp-file.h>

#include "empathy-marshal.h"

#include "extensions/extensions.h"

#define DEBUG_FLAG EMPATHY_DEBUG_DISPATCHER
#include <libempathy/empathy-debug.h>

G_DEFINE_TYPE(EmpathyDispatchOperation, empathy_dispatch_operation,
  G_TYPE_OBJECT)

static void empathy_dispatch_operation_set_status (
  EmpathyDispatchOperation *self, EmpathyDispatchOperationState status);
static void empathy_dispatch_operation_channel_ready_cb (TpChannel *channel,
  const GError *error, gpointer user_data);

/* signal enum */
enum
{
    /* Ready for dispatching */
    READY,
    /* Approved by an approver, can only happens on incoming operations */
    APPROVED,
    /* Claimed by a handler */
    CLAIMED,
    /* Error, channel went away, inspecting it failed etc */
    INVALIDATED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* properties */
enum {
  PROP_CONNECTION = 1,
  PROP_CHANNEL,
  PROP_CHANNEL_WRAPPER,
  PROP_CONTACT,
  PROP_INCOMING,
  PROP_STATUS,
};

/* private structure */
typedef struct _EmpathyDispatchOperationPriv \
  EmpathyDispatchOperationPriv;

struct _EmpathyDispatchOperationPriv
{
  gboolean dispose_has_run;
  TpConnection *connection;
  TpChannel *channel;
  GObject *channel_wrapper;
  EmpathyContact *contact;
  EmpathyDispatchOperationState status;
  gboolean incoming;
  gboolean approved;
  gulong invalidated_handler;
  gulong ready_handler;
};

#define GET_PRIV(o)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EMPATHY_TYPE_DISPATCH_OPERATION, \
    EmpathyDispatchOperationPriv))

static void
empathy_dispatch_operation_init (EmpathyDispatchOperation *obj)
{
  //EmpathyDispatchOperationPriv *priv =
  //  GET_PRIV (obj);

  /* allocate any data required by the object here */
}

static void empathy_dispatch_operation_dispose (GObject *object);
static void empathy_dispatch_operation_finalize (GObject *object);

static void
empathy_dispatch_operation_set_property (GObject *object,
  guint property_id, const GValue *value, GParamSpec *pspec)
{
  EmpathyDispatchOperation *operation = EMPATHY_DISPATCH_OPERATION (object);
  EmpathyDispatchOperationPriv *priv = GET_PRIV (operation);

  switch (property_id)
    {
      case PROP_CONNECTION:
        priv->connection = g_value_dup_object (value);
        break;
      case PROP_CHANNEL:
        priv->channel = g_value_dup_object (value);
        break;
      case PROP_CHANNEL_WRAPPER:
        priv->channel_wrapper = g_value_dup_object (value);
        break;
      case PROP_CONTACT:
        if (priv->contact != NULL)
          g_object_unref (priv->contact);
        priv->contact = g_value_dup_object (value);
        break;
      case PROP_INCOMING:
        priv->incoming = g_value_get_boolean (value);
        break;
    }
}

static void
empathy_dispatch_operation_get_property (GObject *object,
  guint property_id, GValue *value, GParamSpec *pspec)
{
  EmpathyDispatchOperation *operation = EMPATHY_DISPATCH_OPERATION (object);
  EmpathyDispatchOperationPriv *priv = GET_PRIV (operation);

  switch (property_id)
    {
      case PROP_CONNECTION:
        g_value_set_object (value, priv->connection);
        break;
      case PROP_CHANNEL:
        g_value_set_object (value, priv->channel);
        break;
      case PROP_CHANNEL_WRAPPER:
        g_value_set_object (value, priv->channel_wrapper);
        break;
      case PROP_CONTACT:
        g_value_set_object (value, priv->contact);
        break;
      case PROP_INCOMING:
        g_value_set_boolean (value, priv->incoming);
        break;
      case PROP_STATUS:
        g_value_set_enum (value, priv->status);
        break;
    }
}

static void
empathy_dispatch_operation_invalidated (TpProxy *proxy, guint domain,
  gint code, char *message, EmpathyDispatchOperation *self)
{
  empathy_dispatch_operation_set_status (self,
    EMPATHY_DISPATCHER_OPERATION_STATE_INVALIDATED);

  g_signal_emit (self, signals[INVALIDATED], 0, domain, code, message);
}

static void
empathy_dispatch_operation_constructed (GObject *object)
{
  EmpathyDispatchOperation *self = EMPATHY_DISPATCH_OPERATION (object);
  EmpathyDispatchOperationPriv *priv = GET_PRIV (self);

  empathy_dispatch_operation_set_status (self,
    EMPATHY_DISPATCHER_OPERATION_STATE_PREPARING);

  priv->invalidated_handler =
    g_signal_connect (priv->channel, "invalidated",
      G_CALLBACK (empathy_dispatch_operation_invalidated), self);

  tp_channel_call_when_ready (priv->channel,
    empathy_dispatch_operation_channel_ready_cb, self);
}

static void
empathy_dispatch_operation_class_init (
  EmpathyDispatchOperationClass *empathy_dispatch_operation_class)
{
  GObjectClass *object_class =
    G_OBJECT_CLASS (empathy_dispatch_operation_class);
  GParamSpec *param_spec;

  g_type_class_add_private (empathy_dispatch_operation_class,
    sizeof (EmpathyDispatchOperationPriv));

  object_class->set_property = empathy_dispatch_operation_set_property;
  object_class->get_property = empathy_dispatch_operation_get_property;

  object_class->dispose = empathy_dispatch_operation_dispose;
  object_class->finalize = empathy_dispatch_operation_finalize;
  object_class->constructed = empathy_dispatch_operation_constructed;

  signals[READY] = g_signal_new ("ready",
    G_OBJECT_CLASS_TYPE(empathy_dispatch_operation_class),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  signals[APPROVED] = g_signal_new ("approved",
    G_OBJECT_CLASS_TYPE(empathy_dispatch_operation_class),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  signals[CLAIMED] = g_signal_new ("claimed",
    G_OBJECT_CLASS_TYPE(empathy_dispatch_operation_class),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  signals[INVALIDATED] = g_signal_new ("invalidated",
    G_OBJECT_CLASS_TYPE(empathy_dispatch_operation_class),
      G_SIGNAL_RUN_LAST,
      0,
      NULL, NULL,
      _empathy_marshal_VOID__UINT_INT_STRING,
      G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_INT, G_TYPE_STRING);

  param_spec = g_param_spec_object ("connection",
    "connection", "The telepathy connection",
    TP_TYPE_CONNECTION,
    G_PARAM_CONSTRUCT_ONLY |
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION,
                                  param_spec);

  param_spec = g_param_spec_object ("channel",
    "channel", "The telepathy channel",
    TP_TYPE_CHANNEL,
    G_PARAM_CONSTRUCT_ONLY |
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL,
                                  param_spec);

  param_spec = g_param_spec_object ("channel-wrapper",
    "channel wrapper", "The empathy specific channel wrapper",
    G_TYPE_OBJECT,
    G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CHANNEL_WRAPPER,
                                  param_spec);

  param_spec = g_param_spec_object ("contact",
    "contact", "The empathy contact",
    EMPATHY_TYPE_CONTACT,
    G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONTACT,
                                  param_spec);

  param_spec = g_param_spec_boolean ("incoming",
    "incoming", "Whether or not the channel is incoming",
    FALSE,
    G_PARAM_CONSTRUCT_ONLY |
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INCOMING,
                                  param_spec);

  param_spec = g_param_spec_enum ("status",
    "status", "Status of the dispatch operation",
    EMPATHY_TYPE_DISPATCH_OPERATION_STATE, 0,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_STATUS, param_spec);
}

void
empathy_dispatch_operation_dispose (GObject *object)
{
  EmpathyDispatchOperation *self = EMPATHY_DISPATCH_OPERATION (object);
  EmpathyDispatchOperationPriv *priv =
    GET_PRIV (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  g_object_unref (priv->connection);

  if (priv->channel_wrapper != NULL)
    g_object_unref (priv->channel_wrapper);

  if (priv->ready_handler != 0)
    g_signal_handler_disconnect (priv->channel_wrapper,
      priv->invalidated_handler);


  g_signal_handler_disconnect (priv->channel, priv->invalidated_handler);
  g_object_unref (priv->channel);


  if (priv->contact != NULL)
    g_object_unref (priv->contact);

  if (G_OBJECT_CLASS (empathy_dispatch_operation_parent_class)->dispose)
    G_OBJECT_CLASS (empathy_dispatch_operation_parent_class)->dispose (object);
}

void
empathy_dispatch_operation_finalize (GObject *object)
{
  /* free any data held directly by the object here */
  G_OBJECT_CLASS (empathy_dispatch_operation_parent_class)->finalize (object);
}

static void
empathy_dispatch_operation_set_status (EmpathyDispatchOperation *self,
  EmpathyDispatchOperationState status)
{
  EmpathyDispatchOperationPriv *priv = GET_PRIV (self);

  g_assert (status >= priv->status);


  if (priv->status != status)
    {
      DEBUG ("Dispatch operation %s status: %d -> %d",
        empathy_dispatch_operation_get_object_path (self),
        priv->status, status);

      priv->status = status;
      g_object_notify (G_OBJECT (self), "status");

      if (status == EMPATHY_DISPATCHER_OPERATION_STATE_PENDING)
        g_signal_emit (self, signals[READY], 0);
    }
}

static void
empathy_dispatcher_operation_tp_chat_ready_cb (GObject *object,
  GParamSpec *spec, gpointer user_data)
{
  EmpathyDispatchOperation *self = EMPATHY_DISPATCH_OPERATION (user_data);
  EmpathyDispatchOperationPriv *priv = GET_PRIV (self);

  if (!empathy_tp_chat_is_ready (EMPATHY_TP_CHAT (priv->channel_wrapper)))
    return;

  g_signal_handler_disconnect (priv->channel_wrapper, priv->ready_handler);
  priv->ready_handler = 0;

  empathy_dispatch_operation_set_status (self,
    EMPATHY_DISPATCHER_OPERATION_STATE_PENDING);
}

static void
empathy_dispatch_operation_channel_ready_cb (TpChannel *channel,
  const GError *error, gpointer user_data)
{
  EmpathyDispatchOperation *self = EMPATHY_DISPATCH_OPERATION (user_data);
  EmpathyDispatchOperationPriv *priv = GET_PRIV (self);
  GQuark channel_type;

  /* The error will be handled in empathy_dispatch_operation_invalidated */
  if (error != NULL)
    return;

  g_assert (channel == priv->channel);

  /* If the channel wrapper is defined, we assume it's ready */
  if (priv->channel_wrapper != NULL)
    goto ready;

  channel_type = tp_channel_get_channel_type_id (channel);

  if (channel_type == TP_IFACE_QUARK_CHANNEL_TYPE_TEXT)
    {
      EmpathyTpChat *chat= empathy_tp_chat_new (channel);
      priv->channel_wrapper = G_OBJECT (chat);

      if (!empathy_tp_chat_is_ready (chat))
        {
          priv->ready_handler = g_signal_connect (chat, "notify::ready",
            G_CALLBACK (empathy_dispatcher_operation_tp_chat_ready_cb), self);
          return;
        }

    }
  else if (channel_type == TP_IFACE_QUARK_CHANNEL_TYPE_STREAMED_MEDIA)
    {
       EmpathyTpCall *call = empathy_tp_call_new (channel);
       priv->channel_wrapper = G_OBJECT (call);

    }
  else if (channel_type == TP_IFACE_QUARK_CHANNEL_TYPE_FILE_TRANSFER)
    {
       EmpathyTpFile *file = empathy_tp_file_new (channel);
       priv->channel_wrapper = G_OBJECT (file);
    }

ready:
  empathy_dispatch_operation_set_status (self,
    EMPATHY_DISPATCHER_OPERATION_STATE_PENDING);
}

EmpathyDispatchOperation *
empathy_dispatch_operation_new (TpConnection *connection, TpChannel *channel,
  EmpathyContact *contact, gboolean incoming)
{
  return empathy_dispatch_operation_new_with_wrapper (connection, channel,
    contact, incoming, NULL);
}

EmpathyDispatchOperation *
empathy_dispatch_operation_new_with_wrapper (TpConnection *connection,
  TpChannel *channel, EmpathyContact *contact, gboolean incoming,
  GObject *wrapper)
{
  g_return_val_if_fail (connection != NULL, NULL);
  g_return_val_if_fail (channel != NULL, NULL);

  return EMPATHY_DISPATCH_OPERATION (
    g_object_new (EMPATHY_TYPE_DISPATCH_OPERATION,
      "connection", connection,
      "channel", channel,
      "channel-wrapper", wrapper,
      "contact", contact,
      "incoming", incoming,
      NULL));
}

void
empathy_dispatch_operation_start (EmpathyDispatchOperation *operation)
{
  EmpathyDispatchOperationPriv *priv;

  g_return_if_fail (EMPATHY_IS_DISPATCH_OPERATION (operation));

  priv = GET_PRIV (operation);

  g_return_if_fail (
    priv->status == EMPATHY_DISPATCHER_OPERATION_STATE_PENDING);

  if (priv->incoming && !priv->approved)
    empathy_dispatch_operation_set_status (operation,
      EMPATHY_DISPATCHER_OPERATION_STATE_APPROVING);
  else
    empathy_dispatch_operation_set_status (operation,
      EMPATHY_DISPATCHER_OPERATION_STATE_DISPATCHING);
}

void
empathy_dispatch_operation_approve (EmpathyDispatchOperation *operation)
{
  EmpathyDispatchOperationPriv *priv;

  g_return_if_fail (EMPATHY_IS_DISPATCH_OPERATION (operation));

  priv = GET_PRIV (operation);

  if (priv->status == EMPATHY_DISPATCHER_OPERATION_STATE_APPROVING)
    {
      DEBUG ("Approving operation %s",
        empathy_dispatch_operation_get_object_path (operation));

      empathy_dispatch_operation_set_status (operation,
        EMPATHY_DISPATCHER_OPERATION_STATE_DISPATCHING);

      g_signal_emit (operation, signals[APPROVED], 0);
    }
  else if (priv->status < EMPATHY_DISPATCHER_OPERATION_STATE_APPROVING)
    {
      DEBUG ("Pre-approving operation %s",
        empathy_dispatch_operation_get_object_path (operation));
      priv->approved = TRUE;
    }
  else
    {
      DEBUG (
        "Ignoring approval for %s as it's already past the approval stage",
        empathy_dispatch_operation_get_object_path (operation));
    }
}

/* Returns whether or not the operation was successfully claimed */
gboolean
empathy_dispatch_operation_claim (EmpathyDispatchOperation *operation)
{
  EmpathyDispatchOperationPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_DISPATCH_OPERATION (operation), FALSE);

  priv = GET_PRIV (operation);

  if (priv->status == EMPATHY_DISPATCHER_OPERATION_STATE_CLAIMED)
    return FALSE;

  empathy_dispatch_operation_set_status (operation,
    EMPATHY_DISPATCHER_OPERATION_STATE_CLAIMED);

  g_signal_emit (operation, signals[CLAIMED], 0);

  return TRUE;
}

TpConnection *
empathy_dispatch_operation_get_tp_connection (
  EmpathyDispatchOperation *operation)
{
  EmpathyDispatchOperationPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_DISPATCH_OPERATION (operation), NULL);

  priv = GET_PRIV (operation);

  return g_object_ref (priv->connection);
}

TpChannel *
empathy_dispatch_operation_get_channel (EmpathyDispatchOperation *operation)
{
  EmpathyDispatchOperationPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_DISPATCH_OPERATION (operation), NULL);

  priv = GET_PRIV (operation);

  return priv->channel;
}

GObject *
empathy_dispatch_operation_get_channel_wrapper (
  EmpathyDispatchOperation *operation)
{
  EmpathyDispatchOperationPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_DISPATCH_OPERATION (operation), NULL);

  priv = GET_PRIV (operation);

  return priv->channel_wrapper;
}

const gchar *
empathy_dispatch_operation_get_channel_type (
  EmpathyDispatchOperation *operation)
{
  EmpathyDispatchOperationPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_DISPATCH_OPERATION (operation), NULL);

  priv = GET_PRIV (operation);

  return tp_channel_get_channel_type (priv->channel);
}

GQuark
empathy_dispatch_operation_get_channel_type_id (
  EmpathyDispatchOperation *operation)
{
  EmpathyDispatchOperationPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_DISPATCH_OPERATION (operation), 0);

  priv = GET_PRIV (operation);

  return tp_channel_get_channel_type_id (priv->channel);
}

const gchar *
empathy_dispatch_operation_get_object_path (
  EmpathyDispatchOperation *operation)
{
  EmpathyDispatchOperationPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_DISPATCH_OPERATION (operation), NULL);

  priv = GET_PRIV (operation);

  return tp_proxy_get_object_path (TP_PROXY (priv->channel));
}

EmpathyDispatchOperationState
empathy_dispatch_operation_get_status (EmpathyDispatchOperation *operation)
{
  EmpathyDispatchOperationPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_DISPATCH_OPERATION (operation),
    EMPATHY_DISPATCHER_OPERATION_STATE_PREPARING);

  priv = GET_PRIV (operation);

  return priv->status;
}

gboolean
empathy_dispatch_operation_is_incoming (EmpathyDispatchOperation *operation)
{
  EmpathyDispatchOperationPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_DISPATCH_OPERATION (operation), FALSE);

  priv = GET_PRIV (operation);

  return priv->incoming;
}
