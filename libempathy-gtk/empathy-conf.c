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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Richard Hult <richard@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <gconf/gconf-client.h>

#include <libempathy/empathy-utils.h>
#include "empathy-conf.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

#define EMPATHY_CONF_ROOT       "/apps/empathy"
#define DESKTOP_INTERFACE_ROOT  "/desktop/gnome/interface"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyConf)
typedef struct {
	GConfClient *gconf_client;
} EmpathyConfPriv;

typedef struct {
	EmpathyConf           *conf;
	EmpathyConfNotifyFunc  func;
	gpointer               user_data;
} EmpathyConfNotifyData;

static void conf_finalize (GObject *object);

G_DEFINE_TYPE (EmpathyConf, empathy_conf, G_TYPE_OBJECT);

static EmpathyConf *global_conf = NULL;

static void
empathy_conf_class_init (EmpathyConfClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = conf_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyConfPriv));
}

static void
empathy_conf_init (EmpathyConf *conf)
{
	EmpathyConfPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (conf,
		EMPATHY_TYPE_CONF, EmpathyConfPriv);

	conf->priv = priv;
	priv->gconf_client = gconf_client_get_default ();

	gconf_client_add_dir (priv->gconf_client,
			      EMPATHY_CONF_ROOT,
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
	EmpathyConfPriv *priv;

	priv = GET_PRIV (object);

	gconf_client_remove_dir (priv->gconf_client,
				 EMPATHY_CONF_ROOT,
				 NULL);
	gconf_client_remove_dir (priv->gconf_client,
				 DESKTOP_INTERFACE_ROOT,
				 NULL);

	g_object_unref (priv->gconf_client);

	G_OBJECT_CLASS (empathy_conf_parent_class)->finalize (object);
}

EmpathyConf *
empathy_conf_get (void)
{
	if (!global_conf) {
		global_conf = g_object_new (EMPATHY_TYPE_CONF, NULL);
	}

	return global_conf;
}

void
empathy_conf_shutdown (void)
{
	if (global_conf) {
		g_object_unref (global_conf);
		global_conf = NULL;
	}
}

gboolean
empathy_conf_set_int (EmpathyConf  *conf,
		     const gchar *key,
		     gint         value)
{
	EmpathyConfPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONF (conf), FALSE);

	DEBUG ("Setting int:'%s' to %d", key, value);

	priv = GET_PRIV (conf);

	return gconf_client_set_int (priv->gconf_client,
				     key,
				     value,
				     NULL);
}

gboolean
empathy_conf_get_int (EmpathyConf  *conf,
		     const gchar *key,
		     gint        *value)
{
	EmpathyConfPriv *priv;
	GError          *error = NULL;

	*value = 0;

	g_return_val_if_fail (EMPATHY_IS_CONF (conf), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	priv = GET_PRIV (conf);

	*value = gconf_client_get_int (priv->gconf_client,
				       key,
				       &error);

	if (error) {
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

gboolean
empathy_conf_set_bool (EmpathyConf  *conf,
		      const gchar *key,
		      gboolean     value)
{
	EmpathyConfPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONF (conf), FALSE);

	DEBUG ("Setting bool:'%s' to %d ---> %s", key, value,
		value ? "true" : "false");

	priv = GET_PRIV (conf);

	return gconf_client_set_bool (priv->gconf_client,
				      key,
				      value,
				      NULL);
}

gboolean
empathy_conf_get_bool (EmpathyConf  *conf,
		      const gchar *key,
		      gboolean    *value)
{
	EmpathyConfPriv *priv;
	GError          *error = NULL;

	*value = FALSE;

	g_return_val_if_fail (EMPATHY_IS_CONF (conf), FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	priv = GET_PRIV (conf);

	*value = gconf_client_get_bool (priv->gconf_client,
					key,
					&error);

	if (error) {
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

gboolean
empathy_conf_set_string (EmpathyConf  *conf,
			const gchar *key,
			const gchar *value)
{
	EmpathyConfPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONF (conf), FALSE);

	DEBUG ("Setting string:'%s' to '%s'", key, value);

	priv = GET_PRIV (conf);

	return gconf_client_set_string (priv->gconf_client,
					key,
					value,
					NULL);
}

gboolean
empathy_conf_get_string (EmpathyConf   *conf,
			const gchar  *key,
			gchar       **value)
{
	EmpathyConfPriv *priv;
	GError          *error = NULL;

	*value = NULL;

	g_return_val_if_fail (EMPATHY_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	*value = gconf_client_get_string (priv->gconf_client,
					  key,
					  &error);

	if (error) {
		g_error_free (error);
		return FALSE;
	}

	return TRUE;
}

gboolean
empathy_conf_set_string_list (EmpathyConf  *conf,
			     const gchar *key,
			     GSList      *value)
{
	EmpathyConfPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	return gconf_client_set_list (priv->gconf_client,
				      key,
				      GCONF_VALUE_STRING,
				      value,
				      NULL);
}

gboolean
empathy_conf_get_string_list (EmpathyConf   *conf,
			     const gchar  *key,
			     GSList      **value)
{
	EmpathyConfPriv *priv;
	GError          *error = NULL;

	*value = NULL;

	g_return_val_if_fail (EMPATHY_IS_CONF (conf), FALSE);

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
conf_notify_data_free (EmpathyConfNotifyData *data)
{
	g_object_unref (data->conf);
	g_slice_free (EmpathyConfNotifyData, data);
}

static void
conf_notify_func (GConfClient *client,
		  guint        id,
		  GConfEntry  *entry,
		  gpointer     user_data)
{
	EmpathyConfNotifyData *data;

	data = user_data;

	data->func (data->conf,
		    gconf_entry_get_key (entry),
		    data->user_data);
}

guint
empathy_conf_notify_add (EmpathyConf           *conf,
			const gchar          *key,
			EmpathyConfNotifyFunc func,
			gpointer              user_data)
{
	EmpathyConfPriv       *priv;
	guint                  id;
	EmpathyConfNotifyData *data;

	g_return_val_if_fail (EMPATHY_IS_CONF (conf), 0);

	priv = GET_PRIV (conf);

	data = g_slice_new (EmpathyConfNotifyData);
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
empathy_conf_notify_remove (EmpathyConf *conf,
			   guint       id)
{
	EmpathyConfPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_CONF (conf), FALSE);

	priv = GET_PRIV (conf);

	gconf_client_notify_remove (priv->gconf_client, id);

	return TRUE;
}

