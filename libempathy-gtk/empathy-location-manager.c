/*
 * Copyright (C) 2009 Collabora Ltd.
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
 * Authors: Pierre-Luc Beaudoin <pierre-luc.beaudoin@collabora.co.uk>
 */

#include "config.h"

#include <string.h>
#include <time.h>

#include <glib/gi18n.h>

#include <telepathy-glib/util.h>

#include <geoclue/geoclue-master.h>

#include <extensions/extensions.h>

#include "empathy-location-manager.h"
#include "empathy-conf.h"

#include "libempathy/empathy-account-manager.h"
#include "libempathy/empathy-enum-types.h"
#include "libempathy/empathy-location.h"
#include "libempathy/empathy-tp-contact-factory.h"
#include "libempathy/empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_LOCATION
#include "libempathy/empathy-debug.h"

/* Seconds before updating the location */
#define TIMEOUT 10
static EmpathyLocationManager *location_manager = NULL;

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyLocationManager)
typedef struct {
    gboolean geoclue_is_setup;
    /* Contains the location to be sent to accounts.  Geoclue is used
     * to populate it.  This HashTable uses Telepathy's style (string,
     * GValue). Keys are defined in empathy-location.h
     */
    GHashTable *location;

    GeoclueResourceFlags resources;
    GeoclueMasterClient *gc_client;
    GeocluePosition *gc_position;
    GeoclueAddress *gc_address;

    gboolean reduce_accuracy;
    gdouble reduce_value;
    EmpathyAccountManager *account_manager;

    /* The idle id for publish_on_idle func */
    guint timeout_id;
} EmpathyLocationManagerPriv;

G_DEFINE_TYPE (EmpathyLocationManager, empathy_location_manager, G_TYPE_OBJECT);

static GObject *
location_manager_constructor (GType type,
    guint n_construct_params,
    GObjectConstructParam *construct_params)
{
  GObject *retval;

  if (location_manager == NULL)
    {
      retval = G_OBJECT_CLASS (empathy_location_manager_parent_class)->constructor
          (type, n_construct_params, construct_params);

      location_manager = EMPATHY_LOCATION_MANAGER (retval);
      g_object_add_weak_pointer (retval, (gpointer) &location_manager);
    }
  else
    {
      retval = g_object_ref (location_manager);
    }

  return retval;
}

static void
location_manager_dispose (GObject *object)
{
  EmpathyLocationManagerPriv *priv = GET_PRIV (object);

  if (priv->account_manager != NULL)
  {
    g_object_unref (priv->account_manager);
    priv->account_manager = NULL;
  }

  if (priv->gc_client != NULL)
  {
    g_object_unref (priv->gc_client);
    priv->gc_client = NULL;
  }

  if (priv->gc_position != NULL)
  {
    g_object_unref (priv->gc_position);
    priv->gc_position = NULL;
  }

  if (priv->gc_address != NULL)
  {
    g_object_unref (priv->gc_address);
    priv->gc_address = NULL;
  }

  if (priv->location != NULL)
  {
    g_hash_table_unref (priv->location);
    priv->location = NULL;
  }

  G_OBJECT_CLASS (empathy_location_manager_parent_class)->finalize (object);
}

static void
location_manager_get_property (GObject *object,
                      guint param_id,
                      GValue *value,
                      GParamSpec *pspec)
{
  /*EmpathyLocationManagerPriv *priv = GET_PRIV (object); */

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
  /* EmpathyLocationManagerPriv *priv = GET_PRIV (object); */

  switch (param_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

static void
empathy_location_manager_class_init (EmpathyLocationManagerClass *class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (class);

  object_class->constructor = location_manager_constructor;
  object_class->dispose = location_manager_dispose;
  object_class->get_property = location_manager_get_property;
  object_class->set_property = location_manager_set_property;

  g_type_class_add_private (object_class, sizeof (EmpathyLocationManagerPriv));
}

static void
publish_location (EmpathyLocationManager *location_manager,
    TpConnection *conn,
    gboolean force_publication)
{
  EmpathyLocationManagerPriv *priv = GET_PRIV (location_manager);
  guint connection_status = -1;
  gboolean can_publish;
  EmpathyConf *conf = empathy_conf_get ();
  EmpathyTpContactFactory *factory;

  if (!conn)
    return;

  if (force_publication == FALSE)
    {
      if (!empathy_conf_get_bool (conf, EMPATHY_PREFS_LOCATION_PUBLISH,
            &can_publish))
        return;

      if (can_publish == FALSE)
        return;
    }

  connection_status = tp_connection_get_status (conn, NULL);

  if (connection_status != TP_CONNECTION_STATUS_CONNECTED)
    return;

  DEBUG ("Publishing %s location to connection %p",
      (g_hash_table_size (priv->location) == 0 ? "empty" : ""),
      conn);

  factory = empathy_tp_contact_factory_dup_singleton (conn);
  empathy_tp_contact_factory_set_location (factory, priv->location);
  g_object_unref (factory);
}

static void
publish_to_all_connections (EmpathyLocationManager *location_manager,
    gboolean force_publication)
{
  EmpathyLocationManagerPriv *priv = GET_PRIV (location_manager);
  GList *connections = NULL, *l;

  connections = empathy_account_manager_dup_connections (priv->account_manager);
  for (l = connections; l; l = l->next)
    {
      publish_location (location_manager, l->data, force_publication);
      g_object_unref (l->data);
    }
  g_list_free (connections);

}

static gboolean
publish_on_idle (gpointer user_data)
{
  EmpathyLocationManager *manager = EMPATHY_LOCATION_MANAGER (user_data);
  EmpathyLocationManagerPriv *priv = GET_PRIV (manager);

  priv->timeout_id = 0;
  publish_to_all_connections (manager, TRUE);
  return FALSE;
}

static void
new_connection_cb (EmpathyAccountManager *manager,
    TpConnection *conn,
    gpointer *location_manager)
{
  EmpathyLocationManagerPriv *priv = GET_PRIV (location_manager);
  DEBUG ("New connection %p", conn);

  /* Don't publish if it is already planned (ie startup) */
  if (priv->timeout_id == 0)
    {
      publish_location (EMPATHY_LOCATION_MANAGER (location_manager), conn,
          FALSE);
    }
}

static void
update_timestamp (EmpathyLocationManager *location_manager)
{
  EmpathyLocationManagerPriv *priv= GET_PRIV (location_manager);
  GValue *new_value;
  gint64 stamp64;
  time_t timestamp;

  timestamp = time (NULL);
  stamp64 = (gint64) timestamp;
  new_value = tp_g_value_slice_new_int64 (stamp64);
  g_hash_table_insert (priv->location, g_strdup (EMPATHY_LOCATION_TIMESTAMP),
      new_value);
  DEBUG ("\t - Timestamp: %" G_GINT64_FORMAT, stamp64);
}

static void
address_changed_cb (GeoclueAddress *address,
                    int timestamp,
                    GHashTable *details,
                    GeoclueAccuracy *accuracy,
                    gpointer location_manager)
{
  GeoclueAccuracyLevel level;
  EmpathyLocationManagerPriv *priv = GET_PRIV (location_manager);
  GHashTableIter iter;
  gpointer key, value;

  geoclue_accuracy_get_details (accuracy, &level, NULL, NULL);
  DEBUG ("New address (accuracy level %d):", level);
  /* FIXME: Publish accuracy level also considering the position's */

  g_hash_table_remove (priv->location, EMPATHY_LOCATION_STREET);
  g_hash_table_remove (priv->location, EMPATHY_LOCATION_AREA);
  g_hash_table_remove (priv->location, EMPATHY_LOCATION_REGION);
  g_hash_table_remove (priv->location, EMPATHY_LOCATION_COUNTRY);
  g_hash_table_remove (priv->location, EMPATHY_LOCATION_COUNTRY_CODE);
  g_hash_table_remove (priv->location, EMPATHY_LOCATION_POSTAL_CODE);

  if (g_hash_table_size (details) == 0)
    return;

  g_hash_table_iter_init (&iter, details);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GValue *new_value;
      /* Discard street information if reduced accuracy is on */
      if (priv->reduce_accuracy && strcmp (key, EMPATHY_LOCATION_STREET) == 0)
        continue;

      new_value = tp_g_value_slice_new_string (value);
      g_hash_table_insert (priv->location, g_strdup (key), new_value);

      DEBUG ("\t - %s: %s", (gchar *) key, (gchar *) value);
    }

  update_timestamp (location_manager);
  if (priv->timeout_id == 0)
    priv->timeout_id = g_timeout_add_seconds (TIMEOUT, publish_on_idle, location_manager);
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
    {
      address_changed_cb (address, timestamp, details, accuracy, location_manager);
    }
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
  EmpathyLocationManagerPriv *priv = GET_PRIV (location_manager);
  GeoclueAccuracyLevel level;
  gdouble mean, horizontal, vertical;
  GValue *new_value;


  geoclue_accuracy_get_details (accuracy, &level, &horizontal, &vertical);
  DEBUG ("New position (accuracy level %d)", level);
  if (level == GEOCLUE_ACCURACY_LEVEL_NONE)
    return;

  if (fields & GEOCLUE_POSITION_FIELDS_LONGITUDE)
    {
      longitude += priv->reduce_value;
      new_value = tp_g_value_slice_new_double (longitude);
      g_hash_table_insert (priv->location, g_strdup (EMPATHY_LOCATION_LON),
          new_value);
      DEBUG ("\t - Longitude: %f", longitude);
    }
  else
    {
      g_hash_table_remove (priv->location, EMPATHY_LOCATION_LON);
    }

  if (fields & GEOCLUE_POSITION_FIELDS_LATITUDE)
    {
      latitude += priv->reduce_value;
      new_value = tp_g_value_slice_new_double (latitude);
      g_hash_table_replace (priv->location, g_strdup (EMPATHY_LOCATION_LAT),
          new_value);
      DEBUG ("\t - Latitude: %f", latitude);
    }
  else
    {
      g_hash_table_remove (priv->location, EMPATHY_LOCATION_LAT);
    }

  if (fields & GEOCLUE_POSITION_FIELDS_ALTITUDE)
    {
      new_value = tp_g_value_slice_new_double (altitude);
      g_hash_table_replace (priv->location, g_strdup (EMPATHY_LOCATION_ALT),
          new_value);
      DEBUG ("\t - Altitude: %f", altitude);
    }
  else
    {
      g_hash_table_remove (priv->location, EMPATHY_LOCATION_ALT);
    }

  if (level == GEOCLUE_ACCURACY_LEVEL_DETAILED)
    {
      mean = (horizontal + vertical) / 2.0;
      new_value = tp_g_value_slice_new_double (mean);
      g_hash_table_replace (priv->location,
          g_strdup (EMPATHY_LOCATION_ACCURACY), new_value);
      DEBUG ("\t - Accuracy: %f", mean);
    }
  else
    {
      g_hash_table_remove (priv->location, EMPATHY_LOCATION_ACCURACY);
    }

  update_timestamp (location_manager);
  if (priv->timeout_id == 0)
    priv->timeout_id = g_timeout_add_seconds (TIMEOUT, publish_on_idle, location_manager);
}

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
    {
      position_changed_cb (position, fields, timestamp, latitude, longitude,
          altitude, accuracy, location_manager);
    }
}

static void
update_resources (EmpathyLocationManager *location_manager)
{
  EmpathyLocationManagerPriv *priv = GET_PRIV (location_manager);

  DEBUG ("Updating resources %d", priv->resources);

  if (!geoclue_master_client_set_requirements (priv->gc_client,
          GEOCLUE_ACCURACY_LEVEL_NONE, 0, TRUE, priv->resources,
          NULL))
    {
      DEBUG ("set_requirements failed");
      return;
    }

  if (!priv->geoclue_is_setup)
    return;

  geoclue_address_get_address_async (priv->gc_address,
      initial_address_cb, location_manager);
  geoclue_position_get_position_async (priv->gc_position,
      initial_position_cb, location_manager);
}

static void
setup_geoclue (EmpathyLocationManager *location_manager)
{
  EmpathyLocationManagerPriv *priv = GET_PRIV (location_manager);

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
      DEBUG ("Failed to create GeocluePosition: %s", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect (G_OBJECT (priv->gc_position), "position-changed",
      G_CALLBACK (position_changed_cb), location_manager);

  /* Get updated when the address changes */
  priv->gc_address = geoclue_master_client_create_address (
      priv->gc_client, &error);
  if (priv->gc_address == NULL)
    {
      DEBUG ("Failed to create GeoclueAddress: %s", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect (G_OBJECT (priv->gc_address), "address-changed",
      G_CALLBACK (address_changed_cb), location_manager);

  priv->geoclue_is_setup = TRUE;
}

static void
publish_cb (EmpathyConf *conf,
            const gchar *key,
            gpointer user_data)
{
  EmpathyLocationManager *manager = EMPATHY_LOCATION_MANAGER (user_data);
  EmpathyLocationManagerPriv *priv = GET_PRIV (manager);
  gboolean can_publish;

  DEBUG ("Publish Conf changed");


  if (empathy_conf_get_bool (conf, key, &can_publish) == FALSE)
    return;

  if (can_publish == TRUE)
    {
      if (priv->geoclue_is_setup == FALSE)
        setup_geoclue (manager);
      /* if still not setup than the init failed */
      if (priv->geoclue_is_setup == FALSE)
        return;

      geoclue_address_get_address_async (priv->gc_address,
          initial_address_cb, manager);
      geoclue_position_get_position_async (priv->gc_position,
          initial_position_cb, manager);
    }
  else
    {
      /* As per XEP-0080: send an empty location to have remove current
       * location from the servers
       */
      g_hash_table_remove_all (priv->location);
      publish_to_all_connections (manager, TRUE);
    }

}

static void
resource_cb (EmpathyConf  *conf,
             const gchar *key,
             gpointer user_data)
{
  EmpathyLocationManager *manager = EMPATHY_LOCATION_MANAGER (user_data);
  EmpathyLocationManagerPriv *priv = GET_PRIV (manager);
  GeoclueResourceFlags resource = 0;
  gboolean resource_enabled;

  DEBUG ("%s changed", key);

  if (!empathy_conf_get_bool (conf, key, &resource_enabled))
    return;

  if (strcmp (key, EMPATHY_PREFS_LOCATION_RESOURCE_NETWORK) == 0)
    resource = GEOCLUE_RESOURCE_NETWORK;
  if (strcmp (key, EMPATHY_PREFS_LOCATION_RESOURCE_CELL) == 0)
    resource = GEOCLUE_RESOURCE_CELL;
  if (strcmp (key, EMPATHY_PREFS_LOCATION_RESOURCE_GPS) == 0)
    resource = GEOCLUE_RESOURCE_GPS;

  if (resource_enabled)
    priv->resources |= resource;
  else
    priv->resources &= ~resource;

  if (priv->geoclue_is_setup)
    update_resources (manager);
}

static void
accuracy_cb (EmpathyConf  *conf,
             const gchar *key,
             gpointer user_data)
{
  EmpathyLocationManager *manager = EMPATHY_LOCATION_MANAGER (user_data);
  EmpathyLocationManagerPriv *priv = GET_PRIV (manager);

  gboolean enabled;

  DEBUG ("%s changed", key);

  if (!empathy_conf_get_bool (conf, key, &enabled))
    return;
  priv->reduce_accuracy = enabled;

  if (enabled)
    {
      GRand *rand = g_rand_new_with_seed (time (NULL));
      priv->reduce_value = g_rand_double_range (rand, -0.25, 0.25);
      g_rand_free (rand);
    }
  else
    {
      priv->reduce_value = 0.0;
    }

  if (!priv->geoclue_is_setup)
    return;

  geoclue_address_get_address_async (priv->gc_address,
      initial_address_cb, manager);
  geoclue_position_get_position_async (priv->gc_position,
      initial_position_cb, manager);
}

static void
empathy_location_manager_init (EmpathyLocationManager *location_manager)
{
  EmpathyConf               *conf;
  EmpathyLocationManagerPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (location_manager,
      EMPATHY_TYPE_LOCATION_MANAGER, EmpathyLocationManagerPriv);

  location_manager->priv = priv;
  priv->geoclue_is_setup = FALSE;
  priv->location = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      g_free, (GDestroyNotify) tp_g_value_slice_free);

  /* Setup account status callbacks */
  priv->account_manager = empathy_account_manager_dup_singleton ();
  g_signal_connect (priv->account_manager,
    "new-connection",
    G_CALLBACK (new_connection_cb), location_manager);

  /* Setup settings status callbacks */
  conf = empathy_conf_get ();
  empathy_conf_notify_add (conf, EMPATHY_PREFS_LOCATION_PUBLISH, publish_cb,
      location_manager);
  empathy_conf_notify_add (conf, EMPATHY_PREFS_LOCATION_RESOURCE_NETWORK,
      resource_cb, location_manager);
  empathy_conf_notify_add (conf, EMPATHY_PREFS_LOCATION_RESOURCE_CELL,
      resource_cb, location_manager);
  empathy_conf_notify_add (conf, EMPATHY_PREFS_LOCATION_RESOURCE_GPS,
      resource_cb, location_manager);
  empathy_conf_notify_add (conf, EMPATHY_PREFS_LOCATION_REDUCE_ACCURACY,
      accuracy_cb, location_manager);

  resource_cb (conf, EMPATHY_PREFS_LOCATION_RESOURCE_NETWORK, location_manager);
  resource_cb (conf, EMPATHY_PREFS_LOCATION_RESOURCE_CELL, location_manager);
  resource_cb (conf, EMPATHY_PREFS_LOCATION_RESOURCE_GPS, location_manager);
  accuracy_cb (conf, EMPATHY_PREFS_LOCATION_REDUCE_ACCURACY, location_manager);
  publish_cb (conf, EMPATHY_PREFS_LOCATION_PUBLISH, location_manager);
}

EmpathyLocationManager *
empathy_location_manager_dup_singleton (void)
{
  return EMPATHY_LOCATION_MANAGER (g_object_new (EMPATHY_TYPE_LOCATION_MANAGER,
      NULL));
}
