/*
 * Copyright (C) 2007-2008 Guillaume Desmottes
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
 *
 * Authors: Guillaume Desmottes <gdesmott@gnome.org>
 */

#ifndef __EMPATHY_IRC_NETWORK_MANAGER_H__
#define __EMPATHY_IRC_NETWORK_MANAGER_H__

#include <glib-object.h>

#include "empathy-irc-network.h"

G_BEGIN_DECLS

typedef struct _EmpathyIrcNetworkManager      EmpathyIrcNetworkManager;
typedef struct _EmpathyIrcNetworkManagerClass EmpathyIrcNetworkManagerClass;

struct _EmpathyIrcNetworkManager
{
  GObject parent;
  gpointer priv;
};

struct _EmpathyIrcNetworkManagerClass
{
  GObjectClass parent_class;
};

GType empathy_irc_network_manager_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_IRC_NETWORK_MANAGER \
  (empathy_irc_network_manager_get_type ())
#define EMPATHY_IRC_NETWORK_MANAGER(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_IRC_NETWORK_MANAGER, \
                               EmpathyIrcNetworkManager))
#define EMPATHY_IRC_NETWORK_MANAGER_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_IRC_NETWORK_MANAGER, \
                            EmpathyIrcNetworkManagerClass))
#define EMPATHY_IS_IRC_NETWORK_MANAGER(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_IRC_NETWORK_MANAGER))
#define EMPATHY_IS_IRC_NETWORK_MANAGER_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_IRC_NETWORK_MANAGER))
#define EMPATHY_IRC_NETWORK_MANAGER_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_IRC_NETWORK_MANAGER, \
                              EmpathyIrcNetworkManagerClass))

EmpathyIrcNetworkManager * empathy_irc_network_manager_new (
    const gchar *global_file, const gchar *user_file);

void empathy_irc_network_manager_add (EmpathyIrcNetworkManager *manager,
    EmpathyIrcNetwork *network);

void empathy_irc_network_manager_remove (EmpathyIrcNetworkManager *manager,
    EmpathyIrcNetwork *network);

GSList * empathy_irc_network_manager_get_networks (
    EmpathyIrcNetworkManager *manager);

EmpathyIrcNetwork * empathy_irc_network_manager_find_network_by_address (
    EmpathyIrcNetworkManager *manager, const gchar *address);

G_END_DECLS

#endif /* __EMPATHY_IRC_NETWORK_MANAGER_H__ */
