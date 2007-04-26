/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
 * Copyright (C) 2007 Collabora Ltd.
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

#ifndef __GOSSIP_MESSAGE_H__
#define __GOSSIP_MESSAGE_H__

#include <glib-object.h>

#include "gossip-contact.h"
#include "gossip-time.h"

G_BEGIN_DECLS

#define GOSSIP_TYPE_MESSAGE         (gossip_message_get_gtype ())
#define GOSSIP_MESSAGE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_MESSAGE, GossipMessage))
#define GOSSIP_MESSAGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GOSSIP_TYPE_MESSAGE, GossipMessageClass))
#define GOSSIP_IS_MESSAGE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_MESSAGE))
#define GOSSIP_IS_MESSAGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_MESSAGE))
#define GOSSIP_MESSAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_MESSAGE, GossipMessageClass))

typedef struct _GossipMessage      GossipMessage;
typedef struct _GossipMessageClass GossipMessageClass;

struct _GossipMessage {
	GObject parent;
};

struct _GossipMessageClass {
	GObjectClass parent_class;
};

typedef enum {
	GOSSIP_MESSAGE_TYPE_NORMAL,
	GOSSIP_MESSAGE_TYPE_ACTION,
	GOSSIP_MESSAGE_TYPE_NOTICE,
	GOSSIP_MESSAGE_TYPE_AUTO_REPLY,
	GOSSIP_MESSAGE_TYPE_LAST
} GossipMessageType;

GType             gossip_message_get_gtype               (void) G_GNUC_CONST;
GossipMessage *   gossip_message_new                     (const gchar          *body);
GossipMessageType gossip_message_get_type                (GossipMessage        *message);
void              gossip_message_set_type                (GossipMessage        *message,
							  GossipMessageType     type);
GossipContact *   gossip_message_get_sender              (GossipMessage        *message);
void              gossip_message_set_sender              (GossipMessage        *message,
							  GossipContact        *contact);
const gchar *     gossip_message_get_body                (GossipMessage        *message);
void              gossip_message_set_body                (GossipMessage        *message,
							  const gchar          *body);
/* What return value should we have here? */
GossipTime        gossip_message_get_timestamp           (GossipMessage        *message);
void              gossip_message_set_timestamp           (GossipMessage        *message,
							  GossipTime            timestamp);

G_END_DECLS

#endif /* __GOSSIP_MESSAGE_H__ */
