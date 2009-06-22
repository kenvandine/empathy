/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Christophe Fergeau <teuf@gnome.org>
 * Based on itdb_plist parser from the gtkpod project.
 *
 * The code contained in this file is free software; you can redistribute
 * it and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either version
 * 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this code; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __EMPATHY_PLIST_H__
#define __EMPATHY_PLIST_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

GValue * empathy_plist_parse_from_file (const char *filename);
GValue * empathy_plist_parse_from_memory (const char *data, gsize len);

G_END_DECLS

#endif
