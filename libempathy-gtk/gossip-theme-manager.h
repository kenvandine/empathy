/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
 * 
 * Authors: Richard Hult <richard@imendio.com>
 */

#ifndef __GOSSIP_THEME_MANAGER_H__
#define __GOSSIP_THEME_MANAGER_H__

#include <glib-object.h>

#include "gossip-chat-view.h"

G_BEGIN_DECLS

#define GOSSIP_TYPE_THEME_MANAGER         (gossip_theme_manager_get_type ())
#define GOSSIP_THEME_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GOSSIP_TYPE_THEME_MANAGER, GossipThemeManager))
#define GOSSIP_THEME_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GOSSIP_TYPE_THEME_MANAGER, GossipThemeManagerClass))
#define GOSSIP_IS_THEME_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOSSIP_TYPE_THEME_MANAGER))
#define GOSSIP_IS_THEME_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GOSSIP_TYPE_THEME_MANAGER))
#define GOSSIP_THEME_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GOSSIP_TYPE_THEME_MANAGER, GossipThemeManagerClass))

typedef struct _GossipThemeManager      GossipThemeManager;
typedef struct _GossipThemeManagerClass GossipThemeManagerClass;

struct _GossipThemeManager {
	GObject      parent;
};

struct _GossipThemeManagerClass {
	GObjectClass parent_class;
};

GType               gossip_theme_manager_get_type            (void) G_GNUC_CONST;
GossipThemeManager *gossip_theme_manager_get                 (void);
const gchar **      gossip_theme_manager_get_themes          (void);
void                gossip_theme_manager_apply               (GossipThemeManager *manager,
							      GossipChatView     *view,
							      const gchar        *theme);
void                gossip_theme_manager_apply_saved         (GossipThemeManager *manager,
							      GossipChatView     *view);
void                gossip_theme_manager_update_show_avatars (GossipThemeManager *manager,
							      GossipChatView     *view,
							      gboolean            show);

G_END_DECLS

#endif /* __GOSSIP_THEME_MANAGER_H__ */
