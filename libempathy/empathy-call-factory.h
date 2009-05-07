/*
 * empathy-call-factory.h - Header for EmpathyCallFactory
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

#ifndef __EMPATHY_CALL_FACTORY_H__
#define __EMPATHY_CALL_FACTORY_H__

#include <glib-object.h>

#include <libempathy/empathy-dispatch-operation.h>
#include <libempathy/empathy-call-handler.h>

G_BEGIN_DECLS

typedef struct _EmpathyCallFactory EmpathyCallFactory;
typedef struct _EmpathyCallFactoryClass EmpathyCallFactoryClass;

struct _EmpathyCallFactoryClass {
    GObjectClass parent_class;
};

struct _EmpathyCallFactory {
    GObject parent;
    gpointer priv;
};

GType empathy_call_factory_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_CALL_FACTORY \
  (empathy_call_factory_get_type ())
#define EMPATHY_CALL_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), EMPATHY_TYPE_CALL_FACTORY, \
    EmpathyCallFactory))
#define EMPATHY_CALL_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EMPATHY_TYPE_CALL_FACTORY, \
    EmpathyCallFactoryClass))
#define EMPATHY_IS_CALL_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), EMPATHY_TYPE_CALL_FACTORY))
#define EMPATHY_IS_CALL_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EMPATHY_TYPE_CALL_FACTORY))
#define EMPATHY_CALL_FACTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EMPATHY_TYPE_CALL_FACTORY, \
    EmpathyCallFactoryClass))


EmpathyCallFactory *empathy_call_factory_initialise (void);

EmpathyCallFactory *empathy_call_factory_get (void);

void empathy_call_factory_new_call (EmpathyCallFactory *factory,
  EmpathyContact *contact);

void empathy_call_factory_new_call_with_streams (EmpathyCallFactory *factory,
  EmpathyContact *contact, gboolean initial_audio, gboolean initial_video);

void empathy_call_factory_claim_channel (EmpathyCallFactory *factory,
  EmpathyDispatchOperation *operation);

G_END_DECLS

#endif /* #ifndef __EMPATHY_CALL_FACTORY_H__*/
