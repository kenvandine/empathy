/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Xavier Claessens <xclaesse@gmail.com>
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

#ifndef __EMPATHY_AVATAR_H__
#define __EMPATHY_AVATAR_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_AVATAR (empathy_avatar_get_type ())

typedef struct _EmpathyAvatar EmpathyAvatar;

struct _EmpathyAvatar {
	guchar *data;
	gsize   len;
	gchar  *format;
	guint   refcount;
};

GType          empathy_avatar_get_type (void) G_GNUC_CONST;
EmpathyAvatar * empathy_avatar_new       (guchar       *avatar,
					gsize         len,
					gchar        *format);
EmpathyAvatar * empathy_avatar_ref       (EmpathyAvatar *avatar);
void           empathy_avatar_unref     (EmpathyAvatar *avatar);

G_END_DECLS

#endif /*  __EMPATHY_AVATAR_H__ */
