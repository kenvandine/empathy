/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Collabora Ltd.
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
 * Authors: Jonny Lamb <jonny.lamb@collabora.co.uk>
 */

#ifndef __EMPATHY_LOG_SOURCE_H__
#define __EMPATHY_LOG_SOURCE_H__

#include <glib-object.h>

#include <libmissioncontrol/mc-account.h>

#include "empathy-message.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_LOG_SOURCE (empathy_log_source_get_type ())
#define EMPATHY_LOG_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EMPATHY_TYPE_LOG_SOURCE, \
                               EmpathyLogSource))
#define EMPATHY_IS_LOG_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EMPATHY_TYPE_LOG_SOURCE))
#define EMPATHY_LOG_SOURCE_GET_INTERFACE(inst) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EMPATHY_TYPE_LOG_SOURCE, \
                                  EmpathyLogSourceInterface))

typedef struct _EmpathyLogSource EmpathyLogSource; /* dummy object */
typedef struct _EmpathyLogSourceInterface EmpathyLogSourceInterface;

struct _EmpathyLogSourceInterface
{
  GTypeInterface parent;

  const gchar * (*get_name) (EmpathyLogSource *self);
  gboolean (*exists) (EmpathyLogSource *self, McAccount *account,
      const gchar *chat_id, gboolean chatroom);
  void (*add_message) (EmpathyLogSource *self, const gchar *chat_id,
      gboolean chatroom, EmpathyMessage *message);
  GList * (*get_dates) (EmpathyLogSource *self, McAccount *account,
      const gchar *chat_id, gboolean chatroom);
  GList * (*get_messages_for_date) (EmpathyLogSource *self,
      McAccount *account, const gchar *chat_id, gboolean chatroom,
      const gchar *date);
  GList * (*get_last_messages) (EmpathyLogSource *self, McAccount *account,
      const gchar *chat_id, gboolean chatroom);
  GList * (*get_chats) (EmpathyLogSource *self,
            McAccount         *account);
  GList * (*search_new) (EmpathyLogSource *self, const gchar *text);
  void (*ack_message) (EmpathyLogSource *self, const gchar *chat_id,
      gboolean chatroom, EmpathyMessage *message);
};

GType empathy_log_source_get_type (void) G_GNUC_CONST;

const gchar *empathy_log_source_get_name (EmpathyLogSource *self);
gboolean empathy_log_source_exists (EmpathyLogSource *self,
    McAccount *account, const gchar *chat_id, gboolean chatroom);
void empathy_log_source_add_message (EmpathyLogSource *self,
    const gchar *chat_id, gboolean chatroom, EmpathyMessage *message);
GList *empathy_log_source_get_dates (EmpathyLogSource *self,
    McAccount *account, const gchar *chat_id, gboolean chatroom);
GList *empathy_log_source_get_messages_for_date (EmpathyLogSource *self,
    McAccount *account, const gchar *chat_id, gboolean chatroom,
    const gchar *date);
GList *empathy_log_source_get_last_messages (EmpathyLogSource *self,
    McAccount *account, const gchar *chat_id, gboolean chatroom);
GList *empathy_log_source_get_chats (EmpathyLogSource *self,
    McAccount *account);
GList *empathy_log_source_search_new (EmpathyLogSource *self,
    const gchar *text);
void empathy_log_source_ack_message (EmpathyLogSource *self,
    const gchar *chat_id, gboolean chatroom, EmpathyMessage *message);

G_END_DECLS

#endif /* __EMPATHY_LOG_SOURCE_H__ */
