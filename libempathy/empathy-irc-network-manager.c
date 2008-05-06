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
#include <sys/types.h>
#include <sys/stat.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "empathy-utils.h"
#include "empathy-irc-network-manager.h"

#define DEBUG_FLAG EMPATHY_DEBUG_IRC
#include "empathy-debug.h"

#define IRC_NETWORKS_DTD_FILENAME "empathy-irc-networks.dtd"
#define SAVE_TIMER 4

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyIrcNetworkManager)
typedef struct {
  GHashTable *networks;

  gchar *global_file;
  gchar *user_file;
  guint last_id;

  /* Do we have to save modifications to the user file ? */
  gboolean have_to_save;
  /* Are we loading networks from XML files ? */
  gboolean loading;
  /* source id of the autosave timer */
  gint save_timer_id;
} EmpathyIrcNetworkManagerPriv;

/* properties */
enum
{
  PROP_GLOBAL_FILE = 1,
  PROP_USER_FILE,
  LAST_PROPERTY
};

G_DEFINE_TYPE (EmpathyIrcNetworkManager, empathy_irc_network_manager,
    G_TYPE_OBJECT);

static void irc_network_manager_load_servers (
    EmpathyIrcNetworkManager *manager);
static gboolean irc_network_manager_file_parse (
    EmpathyIrcNetworkManager *manager, const gchar *filename,
    gboolean user_defined);
static gboolean irc_network_manager_file_save (
    EmpathyIrcNetworkManager *manager);

static void
empathy_irc_network_manager_get_property (GObject *object,
                                          guint property_id,
                                          GValue *value,
                                          GParamSpec *pspec)
{
  EmpathyIrcNetworkManager *self = EMPATHY_IRC_NETWORK_MANAGER (object);
  EmpathyIrcNetworkManagerPriv *priv = GET_PRIV (self);

  switch (property_id)
    {
      case PROP_GLOBAL_FILE:
        g_value_set_string (value, priv->global_file);
        break;
      case PROP_USER_FILE:
        g_value_set_string (value, priv->user_file);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_irc_network_manager_set_property (GObject *object,
                                          guint property_id,
                                          const GValue *value,
                                          GParamSpec *pspec)
{
  EmpathyIrcNetworkManager *self = EMPATHY_IRC_NETWORK_MANAGER (object);
  EmpathyIrcNetworkManagerPriv *priv = GET_PRIV (self);

  switch (property_id)
    {
      case PROP_GLOBAL_FILE:
        g_free (priv->global_file);
        priv->global_file = g_value_dup_string (value);
        break;
      case PROP_USER_FILE:
        g_free (priv->user_file);
        priv->user_file = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static GObject *
empathy_irc_network_manager_constructor (GType type,
                                         guint n_props,
                                         GObjectConstructParam *props)
{
  GObject *obj;
  EmpathyIrcNetworkManager *self;

  /* Parent constructor chain */
  obj = G_OBJECT_CLASS (empathy_irc_network_manager_parent_class)->
        constructor (type, n_props, props);

  self = EMPATHY_IRC_NETWORK_MANAGER (obj);
  irc_network_manager_load_servers (self);

  return obj;
}

static void
empathy_irc_network_manager_finalize (GObject *object)
{
  EmpathyIrcNetworkManager *self = EMPATHY_IRC_NETWORK_MANAGER (object);
  EmpathyIrcNetworkManagerPriv *priv = GET_PRIV (self);

  if (priv->save_timer_id > 0)
    {
      g_source_remove (priv->save_timer_id);
    }

  if (priv->have_to_save)
    {
      irc_network_manager_file_save (self);
    }

  g_free (priv->global_file);
  g_free (priv->user_file);

  g_hash_table_destroy (priv->networks);

  G_OBJECT_CLASS (empathy_irc_network_manager_parent_class)->finalize (object);
}

static void
empathy_irc_network_manager_init (EmpathyIrcNetworkManager *self)
{
  EmpathyIrcNetworkManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EMPATHY_TYPE_IRC_NETWORK_MANAGER, EmpathyIrcNetworkManagerPriv);

  self->priv = priv;

  priv->networks = g_hash_table_new_full (g_str_hash, g_str_equal,
      (GDestroyNotify) g_free, (GDestroyNotify) g_object_unref);

  priv->last_id = 0;

  priv->have_to_save = FALSE;
  priv->loading = FALSE;
  priv->save_timer_id = 0;
}

static void
empathy_irc_network_manager_class_init (EmpathyIrcNetworkManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->constructor = empathy_irc_network_manager_constructor;
  object_class->get_property = empathy_irc_network_manager_get_property;
  object_class->set_property = empathy_irc_network_manager_set_property;

  g_type_class_add_private (object_class, sizeof (EmpathyIrcNetworkManagerPriv));

  object_class->finalize = empathy_irc_network_manager_finalize;

  param_spec = g_param_spec_string (
      "global-file",
      "path of the global networks file",
      "The path of the system-wide filename from which we have to load"
      " the networks list",
      NULL,
      G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_GLOBAL_FILE, param_spec);

  param_spec = g_param_spec_string (
      "user-file",
      "path of the user networks file",
      "The path of user's  filename from which we have to load"
      " the networks list and to which we'll save his modifications",
      NULL,
      G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_READWRITE |
      G_PARAM_STATIC_NAME |
      G_PARAM_STATIC_NICK |
      G_PARAM_STATIC_BLURB);
  g_object_class_install_property (object_class, PROP_USER_FILE, param_spec);
}

/**
 * empathy_irc_network_manager_new:
 * @global_file: the path of the global networks file, or %NULL
 * @user_file: the path of the user networks file, or %NULL
 *
 * Creates a new #EmpathyIrcNetworkManager
 *
 * Returns: a new #EmpathyIrcNetworkManager
 */
EmpathyIrcNetworkManager *
empathy_irc_network_manager_new (const gchar *global_file,
                                 const gchar *user_file)
{
  EmpathyIrcNetworkManager *manager;

  manager = g_object_new (EMPATHY_TYPE_IRC_NETWORK_MANAGER,
      "global-file", global_file,
      "user-file", user_file,
      NULL);

  return manager;
}

static gboolean
save_timeout (EmpathyIrcNetworkManager *self)
{
  EmpathyIrcNetworkManagerPriv *priv = GET_PRIV (self);

  priv->save_timer_id = 0;
  irc_network_manager_file_save (self);

  return FALSE;
}

static void
reset_save_timeout (EmpathyIrcNetworkManager *self)
{
  EmpathyIrcNetworkManagerPriv *priv = GET_PRIV (self);

  if (priv->save_timer_id > 0)
    {
      g_source_remove (priv->save_timer_id);
    }

  priv->save_timer_id = g_timeout_add_seconds (SAVE_TIMER,
      (GSourceFunc) save_timeout, self);
}

static void
network_modified (EmpathyIrcNetwork *network,
                  EmpathyIrcNetworkManager *self)
{
  EmpathyIrcNetworkManagerPriv *priv = GET_PRIV (self);

  network->user_defined = TRUE;

  if (!priv->loading)
    {
      priv->have_to_save = TRUE;
      reset_save_timeout (self);
    }
}

static void
add_network (EmpathyIrcNetworkManager *self,
             EmpathyIrcNetwork *network,
             const gchar *id)
{
  EmpathyIrcNetworkManagerPriv *priv = GET_PRIV (self);

  g_hash_table_insert (priv->networks, g_strdup (id), g_object_ref (network));

  g_signal_connect (network, "modified", G_CALLBACK (network_modified), self);
}

/**
 * empathy_irc_network_manager_add:
 * @manager: an #EmpathyIrcNetworkManager
 * @network: the #EmpathyIrcNetwork to add
 *
 * Add an #EmpathyIrcNetwork to the given #EmpathyIrcNetworkManager.
 *
 */
void
empathy_irc_network_manager_add (EmpathyIrcNetworkManager *self,
                                 EmpathyIrcNetwork *network)
{
  EmpathyIrcNetworkManagerPriv *priv;
  gchar *id = NULL;

  g_return_if_fail (EMPATHY_IS_IRC_NETWORK_MANAGER (self));
  g_return_if_fail (EMPATHY_IS_IRC_NETWORK (network));

  priv = GET_PRIV (self);

  /* generate an id for this network */
  do
    {
      g_free (id);
      id = g_strdup_printf ("id%u", ++priv->last_id);
    } while (g_hash_table_lookup (priv->networks, id) != NULL &&
        priv->last_id < G_MAXUINT);

  if (priv->last_id == G_MAXUINT)
    {
      DEBUG ("Can't add network: too many networks using a similiar ID");
      return;
    }

  DEBUG ("add server with \"%s\" as ID", id);

  network->user_defined = TRUE;
  add_network (self, network, id);

  priv->have_to_save = TRUE;
  reset_save_timeout (self);

  g_free (id);
}

/**
 * empathy_irc_network_manager_remove:
 * @manager: an #EmpathyIrcNetworkManager
 * @network: the #EmpathyIrcNetwork to remove
 *
 * Remove an #EmpathyIrcNetwork from the given #EmpathyIrcNetworkManager.
 *
 */
void
empathy_irc_network_manager_remove (EmpathyIrcNetworkManager *self,
                                    EmpathyIrcNetwork *network)
{
  EmpathyIrcNetworkManagerPriv *priv;

  g_return_if_fail (EMPATHY_IS_IRC_NETWORK_MANAGER (self));
  g_return_if_fail (EMPATHY_IS_IRC_NETWORK (network));

  priv = GET_PRIV (self);

  network->user_defined = TRUE;
  network->dropped = TRUE;

  priv->have_to_save = TRUE;
  reset_save_timeout (self);
}

static void
append_network_to_list (const gchar *id,
                        EmpathyIrcNetwork *network,
                        GSList **list)
{
  if (network->dropped)
    return;

  *list = g_slist_prepend (*list, g_object_ref (network));
}

/**
 * empathy_irc_network_manager_get_networks:
 * @manager: an #EmpathyIrcNetworkManager
 *
 * Get the list of #EmpathyIrcNetwork associated with the given
 * manager.
 *
 * Returns: a new #GSList of refed #EmpathyIrcNetwork
 */
GSList *
empathy_irc_network_manager_get_networks (EmpathyIrcNetworkManager *self)
{
  EmpathyIrcNetworkManagerPriv *priv;
  GSList *irc_networks = NULL;

  g_return_val_if_fail (EMPATHY_IS_IRC_NETWORK_MANAGER (self), NULL);

  priv = GET_PRIV (self);

  g_hash_table_foreach (priv->networks, (GHFunc) append_network_to_list,
      &irc_networks);

  return irc_networks;
}

/*
 * API to save/load and parse the irc_networks file.
 */

static void
load_global_file (EmpathyIrcNetworkManager *self)
{
  EmpathyIrcNetworkManagerPriv *priv = GET_PRIV (self);

  if (priv->global_file == NULL)
    return;

  if (!g_file_test (priv->global_file, G_FILE_TEST_EXISTS))
    {
      DEBUG ("Global networks file %s doesn't exist", priv->global_file);
      return;
    }

  irc_network_manager_file_parse (self, priv->global_file, FALSE);
}

static void
load_user_file (EmpathyIrcNetworkManager *self)
{
  EmpathyIrcNetworkManagerPriv *priv = GET_PRIV (self);

  if (priv->user_file == NULL)
    return;

  if (!g_file_test (priv->user_file, G_FILE_TEST_EXISTS))
    {
      DEBUG ("User networks file %s doesn't exist", priv->global_file);
      return;
    }

  irc_network_manager_file_parse (self, priv->user_file, TRUE);
}

static void
irc_network_manager_load_servers (EmpathyIrcNetworkManager *self)
{
  EmpathyIrcNetworkManagerPriv *priv = GET_PRIV (self);

  priv->loading = TRUE;

  load_global_file (self);
  load_user_file (self);

  priv->loading = FALSE;
  priv->have_to_save = FALSE;
}

static void
irc_network_manager_parse_irc_server (EmpathyIrcNetwork *network,
                                      xmlNodePtr node)
{
  xmlNodePtr server_node;

  for (server_node = node->children; server_node;
      server_node = server_node->next)
    {
      gchar *address = NULL, *port = NULL, *ssl = NULL;

      if (strcmp (server_node->name, "server") != 0)
        continue;

      address = xmlGetProp (server_node, "address");
      port = xmlGetProp (server_node, "port");
      ssl = xmlGetProp (server_node, "ssl");

      if (address != NULL)
        {
          gint port_nb = 0;
          gboolean have_ssl = FALSE;
          EmpathyIrcServer *server;

          if (port != NULL)
            port_nb = strtol (port, NULL, 10);

          if (port_nb <= 0 || port_nb > G_MAXUINT16)
            port_nb = 6667;

          if (ssl == NULL || strcmp (ssl, "TRUE") == 0)
            have_ssl = TRUE;

          DEBUG ("parsed server %s port %d ssl %d", address, port_nb, have_ssl);

          server = empathy_irc_server_new (address, port_nb, have_ssl);
          empathy_irc_network_append_server (network, server);
        }

      if (address)
        xmlFree (address);
      if (port)
        xmlFree (port);
      if (ssl)
        xmlFree (ssl);
    }
}

static void
irc_network_manager_parse_irc_network (EmpathyIrcNetworkManager *self,
                                       xmlNodePtr node,
                                       gboolean user_defined)
{
  EmpathyIrcNetworkManagerPriv *priv = GET_PRIV (self);
  EmpathyIrcNetwork  *network;
  xmlNodePtr child;
  gchar *str;
  gchar *id, *name;

  id = xmlGetProp (node, "id");
  if (xmlHasProp (node, "dropped"))
    {
      if (!user_defined)
        {
          DEBUG ("the 'dropped' attribute shouldn't be used in the global file");
        }

      network = g_hash_table_lookup (priv->networks, id);
      if (network != NULL)
        {
          network->dropped = TRUE;
          network->user_defined = TRUE;
        }
       xmlFree (id);
      return;
    }

  if (!xmlHasProp (node, "name"))
    return;

  name = xmlGetProp (node, "name");
  network = empathy_irc_network_new (name);

  if (xmlHasProp (node, "network_charset"))
    {
      gchar *charset;
      charset = xmlGetProp (node, "network_charset");
      g_object_set (network, "charset", charset, NULL);
      xmlFree (charset);
    }

  add_network (self, network, id);
  DEBUG ("add network %s (id %s)", name, id);

  for (child = node->children; child; child = child->next)
    {
      gchar *tag;

      tag = (gchar *) child->name;
      str = (gchar *) xmlNodeGetContent (child);

      if (!str)
        continue;

      if (strcmp (tag, "servers") == 0)
        {
          irc_network_manager_parse_irc_server (network, child);
        }

      xmlFree (str);
    }

  network->user_defined = user_defined;
  g_object_unref (network);
  xmlFree (name);
  xmlFree (id);
}

static gboolean
irc_network_manager_file_parse (EmpathyIrcNetworkManager *self,
                                const gchar *filename,
                                gboolean user_defined)
{
  EmpathyIrcNetworkManagerPriv *priv;
  xmlParserCtxtPtr ctxt;
  xmlDocPtr doc;
  xmlNodePtr networks;
  xmlNodePtr node;

  priv = GET_PRIV (self);

  DEBUG ("Attempting to parse file:'%s'...", filename);

  ctxt = xmlNewParserCtxt ();

  /* Parse and validate the file. */
  doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);
  if (!doc)
    {
      g_warning ("Failed to parse file:'%s'", filename);
      xmlFreeParserCtxt (ctxt);
      return FALSE;
    }

  if (!empathy_xml_validate (doc, IRC_NETWORKS_DTD_FILENAME)) {
    g_warning ("Failed to validate file:'%s'", filename);
    xmlFreeDoc (doc);
    xmlFreeParserCtxt (ctxt);
    return FALSE;
  }

  /* The root node, networks. */
  networks = xmlDocGetRootElement (doc);

  for (node = networks->children; node; node = node->next)
    {
      irc_network_manager_parse_irc_network (self, node, user_defined);
    }

  xmlFreeDoc(doc);
  xmlFreeParserCtxt (ctxt);

  return TRUE;
}

static void
write_network_to_xml (const gchar *id,
                      EmpathyIrcNetwork *network,
                      xmlNodePtr root)
{
  xmlNodePtr network_node, servers_node;
  GSList *servers, *l;
  gchar *name, *charset;

  if (!network->user_defined)
    /* no need to write this network to the XML */
    return;

  network_node = xmlNewChild (root, NULL, "network", NULL);
  xmlNewProp (network_node, "id", id);

  if (network->dropped)
    {
      xmlNewProp (network_node, "dropped", "1");
      return;
    }

  g_object_get (network,
      "name", &name,
      "charset", &charset,
      NULL);
  xmlNewProp (network_node, "name", name);
  xmlNewProp (network_node, "network_charset", charset);
  g_free (name);
  g_free (charset);

  servers = empathy_irc_network_get_servers (network);

  servers_node = xmlNewChild (network_node, NULL, "servers", NULL);
  for (l = servers; l != NULL; l = g_slist_next (l))
    {
      EmpathyIrcServer *server;
      xmlNodePtr server_node;
      gchar *address, *tmp;
      guint port;
      gboolean ssl;

      server = l->data;

      server_node = xmlNewChild (servers_node, NULL, "server", NULL);

      g_object_get (server,
          "address", &address,
          "port", &port,
          "ssl", &ssl,
          NULL);

      xmlNewProp (server_node, "address", address);

      tmp = g_strdup_printf ("%u", port);
      xmlNewProp (server_node, "port", tmp);
      g_free (tmp);

      xmlNewProp (server_node, "ssl", ssl ? "TRUE": "FALSE");

      g_free (address);
    }

  /* free the list */
  g_slist_foreach (servers, (GFunc) g_object_unref, NULL);
  g_slist_free (servers);
}

static gboolean
irc_network_manager_file_save (EmpathyIrcNetworkManager *self)
{
  EmpathyIrcNetworkManagerPriv *priv = GET_PRIV (self);
  xmlDocPtr doc;
  xmlNodePtr root;

  if (priv->user_file == NULL)
    {
      DEBUG ("can't save: no user file defined");
      return FALSE;
    }

  DEBUG ("Saving IRC networks");

  doc = xmlNewDoc ("1.0");
  root = xmlNewNode (NULL, "networks");
  xmlDocSetRootElement (doc, root);

  g_hash_table_foreach (priv->networks, (GHFunc) write_network_to_xml, root);

  /* Make sure the XML is indented properly */
  xmlIndentTreeOutput = 1;

  xmlSaveFormatFileEnc (priv->user_file, doc, "utf-8", 1);
  xmlFreeDoc (doc);

  xmlCleanupParser ();
  xmlMemoryDump ();

  priv->have_to_save = FALSE;

  return TRUE;
}

static gboolean
find_network_by_address (const gchar *id,
                         EmpathyIrcNetwork *network,
                         const gchar *address)
{
  GSList *servers, *l;
  gboolean found = FALSE;

  if (network->dropped)
    return FALSE;

  servers = empathy_irc_network_get_servers (network);

  for (l = servers; l != NULL && !found; l = g_slist_next (l))
    {
      EmpathyIrcServer *server = l->data;
      gchar *_address;

      g_object_get (server, "address", &_address, NULL);
      found = (_address != NULL && strcmp (address, _address) == 0);

      g_free (_address);
    }

  g_slist_foreach (servers, (GFunc) g_object_unref, NULL);
  g_slist_free (servers);

  return found;
}

/**
 * empathy_irc_network_manager_find_network_by_address:
 * @manager: an #EmpathyIrcNetworkManager
 * @address: the server address to look for
 *
 * Find the #EmpathyIrcNetwork which owns an #EmpathyIrcServer
 * that has the given address.
 *
 * Returns: the found #EmpathyIrcNetwork, or %NULL if not found.
 */
EmpathyIrcNetwork *
empathy_irc_network_manager_find_network_by_address (
    EmpathyIrcNetworkManager *self,
    const gchar *address)
{
  EmpathyIrcNetworkManagerPriv *priv = GET_PRIV (self);
  EmpathyIrcNetwork *network;

  g_return_val_if_fail (address != NULL, NULL);

  network = g_hash_table_find (priv->networks,
      (GHRFunc) find_network_by_address, (gchar *) address);

  return network;
}
