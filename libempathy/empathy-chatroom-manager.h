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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 *          Martyn Russell <martyn@imendio.com>
 */

#ifndef __EMPATHY_CHATROOM_MANAGER_H__
#define __EMPATHY_CHATROOM_MANAGER_H__

#include <glib-object.h>

#include <libmissioncontrol/mc-account.h>

#include "empathy-chatroom.h"
#include "empathy-dispatcher.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_CHATROOM_MANAGER         (empathy_chatroom_manager_get_type ())
#define EMPATHY_CHATROOM_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CHATROOM_MANAGER, EmpathyChatroomManager))
#define EMPATHY_CHATROOM_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_CHATROOM_MANAGER, EmpathyChatroomManagerClass))
#define EMPATHY_IS_CHATROOM_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CHATROOM_MANAGER))
#define EMPATHY_IS_CHATROOM_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CHATROOM_MANAGER))
#define EMPATHY_CHATROOM_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CHATROOM_MANAGER, EmpathyChatroomManagerClass))

typedef struct _EmpathyChatroomManager      EmpathyChatroomManager;
typedef struct _EmpathyChatroomManagerClass EmpathyChatroomManagerClass;

struct _EmpathyChatroomManager {
	GObject parent;
	gpointer priv;
};

struct _EmpathyChatroomManagerClass {
	GObjectClass parent_class;
};

GType                  empathy_chatroom_manager_get_type      (void) G_GNUC_CONST;
EmpathyChatroomManager *empathy_chatroom_manager_dup_singleton (const gchar *file);
gboolean               empathy_chatroom_manager_add           (EmpathyChatroomManager *manager,
							      EmpathyChatroom        *chatroom);
void                   empathy_chatroom_manager_remove        (EmpathyChatroomManager *manager,
							      EmpathyChatroom        *chatroom);
EmpathyChatroom *       empathy_chatroom_manager_find          (EmpathyChatroomManager *manager,
							      McAccount             *account,
							      const gchar           *room);
GList *                empathy_chatroom_manager_get_chatrooms (EmpathyChatroomManager *manager,
							      McAccount             *account);
guint                  empathy_chatroom_manager_get_count     (EmpathyChatroomManager *manager,
							      McAccount             *account);
void                   empathy_chatroom_manager_observe       (EmpathyChatroomManager *manager,
							      EmpathyDispatcher *dispatcher);

G_END_DECLS

#endif /* __EMPATHY_CHATROOM_MANAGER_H__ */
