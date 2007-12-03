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
 *          Geert-Jan Van den Bogaerde <geertjan@gnome.org>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_PRIVATE_CHAT_H__
#define __EMPATHY_PRIVATE_CHAT_H__

#include <libempathy/empathy-tp-chat.h>
#include <libempathy/empathy-contact.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_PRIVATE_CHAT         (empathy_private_chat_get_type ())
#define EMPATHY_PRIVATE_CHAT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_PRIVATE_CHAT, EmpathyPrivateChat))
#define EMPATHY_PRIVATE_CHAT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_PRIVATE_CHAT, EmpathyPrivateChatClass))
#define EMPATHY_IS_PRIVATE_CHAT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_PRIVATE_CHAT))
#define EMPATHY_IS_PRIVATE_CHAT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_PRIVATE_CHAT))
#define EMPATHY_PRIVATE_CHAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_PRIVATE_CHAT, EmpathyPrivateChatClass))

typedef struct _EmpathyPrivateChat EmpathyPrivateChat;
typedef struct _EmpathyPrivateChatClass EmpathyPrivateChatClass;
typedef struct _EmpathyPrivateChatPriv EmpathyPrivateChatPriv;

#include "empathy-chat.h"

struct _EmpathyPrivateChat {
	EmpathyChat parent;
};

struct _EmpathyPrivateChatClass {
	EmpathyChatClass parent;
};

GType                empathy_private_chat_get_type    (void);
EmpathyPrivateChat * empathy_private_chat_new         (EmpathyTpChat      *tp_chat);
EmpathyContact *     empathy_private_chat_get_contact (EmpathyPrivateChat *chat);

G_END_DECLS

#endif /* __EMPATHY_PRIVATE_CHAT_H__ */
