/*
 * empathy-tube-dispatch.c - Source for EmpathyTubeDispatch
 * Copyright (C) 2008 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
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
 */


#include <stdio.h>
#include <stdlib.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/proxy-subclass.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>


#include <libempathy/empathy-tube-handler.h>
#include <extensions/extensions.h>


#include "empathy-tube-dispatch.h"
#include "empathy-tube-dispatch-enumtypes.h"

#define DEBUG_FLAG EMPATHY_DEBUG_DISPATCHER
#include <libempathy/empathy-debug.h>

G_DEFINE_TYPE(EmpathyTubeDispatch, empathy_tube_dispatch, G_TYPE_OBJECT)

static void empathy_tube_dispatch_set_ability (
  EmpathyTubeDispatch *tube_dispatch,
  EmpathyTubeDispatchAbility dispatchability);

/* private structure */
typedef struct _EmpathyTubeDispatchPriv EmpathyTubeDispatchPriv;

/* properties */
enum {
    PROP_OPERATION = 1,
    PROP_DISPATCHABILITY
};


struct _EmpathyTubeDispatchPriv
{
  gboolean dispose_has_run;
  EmpathyDispatchOperation *operation;
  EmpathyTubeDispatchAbility dispatchability;
  gchar *service;
  gchar *bus_name;
  gchar *object_path;
  TpDBusDaemon *dbus;
};

#define GET_PRIV(o) \
 (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
  EMPATHY_TYPE_TUBE_DISPATCH, EmpathyTubeDispatchPriv))

static void
empathy_tube_dispatch_init (EmpathyTubeDispatch *obj)
{
  EmpathyTubeDispatchPriv *priv = GET_PRIV (obj);

  priv->dispatchability = EMPATHY_TUBE_DISPATCHABILITY_UNKNOWN;
}

static void empathy_tube_dispatch_dispose (GObject *object);
static void empathy_tube_dispatch_finalize (GObject *object);

static void
empathy_tube_dispatch_list_activatable_names_cb (TpDBusDaemon *proxy,
  const gchar **names, const GError *error, gpointer user_data,
  GObject *object)
{
  EmpathyTubeDispatch *self = EMPATHY_TUBE_DISPATCH (object);
  EmpathyTubeDispatchPriv *priv = GET_PRIV (self);
  gchar **name;

  for (name = (gchar **) names; *name != NULL; name++)
    {
      if (!tp_strdiff (*name, priv->bus_name))
        {
          DEBUG ("Found tube handler. Can dispatch it");
          empathy_tube_dispatch_set_ability (self,
            EMPATHY_TUBE_DISPATCHABILITY_POSSIBLE);
          return;
        }
    }

  DEBUG ("Didn't find tube handler. Can't dispatch it");
  empathy_tube_dispatch_set_ability (self,
    EMPATHY_TUBE_DISPATCHABILITY_IMPOSSIBLE);
}

static void
empathy_tube_dispatch_name_has_owner_cb (TpDBusDaemon *proxy,
  gboolean has_owner, const GError *error, gpointer user_data,
  GObject *object)
{
  EmpathyTubeDispatch *self = EMPATHY_TUBE_DISPATCH (object);
  EmpathyTubeDispatchPriv *priv = GET_PRIV (self);

  if (error != NULL)
    {
      DEBUG ("NameHasOwner failed. Can't dispatch tube");
      empathy_tube_dispatch_set_ability (self,
        EMPATHY_TUBE_DISPATCHABILITY_IMPOSSIBLE);
      return;
    }

  if (has_owner)
    {
      DEBUG ("Tube handler is running. Can dispatch it");
      empathy_tube_dispatch_set_ability (self,
        EMPATHY_TUBE_DISPATCHABILITY_POSSIBLE);
    }
  else
    {
      DEBUG ("Tube handler is not running. Calling ListActivatableNames");
      tp_cli_dbus_daemon_call_list_activatable_names (priv->dbus, -1,
        empathy_tube_dispatch_list_activatable_names_cb, NULL, NULL,
        G_OBJECT (self));
    }
}

static void
empathy_tube_dispatch_constructed (GObject *object)
{
  EmpathyTubeDispatch *self = EMPATHY_TUBE_DISPATCH (object);
  EmpathyTubeDispatchPriv *priv = GET_PRIV (self);
  TpChannel *channel;
  GHashTable *properties;
  const gchar *service;
  const gchar *channel_type;
  TpTubeType type;

  priv->dbus = tp_dbus_daemon_new (tp_get_bus());

  channel = empathy_dispatch_operation_get_channel (priv->operation);
  properties = tp_channel_borrow_immutable_properties (channel);

  channel_type = tp_asv_get_string (properties,
    TP_IFACE_CHANNEL ".ChannelType");
  if (channel_type == NULL)
    goto failed;

  if (!tp_strdiff (channel_type, EMP_IFACE_CHANNEL_TYPE_STREAM_TUBE))
    {
      type = TP_TUBE_TYPE_STREAM;
      service = tp_asv_get_string (properties,
        EMP_IFACE_CHANNEL_TYPE_STREAM_TUBE  ".Service");
    }
  else if (!tp_strdiff (channel_type, EMP_IFACE_CHANNEL_TYPE_DBUS_TUBE))
    {
      type = TP_TUBE_TYPE_DBUS;
      service = tp_asv_get_string (properties,
        EMP_IFACE_CHANNEL_TYPE_DBUS_TUBE  ".ServiceName");
    }
  else
    {
      goto failed;
    }


  if (service == NULL)
    goto failed;

  priv->bus_name = empathy_tube_handler_build_bus_name (type, service);
  priv->object_path = empathy_tube_handler_build_object_path (type, service);

  priv->service = g_strdup (service);

  DEBUG ("Look for tube handler %s\n", priv->bus_name);
  tp_cli_dbus_daemon_call_name_has_owner (priv->dbus, -1, priv->bus_name,
    empathy_tube_dispatch_name_has_owner_cb, NULL, NULL, G_OBJECT (self));

  return;

failed:
  empathy_tube_dispatch_set_ability (self,
    EMPATHY_TUBE_DISPATCHABILITY_IMPOSSIBLE);
}

static void
empathy_tube_dispatch_set_property (GObject *object,
  guint property_id, const GValue *value, GParamSpec *pspec)
{
  EmpathyTubeDispatch *tube_dispatch = EMPATHY_TUBE_DISPATCH (object);
  EmpathyTubeDispatchPriv *priv = GET_PRIV (tube_dispatch);

  switch (property_id)
    {
      case PROP_OPERATION:
        priv->operation = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_tube_dispatch_get_property (GObject *object,
  guint property_id, GValue *value, GParamSpec *pspec)
{
  EmpathyTubeDispatch *tube_dispatch = EMPATHY_TUBE_DISPATCH (object);
  EmpathyTubeDispatchPriv *priv = GET_PRIV (tube_dispatch);

  switch (property_id)
    {
      case PROP_OPERATION:
        g_value_set_object (value, priv->operation);
        break;
      case PROP_DISPATCHABILITY:
        g_value_set_enum (value, priv->dispatchability);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
empathy_tube_dispatch_class_init (
  EmpathyTubeDispatchClass *empathy_tube_dispatch_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (empathy_tube_dispatch_class);
  GParamSpec *param_spec;

  g_type_class_add_private (empathy_tube_dispatch_class,
    sizeof (EmpathyTubeDispatchPriv));

  object_class->set_property = empathy_tube_dispatch_set_property;
  object_class->get_property = empathy_tube_dispatch_get_property;

  object_class->constructed = empathy_tube_dispatch_constructed;
  object_class->dispose = empathy_tube_dispatch_dispose;
  object_class->finalize = empathy_tube_dispatch_finalize;

  param_spec = g_param_spec_object ("operation",
    "operation", "The telepathy connection",
    EMPATHY_TYPE_DISPATCH_OPERATION,
    G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_OPERATION, param_spec);

  param_spec = g_param_spec_enum ("dispatchability",
    "dispatchability",
    "Whether or not there is a handler to dispatch the operation to",
    EMPATHY_TYPE_TUBE_DISPATCH_ABILITY, EMPATHY_TUBE_DISPATCHABILITY_UNKNOWN,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_DISPATCHABILITY,
    param_spec);

}

void
empathy_tube_dispatch_dispose (GObject *object)
{
  EmpathyTubeDispatch *self = EMPATHY_TUBE_DISPATCH (object);
  EmpathyTubeDispatchPriv *priv = GET_PRIV (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */
  if (priv->operation != NULL)
    g_object_unref (priv->operation);

  priv->operation = NULL;

  if (priv->dbus != NULL)
    g_object_unref (priv->dbus);

  priv->dbus = NULL;


  if (G_OBJECT_CLASS (empathy_tube_dispatch_parent_class)->dispose)
    G_OBJECT_CLASS (empathy_tube_dispatch_parent_class)->dispose (object);
}

void
empathy_tube_dispatch_finalize (GObject *object)
{
  EmpathyTubeDispatch *self = EMPATHY_TUBE_DISPATCH (object);
  EmpathyTubeDispatchPriv *priv = GET_PRIV (self);

  g_free (priv->bus_name);
  g_free (priv->object_path);
  g_free (priv->service);

  /* free any data held directly by the object here */

  G_OBJECT_CLASS (empathy_tube_dispatch_parent_class)->finalize (object);
}

EmpathyTubeDispatch *
empathy_tube_dispatch_new (EmpathyDispatchOperation *operation)
{
  return EMPATHY_TUBE_DISPATCH (g_object_new (EMPATHY_TYPE_TUBE_DISPATCH,
      "operation", operation, NULL));
}

EmpathyTubeDispatchAbility
empathy_tube_dispatch_is_dispatchable (EmpathyTubeDispatch *tube_dispatch)
{
  EmpathyTubeDispatchPriv *priv = GET_PRIV (tube_dispatch);

  return priv->dispatchability;
}

static void
empathy_tube_dispatch_set_ability (EmpathyTubeDispatch *tube_dispatch,
  EmpathyTubeDispatchAbility dispatchability)
{
  EmpathyTubeDispatchPriv *priv = GET_PRIV (tube_dispatch);

  if (priv->dispatchability == dispatchability)
    return;

  priv->dispatchability = dispatchability;
  g_object_notify (G_OBJECT (tube_dispatch), "dispatchability");
}

static void
empathy_tube_dispatch_show_error (EmpathyTubeDispatch *self, gchar *message)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
      GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, "%s", message);

  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
}

static void
empathy_tube_dispatch_handle_tube_cb (TpProxy *proxy, const GError *error,
  gpointer user_data, GObject *object)
{
  EmpathyTubeDispatch *self = EMPATHY_TUBE_DISPATCH (object);
  EmpathyTubeDispatchPriv *priv = GET_PRIV (self);

  if (error != NULL)
    {
      gchar *msg = g_strdup_printf (
        _("Unable to start application for service %s: %s"),
          priv->service,  error->message);

      empathy_tube_dispatch_show_error (self, msg);
      g_free (msg);
    }

  /* Remove the ref we were holding because of the dispatching */
  g_object_unref (object);
}

static void
empathy_tube_do_dispatch (EmpathyTubeDispatch *self)
{
  EmpathyTubeDispatchPriv *priv = GET_PRIV (self);
  TpChannel *channel;
  TpProxy *connection;
  TpProxy *thandler;
  gchar   *object_path;
  guint    handle_type;
  guint    handle;

  channel = empathy_dispatch_operation_get_channel (priv->operation);

  /* Create the proxy for the tube handler */
  thandler = g_object_new (TP_TYPE_PROXY,
    "dbus-connection", tp_get_bus (),
    "bus-name", priv->bus_name,
    "object-path", priv->object_path,
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
    connection->bus_name, connection->object_path,
    object_path, handle_type, handle,
    empathy_tube_dispatch_handle_tube_cb, NULL, NULL, G_OBJECT (self));

  g_object_unref (thandler);
  g_object_unref (connection);
  g_free (object_path);
}

void
empathy_tube_dispatch_handle (EmpathyTubeDispatch *tube_dispatch)
{
  EmpathyTubeDispatchPriv *priv = GET_PRIV (tube_dispatch);

  /* Keep ourselves alive untill the dispatching is finished */
  g_object_ref (tube_dispatch);

  /* If we can't claim it, don't do anything */
  if (!empathy_dispatch_operation_claim (priv->operation))
    goto done;

  if (priv->dispatchability != EMPATHY_TUBE_DISPATCHABILITY_POSSIBLE)
    {
      gchar *msg;
      TpChannel *channel;

      channel = empathy_dispatch_operation_get_channel (priv->operation);

      msg = g_strdup_printf (
        _("An invitation was offered for service %s, but you don't have the "
          "needed application to handle it"), priv->service);

      empathy_tube_dispatch_show_error (tube_dispatch, msg);

      g_free (msg);

      tp_cli_channel_call_close (channel, -1, NULL, NULL, NULL, NULL);

      goto done;
    }
  else
    {
      empathy_tube_do_dispatch (tube_dispatch);
    }

  return;
done:
  g_object_unref (tube_dispatch);
}

