/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_MESSAGE_H__
#define __EMPATHY_MESSAGE_H__

#include <glib-object.h>

#include "empathy-contact.h"
#include "empathy-time.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_MESSAGE         (empathy_message_get_type ())
#define EMPATHY_MESSAGE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_MESSAGE, EmpathyMessage))
#define EMPATHY_MESSAGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_MESSAGE, EmpathyMessageClass))
#define EMPATHY_IS_MESSAGE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_MESSAGE))
#define EMPATHY_IS_MESSAGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_MESSAGE))
#define EMPATHY_MESSAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_MESSAGE, EmpathyMessageClass))

typedef struct _EmpathyMessage      EmpathyMessage;
typedef struct _EmpathyMessageClass EmpathyMessageClass;

struct _EmpathyMessage {
	GObject parent;
	gpointer priv;
};

struct _EmpathyMessageClass {
	GObjectClass parent_class;
};

GType                    empathy_message_get_type          (void) G_GNUC_CONST;
EmpathyMessage *         empathy_message_new               (const gchar              *body);
TpChannelTextMessageType empathy_message_get_tptype        (EmpathyMessage           *message);
void                     empathy_message_set_tptype        (EmpathyMessage           *message,
							    TpChannelTextMessageType  type);
EmpathyContact *         empathy_message_get_sender        (EmpathyMessage           *message);
void                     empathy_message_set_sender        (EmpathyMessage           *message,
							    EmpathyContact           *contact);
EmpathyContact *         empathy_message_get_receiver      (EmpathyMessage           *message);
void                     empathy_message_set_receiver      (EmpathyMessage           *message,
							    EmpathyContact           *contact);
const gchar *            empathy_message_get_body          (EmpathyMessage           *message);
void                     empathy_message_set_body          (EmpathyMessage           *message,
							    const gchar              *body);
time_t                   empathy_message_get_timestamp     (EmpathyMessage           *message);
void                     empathy_message_set_timestamp     (EmpathyMessage           *message,
							    time_t                    timestamp);
gboolean                 empathy_message_should_highlight  (EmpathyMessage           *message);
TpChannelTextMessageType empathy_message_type_from_str     (const gchar              *type_str);
const gchar *            empathy_message_type_to_str       (TpChannelTextMessageType  type);

guint                    empathy_message_get_id (EmpathyMessage *message);
void                     empathy_message_set_id (EmpathyMessage *message, guint id);

gboolean                 empathy_message_equal (EmpathyMessage *message1, EmpathyMessage *message2);

G_END_DECLS

#endif /* __EMPATHY_MESSAGE_H__ */
