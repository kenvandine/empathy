/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
 * Copyright (C) 2007 Nokia Corporation
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
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <telepathy-glib/debug.h>

#include "empathy-debug.h"

#include "empathy-debugger.h"

#ifdef ENABLE_DEBUG

static EmpathyDebugFlags flags = 0;

static GDebugKey keys[] = {
  { "Tp", EMPATHY_DEBUG_TP },
  { "Chat", EMPATHY_DEBUG_CHAT },
  { "Contact", EMPATHY_DEBUG_CONTACT },
  { "Account", EMPATHY_DEBUG_ACCOUNT },
  { "Irc", EMPATHY_DEBUG_IRC },
  { "Dispatcher", EMPATHY_DEBUG_DISPATCHER },
  { "Ft", EMPATHY_DEBUG_FT },
  { "Location", EMPATHY_DEBUG_LOCATION },
  { "Other", EMPATHY_DEBUG_OTHER },
  { 0, }
};

static void
debug_set_flags (EmpathyDebugFlags new_flags)
{
  flags |= new_flags;
}

void
empathy_debug_set_flags (const gchar *flags_string)
{
  guint nkeys;

  for (nkeys = 0; keys[nkeys].value; nkeys++);

  tp_debug_set_flags (flags_string);

  if (flags_string)
      debug_set_flags (g_parse_debug_string (flags_string, keys, nkeys));
}

gboolean
empathy_debug_flag_is_set (EmpathyDebugFlags flag)
{
  return (flag & flags) != 0;
}

GHashTable *flag_to_keys = NULL;

static const gchar *
debug_flag_to_key (EmpathyDebugFlags flag)
{
  if (flag_to_keys == NULL)
    {
      guint i;

      flag_to_keys = g_hash_table_new_full (g_direct_hash, g_direct_equal,
          NULL, g_free);

      for (i = 0; keys[i].value; i++)
        {
          GDebugKey key = (GDebugKey) keys[i];
          g_hash_table_insert (flag_to_keys, GUINT_TO_POINTER (key.value),
              g_strdup (key.key));
        }
    }

  return g_hash_table_lookup (flag_to_keys, GUINT_TO_POINTER (flag));
}

void
empathy_debug_free (void)
{
  if (flag_to_keys == NULL)
    return;

  g_hash_table_destroy (flag_to_keys);
  flag_to_keys = NULL;
}

static void
log_to_debugger (EmpathyDebugFlags flag,
    const gchar *message)
{
  EmpathyDebugger *dbg = empathy_debugger_get_singleton ();
  gchar *domain;
  GTimeVal now;

  g_get_current_time (&now);

  domain = g_strdup_printf ("%s/%s", G_LOG_DOMAIN, debug_flag_to_key (flag));

  empathy_debugger_add_message (dbg, &now, domain, G_LOG_LEVEL_DEBUG, message);

  g_free (domain);
}

void
empathy_debug (EmpathyDebugFlags flag,
    const gchar *format,
    ...)
{
  gchar *message;
  va_list args;

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  log_to_debugger (flag, message);

  if (flag & flags)
    g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", message);

  g_free (message);
}

#else

gboolean
empathy_debug_flag_is_set (EmpathyDebugFlags flag)
{
  return FALSE;
}

void
empathy_debug (EmpathyDebugFlags flag, const gchar *format, ...)
{
}

void
empathy_debug_set_flags (const gchar *flags_string)
{
}

#endif /* ENABLE_DEBUG */

