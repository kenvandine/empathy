/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

void
empathy_debug (EmpathyDebugFlags flag,
               const gchar *format,
               ...)
{
  if (flag & flags)
    {
      va_list args;
      va_start (args, format);
      g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
      va_end (args);
    }
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

