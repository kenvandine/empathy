/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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
 */

#ifndef __EMPATHY_DEBUG_H__
#define __EMPATHY_DEBUG_H__

#include <glib.h>

G_BEGIN_DECLS

#ifdef G_HAVE_ISO_VARARGS
#  ifdef EMPATHY_DISABLE_DEBUG
#    define empathy_debug(...)
#  else
#    define empathy_debug(...) empathy_debug_impl (__VA_ARGS__)
#  endif
#elif defined(G_HAVE_GNUC_VARARGS)
#  if EMPATHY_DISABLE_DEBUG
#    define empathy_debug(fmt...)
#  else
#    define empathy_debug(fmt...) empathy_debug_impl(fmt)
#  endif
#else
#  if EMPATHY_DISABLE_DEBUG
#    define empathy_debug(x)
#  else
#    define empathy_debug empathy_debug_impl
#  endif
#endif

void empathy_debug_impl                  (const gchar *domain,
					  const gchar *msg,
					  ...);
void empathy_debug_set_log_file_from_env (void);

G_END_DECLS

#endif /* __EMPATHY_DEBUG_H__ */

