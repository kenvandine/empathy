/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
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
 */

#include <config.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>

#include "empathy-log-manager.h"
#include "empathy-log-source-empathy.h"
#include "empathy-log-source.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include "empathy-debug.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyLogManager)
typedef struct
{
  GList *sources;
} EmpathyLogManagerPriv;

G_DEFINE_TYPE (EmpathyLogManager, empathy_log_manager, G_TYPE_OBJECT);

static EmpathyLogManager * manager_singleton = NULL;

static void
empathy_log_manager_init (EmpathyLogManager *manager)
{
  EmpathyLogManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
      EMPATHY_TYPE_LOG_MANAGER, EmpathyLogManagerPriv);
  manager->priv = priv;
}

static void
log_manager_finalize (GObject *object)
{
  EmpathyLogManagerPriv *priv;
  GList *l;

  priv = GET_PRIV (object);

  for (l = priv->sources; l; l = l->next)
    {
      EmpathyLogSource *source = (EmpathyLogSource *) l->data;
      g_object_unref (source);
    }

  g_list_free (priv->sources);
}

static GObject *
log_manager_constructor (GType type,
                         guint n_props,
                         GObjectConstructParam *props)
{
  GObject *retval;

  if (manager_singleton)
    {
      retval = g_object_ref (manager_singleton);
    }
  else
    {
      retval = G_OBJECT_CLASS (empathy_log_manager_parent_class)->constructor
          (type, n_props, props);

      manager_singleton = EMPATHY_LOG_MANAGER (retval);
      g_object_add_weak_pointer (retval, (gpointer *) &manager_singleton);

      priv = GET_PRIV (manager_singleton);

      priv->sources = g_list_append (priv->sources,
          empathy_log_source_empathy_get_source ());
    }

  return retval;
}

static void
empathy_log_manager_class_init (EmpathyLogManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = log_manager_finalize;

  g_type_class_add_private (object_class, sizeof (EmpathyLogManagerPriv));
}

static void
empathy_log_manager_init (EmpathyLogManager *manager)
{
  EmpathyLogManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
      EMPATHY_TYPE_LOG_MANAGER, EmpathyLogManagerPriv);

  priv->sources = g_list_append (priv->sources,
      g_object_new (EMPATHY_TYPE_LOG_SOURCE_EMPATHY, NULL));

/*  priv->sources = g_list_append (priv->sources,
      g_object_new (EMPATHY_LOG_SOURCE_PIDGIN, NULL));*/

  manager->priv = priv;

  priv->sources = g_list_append (priv->sources,
      empathy_log_source_empathy_get_source ());
}

EmpathyLogManager *
empathy_log_manager_dup_singleton (void)
{
  return g_object_new (EMPATHY_TYPE_LOG_MANAGER, NULL);
}

void
empathy_log_manager_add_message (EmpathyLogManager *manager,
                                 const gchar *chat_id,
                                 gboolean chatroom,
                                 EmpathyMessage *message)
{
  EmpathyLogManagerPriv *priv;
  GList *l;

  g_return_if_fail (EMPATHY_IS_LOG_MANAGER (manager));
  g_return_if_fail (chat_id != NULL);
  g_return_if_fail (EMPATHY_IS_MESSAGE (message));

  priv = GET_PRIV (manager);

  for (l = priv->sources; l; l = l->next)
    {
      empathy_log_source_add_message (EMPATHY_LOG_SOURCE (l->data),
          chat_id, chatroom, message);
    }
}

gboolean
empathy_log_manager_exists (EmpathyLogManager *manager,
                            McAccount *account,
                            const gchar *chat_id,
                            gboolean chatroom)
{
  GList *l;
  EmpathyLogManagerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), FALSE);
  g_return_val_if_fail (MC_IS_ACCOUNT (account), FALSE);
  g_return_val_if_fail (chat_id != NULL, FALSE);

  priv = GET_PRIV (manager);

  for (l = priv->sources; l; l = l->next)
    {
      if (empathy_log_source_exists (EMPATHY_LOG_SOURCE (l->data),
            account, chat_id, chatroom))
        return TRUE;
    }

  return FALSE;
}

static void
log_manager_get_dates_foreach (gpointer data,
                               gpointer user_data)
{
  /* g_list_first is needed in case an older date was last inserted */
  GList *orig = g_list_first (user_data);

  if (g_list_find_custom (orig, data, (GCompareFunc) strcmp))
    orig = g_list_insert_sorted (orig, g_strdup (data), (GCompareFunc) strcmp);
}

GList *
empathy_log_manager_get_dates (EmpathyLogManager *manager,
                               McAccount *account,
                               const gchar *chat_id,
                               gboolean chatroom)
{
  GList *l, *out = NULL;
  EmpathyLogManagerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (chat_id != NULL, NULL);

  priv = GET_PRIV (manager);

  for (l = priv->sources; l; l = l->next)
    {
      EmpathyLogSource *source = EMPATHY_LOG_SOURCE (l->data);

      if (!out)
        out = empathy_log_source_get_dates (source, account, chat_id, chatroom);
      else
        {
          GList *new = empathy_log_source_get_dates (source, account, chat_id,
              chatroom);
          g_list_foreach (new, log_manager_get_dates_foreach, out);

          g_list_foreach (new, (GFunc) g_free, NULL);
          g_list_free (new);

          /* Similar reason for using g_list_first here as before */
          out = g_list_first (out);
        }
    }

  return out;
}

GList *
empathy_log_manager_get_messages_for_date (EmpathyLogManager *manager,
                                           McAccount *account,
                                           const gchar *chat_id,
                                           gboolean chatroom,
                                           const gchar *date)
{
  GList *l, *out = NULL;
  EmpathyLogManagerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (chat_id != NULL, NULL);

  priv = GET_PRIV (manager);

  for (l = priv->sources; l; l = l->next)
    {
      EmpathyLogSource *source = EMPATHY_LOG_SOURCE (l->data);

      if (!out)
        out = empathy_log_source_get_messages_for_date (source, account,
            chat_id, chatroom, date);
      else
        out = g_list_concat (out, empathy_log_source_get_messages_for_date (
              source, account, chat_id, chatroom, date));
    }

  return out;
}

GList *
empathy_log_manager_get_last_messages (EmpathyLogManager *manager,
                                       McAccount *account,
                                       const gchar *chat_id,
                                       gboolean chatroom)
{
  GList *messages = NULL;
  GList *dates;
  GList *l;

  g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (chat_id != NULL, NULL);

  dates = empathy_log_manager_get_dates (manager, account, chat_id, chatroom);

  l = g_list_last (dates);
  if (l)
    messages = empathy_log_manager_get_messages_for_date (manager, account,
        chat_id, chatroom, l->data);

  g_list_foreach (dates, (GFunc) g_free, NULL);
  g_list_free (dates);

  return messages;
}

GList *
empathy_log_manager_get_chats (EmpathyLogManager *manager,
                               McAccount *account)
{
  GList *l, *out = NULL;
  EmpathyLogManagerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

  priv = GET_PRIV (manager);

  for (l = priv->sources; l; l = l->next)
    {
      EmpathyLogSource *source = EMPATHY_LOG_SOURCE (l->data);

      if (!out)
        out = empathy_log_source_get_chats (source, account);
      else
        out = g_list_concat (out,
            empathy_log_source_get_chats (source, account));
    }

  return out;
}

GList *
empathy_log_manager_search_new (EmpathyLogManager *manager,
                                const gchar *text)
{
  GList *l, *out = NULL;
  EmpathyLogManagerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
  g_return_val_if_fail (!EMP_STR_EMPTY (text), NULL);

  priv = GET_PRIV (manager);

  for (l = priv->sources; l; l = l->next)
    {
      EmpathyLogSource *source = EMPATHY_LOG_SOURCE (l->data);

      if (!out)
        out = empathy_log_source_search_new (source, text);
      else
        out = g_list_concat (out,
            empathy_log_source_search_new (source, text));
    }

  return out;
}

void
empathy_log_manager_search_hit_free (EmpathyLogSearchHit *hit)
{
  if (hit->account)
    g_object_unref (hit->account);

  g_free (hit->date);
  g_free (hit->filename);
  g_free (hit->chat_id);

  g_slice_free (EmpathyLogSearchHit, hit);
}

void
empathy_log_manager_search_free (GList *hits)
{
  GList *l;

  for (l = hits; l; l = l->next)
    {
      empathy_log_manager_search_hit_free (l->data);
    }

  g_list_free (hits);
}

/* Format is just date, 20061201. */
gchar *
empathy_log_manager_get_date_readable (const gchar *date)
{
  time_t t;

  t = empathy_time_parse (date);

  return empathy_time_to_string_local (t, "%a %d %b %Y");
}
