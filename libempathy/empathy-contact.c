/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 * Copyright (C) 2004 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 *          Sjoerd Simons <sjoerd.simons@collabora.co.uk>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include <telepathy-glib/util.h>
#include <libmissioncontrol/mc-enum-types.h>

#include "empathy-contact.h"
#include "empathy-contact-factory.h"
#include "empathy-utils.h"
#include "empathy-enum-types.h"
#include "empathy-marshal.h"

#define DEBUG_FLAG EMPATHY_DEBUG_CONTACT
#include "empathy-debug.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyContact)
typedef struct {
  EmpathyContactFactory *factory;
  gchar *id;
  gchar *name;
  EmpathyAvatar *avatar;
  McAccount *account;
  McPresence presence;
  gchar *presence_message;
  guint handle;
  EmpathyCapabilities capabilities;
  gboolean is_user;
  guint hash;
  EmpathyContactReady ready;
  GList *ready_callbacks;
} EmpathyContactPriv;

typedef struct {
    EmpathyContactReady ready;
    EmpathyContactReadyCb *callback;
    gpointer user_data;
    GDestroyNotify destroy;
    GObject *weak_object;
} ReadyCbData;

static void contact_finalize (GObject *object);
static void contact_get_property (GObject *object, guint param_id,
    GValue *value, GParamSpec *pspec);
static void contact_set_property (GObject *object, guint param_id,
    const GValue *value, GParamSpec *pspec);

G_DEFINE_TYPE (EmpathyContact, empathy_contact, G_TYPE_OBJECT);

enum
{
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_AVATAR,
  PROP_ACCOUNT,
  PROP_PRESENCE,
  PROP_PRESENCE_MESSAGE,
  PROP_HANDLE,
  PROP_CAPABILITIES,
  PROP_IS_USER,
  PROP_READY
};

enum {
  PRESENCE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
contact_dispose (GObject *object)
{
  EmpathyContactPriv *priv = GET_PRIV (object);

  if (priv->account)
    g_object_unref (priv->account);
  priv->account = NULL;

  if (priv->factory)
    g_object_unref (priv->factory);
  priv->factory = NULL;

  G_OBJECT_CLASS (empathy_contact_parent_class)->dispose (object);
}

static void
empathy_contact_class_init (EmpathyContactClass *class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (class);

  object_class->finalize = contact_finalize;
  object_class->dispose = contact_dispose;
  object_class->get_property = contact_get_property;
  object_class->set_property = contact_set_property;

  g_object_class_install_property (object_class,
      PROP_ID,
      g_param_spec_string ("id",
        "Contact id",
        "String identifying contact",
        NULL,
        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_NAME,
      g_param_spec_string ("name",
        "Contact Name",
        "The name of the contact",
        NULL,
        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_AVATAR,
      g_param_spec_boxed ("avatar",
        "Avatar image",
        "The avatar image",
        EMPATHY_TYPE_AVATAR,
        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_ACCOUNT,
      g_param_spec_object ("account",
        "Contact Account",
        "The account associated with the contact",
        MC_TYPE_ACCOUNT,
        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_PRESENCE,
      g_param_spec_uint ("presence",
        "Contact presence",
        "Presence of contact",
        MC_PRESENCE_UNSET,
        LAST_MC_PRESENCE,
        MC_PRESENCE_UNSET,
        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_PRESENCE_MESSAGE,
      g_param_spec_string ("presence-message",
        "Contact presence message",
        "Presence message of contact",
        NULL,
        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_HANDLE,
      g_param_spec_uint ("handle",
        "Contact Handle",
        "The handle of the contact",
        0,
        G_MAXUINT,
        0,
        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_CAPABILITIES,
      g_param_spec_flags ("capabilities",
        "Contact Capabilities",
        "Capabilities of the contact",
        EMPATHY_TYPE_CAPABILITIES,
        EMPATHY_CAPABILITIES_UNKNOWN,
        G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_IS_USER,
      g_param_spec_boolean ("is-user",
        "Contact is-user",
        "Is contact the user",
        FALSE,
        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
      PROP_READY,
      g_param_spec_flags ("ready",
        "Contact ready flags",
        "Flags for ready properties",
        EMPATHY_TYPE_CONTACT_READY,
        EMPATHY_CONTACT_READY_NONE,
        G_PARAM_READABLE));

  signals[PRESENCE_CHANGED] =
    g_signal_new ("presence-changed",
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _empathy_marshal_VOID__ENUM_ENUM,
                  G_TYPE_NONE,
                  2, MC_TYPE_PRESENCE,
                  MC_TYPE_PRESENCE);

  g_type_class_add_private (object_class, sizeof (EmpathyContactPriv));
}

static void
empathy_contact_init (EmpathyContact *contact)
{
  EmpathyContactPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (contact,
    EMPATHY_TYPE_CONTACT, EmpathyContactPriv);

  contact->priv = priv;

  /* Keep a ref to the factory to be sure it is not finalized while there is
   * still contacts alive. */
  priv->factory = empathy_contact_factory_dup_singleton ();
}

static void
contact_finalize (GObject *object)
{
  EmpathyContactPriv *priv;
  GList *l;

  priv = GET_PRIV (object);

  DEBUG ("finalize: %p", object);

  g_free (priv->name);
  g_free (priv->id);
  g_free (priv->presence_message);

  for (l = priv->ready_callbacks; l != NULL; l = g_list_next (l))
    {
      ReadyCbData *d = (ReadyCbData *)l->data;

      if (d->destroy != NULL)
        d->destroy (d->user_data);
      g_slice_free (ReadyCbData, d);
    }

  g_list_free (priv->ready_callbacks);
  priv->ready_callbacks = NULL;

  if (priv->avatar)
      empathy_avatar_unref (priv->avatar);

  G_OBJECT_CLASS (empathy_contact_parent_class)->finalize (object);
}

static void
contact_get_property (GObject *object,
                      guint param_id,
                      GValue *value,
                      GParamSpec *pspec)
{
  EmpathyContactPriv *priv;

  priv = GET_PRIV (object);

  switch (param_id)
    {
      case PROP_ID:
        g_value_set_string (value, priv->id);
        break;
      case PROP_NAME:
        g_value_set_string (value,
            empathy_contact_get_name (EMPATHY_CONTACT (object)));
        break;
      case PROP_AVATAR:
        g_value_set_boxed (value, priv->avatar);
        break;
      case PROP_ACCOUNT:
        g_value_set_object (value, priv->account);
        break;
      case PROP_PRESENCE:
        g_value_set_uint (value, priv->presence);
        break;
      case PROP_PRESENCE_MESSAGE:
        g_value_set_string (value, priv->presence_message);
        break;
      case PROP_HANDLE:
        g_value_set_uint (value, priv->handle);
        break;
      case PROP_CAPABILITIES:
        g_value_set_flags (value, priv->capabilities);
        break;
      case PROP_IS_USER:
        g_value_set_boolean (value, priv->is_user);
        break;
      case PROP_READY:
        g_value_set_flags (value, priv->ready);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

static void
contact_set_property (GObject *object,
                      guint param_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
  EmpathyContactPriv *priv;

  priv = GET_PRIV (object);

  switch (param_id)
    {
      case PROP_ID:
        empathy_contact_set_id (EMPATHY_CONTACT (object),
        g_value_get_string (value));
        break;
      case PROP_NAME:
        empathy_contact_set_name (EMPATHY_CONTACT (object),
        g_value_get_string (value));
        break;
      case PROP_AVATAR:
        empathy_contact_set_avatar (EMPATHY_CONTACT (object),
        g_value_get_boxed (value));
        break;
      case PROP_ACCOUNT:
        empathy_contact_set_account (EMPATHY_CONTACT (object),
        MC_ACCOUNT (g_value_get_object (value)));
        break;
      case PROP_PRESENCE:
        empathy_contact_set_presence (EMPATHY_CONTACT (object),
        g_value_get_uint (value));
        break;
      case PROP_PRESENCE_MESSAGE:
        empathy_contact_set_presence_message (EMPATHY_CONTACT (object),
        g_value_get_string (value));
        break;
      case PROP_HANDLE:
        empathy_contact_set_handle (EMPATHY_CONTACT (object),
        g_value_get_uint (value));
        break;
      case PROP_CAPABILITIES:
        empathy_contact_set_capabilities (EMPATHY_CONTACT (object),
        g_value_get_flags (value));
        break;
      case PROP_IS_USER:
        empathy_contact_set_is_user (EMPATHY_CONTACT (object),
        g_value_get_boolean (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

static gboolean
contact_is_ready (EmpathyContact *contact, EmpathyContactReady ready)
{
  EmpathyContactPriv *priv = GET_PRIV (contact);

  /* When the name is NULL, empathy_contact_get_name() fallback to the id.
   * When the caller want to wait the name to be ready, it also want to wait
   * the id to be ready in case of fallback. */
  if ((ready & EMPATHY_CONTACT_READY_NAME) && EMP_STR_EMPTY (priv->name))
      ready |= EMPATHY_CONTACT_READY_ID;

  return (priv->ready & ready) == ready;
}

static void
contact_weak_object_notify (gpointer data, GObject *old_object)
{
  EmpathyContact *contact = EMPATHY_CONTACT (data);
  EmpathyContactPriv *priv = GET_PRIV (contact);

  GList *l, *ln;

  for (l = priv->ready_callbacks ; l != NULL ; l = ln )
    {
      ReadyCbData *d = (ReadyCbData *)l->data;
      ln = g_list_next (l);

      if (d->weak_object == old_object)
        {
          if (d->destroy != NULL)
            d->destroy (d->user_data);

          priv->ready_callbacks = g_list_delete_link (priv->ready_callbacks,
            l);

          g_slice_free (ReadyCbData, d);
        }
    }
}

static void
contact_call_ready_callback (EmpathyContact *contact, const GError *error,
  ReadyCbData *data)
{
  data->callback (contact, error, data->user_data, data->weak_object);
  if (data->destroy != NULL)
    data->destroy (data->user_data);

  if (data->weak_object)
    g_object_weak_unref (data->weak_object,
      contact_weak_object_notify, contact);
}


static void
contact_set_ready_flag (EmpathyContact *contact,
                        EmpathyContactReady flag)
{
  EmpathyContactPriv *priv = GET_PRIV (contact);

  if (!(priv->ready & flag))
    {
      GList *l, *ln;

      priv->ready |= flag;
      g_object_notify (G_OBJECT (contact), "ready");

      for (l = priv->ready_callbacks ; l != NULL ; l = ln )
        {
          ReadyCbData *d = (ReadyCbData *)l->data;
          ln = g_list_next (l);

          if (contact_is_ready (contact, d->ready))
            {
              contact_call_ready_callback (contact, NULL, d);
              priv->ready_callbacks = g_list_delete_link
                (priv->ready_callbacks, l);
              g_slice_free (ReadyCbData, d);
            }
        }
    }
}

EmpathyContact *
empathy_contact_new (McAccount *account)
{
  return g_object_new (EMPATHY_TYPE_CONTACT,
      "account", account,
      NULL);
}

EmpathyContact *
empathy_contact_new_full (McAccount  *account,
                          const gchar *id,
                          const gchar *name)
{
  return g_object_new (EMPATHY_TYPE_CONTACT,
      "account", account,
       "name", name,
       "id", id,
       NULL);
}

const gchar *
empathy_contact_get_id (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

  priv = GET_PRIV (contact);

  return priv->id;
}

void
empathy_contact_set_id (EmpathyContact *contact,
                        const gchar *id)
{
  EmpathyContactPriv *priv;

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));
  g_return_if_fail (id != NULL);

  priv = GET_PRIV (contact);

  /* We temporally ref the contact because it could be destroyed
   * during the signal emition */
  g_object_ref (contact);
  if (tp_strdiff (id, priv->id))
    {
      g_free (priv->id);
      priv->id = g_strdup (id);

      g_object_notify (G_OBJECT (contact), "id");
      if (EMP_STR_EMPTY (priv->name))
          g_object_notify (G_OBJECT (contact), "name");
    }
  contact_set_ready_flag (contact, EMPATHY_CONTACT_READY_ID);

  g_object_unref (contact);
}

const gchar *
empathy_contact_get_name (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

  priv = GET_PRIV (contact);

  if (EMP_STR_EMPTY (priv->name))
      return empathy_contact_get_id (contact);

  return priv->name;
}

void
empathy_contact_set_name (EmpathyContact *contact,
                          const gchar *name)
{
  EmpathyContactPriv *priv;

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));

  priv = GET_PRIV (contact);

  g_object_ref (contact);
  if (tp_strdiff (name, priv->name))
    {
      g_free (priv->name);
      priv->name = g_strdup (name);
      g_object_notify (G_OBJECT (contact), "name");
    }
  contact_set_ready_flag (contact, EMPATHY_CONTACT_READY_NAME);
  g_object_unref (contact);
}

EmpathyAvatar *
empathy_contact_get_avatar (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

  priv = GET_PRIV (contact);

  return priv->avatar;
}

void
empathy_contact_set_avatar (EmpathyContact *contact,
                            EmpathyAvatar *avatar)
{
  EmpathyContactPriv *priv;

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));

  priv = GET_PRIV (contact);

  if (priv->avatar == avatar)
    return;

  if (priv->avatar)
    {
      empathy_avatar_unref (priv->avatar);
      priv->avatar = NULL;
    }

  if (avatar)
      priv->avatar = empathy_avatar_ref (avatar);

  g_object_notify (G_OBJECT (contact), "avatar");
}

McAccount *
empathy_contact_get_account (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

  priv = GET_PRIV (contact);

  return priv->account;
}

void
empathy_contact_set_account (EmpathyContact *contact,
                             McAccount *account)
{
  EmpathyContactPriv *priv;

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));
  g_return_if_fail (MC_IS_ACCOUNT (account));

  priv = GET_PRIV (contact);

  if (account == priv->account)
    return;

  if (priv->account)
      g_object_unref (priv->account);
  priv->account = g_object_ref (account);

  g_object_notify (G_OBJECT (contact), "account");
}

McPresence
empathy_contact_get_presence (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), MC_PRESENCE_UNSET);

  priv = GET_PRIV (contact);

  return priv->presence;
}

void
empathy_contact_set_presence (EmpathyContact *contact,
                              McPresence presence)
{
  EmpathyContactPriv *priv;
  McPresence old_presence;

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));

  priv = GET_PRIV (contact);

  if (presence == priv->presence)
    return;

  old_presence = priv->presence;
  priv->presence = presence;

  g_signal_emit (contact, signals[PRESENCE_CHANGED], 0, presence, old_presence);

  g_object_notify (G_OBJECT (contact), "presence");
}

const gchar *
empathy_contact_get_presence_message (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

  priv = GET_PRIV (contact);

  return priv->presence_message;
}

void
empathy_contact_set_presence_message (EmpathyContact *contact,
                                      const gchar *message)
{
  EmpathyContactPriv *priv = GET_PRIV (contact);

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));

  if (!tp_strdiff (message, priv->presence_message))
    return;

  g_free (priv->presence_message);
  priv->presence_message = g_strdup (message);

  g_object_notify (G_OBJECT (contact), "presence-message");
}

guint
empathy_contact_get_handle (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), 0);

  priv = GET_PRIV (contact);

  return priv->handle;
}

void
empathy_contact_set_handle (EmpathyContact *contact,
                            guint handle)
{
  EmpathyContactPriv *priv;

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));

  priv = GET_PRIV (contact);

  g_object_ref (contact);
  if (handle != priv->handle)
    {
      priv->handle = handle;
      g_object_notify (G_OBJECT (contact), "handle");
    }
  contact_set_ready_flag (contact, EMPATHY_CONTACT_READY_HANDLE);
  g_object_unref (contact);
}

EmpathyCapabilities
empathy_contact_get_capabilities (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), 0);

  priv = GET_PRIV (contact);

  return priv->capabilities;
}

void
empathy_contact_set_capabilities (EmpathyContact *contact,
                                  EmpathyCapabilities capabilities)
{
  EmpathyContactPriv *priv;

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));

  priv = GET_PRIV (contact);

  if (priv->capabilities == capabilities)
    return;

  priv->capabilities = capabilities;

  g_object_notify (G_OBJECT (contact), "capabilities");
}

gboolean
empathy_contact_is_user (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), FALSE);

  priv = GET_PRIV (contact);

  return priv->is_user;
}

void
empathy_contact_set_is_user (EmpathyContact *contact,
                             gboolean is_user)
{
  EmpathyContactPriv *priv;

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));

  priv = GET_PRIV (contact);

  if (priv->is_user == is_user)
    return;

  priv->is_user = is_user;

  g_object_notify (G_OBJECT (contact), "is-user");
}

gboolean
empathy_contact_is_online (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), FALSE);

  priv = GET_PRIV (contact);

  return (priv->presence > MC_PRESENCE_OFFLINE);
}

const gchar *
empathy_contact_get_status (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), "");

  priv = GET_PRIV (contact);

  if (priv->presence_message)
    return priv->presence_message;

  return empathy_presence_get_default_message (priv->presence);
}

gboolean
empathy_contact_can_voip (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), FALSE);

  priv = GET_PRIV (contact);

  return priv->capabilities & (EMPATHY_CAPABILITIES_AUDIO |
      EMPATHY_CAPABILITIES_VIDEO);
}

gboolean
empathy_contact_can_send_files (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), FALSE);

  priv = GET_PRIV (contact);

  return priv->capabilities & EMPATHY_CAPABILITIES_FT;
}

EmpathyContactReady
empathy_contact_get_ready (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), FALSE);

  priv = GET_PRIV (contact);

  return priv->ready;
}

gboolean
empathy_contact_equal (gconstpointer v1,
                       gconstpointer v2)
{
  McAccount *account_a;
  McAccount *account_b;
  const gchar *id_a;
  const gchar *id_b;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (v1), FALSE);
  g_return_val_if_fail (EMPATHY_IS_CONTACT (v2), FALSE);

  account_a = empathy_contact_get_account (EMPATHY_CONTACT (v1));
  account_b = empathy_contact_get_account (EMPATHY_CONTACT (v2));

  id_a = empathy_contact_get_id (EMPATHY_CONTACT (v1));
  id_b = empathy_contact_get_id (EMPATHY_CONTACT (v2));

  return empathy_account_equal (account_a, account_b) &&
      !tp_strdiff (id_a, id_b);
}

guint
empathy_contact_hash (gconstpointer key)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (key), +1);

  priv = GET_PRIV (EMPATHY_CONTACT (key));

  if (priv->hash == 0)
    {
      priv->hash = empathy_account_hash (priv->account) ^
          g_str_hash (priv->id);
    }

  return priv->hash;
}

void empathy_contact_call_when_ready (EmpathyContact *contact,
  EmpathyContactReady ready, EmpathyContactReadyCb *callback,
  gpointer user_data, GDestroyNotify destroy, GObject *weak_object)
{
  EmpathyContactPriv *priv = GET_PRIV (contact);

  g_return_if_fail (contact != NULL);
  g_return_if_fail (callback != NULL);

  if (contact_is_ready (contact, ready))
    {
      callback (contact, NULL, user_data, weak_object);
      if (destroy != NULL)
        destroy (user_data);
    }
  else
    {
      ReadyCbData *d = g_slice_new0 (ReadyCbData);
      d->ready = ready;
      d->callback = callback;
      d->user_data = user_data;
      d->destroy = destroy;
      d->weak_object = weak_object;

      if (weak_object != NULL)
        g_object_weak_ref (weak_object, contact_weak_object_notify, contact);

      priv->ready_callbacks = g_list_prepend (priv->ready_callbacks, d);
    }
}

static gboolean
contact_is_ready_func (GObject *contact,
                       gpointer user_data)
{
  return contact_is_ready (EMPATHY_CONTACT (contact),
    GPOINTER_TO_UINT (user_data));
}

void
empathy_contact_run_until_ready (EmpathyContact *contact,
                                 EmpathyContactReady ready,
                                 GMainLoop **loop)
{
  empathy_run_until_ready_full (contact, "notify::ready",
      contact_is_ready_func, GUINT_TO_POINTER (ready),
      loop);
}

static gchar *
contact_get_avatar_filename (EmpathyContact *contact,
                             const gchar *token)
{
  EmpathyContactPriv *priv = GET_PRIV (contact);
  gchar *avatar_path;
  gchar *avatar_file;
  gchar *token_escaped;
  gchar *contact_escaped;

  if (EMP_STR_EMPTY (priv->id))
    return NULL;

  contact_escaped = tp_escape_as_identifier (priv->id);
  token_escaped = tp_escape_as_identifier (token);

  avatar_path = g_build_filename (g_get_user_cache_dir (),
      PACKAGE_NAME,
      "avatars",
      mc_account_get_unique_name (priv->account),
      contact_escaped,
      NULL);
  g_mkdir_with_parents (avatar_path, 0700);

  avatar_file = g_build_filename (avatar_path, token_escaped, NULL);

  g_free (contact_escaped);
  g_free (token_escaped);
  g_free (avatar_path);

  return avatar_file;
}

void
empathy_contact_load_avatar_data (EmpathyContact *contact,
                                  const guchar  *data,
                                  const gsize len,
                                  const gchar *format,
                                  const gchar *token)
{
  EmpathyAvatar *avatar;
  gchar *filename;
  GError *error = NULL;

  g_return_if_fail (EMPATHY_IS_CONTACT (contact));
  g_return_if_fail (data != NULL);
  g_return_if_fail (len > 0);
  g_return_if_fail (format != NULL);
  g_return_if_fail (!EMP_STR_EMPTY (token));

  /* Load and set the avatar */
  avatar = empathy_avatar_new (g_memdup (data, len), len, g_strdup (format),
      g_strdup (token));
  empathy_contact_set_avatar (contact, avatar);
  empathy_avatar_unref (avatar);

  /* Save to cache if not yet in it */
  filename = contact_get_avatar_filename (contact, token);
  if (filename && !g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      if (!empathy_avatar_save_to_file (avatar, filename, &error))
        {
          DEBUG ("Failed to save avatar in cache: %s",
            error ? error->message : "No error given");
          g_clear_error (&error);
        }
      else
          DEBUG ("Avatar saved to %s", filename);
    }
  g_free (filename);
}

gboolean
empathy_contact_load_avatar_cache (EmpathyContact *contact,
                                   const gchar *token)
{
  EmpathyAvatar *avatar = NULL;
  gchar *filename;
  gchar *data = NULL;
  gsize len;
  GError *error = NULL;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), FALSE);
  g_return_val_if_fail (!EMP_STR_EMPTY (token), FALSE);

  /* Load the avatar from file if it exists */
  filename = contact_get_avatar_filename (contact, token);
  if (filename && g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      if (!g_file_get_contents (filename, &data, &len, &error))
        {
          DEBUG ("Failed to load avatar from cache: %s",
            error ? error->message : "No error given");
          g_clear_error (&error);
        }
    }

  if (data)
    {
      DEBUG ("Avatar loaded from %s", filename);
      avatar = empathy_avatar_new (data, len, NULL, g_strdup (token));
      empathy_contact_set_avatar (contact, avatar);
      empathy_avatar_unref (avatar);
    }

  g_free (filename);

  return data != NULL;
}

GType
empathy_avatar_get_type (void)
{
  static GType type_id = 0;

  if (!type_id)
    {
      type_id = g_boxed_type_register_static ("EmpathyAvatar",
          (GBoxedCopyFunc) empathy_avatar_ref,
          (GBoxedFreeFunc) empathy_avatar_unref);
    }

  return type_id;
}

EmpathyAvatar *
empathy_avatar_new (guchar *data,
                    gsize len,
                    gchar *format,
                    gchar *token)
{
  EmpathyAvatar *avatar;

  avatar = g_slice_new0 (EmpathyAvatar);
  avatar->data = data;
  avatar->len = len;
  avatar->format = format;
  avatar->token = token;
  avatar->refcount = 1;

  return avatar;
}

void
empathy_avatar_unref (EmpathyAvatar *avatar)
{
  g_return_if_fail (avatar != NULL);

  avatar->refcount--;
  if (avatar->refcount == 0)
    {
      g_free (avatar->data);
      g_free (avatar->format);
      g_free (avatar->token);
      g_slice_free (EmpathyAvatar, avatar);
    }
}

EmpathyAvatar *
empathy_avatar_ref (EmpathyAvatar *avatar)
{
  g_return_val_if_fail (avatar != NULL, NULL);

  avatar->refcount++;

  return avatar;
}

/**
 * empathy_avatar_save_to_file:
 * @avatar: the avatar
 * @filename: name of a file to write avatar to
 * @error: return location for a GError, or NULL
 *
 * Save the avatar to a file named filename
 *
 * Returns: %TRUE on success, %FALSE if an error occurred 
 */
gboolean
empathy_avatar_save_to_file (EmpathyAvatar *self,
                             const gchar *filename,
                             GError **error)
{
  return g_file_set_contents (filename, self->data, self->len, error);
}

