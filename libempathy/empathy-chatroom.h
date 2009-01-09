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

#ifndef __EMPATHY_CHATROOM_H__
#define __EMPATHY_CHATROOM_H__

#include <glib-object.h>

#include <libmissioncontrol/mc-account.h>

#include <libempathy/empathy-tp-chat.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_CHATROOM             (empathy_chatroom_get_type ())
#define EMPATHY_CHATROOM(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CHATROOM, EmpathyChatroom))
#define EMPATHY_CHATROOM_CLASS(k)         (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_CHATROOM, EmpathyChatroomClass))
#define EMPATHY_IS_CHATROOM(o)            (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CHATROOM))
#define EMPATHY_IS_CHATROOM_CLASS(k)      (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CHATROOM))
#define EMPATHY_CHATROOM_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CHATROOM, EmpathyChatroomClass))

#define EMPATHY_TYPE_CHATROOM_INVITE       (empathy_chatroom_invite_get_gtype ())

typedef struct _EmpathyChatroom      EmpathyChatroom;
typedef struct _EmpathyChatroomClass EmpathyChatroomClass;

struct _EmpathyChatroom {
	GObject parent;
	gpointer priv;
};

struct _EmpathyChatroomClass {
	GObjectClass parent_class;
};

GType            empathy_chatroom_get_type        (void) G_GNUC_CONST;
EmpathyChatroom *empathy_chatroom_new             (McAccount       *account);
EmpathyChatroom *empathy_chatroom_new_full        (McAccount       *account,
						   const gchar     *room,
						   const gchar     *name,
						   gboolean         auto_connect);
McAccount *     empathy_chatroom_get_account      (EmpathyChatroom *chatroom);
void            empathy_chatroom_set_account      (EmpathyChatroom *chatroom,
						   McAccount       *account);
const gchar *   empathy_chatroom_get_room         (EmpathyChatroom *chatroom);
void            empathy_chatroom_set_room         (EmpathyChatroom *chatroom,
						   const gchar     *room);
const gchar *   empathy_chatroom_get_name         (EmpathyChatroom *chatroom);
void            empathy_chatroom_set_name         (EmpathyChatroom *chatroom,
						   const gchar     *name);
gboolean        empathy_chatroom_get_auto_connect (EmpathyChatroom *chatroom);
void            empathy_chatroom_set_auto_connect (EmpathyChatroom *chatroom,
						   gboolean         auto_connect);
gboolean        empathy_chatroom_equal            (gconstpointer    v1,
						   gconstpointer    v2);
EmpathyTpChat * empathy_chatroom_get_tp_chat      (EmpathyChatroom *chatroom);

G_END_DECLS

#endif /* __EMPATHY_CHATROOM_H__ */
