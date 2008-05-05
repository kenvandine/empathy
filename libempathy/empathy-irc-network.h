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

#ifndef __EMPATHY_IRC_NETWORK_H__
#define __EMPATHY_IRC_NETWORK_H__

#include <glib-object.h>

#include "empathy-irc-server.h"

G_BEGIN_DECLS

typedef struct _EmpathyIrcNetwork EmpathyIrcNetwork;
typedef struct _EmpathyIrcNetworkClass EmpathyIrcNetworkClass;

struct _EmpathyIrcNetwork
{
  GObject parent;
  gpointer priv;

  gboolean user_defined;
  gboolean dropped;
};

struct _EmpathyIrcNetworkClass
{
    GObjectClass parent_class;
};

GType empathy_irc_network_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_IRC_NETWORK (empathy_irc_network_get_type ())
#define EMPATHY_IRC_NETWORK(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_IRC_NETWORK, \
                               EmpathyIrcNetwork))
#define EMPATHY_IRC_NETWORK_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_IRC_NETWORK,\
                            EmpathyIrcNetworkClass))
#define EMPATHY_IS_IRC_NETWORK(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_IRC_NETWORK))
#define EMPATHY_IS_IRC_NETWORK_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_IRC_NETWORK))
#define EMPATHY_IRC_NETWORK_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_IRC_NETWORK, \
                              EmpathyIrcNetworkClass))

EmpathyIrcNetwork * empathy_irc_network_new (const gchar *name);

GSList * empathy_irc_network_get_servers (EmpathyIrcNetwork *network);

void empathy_irc_network_append_server (EmpathyIrcNetwork *network,
    EmpathyIrcServer *server);

void empathy_irc_network_remove_server (EmpathyIrcNetwork *network,
    EmpathyIrcServer *server);

void empathy_irc_network_set_server_position (EmpathyIrcNetwork *network,
    EmpathyIrcServer *server, gint pos);

G_END_DECLS

#endif /* __EMPATHY_IRC_NETWORK_H__ */
