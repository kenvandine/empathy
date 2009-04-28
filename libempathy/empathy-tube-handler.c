/*
 * Copyright (C) 2008 Collabora Ltd.
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 *          Elliot Fairweather <elliot.fairweather@collabora.co.uk>
 */

#include <config.h>

#include <dbus/dbus-glib.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#include <extensions/extensions.h>

#include "empathy-tp-tube.h"
#include "empathy-tube-handler.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include "empathy-debug.h"

static void empathy_tube_handler_iface_init (EmpSvcTubeHandlerClass *klass);

enum
{
  NEW_TUBE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (EmpathyTubeHandler, empathy_tube_handler,
    G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (EMP_TYPE_SVC_TUBE_HANDLER,
    empathy_tube_handler_iface_init))

typedef struct
{
  EmpathyTubeHandler *thandler;
  gchar *bus_name;
  gchar *connection;
  gchar *channel;
  guint handle_type;
  guint handle;
  EmpathyTpTube *tube;
} IdleData;

static void
tube_ready_cb (EmpathyTpTube *tube,
    const GError *error,
    gpointer user_data,
    GObject *weak_object)
{
  IdleData *idle_data = user_data;

  g_signal_emit (idle_data->thandler, signals[NEW_TUBE], 0, tube);
}

static void
tube_ready_destroy_notify (gpointer data)
{
  IdleData *idle_data = data;

  g_object_unref (idle_data->tube);
  g_free (idle_data->bus_name);
  g_free (idle_data->connection);
  g_free (idle_data->channel);
  g_slice_free (IdleData, idle_data);
}

static void
channel_ready_cb (TpChannel *channel,
                  const GError *error,
                  gpointer data)
{
  IdleData *idle_data = data;

  if (error != NULL)
    {
      DEBUG ("channel has been invalidated: %s", error->message);
      tube_ready_destroy_notify (data);
      g_object_unref (channel);
      return;
    }

  idle_data->tube = empathy_tp_tube_new (channel);
  empathy_tp_tube_call_when_ready (idle_data->tube, tube_ready_cb, idle_data,
      tube_ready_destroy_notify, NULL);

  g_object_unref (channel);
}

static void
connection_ready_cb (TpConnection *connection,
                     const GError *error,
                     gpointer data)
{
  TpChannel *channel;
  IdleData *idle_data = data;

  if (error != NULL)
    {
      DEBUG ("connection has been invalidated: %s", error->message);
      tube_ready_destroy_notify (data);
      g_object_unref (connection);
      return;
    }

  channel = tp_channel_new (connection, idle_data->channel,
      TP_IFACE_CHANNEL_TYPE_TUBES, idle_data->handle_type,
      idle_data->handle, NULL);
  tp_channel_call_when_ready (channel, channel_ready_cb, idle_data);

  g_object_unref (connection);
}

static gboolean
tube_handler_handle_tube_idle_cb (gpointer data)
{
  IdleData *idle_data = data;
  TpConnection *connection;
  static TpDBusDaemon *daemon = NULL;

  DEBUG ("New tube to be handled");

  if (!daemon)
    daemon = tp_dbus_daemon_new (tp_get_bus ());

  connection = tp_connection_new (daemon, idle_data->bus_name,
      idle_data->connection, NULL);
  tp_connection_call_when_ready (connection,
      connection_ready_cb, idle_data);

  return FALSE;
}

static void
tube_handler_handle_tube (EmpSvcTubeHandler *self,
                          const gchar *bus_name,
                          const gchar *connection,
                          const gchar *channel,
                          guint handle_type,
                          guint handle,
                          DBusGMethodInvocation *context)
{
  EmpathyTubeHandler *thandler = EMPATHY_TUBE_HANDLER (self);
  IdleData *data;

  data = g_slice_new (IdleData);
  data->thandler = thandler;
  data->bus_name = g_strdup (bus_name);
  data->connection = g_strdup (connection);
  data->channel = g_strdup (channel);
  data->handle_type = handle_type;
  data->handle = handle;

  g_idle_add_full (G_PRIORITY_HIGH, tube_handler_handle_tube_idle_cb,
      data, NULL);

  emp_svc_tube_handler_return_from_handle_tube (context);
}

static void
empathy_tube_handler_class_init (EmpathyTubeHandlerClass *klass)
{
  signals[NEW_TUBE] =
      g_signal_new ("new-tube", G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, EMPATHY_TYPE_TP_TUBE);
}

static void
empathy_tube_handler_iface_init (EmpSvcTubeHandlerClass *klass)
{
  emp_svc_tube_handler_implement_handle_tube (klass,
      tube_handler_handle_tube);
}

static void
empathy_tube_handler_init (EmpathyTubeHandler *thandler)
{
}

EmpathyTubeHandler *
empathy_tube_handler_new (TpTubeType type, const gchar *service)
{
  EmpathyTubeHandler *thandler = NULL;
  DBusGProxy *proxy;
  guint result;
  gchar *bus_name;
  gchar *object_path;
  GError *error = NULL;

  g_return_val_if_fail (type < NUM_TP_TUBE_TYPES, NULL);
  g_return_val_if_fail (service != NULL, NULL);

  bus_name = empathy_tube_handler_build_bus_name (type, service);
  object_path = empathy_tube_handler_build_object_path (type, service);

  proxy = dbus_g_proxy_new_for_name (tp_get_bus (), DBUS_SERVICE_DBUS,
      DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

  if (!dbus_g_proxy_call (proxy, "RequestName", &error,
      G_TYPE_STRING, bus_name, G_TYPE_UINT, DBUS_NAME_FLAG_DO_NOT_QUEUE,
      G_TYPE_INVALID, G_TYPE_UINT, &result, G_TYPE_INVALID))
    {
      DEBUG ("Failed to request name: %s",
          error ? error->message : "No error given");
      g_clear_error (&error);
      goto OUT;
    }

  DEBUG ("Creating tube handler %s", bus_name);
  thandler = g_object_new (EMPATHY_TYPE_TUBE_HANDLER, NULL);
  dbus_g_connection_register_g_object (tp_get_bus (), object_path,
      G_OBJECT (thandler));

OUT:
  g_object_unref (proxy);
  g_free (bus_name);
  g_free (object_path);

  return thandler;
}

static gchar *
sanitize_service_name (const gchar *str)
{
  guint i;
  gchar *result;

  g_assert (str != NULL);
  result = g_strdup (str);

  for (i = 0; result[i] != '\0'; i++)
    {
      if (!g_ascii_isalnum (result[i]))
        result[i] = '_';
    }

  return result;
}

gchar *
empathy_tube_handler_build_bus_name (TpTubeType type,
  const gchar *service)
{
  gchar *str = NULL;
  const gchar *prefix = NULL;
  gchar *service_escaped;

  g_return_val_if_fail (type < NUM_TP_TUBE_TYPES, NULL);
  g_return_val_if_fail (service != NULL, NULL);

  if (type == TP_TUBE_TYPE_DBUS)
    prefix = "org.gnome.Empathy.DTubeHandler.";
  else if (type == TP_TUBE_TYPE_STREAM)
    prefix = "org.gnome.Empathy.StreamTubeHandler.";
  else
    g_return_val_if_reached (NULL);

  service_escaped = sanitize_service_name (service);
  str = g_strconcat (prefix, service_escaped, NULL);
  g_free (service_escaped);

  return str;
}

gchar *
empathy_tube_handler_build_object_path (TpTubeType type,
  const gchar *service)
{
  gchar *bus_name;
  gchar *str;

  g_return_val_if_fail (type < NUM_TP_TUBE_TYPES, NULL);
  g_return_val_if_fail (service != NULL, NULL);

  bus_name = empathy_tube_handler_build_bus_name (type, service);
  str = g_strdelimit (g_strdup_printf ("/%s", bus_name), ".", '/');
  g_free (bus_name);

  return str;
}

