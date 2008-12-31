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

#include <config.h>

#include <sys/stat.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <champlain/champlain.h>
#include <champlain-gtk/champlain-gtk.h>
#include <clutter-gtk/gtk-clutter-embed.h>
#include <geoclue/geoclue-geocode.h>
#include <telepathy-glib/util.h>

#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-chatroom-manager.h>
#include <libempathy/empathy-chatroom.h>
#include <libempathy/empathy-contact-list.h>
#include <libempathy/empathy-contact-manager.h>
#include <libempathy/empathy-contact-factory.h>
#include <libempathy/empathy-location.h>
#include <libempathy/empathy-status-presets.h>

#include <libempathy-gtk/empathy-contact-dialogs.h>
#include <libempathy-gtk/empathy-contact-list-store.h>
#include <libempathy-gtk/empathy-contact-list-view.h>
#include <libempathy-gtk/empathy-presence-chooser.h>
#include <libempathy-gtk/empathy-ui-utils.h>
#include <libempathy-gtk/empathy-geometry.h>
#include <libempathy-gtk/empathy-conf.h>
#include <libempathy-gtk/empathy-log-window.h>
#include <libempathy-gtk/empathy-new-message-dialog.h>
#include <libempathy-gtk/empathy-gtk-enum-types.h>

#include "empathy-accounts-dialog.h"
#include "empathy-map-view.h"
#include "ephy-spinner.h"
#include "empathy-preferences.h"
#include "empathy-about-dialog.h"
#include "empathy-new-chatroom-dialog.h"
#include "empathy-chatrooms-window.h"
#include "empathy-event-manager.h"

#define DEBUG_FLAG EMPATHY_DEBUG_LOCATION
#include <libempathy/empathy-debug.h>

typedef struct {
  EmpathyContactListStore *list_store;

  GtkWidget *window;
  GtkWidget *zoom_in;
  GtkWidget *zoom_out;
  ChamplainView *map_view;
  ChamplainLayer *layer;

} EmpathyMapView;

static void map_view_destroy_cb (GtkWidget *widget,
    EmpathyMapView *window);
static gboolean map_view_contacts_foreach (GtkTreeModel *model,
    GtkTreePath *path, GtkTreeIter *iter, gpointer user_data);
static gboolean map_view_contacts_foreach_disconnect (GtkTreeModel *model,
    GtkTreePath *path, GtkTreeIter *iter, gpointer user_data);
static void map_view_zoom_in_cb (GtkWidget *widget, EmpathyMapView *window);
static void map_view_zoom_out_cb (GtkWidget *widget, EmpathyMapView *window);
static void map_view_contact_location_notify (GObject *gobject,
    GParamSpec *arg1, gpointer user_data);
static gchar * get_dup_string (GHashTable *location, gchar *key);

// FIXME: Make it so that only one window can be shown
GtkWidget *
empathy_map_view_show (EmpathyContactListStore *list_store)
{
  static EmpathyMapView *window = NULL;
  GtkBuilder *gui;
  GtkWidget *sw;
  GtkWidget *embed;
  gchar *filename;
  GtkTreeModel *model;

  /*
  if (window)
    {
      empathy_window_present (GTK_WINDOW (window->window), TRUE);
      return window->window;
    }
  */

  window = g_new0 (EmpathyMapView, 1);

  /* Set up interface */
  filename = empathy_file_lookup ("empathy-map-view.ui", "src");
  gui = empathy_builder_get_file (filename, "map_view",
      &window->window, "zoom_in", &window->zoom_in, "zoom_out",
      &window->zoom_out, "map_scrolledwindow", &sw, NULL);
  g_free (filename);

  empathy_builder_connect (gui, window, "map_view", "destroy",
      map_view_destroy_cb, "zoom_in", "clicked", map_view_zoom_in_cb,
      "zoom_out", "clicked", map_view_zoom_out_cb, NULL);

  g_object_unref (gui);

  window->list_store = list_store;

  /* Set up map view */
  window->map_view = CHAMPLAIN_VIEW (champlain_view_new (
      CHAMPLAIN_VIEW_MODE_KINETIC));
  g_object_set (G_OBJECT (window->map_view), "zoom-level", 1, NULL);
  champlain_view_center_on (window->map_view, 36, 0);

  embed = champlain_view_embed_new (window->map_view);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw),
     GTK_WIDGET (embed));
  gtk_widget_show_all (embed);

  window->layer = champlain_layer_new ();
  champlain_view_add_layer (CHAMPLAIN_VIEW (window->map_view), window->layer);

  /* Set up contact list. */
  model = GTK_TREE_MODEL (window->list_store);
  gtk_tree_model_foreach (model, map_view_contacts_foreach, window);

  empathy_window_present (GTK_WINDOW (window->window), TRUE);
  return window->window;
}

static gboolean
map_view_contacts_foreach_disconnect (GtkTreeModel *model,
                                      GtkTreePath *path,
                                      GtkTreeIter *iter,
                                      gpointer user_data)
{
  EmpathyContact *contact;
  guint handle_id;

  gtk_tree_model_get (model, iter, EMPATHY_CONTACT_LIST_STORE_COL_CONTACT,
     &contact, -1);
  if (!contact)
    return FALSE;

  handle_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (contact), "map-view-handle"));

  g_signal_handler_disconnect (contact, handle_id);
  return FALSE;
}

static void
map_view_destroy_cb (GtkWidget *widget,
                     EmpathyMapView *window)
{
  GtkTreeModel *model;

  /* Set up contact list. */
  model = GTK_TREE_MODEL (window->list_store);
  gtk_tree_model_foreach (model, map_view_contacts_foreach_disconnect, window);
  g_free (window);
}

#define GEOCODE_SERVICE "org.freedesktop.Geoclue.Providers.Yahoo"
#define GEOCODE_PATH "/org/freedesktop/Geoclue/Providers/Yahoo"

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
  gboolean found = FALSE;
  GHashTable *location = empathy_contact_get_location (EMPATHY_CONTACT (userdata));
  g_hash_table_ref (location);

  if (error)
  {
      return;
  }

  if (fields & GEOCLUE_POSITION_FIELDS_LONGITUDE)
    {
      new_value = tp_g_value_slice_new (G_TYPE_DOUBLE);
      g_value_set_double (new_value, longitude);
      g_hash_table_replace (location, EMPATHY_LOCATION_LON, new_value);
      DEBUG ("\t - Longitude: %f", longitude);
    }
  if (fields & GEOCLUE_POSITION_FIELDS_LATITUDE)
    {
      new_value = tp_g_value_slice_new (G_TYPE_DOUBLE);
      g_value_set_double (new_value, latitude);
      g_hash_table_replace (location, EMPATHY_LOCATION_LAT, new_value);
      DEBUG ("\t - Latitude: %f", latitude);
      found = TRUE;
    }
  if (fields & GEOCLUE_POSITION_FIELDS_ALTITUDE)
    {
      new_value = tp_g_value_slice_new (G_TYPE_DOUBLE);
      g_value_set_double (new_value, altitude);
      g_hash_table_replace (location, EMPATHY_LOCATION_ALT, new_value);
      DEBUG ("\t - Altitude: %f", altitude);
    }

  //Don't change the accuracy as we used an address to get this position
  if (found)
    empathy_contact_set_location (EMPATHY_CONTACT (userdata), location);
  g_hash_table_unref (location);
}


static gchar *
get_dup_string (GHashTable *location, gchar *key)
{
  GValue *value;

  value = g_hash_table_lookup (location, key);
  if (value)
  {
      return g_value_dup_string (value);
  }
  return NULL;

}

static void
map_view_marker_update (ChamplainMarker *marker,
                        EmpathyContact *contact)
{
  gdouble lon, lat;
  GValue *value;
  GHashTable *location = empathy_contact_get_location (contact);

  if (g_hash_table_size (location) == 0)
  {
    clutter_actor_hide (CLUTTER_ACTOR (marker));
    return;
  }

  value = g_hash_table_lookup (location, EMPATHY_LOCATION_LAT);
  if (value == NULL)
    {
      GeoclueGeocode * geocode = geoclue_geocode_new (GEOCODE_SERVICE,
          GEOCODE_PATH);
      gchar *str;

      GHashTable *address = geoclue_address_details_new();
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

      geoclue_geocode_address_to_position_async (geocode, address,
          map_view_geocode_cb, contact);
      clutter_actor_hide (CLUTTER_ACTOR (marker));
      return;
    }
  lat = g_value_get_double (value);

  value = g_hash_table_lookup (location, EMPATHY_LOCATION_LON);
  if (value == NULL)
    return;
  lon = g_value_get_double (value);

  clutter_actor_show (CLUTTER_ACTOR (marker));
  champlain_marker_set_position (marker, lat, lon);
}


static void
map_view_contact_location_notify (GObject *gobject,
                                  GParamSpec *arg1,
                                  gpointer user_data)
{
  ChamplainMarker *marker = CHAMPLAIN_MARKER (user_data);
  EmpathyContact *contact = EMPATHY_CONTACT (gobject);
  map_view_marker_update (marker, contact);
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
  GValue *value;
  GdkPixbuf *avatar;
  guint handle_id;

  gtk_tree_model_get (model, iter, EMPATHY_CONTACT_LIST_STORE_COL_CONTACT,
     &contact, -1);
  if (!contact)
    return FALSE;

  marker = champlain_marker_new ();

  handle_id = g_signal_connect (contact, "notify::location",
      G_CALLBACK (map_view_contact_location_notify), marker);
  g_object_set_data (G_OBJECT (contact), "map-view-handle",
      GINT_TO_POINTER (handle_id));

  texture = clutter_texture_new ();
  avatar = empathy_pixbuf_avatar_from_contact_scaled (contact, 32, 32);

  if (!avatar)
    goto cleanup;

  gtk_clutter_texture_set_from_pixbuf (CLUTTER_TEXTURE (texture), avatar);
  clutter_actor_set_position (CLUTTER_ACTOR (texture), 5, 5);

  clutter_container_add (CLUTTER_CONTAINER (marker), texture, NULL);
  clutter_actor_set_anchor_point (marker, 25, 50);

  map_view_marker_update (CHAMPLAIN_MARKER (marker), contact);

  clutter_container_add (CLUTTER_CONTAINER (window->layer), marker, NULL);

cleanup:
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
