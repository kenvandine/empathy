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

#ifndef __EMPATHY_DEBUG_H__
#define __EMPATHY_DEBUG_H__

#include "config.h"


#include <glib.h>
#include <telepathy-glib/debug.h>

G_BEGIN_DECLS

/* Please keep this enum in sync with #keys in empathy-debug.c */
typedef enum
{
  EMPATHY_DEBUG_TP = 1 << 1,
  EMPATHY_DEBUG_CHAT = 1 << 2,
  EMPATHY_DEBUG_CONTACT = 1 << 3,
  EMPATHY_DEBUG_ACCOUNT = 1 << 4,
  EMPATHY_DEBUG_IRC = 1 << 5,
  EMPATHY_DEBUG_DISPATCHER = 1 << 6,
  EMPATHY_DEBUG_FT = 1 << 7,
  EMPATHY_DEBUG_OTHER = 1 << 8,
} EmpathyDebugFlags;

gboolean empathy_debug_flag_is_set (EmpathyDebugFlags flag);
void empathy_debug (EmpathyDebugFlags flag, const gchar *format, ...)
    G_GNUC_PRINTF (2, 3);
void empathy_debug_set_flags (const gchar *flags_string);
G_END_DECLS

#endif /* __EMPATHY_DEBUG_H__ */

/* ------------------------------------ */

/* Below this point is outside the __DEBUG_H__ guard - so it can take effect
 * more than once. So you can do:
 *
 * #define DEBUG_FLAG EMPATHY_DEBUG_ONE_THING
 * #include "internal-debug.h"
 * ...
 * DEBUG ("if we're debugging one thing");
 * ...
 * #undef DEBUG_FLAG
 * #define DEBUG_FLAG EMPATHY_DEBUG_OTHER_THING
 * #include "internal-debug.h"
 * ...
 * DEBUG ("if we're debugging the other thing");
 * ...
 */

#ifdef DEBUG_FLAG
#ifdef ENABLE_DEBUG

#undef DEBUG
#define DEBUG(format, ...) \
  empathy_debug (DEBUG_FLAG, "%s: " format, G_STRFUNC, ##__VA_ARGS__)

#undef DEBUGGING
#define DEBUGGING empathy_debug_flag_is_set (DEBUG_FLAG)

#else /* !defined (ENABLE_DEBUG) */

#undef DEBUG
#define DEBUG(format, ...) do {} while (0)

#undef DEBUGGING
#define DEBUGGING 0

#endif /* !defined (ENABLE_DEBUG) */
#endif /* defined (DEBUG_FLAG) */
