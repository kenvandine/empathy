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

#ifndef __EMPATHY_IRC_SERVER_H__
#define __EMPATHY_IRC_SERVER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _EmpathyIrcServer EmpathyIrcServer;
typedef struct _EmpathyIrcServerClass EmpathyIrcServerClass;

struct _EmpathyIrcServer
{
  GObject parent;
  gpointer priv;
};

struct _EmpathyIrcServerClass
{
    GObjectClass parent_class;
};

GType empathy_irc_server_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_IRC_SERVER (empathy_irc_server_get_type ())
#define EMPATHY_IRC_SERVER(o)  \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_IRC_SERVER, EmpathyIrcServer))
#define EMPATHY_IRC_SERVER_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_IRC_SERVER, \
                            EmpathyIrcServerClass))
#define EMPATHY_IS_IRC_SERVER(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_IRC_SERVER))
#define EMPATHY_IS_IRC_SERVER_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_IRC_SERVER))
#define EMPATHY_IRC_SERVER_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_IRC_SERVER,\
                              EmpathyIrcServerClass))

EmpathyIrcServer * empathy_irc_server_new (const gchar *address, guint port,
    gboolean ssl);

G_END_DECLS

#endif /* __EMPATHY_IRC_SERVER_H__ */
