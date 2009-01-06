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
 * Authors: Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
 */

#include "config.h"

#include <glib-object.h>
#include <libmissioncontrol/mc-enum-types.h>

#include "empathy-contact-monitor.h"
#include "empathy-contact-list.h"

#include "empathy-contact.h"
#include "empathy-utils.h"
#include "empathy-marshal.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyContactMonitor)

typedef struct {
  EmpathyContactList *proxy;
  GPtrArray *contacts;

  gboolean dispose_run;
} EmpathyContactMonitorPriv;

enum {
  CONTACT_ADDED,
  CONTACT_AVATAR_CHANGED,
  CONTACT_CAPABILITIES_CHANGED,
  CONTACT_NAME_CHANGED,
  CONTACT_PRESENCE_CHANGED,
  CONTACT_PRESENCE_MESSAGE_CHANGED,
  CONTACT_REMOVED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_PROXY
};

static void  contact_remove_foreach (EmpathyContact *contact,
    EmpathyContactMonitor *monitor);
static void  cl_members_changed_cb  (EmpathyContactList    *cl,
    EmpathyContact *contact, EmpathyContact *actor, guint reason,
    gchar *message, gboolean is_member, EmpathyContactMonitor *monitor);

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EmpathyContactMonitor, empathy_contact_monitor, G_TYPE_OBJECT);

static void
do_set_property (GObject      *object,
                 guint         param_id,
                 const GValue *value,
                 GParamSpec   *pspec)
{
  switch (param_id)
    {
      case PROP_PROXY:
        empathy_contact_monitor_set_proxy (EMPATHY_CONTACT_MONITOR (object),
                                           g_value_get_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

static void
do_get_property (GObject    *object,
                 guint       param_id,
                 GValue     *value,
                 GParamSpec *pspec)
{
  EmpathyContactMonitorPriv *priv = GET_PRIV (object);

  switch (param_id)
    {
      case PROP_PROXY:
        g_value_set_object (value, priv->proxy);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

static void
do_finalize (GObject *obj)
{
  EmpathyContactMonitorPriv *priv;

  priv = GET_PRIV (obj);

  if (priv->contacts)
    {
      g_ptr_array_free (priv->contacts, TRUE);
      priv->contacts = NULL;
    }

  if (priv->proxy)
    g_signal_handlers_disconnect_by_func (priv->proxy,
                                          cl_members_changed_cb, obj);

  G_OBJECT_CLASS (empathy_contact_monitor_parent_class)->finalize (obj);
}

static void
do_dispose (GObject *obj)
{
  EmpathyContactMonitorPriv *priv;

  priv = GET_PRIV (obj);

  if (priv->dispose_run)
    return;

  priv->dispose_run = TRUE;

  if (priv->contacts)
    g_ptr_array_foreach (priv->contacts,
                         (GFunc) contact_remove_foreach, obj);

  if (priv->proxy)
    g_signal_handlers_disconnect_by_func (priv->proxy,
                                          cl_members_changed_cb, obj);

  G_OBJECT_CLASS (empathy_contact_monitor_parent_class)->dispose (obj);
}

static void
empathy_contact_monitor_class_init (EmpathyContactMonitorClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->finalize = do_finalize;
  oclass->dispose = do_dispose;
  oclass->get_property = do_get_property;
  oclass->set_property = do_set_property;

  g_object_class_install_property (oclass,
                                   PROP_PROXY,
                                   g_param_spec_object ("proxy",
                                                        "Monitor's proxy",
                                                        "The contact list associated we're monitoring",
                                                        EMPATHY_TYPE_CONTACT_LIST,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  signals[CONTACT_ADDED] =
    g_signal_new ("contact-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, EMPATHY_TYPE_CONTACT);
  signals[CONTACT_AVATAR_CHANGED] =
    g_signal_new ("contact-avatar-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, EMPATHY_TYPE_CONTACT);
  signals[CONTACT_CAPABILITIES_CHANGED] =
    g_signal_new ("contact-capabilities-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, EMPATHY_TYPE_CONTACT);
  signals[CONTACT_NAME_CHANGED] =
    g_signal_new ("contact-name-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _empathy_marshal_VOID__OBJECT_STRING,
                  G_TYPE_NONE,
                  2, EMPATHY_TYPE_CONTACT,
                  G_TYPE_STRING);
  signals[CONTACT_PRESENCE_CHANGED] =
    g_signal_new ("contact-presence-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _empathy_marshal_VOID__OBJECT_ENUM_ENUM,
                  G_TYPE_NONE,
                  3, EMPATHY_TYPE_CONTACT,
                  MC_TYPE_PRESENCE,
                  MC_TYPE_PRESENCE);
  signals[CONTACT_PRESENCE_MESSAGE_CHANGED] =
    g_signal_new ("contact-presence-message-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _empathy_marshal_VOID__OBJECT_STRING,
                  G_TYPE_NONE,
                  2, EMPATHY_TYPE_CONTACT,
                  G_TYPE_STRING);
  signals[CONTACT_REMOVED] =
    g_signal_new ("contact-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, EMPATHY_TYPE_CONTACT);

  g_type_class_add_private (klass, sizeof (EmpathyContactMonitorPriv));
}

static void
empathy_contact_monitor_init (EmpathyContactMonitor *self)
{
  EmpathyContactMonitorPriv *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, EMPATHY_TYPE_CONTACT_MONITOR,
                                   EmpathyContactMonitorPriv);

  self->priv = priv;
  priv->contacts = NULL;
  priv->proxy = NULL;
  priv->dispose_run = FALSE;
}

static void
contact_monitor_presence_changed_cb (EmpathyContact *contact,
                                     McPresence current_presence,
                                     McPresence previous_presence,
                                     EmpathyContactMonitor *self)
{
  g_signal_emit (self, signals[CONTACT_PRESENCE_CHANGED], 0, contact,
                 current_presence, previous_presence);
}

static void
contact_monitor_presence_message_changed_cb (EmpathyContact *contact,
                                             GParamSpec *pspec,
                                             EmpathyContactMonitor *self)
{
  const char *status;

  /* use the status so that we always have a presence message */
  status = empathy_contact_get_status (contact);

  g_signal_emit (self, signals[CONTACT_PRESENCE_MESSAGE_CHANGED], 0,
                 contact, status);
}

static void
contact_monitor_name_changed_cb (EmpathyContact *contact,
                                 GParamSpec *pspec,
                                 EmpathyContactMonitor *self)
{
  const char *name;

  name = empathy_contact_get_name (contact);

  g_signal_emit (self, signals[CONTACT_NAME_CHANGED], 0, contact, name);
}

static void
contact_monitor_avatar_changed_cb (EmpathyContact *contact,
                                   GParamSpec *pspec,
                                   EmpathyContactMonitor *self)
{
  /* don't emit a pixbuf in the signal, as we don't know how large
   * a client would like it to be.
   */

  g_signal_emit (self, signals[CONTACT_AVATAR_CHANGED], 0, contact);
}

static void
contact_monitor_capabilities_changed_cb (EmpathyContact *contact,
                                         GParamSpec *pspec,
                                         EmpathyContactMonitor *self)
{
  g_signal_emit (self, signals[CONTACT_CAPABILITIES_CHANGED], 0, contact);
}

static void
contact_add (EmpathyContactMonitor *monitor,
             EmpathyContact *contact)
{
  EmpathyContactMonitorPriv *priv = GET_PRIV (monitor);

  g_signal_connect (contact, "presence-changed",
                    G_CALLBACK (contact_monitor_presence_changed_cb),
                    monitor);
  g_signal_connect (contact, "notify::presence-message",
                    G_CALLBACK (contact_monitor_presence_message_changed_cb),
                    monitor);
  g_signal_connect (contact, "notify::name",
                    G_CALLBACK (contact_monitor_name_changed_cb),
                    monitor);
  g_signal_connect (contact, "notify::avatar",
                    G_CALLBACK (contact_monitor_avatar_changed_cb),
                    monitor);
  g_signal_connect (contact, "notify::capabilities",
                    G_CALLBACK (contact_monitor_capabilities_changed_cb),
                    monitor);

  g_ptr_array_add (priv->contacts, g_object_ref (contact));

  g_signal_emit (monitor, signals[CONTACT_ADDED], 0, contact);
}

static void
contact_remove (EmpathyContactMonitor *monitor,
                EmpathyContact *contact)
{
  EmpathyContactMonitorPriv *priv = GET_PRIV (monitor);

  g_signal_handlers_disconnect_by_func (contact,
                                        G_CALLBACK (contact_monitor_presence_changed_cb),
                                        monitor);
  g_signal_handlers_disconnect_by_func (contact,
                                        G_CALLBACK (contact_monitor_presence_message_changed_cb),
                                        monitor);
  g_signal_handlers_disconnect_by_func (contact,
                                        G_CALLBACK (contact_monitor_name_changed_cb),
                                        monitor);
  g_signal_handlers_disconnect_by_func (contact,
                                        G_CALLBACK (contact_monitor_avatar_changed_cb),
                                        monitor);
  g_signal_handlers_disconnect_by_func (contact,
                                        G_CALLBACK (contact_monitor_capabilities_changed_cb),
                                        monitor);

  g_ptr_array_remove (priv->contacts, contact);

  g_signal_emit (monitor, signals[CONTACT_REMOVED], 0, contact);

  g_object_unref (contact);
}

static void
contact_remove_foreach (EmpathyContact *contact,
                        EmpathyContactMonitor *monitor)
{
  contact_remove (monitor, contact);
}

static void
cl_members_changed_cb (EmpathyContactList    *cl,
                       EmpathyContact        *contact,
                       EmpathyContact        *actor,
                       guint                  reason,
                       gchar                 *message,
                       gboolean               is_member,
                       EmpathyContactMonitor *monitor)
{
  if (is_member)
    contact_add (monitor, contact);
  else
    contact_remove (monitor, contact);
}

/* public methods */

void
empathy_contact_monitor_set_proxy (EmpathyContactMonitor *self,
                                   EmpathyContactList *proxy)
{
  EmpathyContactMonitorPriv *priv;

  g_return_if_fail (EMPATHY_IS_CONTACT_MONITOR (self));
  g_return_if_fail (EMPATHY_IS_CONTACT_LIST (proxy));

  priv = GET_PRIV (self);

  if (priv->contacts != NULL)
    {
      g_ptr_array_foreach (priv->contacts,
                           (GFunc) contact_remove_foreach, self);
      g_ptr_array_free (priv->contacts, TRUE);
      priv->contacts = NULL;
    }

  priv->proxy = proxy;
  priv->contacts = g_ptr_array_new ();

  g_signal_connect (proxy, "members-changed",
                    G_CALLBACK (cl_members_changed_cb), self);
}

EmpathyContactMonitor *
empathy_contact_monitor_new_for_proxy (EmpathyContactList *proxy)
{
  EmpathyContactMonitor *retval;

  g_return_val_if_fail (EMPATHY_IS_CONTACT_LIST (proxy), NULL);

  retval = g_object_new (EMPATHY_TYPE_CONTACT_MONITOR,
                         "proxy", proxy, NULL);

  return retval;
}

