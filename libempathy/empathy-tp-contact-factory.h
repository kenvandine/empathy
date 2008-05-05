/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_TP_CONTACT_FACTORY_H__
#define __EMPATHY_TP_CONTACT_FACTORY_H__

#include <glib.h>

#include <libmissioncontrol/mc-account.h>

#include "empathy-contact.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_TP_CONTACT_FACTORY         (empathy_tp_contact_factory_get_type ())
#define EMPATHY_TP_CONTACT_FACTORY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_TP_CONTACT_FACTORY, EmpathyTpContactFactory))
#define EMPATHY_TP_CONTACT_FACTORY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_TP_CONTACT_FACTORY, EmpathyTpContactFactoryClass))
#define EMPATHY_IS_TP_CONTACT_FACTORY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_TP_CONTACT_FACTORY))
#define EMPATHY_IS_TP_CONTACT_FACTORY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_TP_CONTACT_FACTORY))
#define EMPATHY_TP_CONTACT_FACTORY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_TP_CONTACT_FACTORY, EmpathyTpContactFactoryClass))

typedef struct _EmpathyTpContactFactory      EmpathyTpContactFactory;
typedef struct _EmpathyTpContactFactoryClass EmpathyTpContactFactoryClass;

struct _EmpathyTpContactFactory {
	GObject parent;
	gpointer priv;
};

struct _EmpathyTpContactFactoryClass {
	GObjectClass parent_class;
};

GType                    empathy_tp_contact_factory_get_type         (void) G_GNUC_CONST;
EmpathyTpContactFactory *empathy_tp_contact_factory_new              (McAccount *account);
EmpathyContact *         empathy_tp_contact_factory_get_user         (EmpathyTpContactFactory *tp_factory);
EmpathyContact *         empathy_tp_contact_factory_get_from_id      (EmpathyTpContactFactory *tp_factory,
								      const gchar             *id);
EmpathyContact *         empathy_tp_contact_factory_get_from_handle  (EmpathyTpContactFactory *tp_factory,
								      guint                    handle);
GList *                  empathy_tp_contact_factory_get_from_handles (EmpathyTpContactFactory *tp_factory,
								      const GArray            *handles);
void                     empathy_tp_contact_factory_set_alias        (EmpathyTpContactFactory *tp_factory,
								      EmpathyContact          *contact,
								      const gchar             *alias);
void                     empathy_tp_contact_factory_set_avatar       (EmpathyTpContactFactory *tp_factory,
								      const gchar             *data,
								      gsize                    size,
								      const gchar             *mime_type);
gboolean                 empathy_tp_contact_factory_is_ready         (EmpathyTpContactFactory *tp_factory);

G_END_DECLS

#endif /* __EMPATHY_TP_CONTACT_FACTORY_H__ */
