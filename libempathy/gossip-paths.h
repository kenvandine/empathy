/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GOSSIP_PATHS_H__
#define __GOSSIP_PATHS_H__

#include <glib.h>

G_BEGIN_DECLS

gchar *gossip_paths_get_glade_path  (const gchar *filename);
gchar *gossip_paths_get_image_path  (const gchar *filename);
gchar *gossip_paths_get_dtd_path    (const gchar *filename);
gchar *gossip_paths_get_sound_path  (const gchar *filename);
gchar *gossip_paths_get_locale_path (void);

G_END_DECLS

#endif /* __GOSSIP_PATHS_H__ */
