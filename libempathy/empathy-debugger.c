/*
 * Telepathy debug interface implementation
 * Copyright (C) 2009 Collabora Ltd.
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

#include "empathy-debugger.h"
#include "config.h"

#include <telepathy-glib/dbus.h>

#include "extensions/extensions.h"

static EmpathyDebugger *singleton = NULL;

static void
debug_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (EmpathyDebugger, empathy_debugger, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
        tp_dbus_properties_mixin_iface_init);
    G_IMPLEMENT_INTERFACE (EMP_TYPE_SVC_DEBUG, debug_iface_init));

/* properties */
enum
{
  PROP_ENABLED = 1,
  NUM_PROPERTIES
};

static EmpDebugLevel
log_level_flags_to_debug_level (GLogLevelFlags level)
{
  if (level & G_LOG_LEVEL_ERROR)
    return EMP_DEBUG_LEVEL_ERROR;
  else if (level & G_LOG_LEVEL_CRITICAL)
    return EMP_DEBUG_LEVEL_CRITICAL;
  else if (level & G_LOG_LEVEL_WARNING)
    return EMP_DEBUG_LEVEL_WARNING;
  else if (level & G_LOG_LEVEL_MESSAGE)
    return EMP_DEBUG_LEVEL_MESSAGE;
  else if (level & G_LOG_LEVEL_INFO)
    return EMP_DEBUG_LEVEL_INFO;
  else if (level & G_LOG_LEVEL_DEBUG)
    return EMP_DEBUG_LEVEL_DEBUG;
  else
    /* Fall back to DEBUG if all else fails */
    return EMP_DEBUG_LEVEL_DEBUG;
}

static EmpathyDebugMessage *
debug_message_new (GTimeVal *timestamp,
    const gchar *domain,
    GLogLevelFlags level,
    const gchar *string)
{
  EmpathyDebugMessage *msg;

  msg = g_slice_new0 (EmpathyDebugMessage);
  msg->timestamp = timestamp->tv_sec + timestamp->tv_usec / 1e6;
  msg->domain = g_strdup (domain);
  msg->level = log_level_flags_to_debug_level (level);
  msg->string = g_strdup (string);
  return msg;
}

static void
debug_message_free (EmpathyDebugMessage *msg)
{
  g_free (msg->domain);
  g_free (msg->string);
  g_slice_free (EmpathyDebugMessage, msg);
}

static void
empathy_debugger_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  EmpathyDebugger *self = EMPATHY_DEBUGGER (object);

  switch (property_id)
    {
      case PROP_ENABLED:
        g_value_set_boolean (value, self->enabled);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
empathy_debugger_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  EmpathyDebugger *self = EMPATHY_DEBUGGER (object);

  switch (property_id)
    {
      case PROP_ENABLED:
        self->enabled = g_value_get_boolean (value);
        break;

     default:
       G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
empathy_debugger_finalize (GObject *object)
{
  EmpathyDebugger *self = EMPATHY_DEBUGGER (object);

  g_queue_foreach (self->messages, (GFunc) debug_message_free, NULL);
  g_queue_free (self->messages);
  self->messages = NULL;

  G_OBJECT_CLASS (empathy_debugger_parent_class)->finalize (object);
}

static void
empathy_debugger_class_init (EmpathyDebuggerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  static TpDBusPropertiesMixinPropImpl debug_props[] = {
      { "Enabled", "enabled", "enabled" },
      { NULL }
  };
  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
      { EMP_IFACE_DEBUG,
        tp_dbus_properties_mixin_getter_gobject_properties,
        tp_dbus_properties_mixin_setter_gobject_properties,
        debug_props,
      },
      { NULL }
  };

  object_class->get_property = empathy_debugger_get_property;
  object_class->set_property = empathy_debugger_set_property;
  object_class->finalize = empathy_debugger_finalize;

  g_object_class_install_property (object_class, PROP_ENABLED,
      g_param_spec_boolean ("enabled", "Enabled?",
          "True if the new-debug-message signal is enabled.",
          FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  klass->dbus_props_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (EmpathyDebuggerClass, dbus_props_class));
}

static void
get_messages (EmpSvcDebug *self,
    DBusGMethodInvocation *context)
{
  EmpathyDebugger *dbg = EMPATHY_DEBUGGER (self);
  GPtrArray *messages;
  static GType struct_type = 0;
  GList *i;
  guint j;

  if (G_UNLIKELY (struct_type == 0))
    {
      struct_type = dbus_g_type_get_struct (
          "GValueArray", G_TYPE_DOUBLE, G_TYPE_STRING, G_TYPE_UINT,
          G_TYPE_STRING, G_TYPE_INVALID);
    }

  messages = g_ptr_array_sized_new (g_queue_get_length (dbg->messages));

  for (i = dbg->messages->head; i; i = i->next)
    {
      GValue gvalue = { 0 };
      EmpathyDebugMessage *message = (EmpathyDebugMessage *) i->data;

      g_value_init (&gvalue, struct_type);
      g_value_take_boxed (&gvalue,
          dbus_g_type_specialized_construct (struct_type));
      dbus_g_type_struct_set (&gvalue,
          0, message->timestamp,
          1, message->domain,
          2, message->level,
          3, message->string,
          G_MAXUINT);
      g_ptr_array_add (messages, g_value_get_boxed (&gvalue));
    }

  emp_svc_debug_return_from_get_messages (context, messages);

  for (j = 0; j < messages->len; j++)
    g_boxed_free (struct_type, messages->pdata[j]);

  g_ptr_array_free (messages, TRUE);
}

static void
debug_iface_init (gpointer g_iface,
    gpointer iface_data)
{
  EmpSvcDebugClass *klass = (EmpSvcDebugClass *) g_iface;

  emp_svc_debug_implement_get_messages (klass, get_messages);
}

static void
empathy_debugger_init (EmpathyDebugger *self)
{
  self->messages = g_queue_new ();
}

EmpathyDebugger *
empathy_debugger_get_singleton (void)
{
  if (G_UNLIKELY (singleton == NULL))
    {
      DBusGConnection *bus;

      singleton = g_object_new (EMPATHY_TYPE_DEBUGGER, NULL);
      bus = tp_get_bus ();
      dbus_g_connection_register_g_object (bus,
          "/org/freedesktop/Telepathy/debug", (GObject *) singleton);
    }

  return singleton;
}

void
empathy_debugger_add_message (EmpathyDebugger *self,
    GTimeVal *timestamp,
    const gchar *domain,
    GLogLevelFlags level,
    const gchar *string)
{
  EmpathyDebugMessage *new_msg;

  if (g_queue_get_length (self->messages) >= DEBUG_MESSAGE_LIMIT)
    {
      EmpathyDebugMessage *old_head =
        (EmpathyDebugMessage *) g_queue_pop_head (self->messages);

      debug_message_free (old_head);
    }

  new_msg = debug_message_new (timestamp, domain, level, string);
  g_queue_push_tail (self->messages, new_msg);

  if (self->enabled)
    {
      emp_svc_debug_emit_new_debug_message (self, new_msg->timestamp,
          domain, new_msg->level, string);
    }
}
