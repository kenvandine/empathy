/*
 * empathy-account.c - Source for EmpathyAccount
 * Copyright (C) 2009 Collabora Ltd.
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

#include <telepathy-glib/enums.h>

#define DEBUG_FLAG EMPATHY_DEBUG_ACCOUNT
#include <libempathy/empathy-debug.h>

#include "empathy-account.h"
#include "empathy-account-priv.h"
#include "empathy-utils.h"
#include "empathy-marshal.h"

/* signals */
enum {
  STATUS_CHANGED,
  PRESENCE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* properties */
enum {
  PROP_ENABLED = 1,
  PROP_PRESENCE,
  PROP_CONNECTION_STATUS,
  PROP_CONNECTION_STATUS_REASON,
  PROP_CONNECTION,
  PROP_UNIQUE_NAME,
  PROP_DISPLAY_NAME
};

G_DEFINE_TYPE(EmpathyAccount, empathy_account, G_TYPE_OBJECT)

/* private structure */
typedef struct _EmpathyAccountPriv EmpathyAccountPriv;

struct _EmpathyAccountPriv
{
  gboolean dispose_has_run;

  TpConnection *connection;
  guint connection_invalidated_id;

  TpConnectionStatus status;
  TpConnectionStatusReason reason;
  TpConnectionPresenceType presence;

  gboolean enabled;
  glong connect_time;

  McAccount *mc_account;
};

#define GET_PRIV(obj)  EMPATHY_GET_PRIV (obj, EmpathyAccount)

static void
empathy_account_init (EmpathyAccount *obj)
{
  EmpathyAccountPriv *priv;

  priv =  G_TYPE_INSTANCE_GET_PRIVATE (obj,
    EMPATHY_TYPE_ACCOUNT, EmpathyAccountPriv);

  obj->priv = priv;

  priv->status = TP_CONNECTION_STATUS_DISCONNECTED;
}

static void
empathy_account_get_property (GObject *object,
    guint prop_id,
    GValue *value,
    GParamSpec *pspec)
{
  EmpathyAccount *account = EMPATHY_ACCOUNT (object);
  EmpathyAccountPriv *priv = GET_PRIV (account);

  switch (prop_id)
    {
      case PROP_ENABLED:
        g_value_set_boolean (value, priv->enabled);
        break;
      case PROP_PRESENCE:
        g_value_set_uint (value, priv->presence);
        break;
      case PROP_CONNECTION_STATUS:
        g_value_set_uint (value, priv->status);
        break;
      case PROP_CONNECTION_STATUS_REASON:
        g_value_set_uint (value, priv->reason);
        break;
      case PROP_CONNECTION:
        g_value_set_object (value,
            empathy_account_get_connection (account));
        break;
      case PROP_UNIQUE_NAME:
        g_value_set_string (value,
            empathy_account_get_unique_name (account));
        break;
      case PROP_DISPLAY_NAME:
        g_value_set_string (value,
            empathy_account_get_display_name (account));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void empathy_account_dispose (GObject *object);
static void empathy_account_finalize (GObject *object);

static void
empathy_account_class_init (EmpathyAccountClass *empathy_account_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (empathy_account_class);

  g_type_class_add_private (empathy_account_class,
    sizeof (EmpathyAccountPriv));

  object_class->get_property = empathy_account_get_property;
  object_class->dispose = empathy_account_dispose;
  object_class->finalize = empathy_account_finalize;

  g_object_class_install_property (object_class, PROP_ENABLED,
    g_param_spec_boolean ("enabled",
      "Enabled",
      "Whether this account is enabled or not",
      FALSE,
      G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_PRESENCE,
    g_param_spec_uint ("presence",
      "Presence",
      "The account connections presence type",
      0,
      NUM_TP_CONNECTION_PRESENCE_TYPES,
      TP_CONNECTION_PRESENCE_TYPE_UNSET,
      G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_CONNECTION_STATUS,
    g_param_spec_uint ("status",
      "ConnectionStatus",
      "The accounts connections status type",
      0,
      NUM_TP_CONNECTION_STATUSES,
      TP_CONNECTION_STATUS_DISCONNECTED,
      G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_CONNECTION_STATUS_REASON,
    g_param_spec_uint ("status-reason",
      "ConnectionStatusReason",
      "The account connections status reason",
      0,
      NUM_TP_CONNECTION_STATUS_REASONS,
      TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED,
      G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_CONNECTION,
    g_param_spec_object ("connection",
      "Connection",
      "The accounts connection",
      TP_TYPE_CONNECTION,
      G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_UNIQUE_NAME,
    g_param_spec_string ("unique-name",
      "UniqueName",
      "The accounts unique name",
      NULL,
      G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  g_object_class_install_property (object_class, PROP_DISPLAY_NAME,
    g_param_spec_string ("display-name",
      "DisplayName",
      "The accounts display name",
      NULL,
      G_PARAM_STATIC_STRINGS | G_PARAM_READABLE));

  signals[STATUS_CHANGED] = g_signal_new ("status-changed",
    G_TYPE_FROM_CLASS (object_class),
    G_SIGNAL_RUN_LAST,
    0, NULL, NULL,
    _empathy_marshal_VOID__UINT_UINT_UINT,
    G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);

  signals[PRESENCE_CHANGED] = g_signal_new ("presence-changed",
    G_TYPE_FROM_CLASS (object_class),
    G_SIGNAL_RUN_LAST,
    0, NULL, NULL,
    _empathy_marshal_VOID__UINT_UINT,
    G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
}

void
empathy_account_dispose (GObject *object)
{
  EmpathyAccount *self = EMPATHY_ACCOUNT (object);
  EmpathyAccountPriv *priv = GET_PRIV (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->connection_invalidated_id != 0)
    g_signal_handler_disconnect (priv->connection,
        priv->connection_invalidated_id);
  priv->connection_invalidated_id = 0;

  if (priv->connection != NULL)
    g_object_unref (priv->connection);
  priv->connection = NULL;

  /* release any references held by the object here */
  if (G_OBJECT_CLASS (empathy_account_parent_class)->dispose != NULL)
    G_OBJECT_CLASS (empathy_account_parent_class)->dispose (object);
}

void
empathy_account_finalize (GObject *object)
{
  /* free any data held directly by the object here */
  if (G_OBJECT_CLASS (empathy_account_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (empathy_account_parent_class)->finalize (object);
}

gboolean
empathy_account_is_just_connected (EmpathyAccount *account)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);
  GTimeVal val;

  if (priv->status != TP_CONNECTION_STATUS_CONNECTED)
    return FALSE;

  g_get_current_time (&val);

  return (val.tv_sec - priv->connect_time) < 10;
}

/**
 * empathy_account_get_connection:
 * @account: a #EmpathyAccount
 *
 * Get the connection of the account, or NULL if account is offline or the
 * connection is not yet ready. This function does not return a new ref.
 *
 * Returns: the connection of the account.
 **/
TpConnection *
empathy_account_get_connection (EmpathyAccount *account)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);

  if (priv->connection != NULL &&
      tp_connection_is_ready (priv->connection))
    return priv->connection;

  return NULL;
}

/**
 * empathy_account_get_unique_name:
 * @account: a #EmpathyAccount
 *
 * Returns: the unique name of the account.
 **/
const gchar *
empathy_account_get_unique_name (EmpathyAccount *account)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);

  return mc_account_get_unique_name (priv->mc_account);
}

/**
 * empathy_account_get_display_name:
 * @account: a #EmpathyAccount
 *
 * Returns: the display name of the account.
 **/
const gchar *
empathy_account_get_display_name (EmpathyAccount *account)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);

  return mc_account_get_display_name (priv->mc_account);
}

gboolean
empathy_account_is_valid (EmpathyAccount *account)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);

  return mc_account_is_complete (priv->mc_account);
}

void
empathy_account_set_enabled (EmpathyAccount *account, gboolean enabled)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);

  mc_account_set_enabled (priv->mc_account, enabled);
}

gboolean
empathy_account_is_enabled (EmpathyAccount *account)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);

  return priv->enabled;
}

void
empathy_account_unset_param (EmpathyAccount *account, const gchar *param)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);

  mc_account_unset_param (priv->mc_account, param);
}

gchar *
empathy_account_get_param_string (EmpathyAccount *account, const gchar *param)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);
  gchar *value = NULL;

  mc_account_get_param_string (priv->mc_account, param, &value);
  return value;
}

gint
empathy_account_get_param_int (EmpathyAccount *account, const gchar *param)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);
  int value;

  mc_account_get_param_int (priv->mc_account, param, &value);
  return value;
}

gboolean
empathy_account_get_param_boolean (EmpathyAccount *account, const gchar *param)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);
  gboolean value;

  mc_account_get_param_boolean (priv->mc_account, param, &value);
  return value;
}

void
empathy_account_set_param_string (EmpathyAccount *account,
  const gchar *param,
  const gchar *value)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);
  mc_account_set_param_string (priv->mc_account, param, value);
}

void
empathy_account_set_param_int (EmpathyAccount *account,
  const gchar *param,
  gint value)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);
  mc_account_set_param_int (priv->mc_account, param, value);
}

void
empathy_account_set_param_boolean (EmpathyAccount *account,
  const gchar *param,
  gboolean value)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);
  mc_account_set_param_boolean (priv->mc_account, param, value);
}

void
empathy_account_set_display_name (EmpathyAccount *account,
    const gchar *display_name)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);
  mc_account_set_display_name (priv->mc_account, display_name);
}

McProfile *
empathy_account_get_profile (EmpathyAccount *account)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);
  return mc_account_get_profile (priv->mc_account);
}

EmpathyAccount *
_empathy_account_new (McAccount *mc_account)
{
  EmpathyAccount *account;
  EmpathyAccountPriv *priv;

  account = g_object_new (EMPATHY_TYPE_ACCOUNT, NULL);
  priv = GET_PRIV (account);
  priv->mc_account = mc_account;

  return account;
}

void _empathy_account_set_status (EmpathyAccount *account,
    TpConnectionStatus status,
    TpConnectionStatusReason reason,
    TpConnectionPresenceType presence)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);
  TpConnectionStatus old_s = priv->status;
  TpConnectionPresenceType old_p = priv->presence;

  priv->status = status;
  priv->presence = presence;

  if (priv->status != old_s)
    {
      if (priv->status == TP_CONNECTION_STATUS_CONNECTED)
        {
          GTimeVal val;
          g_get_current_time (&val);

          priv->connect_time = val.tv_sec;
        }

      priv->reason = reason;
      g_signal_emit (account, signals[STATUS_CHANGED], 0,
        old_s, priv->status, reason);

      g_object_notify (G_OBJECT (account), "status");
    }

  if (priv->presence != old_p)
    {
      g_signal_emit (account, signals[PRESENCE_CHANGED], 0,
        old_p, priv->presence);
      g_object_notify (G_OBJECT (account), "presence");
    }
}

static void
empathy_account_connection_ready_cb (TpConnection *connection,
    const GError *error,
    gpointer user_data)
{
  EmpathyAccount *account = EMPATHY_ACCOUNT (user_data);
  EmpathyAccountPriv *priv = GET_PRIV (account);

  if (error != NULL)
    {
      DEBUG ("(%s) Connection failed to become ready: %s",
        empathy_account_get_unique_name (account), error->message);
      priv->connection = NULL;
    }
  else
    {
      DEBUG ("(%s) Connection ready",
        empathy_account_get_unique_name (account));
      g_object_notify (G_OBJECT (account), "connection");
    }
}

static void
_empathy_account_connection_invalidated_cb (TpProxy *self,
  guint    domain,
  gint     code,
  gchar   *message,
  gpointer user_data)
{
  EmpathyAccount *account = EMPATHY_ACCOUNT (user_data);
  EmpathyAccountPriv *priv = GET_PRIV (account);

  if (priv->connection == NULL)
    return;

  DEBUG ("(%s) Connection invalidated",
    empathy_account_get_unique_name (account));

  g_assert (priv->connection == TP_CONNECTION (self));

  g_signal_handler_disconnect (priv->connection,
    priv->connection_invalidated_id);
  priv->connection_invalidated_id = 0;

  g_object_unref (priv->connection);
  priv->connection = NULL;

  g_object_notify (G_OBJECT (account), "connection");
}

void
_empathy_account_set_connection (EmpathyAccount *account,
    TpConnection *connection)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);

  if (priv->connection == connection)
    return;

  /* Connection already set, don't set the new one */
  if (connection != NULL && priv->connection != NULL)
    return;

  if (connection == NULL)
    {
      g_signal_handler_disconnect (priv->connection,
        priv->connection_invalidated_id);
      priv->connection_invalidated_id = 0;

      g_object_unref (priv->connection);
      priv->connection = NULL;
      g_object_notify (G_OBJECT (account), "connection");
    }
  else
    {
      priv->connection = g_object_ref (connection);
      priv->connection_invalidated_id = g_signal_connect (priv->connection,
          "invalidated",
          G_CALLBACK (_empathy_account_connection_invalidated_cb),
          account);

      tp_connection_call_when_ready (priv->connection,
        empathy_account_connection_ready_cb, account);
    }
}

void
_empathy_account_set_enabled (EmpathyAccount *account,
    gboolean enabled)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);

  if (priv->enabled == enabled)
    return;

  priv->enabled = enabled;
  g_object_notify (G_OBJECT (account), "enabled");
}

McAccount *
_empathy_account_get_mc_account (EmpathyAccount *account)
{
  EmpathyAccountPriv *priv = GET_PRIV (account);
  return priv->mc_account;
}
