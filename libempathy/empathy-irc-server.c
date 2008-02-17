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

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <telepathy-glib/util.h>

#include "empathy-marshal.h"
#include "empathy-irc-server.h"

G_DEFINE_TYPE (EmpathyIrcServer, empathy_irc_server, G_TYPE_OBJECT);

/* properties */
enum
{
  PROP_ADDRESS = 1,
  PROP_PORT,
  PROP_SSL,
  LAST_PROPERTY
};

/* signals */
enum
{
  MODIFIED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

typedef struct _EmpathyIrcServerPrivate EmpathyIrcServerPrivate;

struct _EmpathyIrcServerPrivate
{
  gchar *address;
  gint port;
  gboolean ssl;
};

#define EMPATHY_IRC_SERVER_GET_PRIVATE(obj)\
    ((EmpathyIrcServerPrivate *) obj->priv)

static void
empathy_irc_server_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
  EmpathyIrcServer *self = EMPATHY_IRC_SERVER (object);
  EmpathyIrcServerPrivate *priv = EMPATHY_IRC_SERVER_GET_PRIVATE (self);

  switch (property_id)
    {
      case PROP_ADDRESS:
        g_value_set_string (value, priv->address);
        break;
      case PROP_PORT:
        g_value_set_uint (value, priv->port);
        break;
      case PROP_SSL:
        g_value_set_boolean (value, priv->ssl);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_irc_server_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
  EmpathyIrcServer *self = EMPATHY_IRC_SERVER (object);
  EmpathyIrcServerPrivate *priv = EMPATHY_IRC_SERVER_GET_PRIVATE (self);

  switch (property_id)
    {
      case PROP_ADDRESS:
        if (tp_strdiff (priv->address, g_value_get_string (value)))
          {
            g_free (priv->address);
            priv->address = g_value_dup_string (value);
            g_signal_emit (object, signals[MODIFIED], 0);
          }
        break;
      case PROP_PORT:
        if (priv->port != g_value_get_uint (value))
          {
            priv->port = g_value_get_uint (value);
            g_signal_emit (object, signals[MODIFIED], 0);
          }
        break;
      case PROP_SSL:
        if (priv->ssl != g_value_get_boolean (value))
          {
            priv->ssl = g_value_get_boolean (value);
            g_signal_emit (object, signals[MODIFIED], 0);
          }
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_irc_server_finalize (GObject *object)
{
  EmpathyIrcServer *self = EMPATHY_IRC_SERVER (object);
  EmpathyIrcServerPrivate *priv = EMPATHY_IRC_SERVER_GET_PRIVATE (self);

  g_free (priv->address);

  G_OBJECT_CLASS (empathy_irc_server_parent_class)->finalize (object);
}

static void
empathy_irc_server_init (EmpathyIrcServer *self)
{
  EmpathyIrcServerPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_IRC_SERVER, EmpathyIrcServerPrivate);

  self->priv = priv;
}

static void
empathy_irc_server_class_init (EmpathyIrcServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->get_property = empathy_irc_server_get_property;
  object_class->set_property = empathy_irc_server_set_property;

  g_type_class_add_private (object_class,
          sizeof (EmpathyIrcServerPrivate));

  object_class->finalize = empathy_irc_server_finalize;

  param_spec = g_param_spec_string (
      "address",
      "Server address",
      "The address of this server",
      NULL,
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_ADDRESS, param_spec);

  param_spec = g_param_spec_uint (
      "port",
      "Server port",
      "The port to use to connect on this server",
      1, G_MAXUINT16, 6667,
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_PORT, param_spec);

  param_spec = g_param_spec_boolean (
      "ssl",
      "SSL",
      "If this server needs SSL connection",
      FALSE,
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_SSL, param_spec);

  /**
   * EmpathyIrcServer::modified:
   * @server: the object that received the signal
   *
   * Emitted when a property of the server is modified.
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
 * empathy_irc_server_new:
 * @address: the address
 * @port: the port
 * @ssl: %TRUE if the server needs a SSL connection
 *
 * Creates a new #EmpathyIrcServer
 *
 * Returns: a new #EmpathyIrcServer
 */
EmpathyIrcServer *
empathy_irc_server_new (const gchar *address,
                        guint port,
                        gboolean ssl)
{
  return g_object_new (EMPATHY_TYPE_IRC_SERVER,
      "address", address,
      "port", port,
      "ssl", ssl,
      NULL);
}
