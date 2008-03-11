/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
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

#ifndef __EMPATHY_TP_CHATROOM_H__
#define __EMPATHY_TP_CHATROOM_H__

#include <glib.h>

#include <libtelepathy/tp-chan.h>

#include <libmissioncontrol/mc-account.h>

#include "empathy-tp-chat.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_TP_CHATROOM         (empathy_tp_chatroom_get_type ())
#define EMPATHY_TP_CHATROOM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_TP_CHATROOM, EmpathyTpChatroom))
#define EMPATHY_TP_CHATROOM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_TP_CHATROOM, EmpathyTpChatroomClass))
#define EMPATHY_IS_TP_CHATROOM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_TP_CHATROOM))
#define EMPATHY_IS_TP_CHATROOM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_TP_CHATROOM))
#define EMPATHY_TP_CHATROOM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_TP_CHATROOM, EmpathyTpChatroomClass))

typedef struct _EmpathyTpChatroom      EmpathyTpChatroom;
typedef struct _EmpathyTpChatroomClass EmpathyTpChatroomClass;
typedef struct _EmpathyTpChatroomPriv  EmpathyTpChatroomPriv;

struct _EmpathyTpChatroom {
	EmpathyTpChat parent;
};

struct _EmpathyTpChatroomClass {
	EmpathyTpChatClass parent_class;
};

GType              empathy_tp_chatroom_get_type          (void) G_GNUC_CONST;
EmpathyTpChatroom *empathy_tp_chatroom_new               (McAccount          *account,
							  TpChan             *tp_chan);
gboolean           empathy_tp_chatroom_get_invitation    (EmpathyTpChatroom  *chatroom,
							  EmpathyContact     **contact,
							  const gchar       **message);
void               empathy_tp_chatroom_accept_invitation (EmpathyTpChatroom *chatroom);
G_END_DECLS

#endif /* __EMPATHY_TP_CHATROOM_H__ */
