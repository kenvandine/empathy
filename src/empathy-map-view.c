/*
 * Copyright (C) 2008, 2009 Collabora Ltd.
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
 * Authors: Pierre-Luc Beaudoin <pierre-luc.beaudoin@collabora.co.uk>
 */

#include <config.h>

#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <champlain/champlain.h>
#include <champlain-gtk/champlain-gtk.h>
#include <clutter-gtk/gtk-clutter-embed.h>
#if HAVE_GEOCLUE
#include <geoclue/geoclue-geocode.h>
#endif
#include <telepathy-glib/util.h>

#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-location.h>

#include <libempathy-gtk/empathy-contact-list-store.h>
#include <libempathy-gtk/empathy-contact-list-view.h>
#include <libempathy-gtk/empathy-presence-chooser.h>
#include <libempathy-gtk/empathy-ui-utils.h>

#include "empathy-map-view.h"

#define DEBUG_FLAG EMPATHY_DEBUG_LOCATION
#include <libempathy/empathy-debug.h>

typedef struct {
  EmpathyContactListStore *list_store;

  GtkWidget *window;
  GtkWidget *zoom_in;
  GtkWidget *zoom_out;
  ChamplainView *map_view;
  ChamplainLayer *layer;
#if HAVE_GEOCLUE
  GeoclueGeocode *geocode;
#endif
} EmpathyMapView;

static void map_view_destroy_cb (GtkWidget *widget,
    EmpathyMapView *window);
static gboolean map_view_contacts_foreach (GtkTreeModel *model,
    GtkTreePath *path,
    GtkTreeIter *iter,
    gpointer user_data);
static void map_view_zoom_in_cb (GtkWidget *widget,
    EmpathyMapView *window);
static void map_view_zoom_out_cb (GtkWidget *widget,
    EmpathyMapView *window);
static void map_view_contact_location_notify (GObject *gobject,
    GParamSpec *arg1,
    gpointer user_data);
static gchar * get_dup_string (GHashTable *location,
    gchar *key);

GtkWidget *
empathy_map_view_show ()
{
  static EmpathyMapView *window = NULL;
  GtkBuilder *gui;
  GtkWidget *sw;
  GtkWidget *embed;
  gchar *filename;
  GtkTreeModel *model;
  EmpathyContactList *list_iface;
  EmpathyContactListStore *list_store;

  if (window)
    {
      empathy_window_present (GTK_WINDOW (window->window), TRUE);
      return window->window;
    }

  window = g_slice_new0 (EmpathyMapView);

  /* Set up interface */
  filename = empathy_file_lookup ("empathy-map-view.ui", "src");
  gui = empathy_builder_get_file (filename,
     "map_view", &window->window,
     "zoom_in", &window->zoom_in,
     "zoom_out", &window->zoom_out,
     "map_scrolledwindow", &sw,
     NULL);
  g_free (filename);

  empathy_builder_connect (gui, window,
      "map_view", "destroy", map_view_destroy_cb,
      "zoom_in", "clicked", map_view_zoom_in_cb,
      "zoom_out", "clicked", map_view_zoom_out_cb,
      NULL);

  g_object_unref (gui);

  /* Clear the static pointer to window if the dialog is destroyed */
  g_object_add_weak_pointer (G_OBJECT (window->window), (gpointer *) &window);

  list_iface = EMPATHY_CONTACT_LIST (empathy_contact_manager_dup_singleton ());
  list_store = empathy_contact_list_store_new (list_iface);
  empathy_contact_list_store_set_show_groups (list_store, FALSE);
  empathy_contact_list_store_set_show_avatars (list_store, TRUE);
  g_object_unref (list_iface);

  window->list_store = list_store;

  /* Set up map view */
  window->map_view = CHAMPLAIN_VIEW (champlain_view_new ());
  g_object_set (G_OBJECT (window->map_view), "zoom-level", 1,
     "scroll-mode", CHAMPLAIN_SCROLL_MODE_KINETIC, NULL);
  champlain_view_center_on (window->map_view, 36, 0);

  embed = champlain_view_embed_new (window->map_view);
  gtk_container_add (GTK_CONTAINER (sw),
     GTK_WIDGET (embed));
  gtk_widget_show_all (embed);

  window->layer = g_object_ref (champlain_layer_new ());
  champlain_view_add_layer (window->map_view, window->layer);

  /* Set up contact list. */
  model = GTK_TREE_MODEL (window->list_store);
  gtk_tree_model_foreach (model, map_view_contacts_foreach, window);

  empathy_window_present (GTK_WINDOW (window->window), TRUE);
  return window->window;
}

static void
map_view_destroy_cb (GtkWidget *widget,
    EmpathyMapView *window)
{
  GtkTreeModel *model;

  g_object_unref (window->list_store);
  g_object_unref (window->layer);
#if HAVE_GEOCLUE
  if (window->geocode != NULL)
    g_object_unref (window->geocode);
#endif
  g_slice_free (EmpathyMapView, window);
}

#if HAVE_GEOCLUE
#define GEOCODE_SERVICE "org.freedesktop.Geoclue.Providers.Yahoo"
#define GEOCODE_PATH "/org/freedesktop/Geoclue/Providers/Yahoo"

/* This callback is called by geoclue when it found a position
 * for the given address.  A position is necessary for a contact
 * to show up on the map
 */
static void
map_view_geocode_cb (GeoclueGeocode *geocode,
    GeocluePositionFields fields,
    double latitude,
    double longitude,
    double altitude,
    GeoclueAccuracy *accuracy,
    GError *error,
    gpointer userdata)
{
  GValue *new_value;
  GHashTable *location;

  location = empathy_contact_get_location (EMPATHY_CONTACT (userdata));

  GHashTable *address = g_object_get_data (userdata, "geoclue-address");
  g_hash_table_destroy (address);
  g_object_set_data (userdata, "geoclue-address", NULL);

  if (error != NULL)
    {
      DEBUG ("Error geocoding location : %s", error->message);
      return;
    }

  if (fields & GEOCLUE_POSITION_FIELDS_LONGITUDE)
    {
      new_value = tp_g_value_slice_new_double (longitude);
      g_hash_table_replace (location, EMPATHY_LOCATION_LON, new_value);
      DEBUG ("\t - Longitude: %f", longitude);
    }
  if (fields & GEOCLUE_POSITION_FIELDS_LATITUDE)
    {
      new_value = tp_g_value_slice_new_double (latitude);
      g_hash_table_replace (location, EMPATHY_LOCATION_LAT, new_value);
      DEBUG ("\t - Latitude: %f", latitude);
    }
  if (fields & GEOCLUE_POSITION_FIELDS_ALTITUDE)
    {
      new_value = tp_g_value_slice_new_double (altitude);
      g_hash_table_replace (location, EMPATHY_LOCATION_ALT, new_value);
      DEBUG ("\t - Altitude: %f", altitude);
    }

  /* Don't change the accuracy as we used an address to get this position */

  /* Ref the location hash table as it will be unref'd in set_location, 
   * and we are only updating it */
  g_hash_table_ref (location);
  empathy_contact_set_location (EMPATHY_CONTACT (userdata), location);
  g_hash_table_unref (location);
}
#endif

static gchar *
get_dup_string (GHashTable *location,
    gchar *key)
{
  GValue *value;

  value = g_hash_table_lookup (location, key);
  if (value != NULL)
    return g_value_dup_string (value);

  return NULL;
}

static void
map_view_marker_update_position (ChamplainMarker *marker,
    EmpathyContact *contact)
{
  gdouble lon, lat;
  GValue *value;
  GHashTable *location;

  location = empathy_contact_get_location (contact);
  if (location == NULL ||
      g_hash_table_size (location) == 0)
  {
    clutter_actor_hide (CLUTTER_ACTOR (marker));
    return;
  }

  value = g_hash_table_lookup (location, EMPATHY_LOCATION_LAT);
  if (value == NULL)
    {
      clutter_actor_hide (CLUTTER_ACTOR (marker));
      return;
    }
  lat = g_value_get_double (value);

  value = g_hash_table_lookup (location, EMPATHY_LOCATION_LON);
  if (value == NULL)
    {
      clutter_actor_hide (CLUTTER_ACTOR (marker));
      return;
    }
  lon = g_value_get_double (value);

  clutter_actor_show (CLUTTER_ACTOR (marker));
  champlain_base_marker_set_position (CHAMPLAIN_BASE_MARKER (marker), lat, lon);
}

static void
map_view_contact_location_notify (GObject *gobject,
    GParamSpec *arg1,
    gpointer user_data)
{
  ChamplainMarker *marker = CHAMPLAIN_MARKER (user_data);
  EmpathyContact *contact = EMPATHY_CONTACT (gobject);
  map_view_marker_update_position (marker, contact);
}

static gboolean
map_view_contacts_foreach (GtkTreeModel *model,
    GtkTreePath *path,
    GtkTreeIter *iter,
    gpointer user_data)
{
  EmpathyMapView *window = (EmpathyMapView*) user_data;
  EmpathyContact *contact;
  ClutterActor *marker;
  ClutterActor *texture;
  GHashTable *location;
  GdkPixbuf *avatar;
  const gchar *name;

  gtk_tree_model_get (model, iter, EMPATHY_CONTACT_LIST_STORE_COL_CONTACT,
     &contact, -1);

  if (contact == NULL)
    return FALSE;

  location = empathy_contact_get_location (contact);

  if (location == NULL)
    return FALSE;

  marker = champlain_marker_new ();

  avatar = empathy_pixbuf_avatar_from_contact_scaled (contact, 32, 32);
  if (avatar != NULL)
    {
      texture = clutter_texture_new ();
      gtk_clutter_texture_set_from_pixbuf (CLUTTER_TEXTURE (texture), avatar);
      champlain_marker_set_image (CHAMPLAIN_MARKER (marker), texture);
    }
  else
    champlain_marker_set_image (CHAMPLAIN_MARKER (marker), NULL);

  name = empathy_contact_get_name (contact);
  champlain_marker_set_text (CHAMPLAIN_MARKER (marker), name);

  clutter_container_add (CLUTTER_CONTAINER (window->layer), marker, NULL);

  g_signal_connect (contact, "notify::location",
      G_CALLBACK (map_view_contact_location_notify), marker);


#if HAVE_GEOCLUE
  gchar *str;
  GHashTable *address;
  GValue *value;

  value = g_hash_table_lookup (location, EMPATHY_LOCATION_LON);
  if (value == NULL)
      {
        if (window->geocode == NULL)
          window->geocode = geoclue_geocode_new (GEOCODE_SERVICE, GEOCODE_PATH);

        address = geoclue_address_details_new();

        str = get_dup_string (location, EMPATHY_LOCATION_COUNTRY);
        if (str != NULL)
          g_hash_table_insert (address, g_strdup ("country"), str);

        str = get_dup_string (location, EMPATHY_LOCATION_POSTAL_CODE);
        if (str != NULL)
          g_hash_table_insert (address, g_strdup ("postalcode"), str);

        str = get_dup_string (location, EMPATHY_LOCATION_LOCALITY);
        if (str != NULL)
          g_hash_table_insert (address, g_strdup ("locality"), str);

        str = get_dup_string (location, EMPATHY_LOCATION_STREET);
        if (str != NULL)
          g_hash_table_insert (address, g_strdup ("street"), str);

        g_object_set_data (G_OBJECT (contact), "geoclue-address", address);

        geoclue_geocode_address_to_position_async (window->geocode, address,
            map_view_geocode_cb, contact);
      }
#endif

  map_view_marker_update_position (CHAMPLAIN_MARKER (marker), contact);

  g_object_unref (contact);
  return FALSE;
}

static void
map_view_zoom_in_cb (GtkWidget *widget,
    EmpathyMapView *window)
{
  champlain_view_zoom_in (window->map_view);
}

static void
map_view_zoom_out_cb (GtkWidget *widget,
    EmpathyMapView *window)
{
  champlain_view_zoom_out (window->map_view);
}
