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

#ifndef __GOSSIP_TELEPATHY_GROUP_H__
#define __GOSSIP_TELEPATHY_GROUP_H__

#include <glib.h>

#include <libtelepathy/tp-chan.h>

G_BEGIN_DECLS

#define GOSSIP_TYPE_TELEPATHY_GROUP         (gossip_telepathy_group_get_type ())
#define GOSSIP_TELEPATHY_GROUP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_TELEPATHY_GROUP, GossipTelepathyGroup))
#define GOSSIP_TELEPATHY_GROUP_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_TELEPATHY_GROUP, GossipTelepathyGroupClass))
#define GOSSIP_IS_TELEPATHY_GROUP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_TELEPATHY_GROUP))
#define GOSSIP_IS_TELEPATHY_GROUP_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_TELEPATHY_GROUP))
#define GOSSIP_TELEPATHY_GROUP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_TELEPATHY_GROUP, GossipTelepathyGroupClass))

typedef struct _GossipTelepathyGroup      GossipTelepathyGroup;
typedef struct _GossipTelepathyGroupClass GossipTelepathyGroupClass;
typedef struct _GossipTelepathyGroupPriv  GossipTelepathyGroupPriv;

struct _GossipTelepathyGroup {
	GObject      parent;
};

struct _GossipTelepathyGroupClass {
	GObjectClass parent_class;
};

typedef struct {
	guint  member;
	guint  actor;
	guint  reason;
	gchar *message;
} GossipTpGroupInfo;

GType                 gossip_telepathy_group_get_type        (void) G_GNUC_CONST;
GossipTelepathyGroup *gossip_telepathy_group_new             (TpChan                *tp_chan,
							      TpConn                *tp_conn);
void                  gossip_telepathy_group_add_members     (GossipTelepathyGroup  *group,
							      GArray                *handles,
							      const gchar           *message);
void                  gossip_telepathy_group_add_member      (GossipTelepathyGroup  *group,
							      guint                  handle,
							      const gchar           *message);
void                  gossip_telepathy_group_remove_members  (GossipTelepathyGroup  *group,
							      GArray                *handle,
							      const gchar           *message);
void                  gossip_telepathy_group_remove_member   (GossipTelepathyGroup  *group,
							      guint                  handle,
							      const gchar           *message);
GArray *              gossip_telepathy_group_get_members     (GossipTelepathyGroup  *group);
void                  gossip_telepathy_group_get_all_members (GossipTelepathyGroup  *group,
							      GArray               **members,
							      GArray               **local_pending,
							      GArray               **remote_pending);
GList *               gossip_telepathy_group_get_local_pending_members_with_info
							     (GossipTelepathyGroup  *group);
void                  gossip_telepathy_group_info_list_free  (GList                 *infos);
const gchar *         gossip_telepathy_group_get_name        (GossipTelepathyGroup  *group);
guint                 gossip_telepathy_group_get_self_handle (GossipTelepathyGroup  *group);
const gchar *         gossip_telepathy_group_get_object_path (GossipTelepathyGroup  *group);
gboolean              gossip_telepathy_group_is_member       (GossipTelepathyGroup  *group,
							      guint                  handle);

G_END_DECLS

#endif /* __GOSSIP_TELEPATHY_GROUP_H__ */
