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
#include "ephy-spinner.h"

#define DEBUG_FLAG EMPATHY_DEBUG_LOCATION
#include <libempathy/empathy-debug.h>

typedef struct {
  EmpathyContactListStore *list_store;

  GtkWidget *window;
  GtkWidget *zoom_in;
  GtkWidget *zoom_out;
  GtkWidget *throbber;
  ChamplainView *map_view;
  ChamplainLayer *layer;
} EmpathyMapView;

static void
map_view_state_changed (ChamplainView *view,
    GParamSpec *gobject,
    EmpathyMapView *window)
{
  ChamplainState state;

  g_object_get (G_OBJECT (view), "state", &state, NULL);
  if (state == CHAMPLAIN_STATE_LOADING)
    ephy_spinner_start (EPHY_SPINNER (window->throbber));
  else
    ephy_spinner_stop (EPHY_SPINNER (window->throbber));
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
map_view_contact_location_notify (EmpathyContact *contact,
    GParamSpec *arg1,
    ChamplainMarker *marker)
{
  map_view_marker_update_position (marker, contact);
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

static gboolean
marker_clicked_cb (ChamplainMarker *marker,
    ClutterButtonEvent *event,
    EmpathyContact *contact)
{
  GtkWidget *menu;

  if (event->button != 3)
    return FALSE;

  menu = empathy_contact_menu_new (contact,
      EMPATHY_CONTACT_FEATURE_CHAT |
      EMPATHY_CONTACT_FEATURE_CALL |
      EMPATHY_CONTACT_FEATURE_LOG |
      EMPATHY_CONTACT_FEATURE_INFO);

  if (menu == NULL)
    return FALSE;

  gtk_widget_show (menu);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
      event->button, event->time);

  return FALSE;
}

static gboolean
map_view_contacts_foreach (GtkTreeModel *model,
    GtkTreePath *path,
    GtkTreeIter *iter,
    gpointer user_data)
{
  EmpathyMapView *window = (EmpathyMapView *) user_data;
  EmpathyContact *contact;
  ClutterActor *marker;
  ClutterActor *texture;
  GHashTable *location;
  GdkPixbuf *avatar;
  const gchar *name;
  gchar *date;
  gchar *label;
  GValue *gtime;
  time_t time;

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
      g_object_unref (avatar);
    }
  else
    champlain_marker_set_image (CHAMPLAIN_MARKER (marker), NULL);

  name = empathy_contact_get_name (contact);
  gtime = g_hash_table_lookup (location, EMPATHY_LOCATION_TIMESTAMP);
  if (gtime != NULL)
    {
      time = g_value_get_int64 (gtime);
      date = empathy_time_to_string_relative (time);
      label = g_strconcat ("<b>", name, "</b>\n<small>", date, "</small>", NULL);
      g_free (date);
    }
  else
    {
      label = g_strconcat ("<b>", name, "</b>\n", NULL);
    }
  champlain_marker_set_use_markup (CHAMPLAIN_MARKER (marker), TRUE);
  champlain_marker_set_text (CHAMPLAIN_MARKER (marker), label);
  g_free (label);

  clutter_actor_set_reactive (CLUTTER_ACTOR (marker), TRUE);
  g_signal_connect (marker, "button-release-event",
      G_CALLBACK (marker_clicked_cb), contact);

  clutter_container_add (CLUTTER_CONTAINER (window->layer), marker, NULL);

  g_signal_connect (contact, "notify::location",
      G_CALLBACK (map_view_contact_location_notify), marker);
  g_object_set_data_full (G_OBJECT (marker), "contact",
      g_object_ref (contact), g_object_unref);

  map_view_marker_update_position (CHAMPLAIN_MARKER (marker), contact);

  g_object_unref (contact);
  return FALSE;
}

static void
map_view_destroy_cb (GtkWidget *widget,
    EmpathyMapView *window)
{
  GList *item;

  item = clutter_container_get_children (CLUTTER_CONTAINER (window->layer));
  while (item != NULL)
  {
    EmpathyContact *contact;
    ChamplainMarker *marker;

    marker = CHAMPLAIN_MARKER (item->data);
    contact = g_object_get_data (G_OBJECT (marker), "contact");
    g_signal_handlers_disconnect_by_func (contact,
        map_view_contact_location_notify, marker);

    item = g_list_next (item);
  }

  g_object_unref (window->list_store);
  g_object_unref (window->layer);
  g_slice_free (EmpathyMapView, window);
}

GtkWidget *
empathy_map_view_show (void)
{
  static EmpathyMapView *window = NULL;
  GtkBuilder *gui;
  GtkWidget *sw;
  GtkWidget *embed;
  GtkWidget *throbber_holder;
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
     "throbber", &throbber_holder,
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

  window->throbber = ephy_spinner_new ();
  ephy_spinner_set_size (EPHY_SPINNER (window->throbber),
      GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_show (window->throbber);
  gtk_container_add (GTK_CONTAINER (throbber_holder), window->throbber);

  window->list_store = list_store;

  /* Set up map view */
  embed = gtk_champlain_embed_new ();
  window->map_view = gtk_champlain_embed_get_view (GTK_CHAMPLAIN_EMBED (embed));
  g_object_set (G_OBJECT (window->map_view), "zoom-level", 1,
     "scroll-mode", CHAMPLAIN_SCROLL_MODE_KINETIC, NULL);
  champlain_view_center_on (window->map_view, 36, 0);

  gtk_container_add (GTK_CONTAINER (sw), embed);
  gtk_widget_show_all (embed);

  window->layer = g_object_ref (champlain_layer_new ());
  champlain_view_add_layer (window->map_view, window->layer);

  g_signal_connect (window->map_view, "notify::state",
      G_CALLBACK (map_view_state_changed), window);

  /* Set up contact list. */
  model = GTK_TREE_MODEL (window->list_store);
  gtk_tree_model_foreach (model, map_view_contacts_foreach, window);

  empathy_window_present (GTK_WINDOW (window->window), TRUE);
  return window->window;
}

