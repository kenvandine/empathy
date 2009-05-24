/*
 * empathy-ft-factory.h - Header for EmpathyFTFactory
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

/* empathy-ft-factory.h */

#ifndef __EMPATHY_FT_FACTORY_H__
#define __EMPATHY_FT_FACTORY_H__

#include <glib-object.h>
#include <gio/gio.h>

#include "empathy-contact.h"
#include "empathy-ft-handler.h"
#include "empathy-dispatch-operation.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_FT_FACTORY empathy_ft_factory_get_type()
#define EMPATHY_FT_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   EMPATHY_TYPE_FT_FACTORY, EmpathyFTFactory))
#define EMPATHY_FT_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   EMPATHY_TYPE_FT_FACTORY, EmpathyFTFactoryClass))
#define EMPATHY_IS_FT_FACTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EMPATHY_TYPE_FT_FACTORY))
#define EMPATHY_IS_FT_FACTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), EMPATHY_TYPE_FT_FACTORY))
#define EMPATHY_FT_FACTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   EMPATHY_TYPE_FT_FACTORY, EmpathyFTFactoryClass))

typedef struct {
  GObject parent;
  gpointer priv;
} EmpathyFTFactory;

typedef struct {
  GObjectClass parent_class;
} EmpathyFTFactoryClass;

GType empathy_ft_factory_get_type (void);

/* public methods */
EmpathyFTFactory* empathy_ft_factory_dup_singleton (void);
void empathy_ft_factory_new_transfer_outgoing (EmpathyFTFactory *factory,
    EmpathyContact *contact,
    GFile *source);
void empathy_ft_factory_claim_channel (EmpathyFTFactory *factory,
    EmpathyDispatchOperation *operation);
void empathy_ft_factory_set_destination_for_incoming_handler (
    EmpathyFTFactory *factory,
    EmpathyFTHandler *handler,
    GFile *destination);

G_END_DECLS

#endif /* __EMPATHY_FT_FACTORY_H__ */

