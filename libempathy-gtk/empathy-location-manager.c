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

#include <extensions/extensions.h>

#include "empathy-location-manager.h"
#include "empathy-conf.h"

#include "libempathy/empathy-enum-types.h"
#include "libempathy/empathy-location.h"
#include "libempathy/empathy-contact-factory.h"
#include "libempathy/empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_LOCATION
#include "libempathy/empathy-debug.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyLocationManager)
typedef struct {
    gboolean is_setup;
    MissionControl *mc;
    GHashTable *location;
    gpointer token;
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
publish_location (EmpathyLocationManager *location_manager, McAccount *account)
{
  EmpathyLocationManagerPriv *priv;
  guint connection_status = -1;
  gboolean can_publish;
  EmpathyConf *conf = empathy_conf_get ();
  EmpathyContactFactory *factory = empathy_contact_factory_new ();
  priv = GET_PRIV (location_manager);

  if (!empathy_conf_get_bool (conf, EMPATHY_PREFS_LOCATION_PUBLISH, &can_publish))
    return;

  if (!can_publish)
    return;

  connection_status = mission_control_get_connection_status (priv->mc,
      account, NULL);

  if (connection_status != TP_CONNECTION_STATUS_CONNECTED)
    return;

  DEBUG ("Publishing location to account %s", mc_account_get_display_name (account));

  empathy_contact_factory_set_location (factory, account, priv->location);
}

static void
publish_location_to_all_accounts (EmpathyLocationManager *location_manager)
{
  GList *accounts = NULL, *l;

  accounts = mc_accounts_list_by_enabled (TRUE);
  for (l = accounts; l; l = l->next)
    {
      publish_location (location_manager, l->data);
    }

  mc_accounts_list_free (accounts);
}

static void
account_status_changed_cb (MissionControl *mc,
                           TpConnectionStatus status,
                           McPresence presence,
                           TpConnectionStatusReason reason,
                           const gchar *unique_name,
                           gpointer *location_manager)
{
  DEBUG ("Account %s changed status to %d", unique_name, status);
  McAccount *account = mc_account_lookup (unique_name);
  if (account && status == TP_CONNECTION_STATUS_CONNECTED)
    publish_location (EMPATHY_LOCATION_MANAGER (location_manager), account);
}

static void
empathy_location_manager_init (EmpathyLocationManager *location_manager)
{
  EmpathyConf               *conf;
  EmpathyLocationManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (location_manager,
    EMPATHY_TYPE_LOCATION_MANAGER, EmpathyLocationManagerPriv);

  location_manager->priv = priv;
  priv->is_setup = FALSE;
  priv->mc = empathy_mission_control_new ();
  priv->location = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
        (GDestroyNotify) tp_g_value_slice_free);

  // Setup settings status callbacks
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

  // Setup account status callbacks
  priv->token = empathy_connect_to_account_status_changed (priv->mc,
      G_CALLBACK (account_status_changed_cb), location_manager, NULL);
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
initial_position_cb (GeocluePosition *position,
                     GeocluePositionFields fields,
                     int timestamp,
                     double latitude,
                     double longitude,
                     double altitude,
                     GeoclueAccuracy *accuracy,
                     GError *error,
                     gpointer location_manager)
{
  if (error)
    {
      DEBUG ("Error: %s", error->message);
      g_error_free (error);
    }
  else
    position_changed_cb (position, fields, timestamp, latitude, longitude,
      altitude, accuracy, location_manager);
}

static void
position_changed_cb (GeocluePosition *position,
                     GeocluePositionFields fields,
                     int timestamp,
                     double latitude,
                     double longitude,
                     double altitude,
                     GeoclueAccuracy *accuracy,
                     gpointer location_manager)
{
  EmpathyLocationManagerPriv *priv;
  priv = GET_PRIV (location_manager);
  GeoclueAccuracyLevel level;
  gdouble mean, horizontal, vertical;
  GValue *new_value;

  geoclue_accuracy_get_details (accuracy, &level, &horizontal, &vertical);
  DEBUG ("New position (accuracy level %d)", level);
  if (level == GEOCLUE_ACCURACY_LEVEL_NONE)
    return;

  if (fields & GEOCLUE_POSITION_FIELDS_LONGITUDE)
    {
      new_value = tp_g_value_slice_new (G_TYPE_DOUBLE);
      g_value_set_double (new_value, longitude);
      g_hash_table_insert (priv->location, EMPATHY_LOCATION_LON, new_value);
      DEBUG ("\t - Longitude: %f", longitude);
    }
  if (fields & GEOCLUE_POSITION_FIELDS_LATITUDE)
    {
      new_value = tp_g_value_slice_new (G_TYPE_DOUBLE);
      g_value_set_double (new_value, latitude);
      g_hash_table_insert (priv->location, EMPATHY_LOCATION_LAT, new_value);
      DEBUG ("\t - Latitude: %f", latitude);
    }
  if (fields & GEOCLUE_POSITION_FIELDS_ALTITUDE)
    {
      new_value = tp_g_value_slice_new (G_TYPE_DOUBLE);
      g_value_set_double (new_value, altitude);
      g_hash_table_insert (priv->location, EMPATHY_LOCATION_ALT, new_value);
      DEBUG ("\t - Altitude: %f", altitude);
    }

  if (level == GEOCLUE_ACCURACY_LEVEL_DETAILED)
    {
      mean = (horizontal + vertical) / 2.0;
      new_value = tp_g_value_slice_new (G_TYPE_DOUBLE);
      g_value_set_double (new_value, mean);
      g_hash_table_insert (priv->location, EMPATHY_LOCATION_ACCURACY, new_value);
      DEBUG ("\t - Accuracy: %f", mean);
    }

  publish_location_to_all_accounts (EMPATHY_LOCATION_MANAGER (location_manager));
}


static void
address_foreach_cb (gpointer key,
                    gpointer value,
                    gpointer location_manager)
{
  if (location_manager == NULL)
    return;

  EmpathyLocationManagerPriv *priv;
  priv = GET_PRIV (location_manager);

  GValue *new_value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (new_value, value);

  g_hash_table_insert (priv->location, g_strdup (key), new_value);
  DEBUG ("\t - %s: %s", (char*) key, (char*) value);
}

static void
initial_address_cb (GeoclueAddress *address,
                    int timestamp,
                    GHashTable *details,
                    GeoclueAccuracy *accuracy,
                    GError *error,
                    gpointer location_manager)
{
  if (error)
    {
      DEBUG ("Error: %s", error->message);
      g_error_free (error);
    }
  else
    address_changed_cb (address, timestamp, details, accuracy, location_manager);
}

static void
address_changed_cb (GeoclueAddress *address,
                    int timestamp,
                    GHashTable *details,
                    GeoclueAccuracy *accuracy,
                    gpointer location_manager)
{
  GeoclueAccuracyLevel level;
  geoclue_accuracy_get_details (accuracy, &level, NULL, NULL);
  EmpathyLocationManagerPriv *priv;

  DEBUG ("New address (accuracy level %d):", level);

  priv = GET_PRIV (location_manager);
  g_hash_table_remove_all (priv->location);

  g_hash_table_foreach (details, address_foreach_cb, (gpointer)location_manager);

  publish_location_to_all_accounts (EMPATHY_LOCATION_MANAGER (location_manager));
}


static void
update_resources (EmpathyLocationManager *location_manager)
{
  EmpathyLocationManagerPriv *priv;

  priv = GET_PRIV (location_manager);

  if (!priv->is_setup)
    return;

  DEBUG ("Updating resources");

  if (!geoclue_master_client_set_requirements (priv->gc_client,
          GEOCLUE_ACCURACY_LEVEL_LOCALITY, 0, FALSE, priv->resources,
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
  geoclue_position_get_position_async (priv->gc_position,
      initial_position_cb, location_manager);

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
  geoclue_address_get_address_async (priv->gc_address,
      initial_address_cb, location_manager);

  priv->is_setup = TRUE;
}

static void
publish_cb (EmpathyConf *conf,
            const gchar *key,
            gpointer user_data)
{
  EmpathyLocationManager *manager = EMPATHY_LOCATION_MANAGER (user_data);
  EmpathyLocationManagerPriv *priv;
  gboolean can_publish;

  DEBUG ("Publish Conf changed");
  priv = GET_PRIV (manager);
  if (!empathy_conf_get_bool (conf, key, &can_publish))
    return;

  if (can_publish)
    {
      if (!priv->is_setup)
        setup_geoclue (manager);
      publish_location_to_all_accounts (manager);
    }
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
  DEBUG ("%s changed", key);

  if (empathy_conf_get_bool (conf, key, &resource_enabled))
    {
      if (strcmp (key, EMPATHY_PREFS_LOCATION_RESOURCE_NETWORK) == 0)
        resource = GEOCLUE_RESOURCE_NETWORK;
      if (strcmp (key, EMPATHY_PREFS_LOCATION_RESOURCE_CELL) == 0)
        resource = GEOCLUE_RESOURCE_CELL;
      if (strcmp (key, EMPATHY_PREFS_LOCATION_RESOURCE_GPS) == 0)
        resource = GEOCLUE_RESOURCE_GPS;
    }
  if (resource_enabled)
    priv->resources |= resource;
  else
    priv->resources &= resource;

  update_resources (manager);
}

#endif
