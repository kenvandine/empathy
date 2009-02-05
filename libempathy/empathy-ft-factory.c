/*
 * empathy-ft-factory.c - Source for EmpathyFTFactory
 * Copyright (C) 2009 Collabora Ltd.
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
 * Author: Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
 */
 
/* empathy-ft-factory.c */

#include <glib.h>

#include "empathy-ft-factory.h"
#include "empathy-ft-handler.h"
#include "empathy-marshal.h"
#include "empathy-utils.h"

G_DEFINE_TYPE (EmpathyFTFactory, empathy_ft_factory, G_TYPE_OBJECT);

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyFTFactory)

enum {
  NEW_FT_HANDLER,
  LAST_SIGNAL
};

typedef struct {
  gboolean dispose_run;
} EmpathyFTFactoryPriv;

static EmpathyFTFactory *factory_singleton = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

static void
do_dispose (GObject *object)
{
  EmpathyFTFactoryPriv *priv = GET_PRIV (object);

  if (priv->dispose_run)
    return;

  priv->dispose_run = TRUE;

  G_OBJECT_CLASS (empathy_ft_factory_parent_class)->dispose (object);
}

static void
do_finalize (GObject *object)
{
  G_OBJECT_CLASS (empathy_ft_factory_parent_class)->finalize (object);
}

static GObject *
do_constructor (GType type,
                guint n_props,
                GObjectConstructParam *props)
{
	GObject *retval;

	if (factory_singleton) {
		retval = g_object_ref (factory_singleton);
	} else {
		retval = G_OBJECT_CLASS (empathy_ft_factory_parent_class)->constructor
			(type, n_props, props);

		factory_singleton = EMPATHY_FT_FACTORY (retval);
		g_object_add_weak_pointer (retval, (gpointer *) &factory_singleton);
	}

	return retval;
}

static void
empathy_ft_factory_class_init (EmpathyFTFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (EmpathyFTFactoryPriv));

  object_class->dispose = do_dispose;
  object_class->finalize = do_finalize;
  object_class->constructor = do_constructor;

  signals[NEW_FT_HANDLER] =
    g_signal_new ("new-ft-handler",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0,
      NULL, NULL,
      _empathy_marshal_VOID__OBJECT_BOOLEAN,
      G_TYPE_NONE,
      2, EMPATHY_TYPE_FT_HANDLER, G_TYPE_BOOLEAN);
}

static void
empathy_ft_factory_init (EmpathyFTFactory *self)
{
  EmpathyFTFactoryPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
    EMPATHY_TYPE_FT_FACTORY, EmpathyFTFactoryPriv);

  self->priv = priv;
}

/* public methods */

EmpathyFTFactory*
empathy_ft_factory_dup_singleton (void)
{
  return g_object_new (EMPATHY_TYPE_FT_FACTORY, NULL);
}

void
empathy_ft_factory_new_transfer (EmpathyFTFactory *factory,
                                 EmpathyContact *contact,
                                 GFile *file)
{
  EmpathyFTHandler *handler;

  g_return_if_fail (EMPATHY_IS_FT_FACTORY (factory));
  g_return_if_fail (EMPATHY_IS_CONTACT (contact));
  g_return_if_fail (G_IS_FILE (file));

  handler = empathy_ft_handler_new (contact, file);
  g_signal_emit (factory, signals[NEW_FT_HANDLER], 0, handler, TRUE);

  g_object_unref (handler);
}

void
empathy_ft_factory_claim_channel (EmpathyFTFactory *factory,
                                  EmpathyDispatchOperation *operation)
{
  g_return_if_fail (EMPATHY_IS_FT_FACTORY (factory));
  g_return_if_fail (EMPATHY_IS_DISPATCH_OPERATION (operation));

  /* TODO */
}

