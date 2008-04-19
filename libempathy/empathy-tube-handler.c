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

#include <dbus/dbus-glib.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/interfaces.h>

#include <extensions/extensions.h>

#include "empathy-debug.h"
#include "empathy-tube.h"
#include "empathy-tube-handler.h"
#include "empathy-tubes.h"

#define DEBUG_DOMAIN "EmpathyTubeHandler"


enum
{
  NEW_TUBE,
  LAST_SIGNAL
};

static void empathy_tube_handler_iface_init (EmpSvcTubeHandlerClass *klass);
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
  guint id;
} IdleData;

static gboolean
empathy_tube_handler_handle_tube_idle_cb (gpointer data)
{
  IdleData *idle_data = data;
  TpConnection *connection;
  TpChannel *channel;
  EmpathyTubes *tubes;
  EmpathyTube *tube;
  static TpDBusDaemon *daemon = NULL;

  if (!daemon)
    daemon = tp_dbus_daemon_new (tp_get_bus ());

  connection = tp_connection_new (daemon, idle_data->bus_name,
      idle_data->connection, NULL);
  channel = tp_channel_new (connection, idle_data->channel,
      TP_IFACE_CHANNEL_TYPE_TUBES, idle_data->handle_type,
      idle_data->handle, NULL);
  tp_channel_run_until_ready (channel, NULL, NULL);

  tubes = empathy_tubes_new (channel);
  tube = empathy_tubes_get_tube (tubes, idle_data->id);

  empathy_debug (DEBUG_DOMAIN, "New tube to be handled");

  g_signal_emit (idle_data->thandler, signals[NEW_TUBE], 0, tube);

  g_object_unref (tube);
  g_object_unref (tubes);
  g_object_unref (channel);
  g_object_unref (connection);
  g_free (idle_data->bus_name);
  g_free (idle_data->connection);
  g_free (idle_data->channel);
  g_slice_free (IdleData, idle_data);

  return FALSE;
}


static void
empathy_tube_handler_handle_tube (EmpSvcTubeHandler *self,
                                  const gchar *bus_name,
                                  const gchar *connection,
                                  const gchar *channel,
                                  guint handle_type,
                                  guint handle,
                                  guint id,
                                  DBusGMethodInvocation *context)
{
  EmpathyTubeHandler *thandler = EMPATHY_TUBE_HANDLER (self);
  IdleData *data = g_slice_new (IdleData);

  data->thandler = thandler;
  data->bus_name = g_strdup (bus_name);
  data->connection = g_strdup (connection);
  data->channel = g_strdup (channel);
  data->handle_type = handle_type;
  data->handle = handle;
  data->id = id;

  g_idle_add_full (G_PRIORITY_HIGH, empathy_tube_handler_handle_tube_idle_cb,
      data, NULL);

  emp_svc_tube_handler_return_from_handle_tube (context);
}


static void
empathy_tube_handler_class_init (EmpathyTubeHandlerClass *klass)
{
  signals[NEW_TUBE] =
      g_signal_new ("new-tube", G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
      G_TYPE_NONE, 1, EMPATHY_TYPE_TUBE);
}


static void
empathy_tube_handler_iface_init (EmpSvcTubeHandlerClass *klass)
{
  emp_svc_tube_handler_implement_handle_tube (klass,
      empathy_tube_handler_handle_tube);
}


static void
empathy_tube_handler_init (EmpathyTubeHandler *thandler)
{
}


EmpathyTubeHandler *
empathy_tube_handler_new (const gchar *bus_name,
                          const gchar *object_path)
{
  EmpathyTubeHandler *thandler;
  DBusGProxy *proxy;
  guint result;
  GError *error = NULL;

  proxy = dbus_g_proxy_new_for_name (tp_get_bus (), DBUS_SERVICE_DBUS,
      DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

  if (!dbus_g_proxy_call (proxy, "RequestName", &error,
      G_TYPE_STRING, bus_name, G_TYPE_UINT, DBUS_NAME_FLAG_DO_NOT_QUEUE,
      G_TYPE_INVALID, G_TYPE_UINT, &result, G_TYPE_INVALID))
    {
      empathy_debug (DEBUG_DOMAIN, "Failed to request name: %s",
          error ? error->message : "No error given");
      g_clear_error (&error);
      return NULL;
    }

  g_object_unref (proxy);

  thandler = g_object_new (EMPATHY_TYPE_TUBE_HANDLER, NULL);
  dbus_g_connection_register_g_object (tp_get_bus (), object_path,
      G_OBJECT (thandler));

  return thandler;
}
