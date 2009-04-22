/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 * Copyright (C) 2007-2009 Collabora Ltd.
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
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include <telepathy-glib/util.h>
#include <libmissioncontrol/mc-enum-types.h>

#include "empathy-contact.h"
#include "empathy-account-manager.h"
#include "empathy-utils.h"
#include "empathy-enum-types.h"
#include "empathy-marshal.h"

#define DEBUG_FLAG EMPATHY_DEBUG_CONTACT
#include "empathy-debug.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyContact)
typedef struct {
  TpContact *tp_contact;
  McAccount *account;
  gchar *id;
  gchar *name;
  EmpathyAvatar *avatar;
  McPresence presence;
  gchar *presence_message;
  guint handle;
  EmpathyCapabilities capabilities;
  gboolean is_user;
  guint hash;
} EmpathyContactPriv;

static void contact_finalize (GObject *object);
static void contact_get_property (GObject *object, guint param_id,
    GValue *value, GParamSpec *pspec);
static void contact_set_property (GObject *object, guint param_id,
    const GValue *value, GParamSpec *pspec);

G_DEFINE_TYPE (EmpathyContact, empathy_contact, G_TYPE_OBJECT);

enum
{
  PROP_0,
  PROP_TP_CONTACT,
  PROP_ACCOUNT,
  PROP_ID,
  PROP_NAME,
  PROP_AVATAR,
  PROP_PRESENCE,
  PROP_PRESENCE_MESSAGE,
  PROP_HANDLE,
  PROP_CAPABILITIES,
  PROP_IS_USER,
};

enum {
  PRESENCE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
tp_contact_notify_cb (TpContact *tp_contact,
                      GParamSpec *param,
                      GObject *contact)
{
  EmpathyContactPriv *priv = GET_PRIV (contact);

  /* Forward property notifications */
  if (!tp_strdiff (param->name, "alias"))
    g_object_notify (contact, "name");
  else if (!tp_strdiff (param->name, "presence-type")) {
    McPresence presence;

    presence = empathy_contact_get_presence (EMPATHY_CONTACT (contact));
    g_signal_emit (contact, signals[PRESENCE_CHANGED], 0, presence, priv->presence);
    priv->presence = presence;
    g_object_notify (contact, "presence");
  }
  else if (!tp_strdiff (param->name, "presence-message"))
    g_object_notify (contact, "presence-message");
  else if (!tp_strdiff (param->name, "identifier"))
    g_object_notify (contact, "id");
  else if (!tp_strdiff (param->name, "handle"))
    g_object_notify (contact, "handle");
}

static void
contact_dispose (GObject *object)
{
  EmpathyContactPriv *priv = GET_PRIV (object);

  if (priv->tp_contact)
    {
      g_signal_handlers_disconnect_by_func (priv->tp_contact,
          tp_contact_notify_cb, object);
      g_object_unref (priv->tp_contact);
    }
  priv->tp_contact = NULL;

  if (priv->account)
    g_object_unref (priv->account);
  priv->account = NULL;

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
      PROP_TP_CONTACT,
      g_param_spec_object ("tp-contact",
        "TpContact",
        "The TpContact associated with the contact",
        TP_TYPE_CONTACT,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_ACCOUNT,
      g_param_spec_object ("account",
        "The account",
        "The account associated with the contact",
        MC_TYPE_ACCOUNT,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_ID,
      g_param_spec_string ("id",
        "Contact id",
        "String identifying contact",
        NULL,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_NAME,
      g_param_spec_string ("name",
        "Contact Name",
        "The name of the contact",
        NULL,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_AVATAR,
      g_param_spec_boxed ("avatar",
        "Avatar image",
        "The avatar image",
        EMPATHY_TYPE_AVATAR,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_PRESENCE,
      g_param_spec_uint ("presence",
        "Contact presence",
        "Presence of contact",
        MC_PRESENCE_UNSET,
        LAST_MC_PRESENCE,
        MC_PRESENCE_UNSET,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_PRESENCE_MESSAGE,
      g_param_spec_string ("presence-message",
        "Contact presence message",
        "Presence message of contact",
        NULL,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_HANDLE,
      g_param_spec_uint ("handle",
        "Contact Handle",
        "The handle of the contact",
        0,
        G_MAXUINT,
        0,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_CAPABILITIES,
      g_param_spec_flags ("capabilities",
        "Contact Capabilities",
        "Capabilities of the contact",
        EMPATHY_TYPE_CAPABILITIES,
        EMPATHY_CAPABILITIES_UNKNOWN,
        G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
      PROP_IS_USER,
      g_param_spec_boolean ("is-user",
        "Contact is-user",
        "Is contact the user",
        FALSE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

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
}

static void
contact_finalize (GObject *object)
{
  EmpathyContactPriv *priv;

  priv = GET_PRIV (object);

  DEBUG ("finalize: %p", object);

  g_free (priv->name);
  g_free (priv->id);
  g_free (priv->presence_message);

  if (priv->avatar)
      empathy_avatar_unref (priv->avatar);

  G_OBJECT_CLASS (empathy_contact_parent_class)->finalize (object);
}

static void
set_tp_contact (EmpathyContact *contact,
                TpContact *tp_contact)
{
  EmpathyContactPriv *priv = GET_PRIV (contact);

  if (tp_contact == NULL)
    return;

  g_assert (priv->tp_contact == NULL);
  priv->tp_contact = g_object_ref (tp_contact);
  priv->presence = empathy_contact_get_presence (contact);

  g_signal_connect (priv->tp_contact, "notify",
    G_CALLBACK (tp_contact_notify_cb), contact);
}

static void
contact_get_property (GObject *object,
                      guint param_id,
                      GValue *value,
                      GParamSpec *pspec)
{
  EmpathyContact *contact = EMPATHY_CONTACT (object);

  switch (param_id)
    {
      case PROP_TP_CONTACT:
        g_value_set_object (value, empathy_contact_get_tp_contact (contact));
        break;
      case PROP_ACCOUNT:
        g_value_set_object (value, empathy_contact_get_account (contact));
        break;
      case PROP_ID:
        g_value_set_string (value, empathy_contact_get_id (contact));
        break;
      case PROP_NAME:
        g_value_set_string (value, empathy_contact_get_name (contact));
        break;
      case PROP_AVATAR:
        g_value_set_boxed (value, empathy_contact_get_avatar (contact));
        break;
      case PROP_PRESENCE:
        g_value_set_uint (value, empathy_contact_get_presence (contact));
        break;
      case PROP_PRESENCE_MESSAGE:
        g_value_set_string (value, empathy_contact_get_presence_message (contact));
        break;
      case PROP_HANDLE:
        g_value_set_uint (value, empathy_contact_get_handle (contact));
        break;
      case PROP_CAPABILITIES:
        g_value_set_flags (value, empathy_contact_get_capabilities (contact));
        break;
      case PROP_IS_USER:
        g_value_set_boolean (value, empathy_contact_is_user (contact));
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
  EmpathyContact *contact = EMPATHY_CONTACT (object);
  EmpathyContactPriv *priv = GET_PRIV (object);

  switch (param_id)
    {
      case PROP_TP_CONTACT:
        set_tp_contact (contact, g_value_get_object (value));
        break;
      case PROP_ACCOUNT:
        g_assert (priv->account == NULL);
        priv->account = g_value_dup_object (value);
        break;
      case PROP_ID:
        empathy_contact_set_id (contact, g_value_get_string (value));
        break;
      case PROP_NAME:
        empathy_contact_set_name (contact, g_value_get_string (value));
        break;
      case PROP_AVATAR:
        empathy_contact_set_avatar (contact, g_value_get_boxed (value));
        break;
      case PROP_PRESENCE:
        empathy_contact_set_presence (contact, g_value_get_uint (value));
        break;
      case PROP_PRESENCE_MESSAGE:
        empathy_contact_set_presence_message (contact, g_value_get_string (value));
        break;
      case PROP_HANDLE:
        empathy_contact_set_handle (contact, g_value_get_uint (value));
        break;
      case PROP_CAPABILITIES:
        empathy_contact_set_capabilities (contact, g_value_get_flags (value));
        break;
      case PROP_IS_USER:
        empathy_contact_set_is_user (contact, g_value_get_boolean (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

EmpathyContact *
empathy_contact_new (TpContact *tp_contact)
{
  g_return_val_if_fail (TP_IS_CONTACT (tp_contact), NULL);

  return g_object_new (EMPATHY_TYPE_CONTACT,
      "tp-contact", tp_contact,
      NULL);
}

EmpathyContact *
empathy_contact_new_for_log (McAccount *account,
                             const gchar *id,
                             const gchar *name,
                             gboolean is_user)
{
  g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  return g_object_new (EMPATHY_TYPE_CONTACT,
      "account", account,
      "id", id,
      "name", name,
      "is-user", is_user,
      NULL);
}

TpContact *
empathy_contact_get_tp_contact (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

  priv = GET_PRIV (contact);

  return priv->tp_contact;
}

const gchar *
empathy_contact_get_id (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

  priv = GET_PRIV (contact);

  if (priv->tp_contact != NULL)
    return tp_contact_get_identifier (priv->tp_contact);

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

  g_object_unref (contact);
}

const gchar *
empathy_contact_get_name (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

  priv = GET_PRIV (contact);

  if (priv->tp_contact != NULL)
    return tp_contact_get_alias (priv->tp_contact);

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

  if (priv->account == NULL && priv->tp_contact != NULL)
    {
      EmpathyAccountManager *manager;
      TpConnection *connection;

      /* FIXME: This assume the account manager already exists */
      manager = empathy_account_manager_dup_singleton ();
      connection = tp_contact_get_connection (priv->tp_contact);
      priv->account = empathy_account_manager_get_account (manager, connection);
      g_object_ref (priv->account);
      g_object_unref (manager);
    }

  return priv->account;
}

TpConnection *
empathy_contact_get_connection (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

  priv = GET_PRIV (contact);

  if (priv->tp_contact != NULL)
    return tp_contact_get_connection (priv->tp_contact);

  return NULL;
}

static McPresence
presence_type_to_mc_presence (TpConnectionPresenceType type)
{
  switch (type)
    {
      case TP_CONNECTION_PRESENCE_TYPE_UNSET:
      case TP_CONNECTION_PRESENCE_TYPE_UNKNOWN:
      case TP_CONNECTION_PRESENCE_TYPE_ERROR:
        return MC_PRESENCE_UNSET;
      case TP_CONNECTION_PRESENCE_TYPE_OFFLINE:
        return MC_PRESENCE_OFFLINE;
      case TP_CONNECTION_PRESENCE_TYPE_AVAILABLE:
        return MC_PRESENCE_AVAILABLE;
      case TP_CONNECTION_PRESENCE_TYPE_AWAY:
        return MC_PRESENCE_AWAY;
      case TP_CONNECTION_PRESENCE_TYPE_EXTENDED_AWAY:
        return MC_PRESENCE_EXTENDED_AWAY;
      case TP_CONNECTION_PRESENCE_TYPE_HIDDEN:
        return MC_PRESENCE_HIDDEN;
      case TP_CONNECTION_PRESENCE_TYPE_BUSY:
        return MC_PRESENCE_DO_NOT_DISTURB;
    }

  return MC_PRESENCE_UNSET;
}

McPresence
empathy_contact_get_presence (EmpathyContact *contact)
{
  EmpathyContactPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), MC_PRESENCE_UNSET);

  priv = GET_PRIV (contact);

  if (priv->tp_contact != NULL)
    return presence_type_to_mc_presence (tp_contact_get_presence_type (
        priv->tp_contact));

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

  if (priv->tp_contact != NULL)
    return tp_contact_get_presence_message (priv->tp_contact);

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

  if (priv->tp_contact != NULL)
    return tp_contact_get_handle (priv->tp_contact);

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
  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), FALSE);

  return (empathy_contact_get_presence (contact) > MC_PRESENCE_OFFLINE);
}

const gchar *
empathy_contact_get_status (EmpathyContact *contact)
{
  const gchar *message;

  g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), "");

  message = empathy_contact_get_presence_message (contact);
  if (!EMP_STR_EMPTY (message))
    return message;

  return empathy_presence_get_default_message (
      empathy_contact_get_presence (contact));
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

static gchar *
contact_get_avatar_filename (EmpathyContact *contact,
                             const gchar *token)
{
  EmpathyContactPriv *priv = GET_PRIV (contact);
  McAccount *account;
  gchar *avatar_path;
  gchar *avatar_file;
  gchar *token_escaped;
  gchar *contact_escaped;

  if (EMP_STR_EMPTY (priv->id))
    return NULL;

  contact_escaped = tp_escape_as_identifier (priv->id);
  token_escaped = tp_escape_as_identifier (token);
  account = empathy_contact_get_account (contact);

  /* FIXME: Do not use the account, but proto/cm instead */
  avatar_path = g_build_filename (g_get_user_cache_dir (),
      PACKAGE_NAME,
      "avatars",
      mc_account_get_unique_name (account),
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

