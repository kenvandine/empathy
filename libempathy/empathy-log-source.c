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

#include "empathy-log-source.h"

GType
empathy_log_source_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (EmpathyLogSourceInterface),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      NULL,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      0,
      0,      /* n_preallocs */
      NULL    /* instance_init */
    };
    type = g_type_register_static (G_TYPE_INTERFACE, "EmpathyLogSource",
        &info, 0);
  }
  return type;
}

gboolean
empathy_log_source_exists (EmpathyLogSource *self,
                           McAccount *account,
                           const gchar *chat_id,
                           gboolean chatroom)
{
  return EMPATHY_LOG_SOURCE_GET_INTERFACE (self)->exists (
      self, account, chat_id, chatroom);
}



void
empathy_log_source_add_message (EmpathyLogSource *self,
                                const gchar *chat_id,
                                gboolean chatroom,
                                EmpathyMessage *message)
{
  EMPATHY_LOG_SOURCE_GET_INTERFACE (self)->add_message (
      self, chat_id, chatroom, message);
}

GList *
empathy_log_source_get_dates (EmpathyLogSource *self,
                              McAccount *account,
                              const gchar *chat_id,
                              gboolean chatroom)
{
  return EMPATHY_LOG_SOURCE_GET_INTERFACE (self)->get_dates (
      self, account, chat_id, chatroom);
}

GList *
empathy_log_source_get_messages_for_date (EmpathyLogSource *self,
                                          McAccount *account,
                                          const gchar *chat_id,
                                          gboolean chatroom,
                                          const gchar *date)
{
  return EMPATHY_LOG_SOURCE_GET_INTERFACE (self)->get_messages_for_date (
      self, account, chat_id, chatroom, date);
}

GList *
empathy_log_source_get_last_messages (EmpathyLogSource *self,
                                      McAccount *account,
                                      const gchar *chat_id,
                                      gboolean chatroom)
{
  return EMPATHY_LOG_SOURCE_GET_INTERFACE (self)->get_last_messages (
      self, account, chat_id, chatroom);
}

GList *
empathy_log_source_get_chats (EmpathyLogSource *self,
                              McAccount *account)
{
  return EMPATHY_LOG_SOURCE_GET_INTERFACE (self)->get_chats (self, account);
}

GList *
empathy_log_source_search_new (EmpathyLogSource *self,
                               const gchar *text)
{
  return EMPATHY_LOG_SOURCE_GET_INTERFACE (self)->search_new (self, text);
}

void
empathy_log_source_ack_message (EmpathyLogSource *self,
                                const gchar *chat_id,
                                gboolean chatroom,
                                EmpathyMessage *message)
{
  EMPATHY_LOG_SOURCE_GET_INTERFACE (self)->ack_message (
      self, chat_id, chatroom, message);
}
