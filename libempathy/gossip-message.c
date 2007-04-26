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

#include "config.h"

#include "gossip-message.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_MESSAGE, GossipMessagePriv))

typedef struct _GossipMessagePriv GossipMessagePriv;

struct _GossipMessagePriv {
	GossipMessageType     type;
	GossipContact        *sender;
	gchar                *body;
	GossipTime            timestamp;

};

static void gossip_message_class_init (GossipMessageClass *class);
static void gossip_message_init       (GossipMessage      *message);
static void gossip_message_finalize   (GObject            *object);
static void message_get_property      (GObject            *object,
				       guint               param_id,
				       GValue             *value,
				       GParamSpec         *pspec);
static void message_set_property      (GObject            *object,
				       guint               param_id,
				       const GValue       *value,
				       GParamSpec         *pspec);

enum {
	PROP_0,
	PROP_TYPE,
	PROP_SENDER,
	PROP_BODY,
	PROP_TIMESTAMP,
};

static gpointer parent_class = NULL;

GType
gossip_message_get_gtype (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GossipMessageClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) gossip_message_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GossipMessage),
			0,    /* n_preallocs */
			(GInstanceInitFunc) gossip_message_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "GossipMessage",
					       &info, 0);
	}

	return type;
}

static void
gossip_message_class_init (GossipMessageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	object_class->finalize     = gossip_message_finalize;
	object_class->get_property = message_get_property;
	object_class->set_property = message_set_property;

	g_object_class_install_property (object_class,
					 PROP_TYPE,
					 g_param_spec_int ("type",
							   "Message Type",
							   "The type of message",
							   GOSSIP_MESSAGE_TYPE_NORMAL,
							   GOSSIP_MESSAGE_TYPE_LAST,
							   GOSSIP_MESSAGE_TYPE_NORMAL,
							   G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SENDER,
					 g_param_spec_object ("sender",
							      "Message Sender",
							      "The sender of the message",
							      GOSSIP_TYPE_CONTACT,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_BODY,
					 g_param_spec_string ("body",
							      "Message Body",
							      "The content of the message",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_TIMESTAMP,
					 g_param_spec_long ("timestamp",
							    "timestamp",
							    "timestamp",
							    -1,
							    G_MAXLONG,
							    -1,
							    G_PARAM_READWRITE));


	g_type_class_add_private (object_class, sizeof (GossipMessagePriv));

}

static void
gossip_message_init (GossipMessage *message)
{
	GossipMessagePriv *priv;

	priv = GET_PRIV (message);

	priv->type = GOSSIP_MESSAGE_TYPE_NORMAL;
	priv->sender = NULL;
	priv->body = NULL;
	priv->timestamp = gossip_time_get_current ();
}

static void
gossip_message_finalize (GObject *object)
{
	GossipMessagePriv *priv;

	priv = GET_PRIV (object);

	if (priv->sender) {
		g_object_unref (priv->sender);
	}

	g_free (priv->body);

	(G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
message_get_property (GObject    *object,
		      guint       param_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	GossipMessagePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_TYPE:
		g_value_set_int (value, priv->type);
		break;
	case PROP_SENDER:
		g_value_set_object (value, priv->sender);
		break;
	case PROP_BODY:
		g_value_set_string (value, priv->body);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
message_set_property (GObject      *object,
		      guint         param_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	GossipMessagePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_TYPE:
		gossip_message_set_type (GOSSIP_MESSAGE (object),
					 g_value_get_int (value));
		break;
	case PROP_SENDER:
		gossip_message_set_sender (GOSSIP_MESSAGE (object),
					   GOSSIP_CONTACT (g_value_get_object (value)));
		break;
	case PROP_BODY:
		gossip_message_set_body (GOSSIP_MESSAGE (object),
					 g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

GossipMessage *
gossip_message_new (const gchar *body)
{
	return g_object_new (GOSSIP_TYPE_MESSAGE,
			     "body", body,
			     NULL);
}

GossipMessageType
gossip_message_get_type (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message),
			      GOSSIP_MESSAGE_TYPE_NORMAL);

	priv = GET_PRIV (message);

	return priv->type;
}

void
gossip_message_set_type (GossipMessage     *message,
			 GossipMessageType  type)
{
	GossipMessagePriv *priv;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	priv = GET_PRIV (message);

	priv->type = type;

	g_object_notify (G_OBJECT (message), "type");
}

GossipContact *
gossip_message_get_sender (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message), NULL);

	priv = GET_PRIV (message);

	return priv->sender;
}

void
gossip_message_set_sender (GossipMessage *message, GossipContact *contact)
{
	GossipMessagePriv *priv;
	GossipContact     *old_sender;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));
	g_return_if_fail (GOSSIP_IS_CONTACT (contact));

	priv = GET_PRIV (message);

	old_sender = priv->sender;
	priv->sender = g_object_ref (contact);

	if (old_sender) {
		g_object_unref (old_sender);
	}

	g_object_notify (G_OBJECT (message), "sender");
}

const gchar *
gossip_message_get_body (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message), NULL);

	priv = GET_PRIV (message);

	return priv->body;
}

void
gossip_message_set_body (GossipMessage *message,
			 const gchar   *body)
{
	GossipMessagePriv *priv;
	GossipMessageType  type;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	priv = GET_PRIV (message);

	g_free (priv->body);
	priv->body = NULL;

	type = GOSSIP_MESSAGE_TYPE_NORMAL;
	if (g_str_has_prefix (body, "/me")) {
		type = GOSSIP_MESSAGE_TYPE_ACTION;
		body += 4;
	}

	if (body) {
		priv->body = g_strdup (body);
	}

	if (type != priv->type) {
		gossip_message_set_type (message, type);
	}

	g_object_notify (G_OBJECT (message), "body");
}

GossipTime
gossip_message_get_timestamp (GossipMessage *message)
{
	GossipMessagePriv *priv;

	g_return_val_if_fail (GOSSIP_IS_MESSAGE (message), -1);

	priv = GET_PRIV (message);

	return priv->timestamp;
}

void
gossip_message_set_timestamp (GossipMessage *message,
			      GossipTime     timestamp)
{
	GossipMessagePriv *priv;

	g_return_if_fail (GOSSIP_IS_MESSAGE (message));
	g_return_if_fail (timestamp >= -1);

	priv = GET_PRIV (message);

	if (timestamp <= 0) {
		priv->timestamp = gossip_time_get_current ();
	} else {
		priv->timestamp = timestamp;
	}

	g_object_notify (G_OBJECT (message), "timestamp");
}

