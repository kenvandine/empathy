/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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

#include "config.h"

#include <string.h>

#include <gconf/gconf-client.h>

#include "gossip-conf.h"
#include "gossip-debug.h"

#define DEBUG_DOMAIN "Config"

#define GOSSIP_CONF_ROOT       "/apps/empathy"
#define DESKTOP_INTERFACE_ROOT "/desktop/gnome/interface"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_CONF, GossipConfPriv))

typedef struct {
	GConfClient *gconf_client;
} GossipConfPriv;

typedef struct {
	GossipConf           *conf;
	GossipConfNotifyFunc  func;
	gpointer               user_data;
} GossipConfNotifyData;

static void conf_finalize (GObject *object);

G_DEFINE_TYPE (GossipConf, gossip_conf, G_TYPE_OBJECT);

static GossipConf *global_conf = NULL;

static void
gossip_conf_class_init (GossipConfClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = conf_finalize;

	g_type_class_add_private (object_class, sizeof (GossipConfPriv));
}

static void
gossip_conf_init (GossipConf *conf)
{
	GossipConfPriv *priv;

	priv = GET_PRIV (conf);

	priv->gconf_client = gconf_client_get_default ();

	gconf_client_add_dir (priv->gconf_client,
			      GOSSIP_CONF_ROOT,
			      GCONF_CLIENT_PRELOAD_ONELEVEL,
			      NULL);
	gconf_client_add_dir (priv->gconf_client,
			      DESKTOP_INTERFACE_ROOT,
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);
}

static void
conf_finalize (GObject *object)
{
	GossipConfPriv *priv;

	priv = GET_PRIV (object);

	gconf_client_remove_dir (priv->gconf_client,
				 GOSSIP_CONF_ROOT,
				 NULL);
	gconf_client_remove_dir (priv->gconf_client,
				 DESKTOP_INTERFACE_ROOT,
				 NULL);

	g_object_unref (priv->gconf_client);

	G_OBJECT_CLASS (gossip_conf_parent_class)->finalize (object);
}

GossipConf *
gossip_conf_get (void)
{
	if (!global_conf) {
		global_conf = g_object_new (GOSSIP_TYPE_CONF, NULL);
	}

	return global_conf;
}

void
gossip_conf_shutdown (void)
{
	if (global_conf) {
		g_object_unref (global_conf);
		global_conf = NULL;
	}
}

gboolean
gossip_conf_set_int (GossipConf  *conf,
		     const gchar *key,
		     gint         value)
{
	GossipConfPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	gossip_debug (DEBUG_DOMAIN, "Setting int:'%s' to %d", key, value);

	priv = GET_PRIV (conf);

	return gconf_client_set_int (priv->gconf_client,
				     key,
				     value,
				     NULL);
}

gboolean
gossip_conf_get_int (GossipConf  *conf,
		     const gchar *key,
		     gint        *value)
{
	GossipConfPriv *priv;
	GError          *error = NULL;

	*value = 0;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	priv = GET_PRIV (conf);

	*value = gconf_client_get_int (priv->gconf_client,
				       key,
				       &error);

	gossip_debug (DEBUG_DOMAIN, "Getting int:'%s' (=%d), error:'%s'",
		      key, *value, error ? error->message : "None");

	if (error) {
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

gboolean
gossip_conf_set_bool (GossipConf  *conf,
		      const gchar *key,
		      gboolean     value)
{
	GossipConfPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	gossip_debug (DEBUG_DOMAIN, "Setting bool:'%s' to %d ---> %s",
		      key, value, value ? "true" : "false");

	priv = GET_PRIV (conf);

	return gconf_client_set_bool (priv->gconf_client,
				      key,
				      value,
				      NULL);
}

gboolean
gossip_conf_get_bool (GossipConf  *conf,
		      const gchar *key,
		      gboolean    *value)
{
	GossipConfPriv *priv;
	GError          *error = NULL;

	*value = FALSE;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	priv = GET_PRIV (conf);

	*value = gconf_client_get_bool (priv->gconf_client,
					key,
					&error);

	gossip_debug (DEBUG_DOMAIN, "Getting bool:'%s' (=%d ---> %s), error:'%s'",
		      key, *value, *value ? "true" : "false",
		      error ? error->message : "None");

	if (error) {
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

gboolean
gossip_conf_set_string (GossipConf  *conf,
			const gchar *key,
			const gchar *value)
{
	GossipConfPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	gossip_debug (DEBUG_DOMAIN, "Setting string:'%s' to '%s'",
		      key, value);

	priv = GET_PRIV (conf);

	return gconf_client_set_string (priv->gconf_client,
					key,
					value,
					NULL);
}

gboolean
gossip_conf_get_string (GossipConf   *conf,
			const gchar  *key,
			gchar       **value)
{
	GossipConfPriv *priv;
	GError          *error = NULL;

	*value = NULL;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	*value = gconf_client_get_string (priv->gconf_client,
					  key,
					  &error);

	gossip_debug (DEBUG_DOMAIN, "Getting string:'%s' (='%s'), error:'%s'",
		      key, *value, error ? error->message : "None");

	if (error) {
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

gboolean
gossip_conf_set_string_list (GossipConf  *conf,
			     const gchar *key,
			     GSList      *value)
{
	GossipConfPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	return gconf_client_set_list (priv->gconf_client,
				      key,
				      GCONF_VALUE_STRING,
				      value,
				      NULL);
}

gboolean
gossip_conf_get_string_list (GossipConf   *conf,
			     const gchar  *key,
			     GSList      **value)
{
	GossipConfPriv *priv;
	GError          *error = NULL;

	*value = NULL;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	*value = gconf_client_get_list (priv->gconf_client,
					key,
					GCONF_VALUE_STRING,
					&error);
	if (error) {
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

static void
conf_notify_data_free (GossipConfNotifyData *data)
{
	g_object_unref (data->conf);
	g_slice_free (GossipConfNotifyData, data);
}

static void
conf_notify_func (GConfClient *client,
		  guint        id,
		  GConfEntry  *entry,
		  gpointer     user_data)
{
	GossipConfNotifyData *data;

	data = user_data;

	data->func (data->conf,
		    gconf_entry_get_key (entry),
		    data->user_data);
}

guint
gossip_conf_notify_add (GossipConf           *conf,
			const gchar          *key,
			GossipConfNotifyFunc func,
			gpointer              user_data)
{
	GossipConfPriv       *priv;
	guint                  id;
	GossipConfNotifyData *data;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), 0);

	priv = GET_PRIV (conf);

	data = g_slice_new (GossipConfNotifyData);
	data->func = func;
	data->user_data = user_data;
	data->conf = g_object_ref (conf);

	id = gconf_client_notify_add (priv->gconf_client,
				      key,
				      conf_notify_func,
				      data,
				      (GFreeFunc) conf_notify_data_free,
				      NULL);

	return id;
}

gboolean
gossip_conf_notify_remove (GossipConf *conf,
			   guint       id)
{
	GossipConfPriv *priv;

	g_return_val_if_fail (GOSSIP_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	gconf_client_notify_remove (priv->gconf_client, id);

	return TRUE;
}

