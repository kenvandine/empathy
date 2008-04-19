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

#include <config.h>

#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/dbus.h>

#include <extensions/extensions.h>

#include <libempathy/empathy-chandler.h>
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-utils.h>

#define DEBUG_DOMAIN "TubesChandler"

static GMainLoop *loop = NULL;


static void
async_cb (TpProxy *channel,
          const GError *error,
          gpointer user_data,
          GObject *weak_object)
{
  if (error)
      empathy_debug (DEBUG_DOMAIN, "Error %s: %s", user_data, error->message);
}

static void
new_tube_cb (TpChannel *channel,
             guint id,
             guint initiator,
             guint type,
             const gchar *service,
             GHashTable *parameters,
             guint state,
             gpointer user_data,
             GObject *weak_object)
{
  GHashTable *channels = (GHashTable *) user_data;
  TpProxy *connection;
  gchar *object_path;
  guint handle_type;
  guint handle;
  gpointer value;
  guint number;
  TpProxy *thandler;
  gchar *thandler_bus_name;
  gchar *thandler_object_path;

  /* Increase tube count */
  value = g_hash_table_lookup (channels, channel);
  number = GPOINTER_TO_UINT (value);
  g_hash_table_replace (channels, g_object_ref (channel),
      GUINT_TO_POINTER (++number));
  empathy_debug (DEBUG_DOMAIN, "Increased tube count for channel %p: %d",
      channel, number);

  /* We dispatch only local pending tubes */
  if (state != TP_TUBE_STATE_LOCAL_PENDING)
      return;

  /* Build the bus-name and object-path of the tube handler */
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
      return;

  empathy_debug (DEBUG_DOMAIN, "Dispatching channel %p id=%d",
      channel, id);

  /* Create the proxy for the tube handler */
  thandler = g_object_new (TP_TYPE_PROXY,
      "dbus-connection", tp_get_bus (),
      "bus-name", thandler_bus_name,
      "object-path", thandler_object_path,
      NULL);
  tp_proxy_add_interface_by_id (thandler, EMP_IFACE_QUARK_TUBE_HANDLER);

  /* Give the tube to the handler */
  g_object_get (channel,
      "connection", &connection,
      "object-path", &object_path,
      "handle_type", &handle_type,
      "handle", &handle,
      NULL);
  emp_cli_tube_handler_call_handle_tube (thandler, -1,
      connection->bus_name,
      connection->object_path,
      object_path, handle_type, handle, id,
      async_cb, "handling tube", NULL, NULL);

  g_free (thandler_bus_name);
  g_free (thandler_object_path);
  g_object_unref (thandler);
  g_object_unref (connection);
  g_free (object_path);
}

static void
list_tubes_cb (TpChannel *channel,
               const GPtrArray *tubes,
               const GError *error,
               gpointer user_data,
               GObject *weak_object)
{
  guint i;

  if (error)
    {
      empathy_debug (DEBUG_DOMAIN, "Error listing tubes: %s", error->message);
      return;
    }

  for (i = 0; i < tubes->len; i++)
    {
      GValueArray *values;

      values = g_ptr_array_index (tubes, i);
      new_tube_cb (channel,
          g_value_get_uint (g_value_array_get_nth (values, 0)),
          g_value_get_uint (g_value_array_get_nth (values, 1)),
          g_value_get_uint (g_value_array_get_nth (values, 2)),
          g_value_get_string (g_value_array_get_nth (values, 3)),
          g_value_get_boxed (g_value_array_get_nth (values, 4)),
          g_value_get_uint (g_value_array_get_nth (values, 5)),
          user_data, weak_object);
    }
}

static void
channel_invalidated_cb (TpProxy *proxy,
                        guint domain,
                        gint code,
                        gchar *message,
                        GHashTable *channels)
{
  empathy_debug (DEBUG_DOMAIN, "Channel invalidated: %p", proxy);
  g_hash_table_remove (channels, proxy);
  if (g_hash_table_size (channels) == 0)
      g_main_loop_quit (loop);
}

static void
tube_closed_cb (TpChannel *channel,
                guint id,
                gpointer user_data,
                GObject *weak_object)
{
  gpointer value;
  guint number;
  GHashTable *channels = (GHashTable *) user_data;

  value = g_hash_table_lookup (channels, channel);

  if (value)
    {
      number = GPOINTER_TO_UINT (value);

      if (number == 1)
        {
          empathy_debug (DEBUG_DOMAIN, "Ended tube count for channel %p, "
              "closing channel", channel);
          tp_cli_channel_call_close (channel, -1, NULL, NULL, NULL, NULL);
        }
      else if (number > 1)
        {
          g_hash_table_replace (channels, channel, GUINT_TO_POINTER (--number));
          empathy_debug (DEBUG_DOMAIN, "Decreased tube count for channel %p: %d",
              channel, number);
        }
    }
}

static void
new_channel_cb (EmpathyChandler *chandler,
                TpChannel *channel,
                GHashTable *channels)
{
  if (g_hash_table_lookup (channels, channel))
      return;

  empathy_debug (DEBUG_DOMAIN, "Handling new channel");

  g_hash_table_insert (channels, g_object_ref (channel), GUINT_TO_POINTER (0));

  g_signal_connect (channel, "invalidated",
      G_CALLBACK (channel_invalidated_cb),
      channels);

  tp_cli_channel_type_tubes_connect_to_tube_closed (channel,
      tube_closed_cb, channels, NULL, NULL, NULL);
  tp_cli_channel_type_tubes_connect_to_new_tube (channel,
      new_tube_cb, channels, NULL, NULL, NULL);
  tp_cli_channel_type_tubes_call_list_tubes (channel, -1,
      list_tubes_cb, channels, NULL, NULL);
}

static guint
channel_hash (gconstpointer key)
{
  TpProxy *channel = TP_PROXY (key);

  return g_str_hash (channel->object_path);
}

static gboolean
channel_equal (gconstpointer a,
               gconstpointer b)
{
  TpProxy *channel_a = TP_PROXY (a);
  TpProxy *channel_b = TP_PROXY (b);

  return g_str_equal (channel_a->object_path, channel_b->object_path);
}

int
main (int argc, char *argv[])
{
  EmpathyChandler *chandler;
  GHashTable *channels;

  g_type_init ();
  emp_cli_init ();

  channels = g_hash_table_new_full (channel_hash, channel_equal,
      g_object_unref, NULL);

  chandler = empathy_chandler_new ("org.gnome.Empathy.TubesChandler",
      "/org/gnome/Empathy/TubesChandler");
  g_signal_connect (chandler, "new-channel",
      G_CALLBACK (new_channel_cb), channels);

  empathy_debug (DEBUG_DOMAIN, "Ready to handle new tubes channels");

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  g_hash_table_destroy (channels);
  g_object_unref (chandler);

  return EXIT_SUCCESS;
}

