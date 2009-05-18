/*
 * Copyright (C) 2007 Guillaume Desmottes
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
 * Authors: Guillaume Desmottes <gdesmott@gnome.org>
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include <telepathy-glib/util.h>

#include "empathy-marshal.h"
#include "empathy-irc-network.h"
#include "empathy-utils.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyIrcNetwork)
typedef struct
{
  gchar *name;
  gchar *charset;
  GSList *servers;
} EmpathyIrcNetworkPriv;

/* properties */
enum
{
  PROP_NAME = 1,
  PROP_CHARSET,
  LAST_PROPERTY
};

/* signals */
enum
{
  MODIFIED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (EmpathyIrcNetwork, empathy_irc_network, G_TYPE_OBJECT);

static void
server_modified_cb (EmpathyIrcServer *server,
                    EmpathyIrcNetwork *self)
{
  g_signal_emit (self, signals[MODIFIED], 0);
}

static void
empathy_irc_network_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
  EmpathyIrcNetwork *self = EMPATHY_IRC_NETWORK (object);
  EmpathyIrcNetworkPriv *priv = GET_PRIV (self);

  switch (property_id)
    {
      case PROP_NAME:
        g_value_set_string (value, priv->name);
        break;
      case PROP_CHARSET:
        g_value_set_string (value, priv->charset);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_irc_network_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  EmpathyIrcNetwork *self = EMPATHY_IRC_NETWORK (object);
  EmpathyIrcNetworkPriv *priv = GET_PRIV (self);

  switch (property_id)
    {
      case PROP_NAME:
        if (tp_strdiff (priv->name, g_value_get_string (value)))
          {
            g_free (priv->name);
            priv->name = g_value_dup_string (value);
            g_signal_emit (object, signals[MODIFIED], 0);
          }
        break;
      case PROP_CHARSET:
        if (tp_strdiff (priv->charset, g_value_get_string (value)))
          {
            g_free (priv->charset);
            priv->charset = g_value_dup_string (value);
            g_signal_emit (object, signals[MODIFIED], 0);
          }
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_irc_network_dispose (GObject *object)
{
  EmpathyIrcNetwork *self = EMPATHY_IRC_NETWORK (object);
  EmpathyIrcNetworkPriv *priv = GET_PRIV (self);
  GSList *l;

  for (l = priv->servers; l != NULL; l = g_slist_next (l))
    {
      g_signal_handlers_disconnect_by_func (l->data,
          G_CALLBACK (server_modified_cb), self);
      g_object_unref (l->data);
    }

  G_OBJECT_CLASS (empathy_irc_network_parent_class)->dispose (object);
}

static void
empathy_irc_network_finalize (GObject *object)
{
  EmpathyIrcNetwork *self = EMPATHY_IRC_NETWORK (object);
  EmpathyIrcNetworkPriv *priv = GET_PRIV (self);

  g_slist_free (priv->servers);
  g_free (priv->name);
  g_free (priv->charset);

  G_OBJECT_CLASS (empathy_irc_network_parent_class)->finalize (object);
}

static void
empathy_irc_network_init (EmpathyIrcNetwork *self)
{
  EmpathyIrcNetworkPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_IRC_NETWORK, EmpathyIrcNetworkPriv);

  self->priv = priv;

  priv->servers = NULL;

  self->user_defined = TRUE;
  self->dropped = FALSE;
}

static void
empathy_irc_network_class_init (EmpathyIrcNetworkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->get_property = empathy_irc_network_get_property;
  object_class->set_property = empathy_irc_network_set_property;

  g_type_class_add_private (object_class, sizeof (EmpathyIrcNetworkPriv));

  object_class->dispose = empathy_irc_network_dispose;
  object_class->finalize = empathy_irc_network_finalize;

  param_spec = g_param_spec_string (
      "name",
      "Network name",
      "The displayed name of this network",
      NULL,
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_NAME, param_spec);

  param_spec = g_param_spec_string (
      "charset",
      "Charset",
      "The charset to use on this network",
      "UTF-8",
      G_PARAM_CONSTRUCT |
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_CHARSET, param_spec);

  /**
   * EmpathyIrcNetwork::modified:
   * @network: the object that received the signal
   *
   * Emitted when either a property or a server of the network is modified.
   *
   */
  signals[MODIFIED] = g_signal_new (
      "modified",
      G_OBJECT_CLASS_TYPE (object_class),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);
}

/**
 * empathy_irc_network_new:
 * @name: the name of the network
 *
 * Creates a new #EmpathyIrcNetwork.
 *
 * Returns: a new #EmpathyIrcNetwork
 */
EmpathyIrcNetwork *
empathy_irc_network_new (const gchar *name)
{
  return g_object_new (EMPATHY_TYPE_IRC_NETWORK,
      "name", name,
      NULL);
}

/**
 * empathy_irc_network_get_servers:
 * @network: an #EmpathyIrcNetwork
 *
 * Get the list of #EmpathyIrcServer that belongs to this network.
 * These servers are sorted according their priority.
 * So the first one will be the first used when trying to connect to
 * the network.
 *
 * Returns: a new #GSList of refed #EmpathyIrcServer.
 */
GSList *
empathy_irc_network_get_servers (EmpathyIrcNetwork *self)
{
  EmpathyIrcNetworkPriv *priv;
  GSList *servers = NULL, *l;

  g_return_val_if_fail (EMPATHY_IS_IRC_NETWORK (self), NULL);
  priv = GET_PRIV (self);

  for (l = priv->servers; l != NULL; l = g_slist_next (l))
    {
      servers = g_slist_prepend (servers, g_object_ref (l->data));
    }

  return g_slist_reverse (servers);
}

/**
 * empathy_irc_network_append_server:
 * @network: an #EmpathyIrcNetwork
 * @server: the #EmpathyIrcServer to add
 *
 * Add an #EmpathyIrcServer to the given #EmpathyIrcNetwork. The server
 * is added at the last position in network's servers list.
 *
 */
void
empathy_irc_network_append_server (EmpathyIrcNetwork *self,
                                   EmpathyIrcServer *server)
{
  EmpathyIrcNetworkPriv *priv;

  g_return_if_fail (EMPATHY_IS_IRC_NETWORK (self));
  g_return_if_fail (server != NULL && EMPATHY_IS_IRC_SERVER (server));

  priv = GET_PRIV (self);

  g_return_if_fail (g_slist_find (priv->servers, server) == NULL);

  priv->servers = g_slist_append (priv->servers, g_object_ref (server));

  g_signal_connect (server, "modified", G_CALLBACK (server_modified_cb), self);

  g_signal_emit (self, signals[MODIFIED], 0);
}

/**
 * empathy_irc_network_remove_server:
 * @network: an #EmpathyIrcNetwork
 * @server: the #EmpathyIrcServer to remove
 *
 * Remove an #EmpathyIrcServer from the servers list of the
 * given #EmpathyIrcNetwork.
 *
 */
void
empathy_irc_network_remove_server (EmpathyIrcNetwork *self,
                                   EmpathyIrcServer *server)
{
  EmpathyIrcNetworkPriv *priv;
  GSList *l;

  g_return_if_fail (EMPATHY_IS_IRC_NETWORK (self));
  g_return_if_fail (server != NULL && EMPATHY_IS_IRC_SERVER (server));

  priv = GET_PRIV (self);

  l = g_slist_find (priv->servers, server);
  if (l == NULL)
    return;

  g_object_unref (l->data);
  priv->servers = g_slist_delete_link (priv->servers, l);
  g_signal_handlers_disconnect_by_func (server, G_CALLBACK (server_modified_cb),
      self);

  g_signal_emit (self, signals[MODIFIED], 0);
}

/**
 * empathy_irc_network_set_server_position:
 * @network: an #EmpathyIrcNetwork
 * @server: the #EmpathyIrcServer to move
 * @pos: the position to move the server. If this is negative, or is larger than
 * the number of servers in the list, the server is moved to the end of the
 * list.
 *
 * Move an #EmpathyIrcServer in the servers list of the given
 * #EmpathyIrcNetwork.
 *
 */
void
empathy_irc_network_set_server_position (EmpathyIrcNetwork *self,
                                         EmpathyIrcServer *server,
                                         gint pos)
{
  EmpathyIrcNetworkPriv *priv;
  GSList *l;

  g_return_if_fail (EMPATHY_IS_IRC_NETWORK (self));
  g_return_if_fail (server != NULL && EMPATHY_IS_IRC_SERVER (server));

  priv = GET_PRIV (self);

  l = g_slist_find (priv->servers, server);
  if (l == NULL)
    return;

  priv->servers = g_slist_delete_link (priv->servers, l);
  priv->servers = g_slist_insert (priv->servers, server, pos);

  g_signal_emit (self, signals[MODIFIED], 0);
}
