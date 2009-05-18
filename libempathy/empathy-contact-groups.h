/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Imendio AB
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#ifndef __EMPATHY_CONTACT_GROUPS_H__
#define __EMPATHY_CONTACT_GROUPS_H__

G_BEGIN_DECLS

#include <glib.h>

void     empathy_contact_groups_get_all     (void);

gboolean empathy_contact_group_get_expanded (const gchar *group);
void     empathy_contact_group_set_expanded (const gchar *group,
					    gboolean     expanded);

G_END_DECLS

#endif /* __EMPATHY_CONTACT_GROUPS_H__ */
