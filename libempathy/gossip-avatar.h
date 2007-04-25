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

#ifndef __GOSSIP_AVATAR_H__
#define __GOSSIP_AVATAR_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_AVATAR (gossip_avatar_get_gtype ())

typedef struct _GossipAvatar GossipAvatar;

struct _GossipAvatar {
	guchar *data;
	gsize   len;
	gchar  *format;
	guint   refcount;
};

GType          gossip_avatar_get_gtype (void) G_GNUC_CONST;
GossipAvatar * gossip_avatar_new       (guchar       *avatar,
					gsize         len,
					gchar        *format);
GossipAvatar * gossip_avatar_ref       (GossipAvatar *avatar);
void           gossip_avatar_unref     (GossipAvatar *avatar);

G_END_DECLS

#endif /*  __GOSSIP_AVATAR_H__ */
