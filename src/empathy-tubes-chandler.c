/*
 *  Copyright (C) 2008 Collabora Ltd.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Authors: Elliot Fairweather <elliot.fairweather@collabora.co.uk>
 */


#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/dbus.h>

#include <extensions/extensions.h>

#include <libempathy/empathy-chandler.h>
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-tube.h>
#include <libempathy/empathy-tubes.h>
#include <libempathy/empathy-utils.h>

#define DEBUG_DOMAIN "EmpathyTubesChandler"

static void
empathy_tubes_chandler_async_cb (TpProxy *proxy,
                                 const GError *error,
                                 gpointer data,
                                 GObject *weak_object)
{
  if (error)
    empathy_debug (DEBUG_DOMAIN, "Error %s: %s", data, error->message);
}


static void
empathy_tubes_chandler_new_tube_cb (EmpathyTubes *tubes,
                                    EmpathyTube *tube,
                                    gpointer data)
{
  GHashTable *channels = (GHashTable *) data;
  TpChannel *channel;
  TpConnection *connection;
  gchar *channel_path;
  gchar *connection_name;
  gchar *connection_path;
  guint handle_type;
  guint handle;
  gpointer value;
  guint number;

  TpProxy *thandler;
  gchar *thandler_bus_name;
  gchar *thandler_object_path;

  guint id;
  guint type;
  gchar *service;
  guint state;

  g_object_get (G_OBJECT (tubes), "channel", &channel, NULL);
  g_object_get (G_OBJECT (channel), "connection", &connection, "object-path",
      &channel_path, NULL);

  value = g_hash_table_lookup (channels, channel_path);

  if (!value)
    {
      g_hash_table_insert (channels, g_strdup (channel_path),
          (GUINT_TO_POINTER (1)));
      empathy_debug (DEBUG_DOMAIN,
          "Started tube count for channel %s - total: 1", channel_path);
    }
  else
    {
      number = GPOINTER_TO_UINT (value);
      g_hash_table_replace (channels, g_strdup (channel_path),
          GUINT_TO_POINTER (++number));
      empathy_debug (DEBUG_DOMAIN,
          "Increased tube count for channel %s - total: %d",
          channel_path, number);
    }

  g_object_get (G_OBJECT (tube), "state", &state, NULL);

  if (state != TP_TUBE_STATE_LOCAL_PENDING)
    {
      g_free (channel_path);
      g_object_unref (channel);
      g_object_unref (connection);
      return;
    }

  g_object_get (G_OBJECT (tube), "service", &service, "type", &type, NULL);

  if (type == TP_TUBE_TYPE_DBUS)
    {
      thandler_bus_name =
          g_strdup_printf ("org.gnome.Empathy.DTube.%s", service);
      thandler_object_path =
          g_strdup_printf ("/org/gnome/Empathy/DTube/%s", service);
    }
  else if (type == TP_TUBE_TYPE_STREAM)
    {
      thandler_bus_name =
          g_strdup_printf ("org.gnome.Empathy.StreamTube.%s", service);
      thandler_object_path =
          g_strdup_printf ("/org/gnome/Empathy/StreamTube/%s", service);
    }
  else
    {
      g_free (channel_path);
      g_object_unref (channel);
      g_object_unref (connection);
      return;
    }

  thandler = g_object_new (TP_TYPE_PROXY, "bus-name", thandler_bus_name,
      "dbus-connection", tp_get_bus (), "object-path", thandler_object_path,
      NULL);
  tp_proxy_add_interface_by_id (thandler, EMP_IFACE_QUARK_TUBE_HANDLER);

  g_object_get (G_OBJECT (tube), "id", &id, NULL);
  g_object_get (G_OBJECT (channel), "handle-type", &handle_type,
      "handle", &handle, NULL);
  g_object_get (G_OBJECT (connection), "bus-name", &connection_name,
      "object-path", &connection_path, NULL);

  empathy_debug (DEBUG_DOMAIN, "Dispatching new tube to %s",
      thandler_bus_name);

  emp_cli_tube_handler_call_handle_tube (thandler, -1, connection_name,
      connection_path, channel_path, handle_type, handle, id,
      empathy_tubes_chandler_async_cb, "handling tube", NULL, NULL);

  g_free (connection_path);
  g_free (connection_name);
  g_free (service);

  g_free (thandler_bus_name);
  g_free (thandler_object_path);
  g_object_unref (thandler);

  g_free (channel_path);
  g_object_unref (channel);
  g_object_unref (connection);
}


static void
empathy_tubes_chandler_tube_closed_cb (EmpathyTubes *tubes,
                                       guint tube_id,
                                       gpointer data)
{
  TpChannel *channel;
  gchar *channel_path;
  gpointer value;
  guint number;
  GHashTable *channels = (GHashTable *) data;

  g_object_get (G_OBJECT (tubes), "channel", &channel, NULL);
  g_object_get (G_OBJECT (channel), "object-path", &channel_path, NULL);

  value = g_hash_table_lookup (channels, channel_path);

  if (value)
    {
      number = GPOINTER_TO_UINT (value);

      if (number == 1)
        {
          g_hash_table_remove (channels, channel_path);
          empathy_tubes_close (tubes);
          empathy_debug (DEBUG_DOMAIN,
              "Ended tube count for channel %s - total: 0", channel_path);
          empathy_debug (DEBUG_DOMAIN, "Closing channel");
        }
      else if (number > 1)
        {
          g_hash_table_replace (channels, channel_path,
              GUINT_TO_POINTER (--number));
          empathy_debug (DEBUG_DOMAIN,
              "Decreased tube count for channel %s - total: %d",
              channel_path, number);
        }
    }

  g_free (channel_path);
  g_object_unref (channel);
}


static void
empathy_tubes_new_channel_cb (EmpathyChandler *chandler,
                              TpChannel *channel,
                              gpointer data)
{
  EmpathyTubes *tubes = empathy_tubes_new (channel);
  GHashTable *channels = (GHashTable *) data;
  GSList *tube_list, *list;

  empathy_debug (DEBUG_DOMAIN, "Handling new channel");

  g_signal_connect (G_OBJECT (tubes), "new-tube",
      G_CALLBACK (empathy_tubes_chandler_new_tube_cb), (gpointer) channels);
  g_signal_connect (G_OBJECT (tubes), "tube-closed",
      G_CALLBACK (empathy_tubes_chandler_tube_closed_cb), (gpointer) channels);

  tube_list = empathy_tubes_list_tubes (tubes);

  for (list = tube_list; list != NULL; list = g_slist_next (list))
    {
      EmpathyTube *tube = EMPATHY_TUBE (list->data);
      empathy_tubes_chandler_new_tube_cb (tubes, tube, (gpointer) channels);
      g_object_unref (tube);
    }

  g_slist_free (tube_list);
}


int main (int argc, char *argv[])
{
  EmpathyChandler *chandler;
  GMainLoop *loop;
  GHashTable *channels;

  g_type_init ();
  emp_cli_init ();

  channels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  chandler = empathy_chandler_new ("org.gnome.Empathy.TubesChandler",
      "/org/gnome/Empathy/TubesChandler");
  g_signal_connect (chandler, "new-channel",
      G_CALLBACK (empathy_tubes_new_channel_cb), (gpointer) channels);

  empathy_debug (DEBUG_DOMAIN, "Ready to handle new tubes channels");

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_hash_table_unref (channels);
  g_object_unref (chandler);

  return 0;
}
