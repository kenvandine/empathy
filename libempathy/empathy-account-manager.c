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
 *          Sjoerd Simons <sjoerd.simons@collabora.co.uk>
 */

#include "config.h"

#include <libmissioncontrol/mc-account-monitor.h>

#include "empathy-account-manager.h"
#include "empathy-account-priv.h"
#include "empathy-marshal.h"
#include "empathy-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_ACCOUNT
#include <libempathy/empathy-debug.h>

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyAccountManager)

typedef struct {
  McAccountMonitor *monitor;
  MissionControl   *mc;

  /* (owned) unique name -> (reffed) EmpathyAccount */
  GHashTable       *accounts;
  int               connected;
  int               connecting;
  gboolean          dispose_run;
} EmpathyAccountManagerPriv;

enum {
  ACCOUNT_CREATED,
  ACCOUNT_DELETED,
  ACCOUNT_ENABLED,
  ACCOUNT_DISABLED,
  ACCOUNT_CHANGED,
  ACCOUNT_CONNECTION_CHANGED,
  ACCOUNT_PRESENCE_CHANGED,
  NEW_CONNECTION,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static EmpathyAccountManager *manager_singleton = NULL;

G_DEFINE_TYPE (EmpathyAccountManager, empathy_account_manager, G_TYPE_OBJECT);

static TpConnectionPresenceType
mc_presence_to_tp_presence (McPresence presence)
{
  switch (presence)
    {
      case MC_PRESENCE_OFFLINE:
        return TP_CONNECTION_PRESENCE_TYPE_OFFLINE;
      case MC_PRESENCE_AVAILABLE:
        return TP_CONNECTION_PRESENCE_TYPE_AVAILABLE;
      case MC_PRESENCE_AWAY:
        return TP_CONNECTION_PRESENCE_TYPE_AWAY;
      case MC_PRESENCE_EXTENDED_AWAY:
        return TP_CONNECTION_PRESENCE_TYPE_EXTENDED_AWAY;
      case MC_PRESENCE_HIDDEN:
        return TP_CONNECTION_PRESENCE_TYPE_HIDDEN;
      case MC_PRESENCE_DO_NOT_DISTURB:
        return TP_CONNECTION_PRESENCE_TYPE_BUSY;
      default:
        return TP_CONNECTION_PRESENCE_TYPE_UNSET;
    }
}

static void
emp_account_connection_cb (EmpathyAccount *account,
  GParamSpec *spec,
  gpointer manager)
{
  TpConnection *connection = empathy_account_get_connection (account);

  DEBUG ("Signalling connection %p of account %s",
      connection, empathy_account_get_unique_name (account));

  if (connection != NULL)
    g_signal_emit (manager, signals[NEW_CONNECTION], 0, connection);
}

static void
emp_account_status_changed_cb (EmpathyAccount *account,
  TpConnectionStatus old,
  TpConnectionStatus new,
  TpConnectionStatusReason reason,
  gpointer user_data)
{
  EmpathyAccountManager *manager = EMPATHY_ACCOUNT_MANAGER (user_data);
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);

  switch (old)
    {
      case TP_CONNECTION_STATUS_CONNECTING:
        priv->connecting--;
        break;
      case TP_CONNECTION_STATUS_CONNECTED:
        priv->connected--;
        break;
      default:
        break;
    }

  switch (new)
    {
      case TP_CONNECTION_STATUS_CONNECTING:
        priv->connecting++;
        break;
      case TP_CONNECTION_STATUS_CONNECTED:
        priv->connected++;
        break;
      default:
        break;
    }

  g_signal_emit (manager, signals[ACCOUNT_CONNECTION_CHANGED], 0,
    account, reason, new, old);
}

static void
emp_account_presence_changed_cb (EmpathyAccount *account,
  TpConnectionPresenceType old,
  TpConnectionPresenceType new,
  gpointer user_data)
{
  EmpathyAccountManager *manager = EMPATHY_ACCOUNT_MANAGER (user_data);
  g_signal_emit (manager, signals[ACCOUNT_PRESENCE_CHANGED], 0,
    account, new, old);
}

static EmpathyAccount *
create_account (EmpathyAccountManager *manager,
  const gchar *account_name,
  McAccount *mc_account)
{
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);
  EmpathyAccount *account;
  TpConnectionStatus status;
  TpConnectionPresenceType presence;
  McPresence mc_presence;
  TpConnection *connection;
  GError *error = NULL;

  if ((account = g_hash_table_lookup (priv->accounts, account_name)) != NULL)
    return account;

  account = _empathy_account_new (mc_account);
  g_hash_table_insert (priv->accounts, g_strdup (account_name),
    account);

  _empathy_account_set_enabled (account,
      mc_account_is_enabled (mc_account));

  g_signal_emit (manager, signals[ACCOUNT_CREATED], 0, account);

  g_signal_connect (account, "notify::connection",
    G_CALLBACK (emp_account_connection_cb), manager);

  connection = mission_control_get_tpconnection (priv->mc,
    mc_account, NULL);
  _empathy_account_set_connection (account, connection);

  status = mission_control_get_connection_status (priv->mc,
     mc_account, &error);

  if (error != NULL)
    {
      status = TP_CONNECTION_STATUS_DISCONNECTED;
      g_clear_error (&error);
    }

  mc_presence = mission_control_get_presence_actual (priv->mc, &error);
  if (error != NULL)
    {
      presence = TP_CONNECTION_PRESENCE_TYPE_UNSET;
      g_clear_error (&error);
    }
  else
    {
      presence = mc_presence_to_tp_presence (mc_presence);
    }

  g_signal_connect (account, "status-changed",
    G_CALLBACK (emp_account_status_changed_cb), manager);

  g_signal_connect (account, "presence-changed",
    G_CALLBACK (emp_account_presence_changed_cb), manager);

  _empathy_account_set_status (account, status,
    TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED,
    presence);

  return account;
}

static void
account_created_cb (McAccountMonitor *mon,
                    gchar *account_name,
                    EmpathyAccountManager *manager)
{
  McAccount *mc_account = mc_account_lookup (account_name);

  if (mc_account != NULL)
    create_account (manager, account_name, mc_account);
}

static void
account_deleted_cb (McAccountMonitor *mon,
                    gchar *account_name,
                    EmpathyAccountManager *manager)
{
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);
  EmpathyAccount *account;

  account = g_hash_table_lookup (priv->accounts, account_name);

  if (account)
    {
      g_signal_emit (manager, signals[ACCOUNT_DELETED], 0, account);
      g_hash_table_remove (priv->accounts, account_name);
    }
}

static void
account_changed_cb (McAccountMonitor *mon,
                    gchar *account_name,
                    EmpathyAccountManager *manager)
{
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);
  EmpathyAccount *account;

  account = g_hash_table_lookup (priv->accounts, account_name);

  if (account != NULL)
    g_signal_emit (manager, signals[ACCOUNT_CHANGED], 0, account);
}

static void
account_disabled_cb (McAccountMonitor *mon,
                     gchar *account_name,
                     EmpathyAccountManager *manager)
{
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);
  EmpathyAccount *account;

  account = g_hash_table_lookup (priv->accounts, account_name);

  if (account)
    {
      _empathy_account_set_enabled (account, FALSE);
      g_signal_emit (manager, signals[ACCOUNT_DISABLED], 0, account);
    }
}

static void
account_enabled_cb (McAccountMonitor *mon,
                    gchar *account_name,
                    EmpathyAccountManager *manager)
{
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);
  EmpathyAccount *account;

  account = g_hash_table_lookup (priv->accounts, account_name);

  if (account)
    {
      _empathy_account_set_enabled (account, TRUE);
      g_signal_emit (manager, signals[ACCOUNT_ENABLED], 0, account);
    }
}

typedef struct {
  TpConnectionStatus status;
  TpConnectionPresenceType presence;
  TpConnectionStatusReason reason;
  gchar *unique_name;
  EmpathyAccountManager *manager;
  McAccount *mc_account;
} ChangedSignalData;

static gboolean
account_status_changed_idle_cb (ChangedSignalData *signal_data)
{
  EmpathyAccount *account;
  EmpathyAccountManager *manager = signal_data->manager;
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);

  account = g_hash_table_lookup (priv->accounts,
    signal_data->unique_name);

  if (account)
    {
      if (empathy_account_get_connection (account) == NULL)
        {
          TpConnection *connection;

          connection = mission_control_get_tpconnection (priv->mc,
             signal_data->mc_account, NULL);

          if (connection != NULL)
            {
              _empathy_account_set_connection (account, connection);
              g_object_unref (connection);
            }
        }

      _empathy_account_set_status (account, signal_data->status,
        signal_data->reason,
        signal_data->presence);
    }

  g_object_unref (signal_data->manager);
  g_object_unref (signal_data->mc_account);
  g_free (signal_data->unique_name);
  g_slice_free (ChangedSignalData, signal_data);

  return FALSE;
}

static void
account_status_changed_cb (MissionControl *mc,
                           TpConnectionStatus status,
                           McPresence presence,
                           TpConnectionStatusReason reason,
                           const gchar *unique_name,
                           EmpathyAccountManager *manager)
{
  ChangedSignalData *data;

  DEBUG ("Status of account %s became "
    "status: %d presence: %d reason: %d", unique_name, status,
    presence, reason);

  data = g_slice_new0 (ChangedSignalData);
  data->status = status;
  data->presence = mc_presence_to_tp_presence (presence);
  data->reason = reason;
  data->unique_name = g_strdup (unique_name);
  data->manager = g_object_ref (manager);
  data->mc_account = mc_account_lookup (unique_name);

  g_idle_add ((GSourceFunc) account_status_changed_idle_cb, data);
}

static void
empathy_account_manager_init (EmpathyAccountManager *manager)
{
  EmpathyAccountManagerPriv *priv;
  GList *mc_accounts, *l;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
      EMPATHY_TYPE_ACCOUNT_MANAGER, EmpathyAccountManagerPriv);

  manager->priv = priv;
  priv->monitor = mc_account_monitor_new ();
  priv->mc = empathy_mission_control_dup_singleton ();
  priv->connected = priv->connecting = 0;
  priv->dispose_run = FALSE;

  priv->accounts = g_hash_table_new_full (g_str_hash, g_str_equal,
      g_free, (GDestroyNotify) g_object_unref);

  mc_accounts = mc_accounts_list ();

  for (l = mc_accounts; l; l = l->next)
    account_created_cb (priv->monitor,
      (char *) mc_account_get_unique_name (l->data), manager);

  g_signal_connect (priv->monitor, "account-created",
      G_CALLBACK (account_created_cb), manager);
  g_signal_connect (priv->monitor, "account-deleted",
      G_CALLBACK (account_deleted_cb), manager);
  g_signal_connect (priv->monitor, "account-disabled",
      G_CALLBACK (account_disabled_cb), manager);
  g_signal_connect (priv->monitor, "account-enabled",
      G_CALLBACK (account_enabled_cb), manager);
  g_signal_connect (priv->monitor, "account-changed",
      G_CALLBACK (account_changed_cb), manager);

  dbus_g_proxy_connect_signal (DBUS_G_PROXY (priv->mc), "AccountStatusChanged",
                               G_CALLBACK (account_status_changed_cb),
                               manager, NULL);

  mc_accounts_list_free (mc_accounts);
}

static void
do_finalize (GObject *obj)
{
  EmpathyAccountManager *manager = EMPATHY_ACCOUNT_MANAGER (obj);
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);

  g_hash_table_destroy (priv->accounts);

  G_OBJECT_CLASS (empathy_account_manager_parent_class)->finalize (obj);
}

static void
do_dispose (GObject *obj)
{
  EmpathyAccountManager *manager = EMPATHY_ACCOUNT_MANAGER (obj);
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);

  if (priv->dispose_run)
    return;

  priv->dispose_run = TRUE;

  dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (priv->mc),
                                  "AccountStatusChanged",
                                  G_CALLBACK (account_status_changed_cb),
                                  obj);

  if (priv->monitor)
    {
      g_signal_handlers_disconnect_by_func (priv->monitor,
                                            account_created_cb, obj);
      g_signal_handlers_disconnect_by_func (priv->monitor,
                                            account_deleted_cb, obj);
      g_signal_handlers_disconnect_by_func (priv->monitor,
                                            account_disabled_cb, obj);
      g_signal_handlers_disconnect_by_func (priv->monitor,
                                            account_enabled_cb, obj);
      g_signal_handlers_disconnect_by_func (priv->monitor,
                                            account_changed_cb, obj);
      g_object_unref (priv->monitor);
      priv->monitor = NULL;
    }

  if (priv->mc)
    g_object_unref (priv->mc);

  g_hash_table_remove_all (priv->accounts);

  G_OBJECT_CLASS (empathy_account_manager_parent_class)->dispose (obj);
}

static GObject *
do_constructor (GType type,
                guint n_construct_params,
                GObjectConstructParam *construct_params)
{
  GObject *retval;

  if (!manager_singleton)
    {
      retval = G_OBJECT_CLASS (empathy_account_manager_parent_class)->constructor (type,
                                                                                   n_construct_params,
                                                                                   construct_params);
      manager_singleton = EMPATHY_ACCOUNT_MANAGER (retval);
      g_object_add_weak_pointer (retval, (gpointer) &manager_singleton);
    }
  else
    {
      retval = g_object_ref (manager_singleton);
    }

  return retval;
}

static void
empathy_account_manager_class_init (EmpathyAccountManagerClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->finalize = do_finalize;
  oclass->dispose = do_dispose;
  oclass->constructor = do_constructor;

  signals[ACCOUNT_CREATED] =
    g_signal_new ("account-created",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, EMPATHY_TYPE_ACCOUNT);

  signals[ACCOUNT_DELETED] =
    g_signal_new ("account-deleted",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, EMPATHY_TYPE_ACCOUNT);

  signals[ACCOUNT_ENABLED] =
    g_signal_new ("account-enabled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, EMPATHY_TYPE_ACCOUNT);

  signals[ACCOUNT_DISABLED] =
    g_signal_new ("account-disabled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, EMPATHY_TYPE_ACCOUNT);

  signals[ACCOUNT_CHANGED] =
    g_signal_new ("account-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, EMPATHY_TYPE_ACCOUNT);

  signals[ACCOUNT_CONNECTION_CHANGED] =
    g_signal_new ("account-connection-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _empathy_marshal_VOID__OBJECT_INT_UINT_UINT,
                  G_TYPE_NONE,
                  4, EMPATHY_TYPE_ACCOUNT,
                  G_TYPE_INT,   /* reason */
                  G_TYPE_UINT,  /* actual connection */
                  G_TYPE_UINT); /* previous connection */

  signals[ACCOUNT_PRESENCE_CHANGED] =
    g_signal_new ("account-presence-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _empathy_marshal_VOID__OBJECT_INT_INT,
                  G_TYPE_NONE,
                  3, EMPATHY_TYPE_ACCOUNT,
                  G_TYPE_INT,  /* actual presence */
                  G_TYPE_INT); /* previous presence */

  signals[NEW_CONNECTION] =
    g_signal_new ("new-connection",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, TP_TYPE_CONNECTION);

  g_type_class_add_private (oclass, sizeof (EmpathyAccountManagerPriv));
}

/* public methods */

EmpathyAccountManager *
empathy_account_manager_dup_singleton (void)
{
  return g_object_new (EMPATHY_TYPE_ACCOUNT_MANAGER, NULL);
}

EmpathyAccount *
empathy_account_manager_create (EmpathyAccountManager *manager,
  McProfile *profile)
{
  McAccount *mc_account = mc_account_create (profile);
  return g_object_ref (create_account (manager,
      mc_account_get_unique_name (mc_account),
      mc_account));
}

int
empathy_account_manager_get_connected_accounts (EmpathyAccountManager *manager)
{
  EmpathyAccountManagerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_ACCOUNT_MANAGER (manager), 0);

  priv = GET_PRIV (manager);

  return priv->connected;
}

int
empathy_account_manager_get_connecting_accounts (EmpathyAccountManager *manager)
{
  EmpathyAccountManagerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_ACCOUNT_MANAGER (manager), 0);

  priv = GET_PRIV (manager);

  return priv->connecting;
}

/**
 * empathy_account_manager_get_count:
 * @manager: a #EmpathyAccountManager
 *
 * Get the number of accounts.
 *
 * Returns: the number of accounts.
 **/
int
empathy_account_manager_get_count (EmpathyAccountManager *manager)
{
  EmpathyAccountManagerPriv *priv;

  g_return_val_if_fail (EMPATHY_IS_ACCOUNT_MANAGER (manager), 0);

  priv = GET_PRIV (manager);

  return g_hash_table_size (priv->accounts);
}

EmpathyAccount *
empathy_account_manager_get_account (EmpathyAccountManager *manager,
                                     TpConnection          *connection)
{
  EmpathyAccountManagerPriv *priv;
  GHashTableIter iter;
  gpointer value;

  g_return_val_if_fail (EMPATHY_IS_ACCOUNT_MANAGER (manager), 0);

  priv = GET_PRIV (manager);

  g_hash_table_iter_init (&iter, priv->accounts);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      EmpathyAccount *account = EMPATHY_ACCOUNT (value);

      if (connection == empathy_account_get_connection (account))
          return account;
    }

  return NULL;
}

EmpathyAccount *
empathy_account_manager_lookup (EmpathyAccountManager *manager,
    const gchar *unique_name)
{
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);
  EmpathyAccount *account;

  account = g_hash_table_lookup (priv->accounts, unique_name);

  if (account != NULL)
    g_object_ref (account);

  return account;
}

GList *
empathy_account_manager_dup_accounts (EmpathyAccountManager *manager)
{
  EmpathyAccountManagerPriv *priv;
  GList *ret;

  g_return_val_if_fail (EMPATHY_IS_ACCOUNT_MANAGER (manager), NULL);

  priv = GET_PRIV (manager);

  ret = g_hash_table_get_values (priv->accounts);
  g_list_foreach (ret, (GFunc) g_object_ref, NULL);

  return ret;
}

/**
 * empathy_account_manager_dup_connections:
 * @manager: a #EmpathyAccountManager
 *
 * Get a #GList of all ready #TpConnection. The list must be freed with
 * g_list_free, and its elements must be unreffed.
 *
 * Returns: the list of connections
 **/
GList *
empathy_account_manager_dup_connections (EmpathyAccountManager *manager)
{
  EmpathyAccountManagerPriv *priv;
  GHashTableIter iter;
  gpointer value;
  GList *ret = NULL;

  g_return_val_if_fail (EMPATHY_IS_ACCOUNT_MANAGER (manager), NULL);

  priv = GET_PRIV (manager);

  g_hash_table_iter_init (&iter, priv->accounts);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      EmpathyAccount *account = EMPATHY_ACCOUNT (value);
      TpConnection *connection;

      connection = empathy_account_get_connection (account);
      if (connection != NULL)
        ret = g_list_prepend (ret, g_object_ref (connection));
    }

  return ret;
}

void
empathy_account_manager_remove (EmpathyAccountManager *manager,
    EmpathyAccount *account)
{
  mc_account_delete (_empathy_account_get_mc_account (account));
}
