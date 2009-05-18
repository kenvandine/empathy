/*
 * empathy-call-factory.c - Source for EmpathyCallFactory
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

#include "empathy-marshal.h"
#include "empathy-call-factory.h"
#include "empathy-utils.h"

G_DEFINE_TYPE(EmpathyCallFactory, empathy_call_factory, G_TYPE_OBJECT)

/* signal enum */
enum
{
    NEW_CALL_HANDLER,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

/* private structure */
typedef struct {
  gboolean dispose_has_run;
} EmpathyCallFactoryPriv;

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyCallFactory)

static GObject *call_factory = NULL;

static void
empathy_call_factory_init (EmpathyCallFactory *obj)
{
  EmpathyCallFactoryPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (obj,
    EMPATHY_TYPE_CALL_FACTORY, EmpathyCallFactoryPriv);

  obj->priv = priv;
}

static GObject *
empathy_call_factory_constructor (GType type, guint n_construct_params,
  GObjectConstructParam *construct_params)
{
  g_return_val_if_fail (call_factory == NULL, NULL);

  call_factory = G_OBJECT_CLASS (empathy_call_factory_parent_class)->constructor
          (type, n_construct_params, construct_params);
  g_object_add_weak_pointer (call_factory, (gpointer)&call_factory);

  return call_factory;
}

static void
empathy_call_factory_finalize (GObject *object)
{
  /* free any data held directly by the object here */

  if (G_OBJECT_CLASS (empathy_call_factory_parent_class)->finalize)
    G_OBJECT_CLASS (empathy_call_factory_parent_class)->finalize (object);
}

static void
empathy_call_factory_dispose (GObject *object)
{
  EmpathyCallFactoryPriv *priv = GET_PRIV (object);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (empathy_call_factory_parent_class)->dispose)
    G_OBJECT_CLASS (empathy_call_factory_parent_class)->dispose (object);
}

static void
empathy_call_factory_class_init (
  EmpathyCallFactoryClass *empathy_call_factory_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (empathy_call_factory_class);

  g_type_class_add_private (empathy_call_factory_class,
    sizeof (EmpathyCallFactoryPriv));

  object_class->constructor = empathy_call_factory_constructor;
  object_class->dispose = empathy_call_factory_dispose;
  object_class->finalize = empathy_call_factory_finalize;

  signals[NEW_CALL_HANDLER] =
    g_signal_new ("new-call-handler",
      G_TYPE_FROM_CLASS (empathy_call_factory_class),
      G_SIGNAL_RUN_LAST, 0,
      NULL, NULL,
      _empathy_marshal_VOID__OBJECT_BOOLEAN,
      G_TYPE_NONE,
      2, EMPATHY_TYPE_CALL_HANDLER, G_TYPE_BOOLEAN);
}

EmpathyCallFactory *
empathy_call_factory_initialise (void)
{
  g_return_val_if_fail (call_factory == NULL, NULL);

  return EMPATHY_CALL_FACTORY (g_object_new (EMPATHY_TYPE_CALL_FACTORY, NULL));
}

EmpathyCallFactory *
empathy_call_factory_get (void)
{
  g_return_val_if_fail (call_factory != NULL, NULL);

  return EMPATHY_CALL_FACTORY (call_factory);
}

/**
 * empathy_call_factory_new_call_with_streams:
 * @factory: an #EmpathyCallFactory
 * @contact: an #EmpathyContact
 * @initial_audio: if %TRUE the call will be started with audio
 * @initial_video: if %TRUE the call will be started with video
 *
 * Initiate a new call with @contact.
 */
void
empathy_call_factory_new_call_with_streams (EmpathyCallFactory *factory,
    EmpathyContact *contact,
    gboolean initial_audio,
    gboolean initial_video)
{
  EmpathyCallHandler *handler;

  g_return_if_fail (factory != NULL);
  g_return_if_fail (contact != NULL);

  handler = empathy_call_handler_new_for_contact_with_streams (contact,
    initial_audio, initial_video);

  g_signal_emit (factory, signals[NEW_CALL_HANDLER], 0,
    handler, TRUE);

  g_object_unref (handler);
}


/**
 * empathy_call_factory_new_call:
 * @factory: an #EmpathyCallFactory
 * @contact: an #EmpathyContact
 *
 * Initiate a new call with @contact.
 */
void
empathy_call_factory_new_call (EmpathyCallFactory *factory,
    EmpathyContact *contact)
{
  empathy_call_factory_new_call_with_streams (factory, contact, TRUE, FALSE);
}

void
empathy_call_factory_claim_channel (EmpathyCallFactory *factory,
  EmpathyDispatchOperation *operation)
{
  EmpathyCallHandler *handler;
  EmpathyTpCall *call;

  g_return_if_fail (factory != NULL);
  g_return_if_fail (operation != NULL);

  call = EMPATHY_TP_CALL (
    empathy_dispatch_operation_get_channel_wrapper (operation));

  handler = empathy_call_handler_new_for_channel (call);
  empathy_dispatch_operation_claim (operation);

  /* FIXME should actually look at the channel */
  g_signal_emit (factory, signals[NEW_CALL_HANDLER], 0,
    handler, FALSE);

  g_object_unref (handler);
}

