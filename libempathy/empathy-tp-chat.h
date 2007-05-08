/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_TP_CHAT_H__
#define __EMPATHY_TP_CHAT_H__

#include <glib.h>

#include <libtelepathy/tp-chan.h>
#include <libtelepathy/tp-constants.h>

#include <libmissioncontrol/mc-account.h>

#include "gossip-message.h"
#include "gossip-contact.h"


G_BEGIN_DECLS

#define EMPATHY_TYPE_TP_CHAT         (empathy_tp_chat_get_type ())
#define EMPATHY_TP_CHAT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_TP_CHAT, EmpathyTpChat))
#define EMPATHY_TP_CHAT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_TP_CHAT, EmpathyTpChatClass))
#define EMPATHY_IS_TP_CHAT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_TP_CHAT))
#define EMPATHY_IS_TP_CHAT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_TP_CHAT))
#define EMPATHY_TP_CHAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_TP_CHAT, EmpathyTpChatClass))

typedef struct _EmpathyTpChat      EmpathyTpChat;
typedef struct _EmpathyTpChatClass EmpathyTpChatClass;
typedef struct _EmpathyTpChatPriv  EmpathyTpChatPriv;

struct _EmpathyTpChat {
	GObject      parent;
};

struct _EmpathyTpChatClass {
	GObjectClass parent_class;
};

GType          empathy_tp_chat_get_type         (void) G_GNUC_CONST;
EmpathyTpChat *empathy_tp_chat_new              (McAccount                 *account,
						 TpChan                    *tp_chan);
EmpathyTpChat *empathy_tp_chat_new_with_contact (GossipContact             *contact);
void           empathy_tp_chat_request_pending  (EmpathyTpChat             *chat);
void           empathy_tp_chat_send             (EmpathyTpChat             *chat,
						 GossipMessage             *message);
void           empathy_tp_chat_set_state        (EmpathyTpChat             *chat,
						 TelepathyChannelChatState  state);
const gchar *  empathy_tp_chat_get_id           (EmpathyTpChat             *chat);

G_END_DECLS

#endif /* __EMPATHY_TP_CHAT_H__ */
