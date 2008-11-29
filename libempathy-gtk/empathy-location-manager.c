/*
 * Copyright (C) 2008 Collabora Ltd.
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
 * Authors: Pierre-Luc Beaudoin <pierre-luc@pierlux.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n.h>

#include <telepathy-glib/util.h>

#if HAVE_GEOCLUE
#include <geoclue/geoclue-master.h>
#endif

#include "empathy-location-manager.h"
#include "empathy-conf.h"

#include "libempathy/empathy-enum-types.h"
#include "libempathy/empathy-location.h"
#include "libempathy/empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_LOCATION
#include "libempathy/empathy-debug.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyLocationManager)
typedef struct {
    gboolean is_setup;
    EmpathyContact *contact;
#if HAVE_GEOCLUE
    GeoclueResourceFlags resources;
    GeoclueMasterClient *gc_client;
    GeocluePosition *gc_position;
    GeoclueAddress *gc_address;
#endif
} EmpathyLocationManagerPriv;

static void location_manager_finalize (GObject *object);
static void location_manager_get_property (GObject *object, guint param_id,
    GValue *value, GParamSpec *pspec);
static void location_manager_set_property (GObject *object, guint param_id,
    const GValue *value, GParamSpec *pspec);
#if HAVE_GEOCLUE
static void position_changed_cb (GeocluePosition *position,
    GeocluePositionFields fields, int timestamp, double latitude,
    double longitude, double altitude, GeoclueAccuracy *accuracy,
    gpointer user_data);
static void address_changed_cb (GeoclueAddress *address, int timestamp,
    GHashTable *details, GeoclueAccuracy *accuracy, gpointer user_data);
static void setup_geoclue (EmpathyLocationManager *location_manager);
static void publish_cb (EmpathyConf  *conf, const gchar *key,
    gpointer user_data);
static void update_resources (EmpathyLocationManager *location_manager);
static void resource_cb (EmpathyConf  *conf, const gchar *key,
    gpointer user_data);
#endif

G_DEFINE_TYPE (EmpathyLocationManager, empathy_location_manager, G_TYPE_OBJECT);

enum
{
  PROP_0,
};

static void
empathy_location_manager_class_init (EmpathyLocationManagerClass *class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (class);

  object_class->finalize = location_manager_finalize;
  object_class->get_property = location_manager_get_property;
  object_class->set_property = location_manager_set_property;

  g_type_class_add_private (object_class, sizeof (EmpathyLocationManagerPriv));
}


static void
empathy_location_manager_init (EmpathyLocationManager *location_manager)
{
  EmpathyConf               *conf;
  EmpathyLocationManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (location_manager,
    EMPATHY_TYPE_LOCATION_MANAGER, EmpathyLocationManagerPriv);

  location_manager->priv = priv;
  priv->is_setup = FALSE;

  conf = empathy_conf_get ();
  empathy_conf_notify_add (conf, EMPATHY_PREFS_LOCATION_PUBLISH, publish_cb,
      location_manager);
  empathy_conf_notify_add (conf, EMPATHY_PREFS_LOCATION_RESOURCE_NETWORK,
      resource_cb, location_manager);
  empathy_conf_notify_add (conf, EMPATHY_PREFS_LOCATION_RESOURCE_CELL,
      resource_cb, location_manager);
  empathy_conf_notify_add (conf, EMPATHY_PREFS_LOCATION_RESOURCE_GPS,
      resource_cb, location_manager);

  publish_cb (conf, EMPATHY_PREFS_LOCATION_PUBLISH, location_manager);
  resource_cb (conf, EMPATHY_PREFS_LOCATION_RESOURCE_NETWORK, location_manager);
  resource_cb (conf, EMPATHY_PREFS_LOCATION_RESOURCE_CELL, location_manager);
  resource_cb (conf, EMPATHY_PREFS_LOCATION_RESOURCE_GPS, location_manager);

}


static void
location_manager_finalize (GObject *object)
{
  EmpathyLocationManagerPriv *priv;

  priv = GET_PRIV (object);

  DEBUG ("finalize: %p", object);

  G_OBJECT_CLASS (empathy_location_manager_parent_class)->finalize (object);
}


static void
location_manager_get_property (GObject *object,
                      guint param_id,
                      GValue *value,
                      GParamSpec *pspec)
{
  EmpathyLocationManagerPriv *priv;

  priv = GET_PRIV (object);

  switch (param_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}


static void
location_manager_set_property (GObject *object,
                      guint param_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
  EmpathyLocationManagerPriv *priv;

  priv = GET_PRIV (object);

  switch (param_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}


EmpathyLocationManager *
empathy_location_manager_get_default (void)
{
  static EmpathyLocationManager *singleton = NULL;
  if (singleton == NULL)
    singleton = g_object_new (EMPATHY_TYPE_LOCATION_MANAGER, NULL);
  return singleton;
}


#if HAVE_GEOCLUE
static void
position_changed_cb (GeocluePosition *position,
                     GeocluePositionFields fields,
                     int timestamp,
                     double latitude,
                     double longitude,
                     double altitude,
                     GeoclueAccuracy *accuracy,
                     gpointer user_data)
{
  GeoclueAccuracyLevel level;

  geoclue_accuracy_get_details (accuracy, &level, NULL, NULL);
  g_print ("New position (accuracy level %d):\n", level);

  if (fields & GEOCLUE_POSITION_FIELDS_LATITUDE &&
      fields & GEOCLUE_POSITION_FIELDS_LONGITUDE) {
    g_print ("\t%f, %f\n\n", latitude, longitude);
    //empathy_location_set_latitude (location, latitude);
    //empathy_location_set_longitude (location, longitude);

  } else {
    g_print ("\nlatitude and longitude not valid.\n");
  }
}


static void
address_changed_cb (GeoclueAddress *address,
                    int timestamp,
                    GHashTable *details,
                    GeoclueAccuracy *accuracy,
                    gpointer user_data)
{
  GeoclueAccuracyLevel level;
  geoclue_accuracy_get_details (accuracy, &level, NULL, NULL);
  g_print ("New address (accuracy level %d):\n", level);
  //g_hash_table_foreach (details, (GHFunc)set_location_from_address, location);
  g_print ("\n");
}


static void
update_resources (EmpathyLocationManager *location_manager)
{
  EmpathyLocationManagerPriv *priv;

  priv = GET_PRIV (location_manager);

  DEBUG ("Updating resources");

  if (!geoclue_master_client_set_requirements (priv->gc_client,
          GEOCLUE_ACCURACY_LEVEL_COUNTRY, 0, TRUE, priv->resources,
          NULL))
    g_printerr ("set_requirements failed");
}


static void
setup_geoclue (EmpathyLocationManager *location_manager)
{
  EmpathyLocationManagerPriv *priv;

  priv = GET_PRIV (location_manager);

  GeoclueMaster *master;
  GError *error = NULL;

  DEBUG ("Setting up Geoclue");
  master = geoclue_master_get_default ();
  priv->gc_client = geoclue_master_create_client (master, NULL, NULL);
  g_object_unref (master);

  update_resources (location_manager);

  /* Get updated when the position is changes */
  priv->gc_position = geoclue_master_client_create_position (
      priv->gc_client, &error);
  if (priv->gc_position == NULL)
    {
      g_printerr ("Failed to create GeocluePosition: %s", error->message);
      return;
    }

  g_signal_connect (G_OBJECT (priv->gc_position), "position-changed",
      G_CALLBACK (position_changed_cb), location_manager);

  /* Get updated when the address changes */
  priv->gc_address = geoclue_master_client_create_address (
      priv->gc_client, &error);
  if (priv->gc_address == NULL)
    {
      g_printerr ("Failed to create GeoclueAddress: %s", error->message);
      return;
    }

  g_signal_connect (G_OBJECT (priv->gc_address), "address-changed",
      G_CALLBACK (address_changed_cb), location_manager);

  priv->is_setup = TRUE;
}

static void
publish_cb (EmpathyConf *conf,
            const gchar *key,
            gpointer user_data)
{
  EmpathyLocationManager *manager = EMPATHY_LOCATION_MANAGER (user_data);
  EmpathyLocationManagerPriv *priv;
  gboolean publish_location;

  DEBUG ("Publish Conf changed");
  priv = GET_PRIV (manager);
  if (!empathy_conf_get_bool (conf, key, &publish_location))
    return;

  if (publish_location && !priv->is_setup)
    setup_geoclue (manager);
}


static void 
resource_cb (EmpathyConf  *conf,
             const gchar *key,
             gpointer user_data)
{
  EmpathyLocationManager *manager = EMPATHY_LOCATION_MANAGER (user_data);
  EmpathyLocationManagerPriv *priv;
  GeoclueResourceFlags resource = 0;
  gboolean resource_enabled;

  priv = GET_PRIV (manager);
  DEBUG ("A Resource Conf changed");

  if (empathy_conf_get_bool (conf, key, &resource_enabled))
    {
      if (strcmp (key, EMPATHY_PREFS_LOCATION_RESOURCE_NETWORK))
        resource = GEOCLUE_RESOURCE_NETWORK;
      if (strcmp (key, EMPATHY_PREFS_LOCATION_RESOURCE_CELL))
        resource = GEOCLUE_RESOURCE_CELL;
      if (strcmp (key, EMPATHY_PREFS_LOCATION_RESOURCE_GPS))
        resource = GEOCLUE_RESOURCE_GPS;
    }
  if (resource_enabled)
    priv->resources |= resource;
  else
    priv->resources &= resource;

  update_resources (manager);
}

#endif
