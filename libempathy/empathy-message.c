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

#include "config.h"

#include <string.h>

#include <telepathy-glib/util.h>

#include "empathy-message.h"
#include "empathy-utils.h"
#include "empathy-enum-types.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyMessage)
typedef struct {
	TpChannelTextMessageType  type;
	EmpathyContact           *sender;
	EmpathyContact           *receiver;
	gchar                    *body;
	time_t                    timestamp;
	guint                     id;
} EmpathyMessagePriv;

static void empathy_message_finalize   (GObject            *object);
static void message_get_property      (GObject            *object,
				       guint               param_id,
				       GValue             *value,
				       GParamSpec         *pspec);
static void message_set_property      (GObject            *object,
				       guint               param_id,
				       const GValue       *value,
				       GParamSpec         *pspec);

G_DEFINE_TYPE (EmpathyMessage, empathy_message, G_TYPE_OBJECT);

enum {
	PROP_0,
	PROP_TYPE,
	PROP_SENDER,
	PROP_RECEIVER,
	PROP_BODY,
	PROP_TIMESTAMP,
};

static void
empathy_message_class_init (EmpathyMessageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize     = empathy_message_finalize;
	object_class->get_property = message_get_property;
	object_class->set_property = message_set_property;

	g_object_class_install_property (object_class,
					 PROP_TYPE,
					 g_param_spec_uint ("type",
							    "Message Type",
							    "The type of message",
							    TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
							    TP_CHANNEL_TEXT_MESSAGE_TYPE_AUTO_REPLY,
							    TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SENDER,
					 g_param_spec_object ("sender",
							      "Message Sender",
							      "The sender of the message",
							      EMPATHY_TYPE_CONTACT,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_RECEIVER,
					 g_param_spec_object ("receiver",
							      "Message Receiver",
							      "The receiver of the message",
							      EMPATHY_TYPE_CONTACT,
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


	g_type_class_add_private (object_class, sizeof (EmpathyMessagePriv));

}

static void
empathy_message_init (EmpathyMessage *message)
{
	EmpathyMessagePriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (message,
		EMPATHY_TYPE_MESSAGE, EmpathyMessagePriv);

	message->priv = priv;
	priv->timestamp = empathy_time_get_current ();
}

static void
empathy_message_finalize (GObject *object)
{
	EmpathyMessagePriv *priv;

	priv = GET_PRIV (object);

	if (priv->sender) {
		g_object_unref (priv->sender);
	}
	if (priv->receiver) {
		g_object_unref (priv->receiver);
	}

	g_free (priv->body);

	G_OBJECT_CLASS (empathy_message_parent_class)->finalize (object);
}

static void
message_get_property (GObject    *object,
		      guint       param_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	EmpathyMessagePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_TYPE:
		g_value_set_uint (value, priv->type);
		break;
	case PROP_SENDER:
		g_value_set_object (value, priv->sender);
		break;
	case PROP_RECEIVER:
		g_value_set_object (value, priv->receiver);
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
	EmpathyMessagePriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_TYPE:
		empathy_message_set_tptype (EMPATHY_MESSAGE (object),
					    g_value_get_uint (value));
		break;
	case PROP_SENDER:
		empathy_message_set_sender (EMPATHY_MESSAGE (object),
					   EMPATHY_CONTACT (g_value_get_object (value)));
		break;
	case PROP_RECEIVER:
		empathy_message_set_receiver (EMPATHY_MESSAGE (object),
					     EMPATHY_CONTACT (g_value_get_object (value)));
		break;
	case PROP_BODY:
		empathy_message_set_body (EMPATHY_MESSAGE (object),
					 g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

EmpathyMessage *
empathy_message_new (const gchar *body)
{
	return g_object_new (EMPATHY_TYPE_MESSAGE,
			     "body", body,
			     NULL);
}

TpChannelTextMessageType
empathy_message_get_tptype (EmpathyMessage *message)
{
	EmpathyMessagePriv *priv;

	g_return_val_if_fail (EMPATHY_IS_MESSAGE (message),
			      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL);

	priv = GET_PRIV (message);

	return priv->type;
}

void
empathy_message_set_tptype (EmpathyMessage           *message,
			    TpChannelTextMessageType  type)
{
	EmpathyMessagePriv *priv;

	g_return_if_fail (EMPATHY_IS_MESSAGE (message));

	priv = GET_PRIV (message);

	priv->type = type;

	g_object_notify (G_OBJECT (message), "type");
}

EmpathyContact *
empathy_message_get_sender (EmpathyMessage *message)
{
	EmpathyMessagePriv *priv;

	g_return_val_if_fail (EMPATHY_IS_MESSAGE (message), NULL);

	priv = GET_PRIV (message);

	return priv->sender;
}

void
empathy_message_set_sender (EmpathyMessage *message, EmpathyContact *contact)
{
	EmpathyMessagePriv *priv;
	EmpathyContact     *old_sender;

	g_return_if_fail (EMPATHY_IS_MESSAGE (message));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	priv = GET_PRIV (message);

	old_sender = priv->sender;
	priv->sender = g_object_ref (contact);

	if (old_sender) {
		g_object_unref (old_sender);
	}

	g_object_notify (G_OBJECT (message), "sender");
}

EmpathyContact *
empathy_message_get_receiver (EmpathyMessage *message)
{
	EmpathyMessagePriv *priv;

	g_return_val_if_fail (EMPATHY_IS_MESSAGE (message), NULL);

	priv = GET_PRIV (message);

	return priv->receiver;
}

void
empathy_message_set_receiver (EmpathyMessage *message, EmpathyContact *contact)
{
	EmpathyMessagePriv *priv;
	EmpathyContact     *old_receiver;

	g_return_if_fail (EMPATHY_IS_MESSAGE (message));
	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	priv = GET_PRIV (message);

	old_receiver = priv->receiver;
	priv->receiver = g_object_ref (contact);

	if (old_receiver) {
		g_object_unref (old_receiver);
	}

	g_object_notify (G_OBJECT (message), "receiver");
}

const gchar *
empathy_message_get_body (EmpathyMessage *message)
{
	EmpathyMessagePriv *priv;

	g_return_val_if_fail (EMPATHY_IS_MESSAGE (message), NULL);

	priv = GET_PRIV (message);

	return priv->body;
}

void
empathy_message_set_body (EmpathyMessage *message,
			  const gchar    *body)
{
	EmpathyMessagePriv       *priv = GET_PRIV (message);
	TpChannelTextMessageType  type;

	g_return_if_fail (EMPATHY_IS_MESSAGE (message));

	g_free (priv->body);
	priv->body = NULL;

	type = TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
	if (g_str_has_prefix (body, "/me")) {
		type = TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION;
		body += 4;
	}
	else if (g_str_has_prefix (body, "/say")) {
		body += 5;
	}

	if (body) {
		priv->body = g_strdup (body);
	}

	if (type != priv->type) {
		empathy_message_set_tptype (message, type);
	}

	g_object_notify (G_OBJECT (message), "body");
}

time_t
empathy_message_get_timestamp (EmpathyMessage *message)
{
	EmpathyMessagePriv *priv;

	g_return_val_if_fail (EMPATHY_IS_MESSAGE (message), -1);

	priv = GET_PRIV (message);

	return priv->timestamp;
}

void
empathy_message_set_timestamp (EmpathyMessage *message,
			       time_t          timestamp)
{
	EmpathyMessagePriv *priv;

	g_return_if_fail (EMPATHY_IS_MESSAGE (message));
	g_return_if_fail (timestamp >= -1);

	priv = GET_PRIV (message);

	if (timestamp <= 0) {
		priv->timestamp = empathy_time_get_current ();
	} else {
		priv->timestamp = timestamp;
	}

	g_object_notify (G_OBJECT (message), "timestamp");
}

#define IS_SEPARATOR(ch) (ch == ' ' || ch == ',' || ch == '.' || ch == ':')
gboolean
empathy_message_should_highlight (EmpathyMessage *message)
{
	EmpathyContact *contact;
	const gchar   *msg, *to;
	gchar         *cf_msg, *cf_to;
	gchar         *ch;
	gboolean       ret_val;

	g_return_val_if_fail (EMPATHY_IS_MESSAGE (message), FALSE);

	ret_val = FALSE;

	msg = empathy_message_get_body (message);
	if (!msg) {
		return FALSE;
	}

	contact = empathy_message_get_receiver (message);
	if (!contact || !empathy_contact_is_user (contact)) {
		return FALSE;
	}

	to = empathy_contact_get_name (contact);
	if (!to) {
		return FALSE;
	}

	cf_msg = g_utf8_casefold (msg, -1);
	cf_to = g_utf8_casefold (to, -1);

	ch = strstr (cf_msg, cf_to);
	if (ch == NULL) {
		goto finished;
	}
	if (ch != cf_msg) {
		/* Not first in the message */
		if (!IS_SEPARATOR (*(ch - 1))) {
			goto finished;
		}
	}

	ch = ch + strlen (cf_to);
	if (ch >= cf_msg + strlen (cf_msg)) {
		ret_val = TRUE;
		goto finished;
	}

	if (IS_SEPARATOR (*ch)) {
		ret_val = TRUE;
		goto finished;
	}

finished:
	g_free (cf_msg);
	g_free (cf_to);

	return ret_val;
}

TpChannelTextMessageType
empathy_message_type_from_str (const gchar *type_str)
{
	if (strcmp (type_str, "normal") == 0) {
		return TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
	}
	if (strcmp (type_str, "action") == 0) {
		return TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION;
	}
	else if (strcmp (type_str, "notice") == 0) {
		return TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE;
	}
	else if (strcmp (type_str, "auto-reply") == 0) {
		return TP_CHANNEL_TEXT_MESSAGE_TYPE_AUTO_REPLY;
	}

	return TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
}

const gchar *
empathy_message_type_to_str (TpChannelTextMessageType type)
{
	switch (type) {
	case TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION:
		return "action";
	case TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE:
		return "notice";
	case TP_CHANNEL_TEXT_MESSAGE_TYPE_AUTO_REPLY:
		return "auto-reply";
	default:
		return "normal";
	}
}

guint
empathy_message_get_id (EmpathyMessage *message)
{
	EmpathyMessagePriv *priv = GET_PRIV (message);

	g_return_val_if_fail (EMPATHY_IS_MESSAGE (message), 0);

	return priv->id;
}

void
empathy_message_set_id (EmpathyMessage *message, guint id)
{
	EmpathyMessagePriv *priv = GET_PRIV (message);

	priv->id = id;
}

gboolean
empathy_message_equal (EmpathyMessage *message1, EmpathyMessage *message2)
{
	EmpathyMessagePriv *priv1;
	EmpathyMessagePriv *priv2;

	g_return_val_if_fail (EMPATHY_IS_MESSAGE (message1), FALSE);
	g_return_val_if_fail (EMPATHY_IS_MESSAGE (message2), FALSE);

	priv1 = GET_PRIV (message1);
	priv2 = GET_PRIV (message2);

	if (priv1->id == priv2->id && !tp_strdiff (priv1->body, priv2->body)) {
		return TRUE;
	}

	return FALSE;
}
