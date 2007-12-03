/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
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
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_GROUP_CHAT_H__
#define __EMPATHY_GROUP_CHAT_H__

G_BEGIN_DECLS

#include <libempathy/empathy-tp-chatroom.h>
#include <libempathy/empathy-contact.h>

#define EMPATHY_TYPE_GROUP_CHAT         (empathy_group_chat_get_type ())
#define EMPATHY_GROUP_CHAT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_GROUP_CHAT, EmpathyGroupChat))
#define EMPATHY_GROUP_CHAT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_GROUP_CHAT, EmpathyGroupChatClass))
#define EMPATHY_IS_GROUP_CHAT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_GROUP_CHAT))
#define EMPATHY_IS_GROUP_CHAT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_GROUP_CHAT))
#define EMPATHY_GROUP_CHAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_GROUP_CHAT, EmpathyGroupChatClass))

typedef struct _EmpathyGroupChat      EmpathyGroupChat;
typedef struct _EmpathyGroupChatClass EmpathyGroupChatClass;
typedef struct _EmpathyGroupChatPriv  EmpathyGroupChatPriv;

#include "empathy-chat.h"

struct _EmpathyGroupChat {
	EmpathyChat parent;

	EmpathyGroupChatPriv *priv;
};

struct _EmpathyGroupChatClass {
	EmpathyChatClass parent_class;
};

GType             empathy_group_chat_get_type          (void) G_GNUC_CONST;
EmpathyGroupChat *empathy_group_chat_new               (EmpathyTpChatroom *tp_chat);
gboolean          empathy_group_chat_get_show_contacts (EmpathyGroupChat  *chat);
void              empathy_group_chat_set_show_contacts (EmpathyGroupChat  *chat,
							gboolean           show);
void              empathy_group_chat_set_topic         (EmpathyGroupChat  *chat);

G_END_DECLS

#endif /* __EMPATHY_GROUP_CHAT_H__ */
