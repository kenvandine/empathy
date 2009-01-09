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

#ifndef __EMPATHY_TP_CHAT_H__
#define __EMPATHY_TP_CHAT_H__

#include <glib.h>

#include <telepathy-glib/channel.h>
#include <telepathy-glib/enums.h>

#include "empathy-message.h"
#include "empathy-contact.h"


G_BEGIN_DECLS

#define EMPATHY_TYPE_TP_CHAT         (empathy_tp_chat_get_type ())
#define EMPATHY_TP_CHAT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_TP_CHAT, EmpathyTpChat))
#define EMPATHY_TP_CHAT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_TP_CHAT, EmpathyTpChatClass))
#define EMPATHY_IS_TP_CHAT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_TP_CHAT))
#define EMPATHY_IS_TP_CHAT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_TP_CHAT))
#define EMPATHY_TP_CHAT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_TP_CHAT, EmpathyTpChatClass))

typedef struct _EmpathyTpChat      EmpathyTpChat;
typedef struct _EmpathyTpChatClass EmpathyTpChatClass;

struct _EmpathyTpChat {
	GObject parent;
	gpointer priv;
};

struct _EmpathyTpChatClass {
	GObjectClass parent_class;
};

GType          empathy_tp_chat_get_type             (void) G_GNUC_CONST;
EmpathyTpChat *empathy_tp_chat_new                  (TpChannel          *channel);
void           empathy_tp_chat_close                (EmpathyTpChat      *chat);
const gchar *  empathy_tp_chat_get_id               (EmpathyTpChat      *chat);
EmpathyContact*empathy_tp_chat_get_remote_contact   (EmpathyTpChat      *chat);
McAccount *    empathy_tp_chat_get_account          (EmpathyTpChat      *chat);
TpChannel *    empathy_tp_chat_get_channel          (EmpathyTpChat      *chat);
gboolean       empathy_tp_chat_is_ready             (EmpathyTpChat      *chat);
guint          empathy_tp_chat_get_members_count    (EmpathyTpChat      *chat);
void           empathy_tp_chat_send                 (EmpathyTpChat      *chat,
						     EmpathyMessage     *message);
void           empathy_tp_chat_set_state            (EmpathyTpChat      *chat,
						     TpChannelChatState  state);
void           empathy_tp_chat_set_property         (EmpathyTpChat      *chat,
						     const gchar        *name,
						     const GValue       *value);

/* Returns a read-only list of pending messages (should be a copy maybe ?) */
const GList *  empathy_tp_chat_get_pending_messages (EmpathyTpChat *chat);
void           empathy_tp_chat_acknowledge_message (EmpathyTpChat *chat,
						     EmpathyMessage *message);
void           empathy_tp_chat_acknowledge_messages (EmpathyTpChat *chat,
						     const GList *messages);

G_END_DECLS

#endif /* __EMPATHY_TP_CHAT_H__ */
