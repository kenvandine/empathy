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

#include <libmissioncontrol/mc-account-monitor.h>

#include "empathy-account-manager.h"
#include "empathy-marshal.h"
#include "empathy-utils.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyAccountManager)

typedef struct {
  McAccountMonitor *monitor;
  MissionControl   *mc;

  GHashTable       *accounts;
  int               connected;
  int               connecting;
  gboolean          dispose_run;
} EmpathyAccountManagerPriv;

typedef struct {
  McPresence presence;
  TpConnectionStatus status;
  gboolean is_enabled;

  guint source_id;
} AccountData;

enum {
  ACCOUNT_CREATED,
  ACCOUNT_DELETED,
  ACCOUNT_ENABLED,
  ACCOUNT_DISABLED,
  ACCOUNT_CHANGED,
  ACCOUNT_CONNECTION_CHANGED,
  ACCOUNT_PRESENCE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static EmpathyAccountManager *manager_singleton = NULL;

G_DEFINE_TYPE (EmpathyAccountManager, empathy_account_manager, G_TYPE_OBJECT);

static AccountData *
account_data_new (McPresence presence,
                  TpConnectionStatus status,
                  gboolean is_enabled)
{
  AccountData *retval;

  retval = g_slice_new0 (AccountData);
  retval->presence = presence;
  retval->status = status;
  retval->is_enabled = is_enabled;
  retval->source_id = 0;

  return retval;
}

static AccountData *
account_data_new_default (MissionControl *mc,
                          McAccount *account)
{
  McPresence actual_p;
  TpConnectionStatus actual_c;
  GError *err = NULL;

  actual_p = mission_control_get_presence_actual (mc, &err);
  if (err != NULL)
    {
      actual_p = MC_PRESENCE_UNSET;
      g_clear_error (&err);
    }

  actual_c = mission_control_get_connection_status (mc, account, &err);

  if (err != NULL)
    {
      actual_c = TP_CONNECTION_STATUS_DISCONNECTED;
      g_error_free (err);
    }

  return account_data_new (actual_p, actual_c, mc_account_is_enabled (account));
}

static void
account_data_free (AccountData *data)
{
  if (data->source_id > 0)
    {
      g_source_remove (data->source_id);
      data->source_id = 0;
    }

  g_slice_free (AccountData, data);
}

static void
account_created_cb (McAccountMonitor *mon,
                    gchar *account_name,
                    EmpathyAccountManager *manager)
{
  McAccount *account;
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);
  TpConnectionStatus initial_status;

  account = mc_account_lookup (account_name);

  if (account)
    {
      AccountData *data;

      data = account_data_new_default (priv->mc, account);

      initial_status = mission_control_get_connection_status (priv->mc,
							      account, NULL);

      if (initial_status == TP_CONNECTION_STATUS_CONNECTED)
	priv->connected++;
      else if (initial_status == TP_CONNECTION_STATUS_CONNECTING)
	priv->connecting++;

      /* the reference returned by mc_account_lookup is owned by the
       * hash table.
       */
      g_hash_table_insert (priv->accounts, account, data);

      g_signal_emit (manager, signals[ACCOUNT_CREATED], 0, account);
    }
}

static void
account_deleted_cb (McAccountMonitor *mon,
                    gchar *account_name,
                    EmpathyAccountManager *manager)
{
  McAccount *account;
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);

  account = mc_account_lookup (account_name);

  if (account)
    {
      g_hash_table_remove (priv->accounts, account);
      g_signal_emit (manager, signals[ACCOUNT_DELETED], 0, account);
      g_object_unref (account);
    }
}

static void
account_changed_cb (McAccountMonitor *mon,
                    gchar *account_name,
                    EmpathyAccountManager *manager)
{
  McAccount *account;

  account = mc_account_lookup (account_name);

  if (account)
    {
      g_signal_emit (manager, signals[ACCOUNT_CHANGED], 0, account);
      g_object_unref (account);
    }
}

static void
account_disabled_cb (McAccountMonitor *mon,
                     gchar *account_name,
                     EmpathyAccountManager *manager)
{
  McAccount *account;
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);
  AccountData *data;

  account = mc_account_lookup (account_name);

  if (account)
    {
      data = g_hash_table_lookup (priv->accounts, account);
      g_assert (data);
      data->is_enabled = FALSE;

      g_signal_emit (manager, signals[ACCOUNT_DISABLED], 0, account);
      g_object_unref (account);
    }
}

static void
account_enabled_cb (McAccountMonitor *mon,
                    gchar *account_name,
                    EmpathyAccountManager *manager)
{
  McAccount *account;
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);
  AccountData *data;

  account = mc_account_lookup (account_name);

  if (account)
    {
      data = g_hash_table_lookup (priv->accounts, account);
      g_assert (data);
      data->is_enabled = TRUE;

      g_signal_emit (manager, signals[ACCOUNT_ENABLED], 0, account);
      g_object_unref (account);
    }
}

static void
update_connection_numbers (EmpathyAccountManager *manager,
                           TpConnectionStatus status,
                           TpConnectionStatus old_s)
{
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);

  if (status == TP_CONNECTION_STATUS_CONNECTED)
    {
      priv->connected++;
      if (old_s == TP_CONNECTION_STATUS_CONNECTING)
        priv->connecting--;
    }

  if (status == TP_CONNECTION_STATUS_CONNECTING)
    {
      priv->connecting++;
      if (old_s == TP_CONNECTION_STATUS_CONNECTED)
        priv->connected--;
    }

  if (status == TP_CONNECTION_STATUS_DISCONNECTED)
    {
      if (old_s == TP_CONNECTION_STATUS_CONNECTED)
        priv->connected--;

      if (old_s == TP_CONNECTION_STATUS_CONNECTING)
        priv->connecting--;
    }
}

static gboolean
remove_data_timeout (gpointer _data)
{
  AccountData *data = _data;

  data->source_id = 0;

  return FALSE;
}

typedef struct {
  TpConnectionStatus status;
  McPresence presence;
  TpConnectionStatusReason reason;
  gchar *unique_name;
  EmpathyAccountManager *manager;
} ChangedSignalData;

static gboolean
account_status_changed_idle_cb (ChangedSignalData *signal_data)
{
  McAccount *account;
  AccountData *data;
  McPresence presence, old_p;
  TpConnectionStatus status, old_s;
  gboolean emit_presence = FALSE, emit_connection = FALSE;
  EmpathyAccountManager *manager = signal_data->manager;
  EmpathyAccountManagerPriv *priv = GET_PRIV (manager);

  presence = signal_data->presence;
  status = signal_data->status;
  account = mc_account_lookup (signal_data->unique_name);

  if (account)
    {
      data = g_hash_table_lookup (priv->accounts, account);
      g_assert (data);

      old_p = data->presence;
      old_s = data->status;

      if (old_p != presence)
        {
          data->presence = presence;
          emit_presence = TRUE;
        }

      if (old_s != status)
        {
          data->status = status;
          update_connection_numbers (manager, status, old_s);

          if (status == TP_CONNECTION_STATUS_CONNECTED)
            {
                if (data->source_id > 0) {
                  g_source_remove (data->source_id);
                  data->source_id = 0;
                }

                data->source_id = g_timeout_add_seconds (10,
                                                         remove_data_timeout,
                                                         data);
            }
          emit_connection = TRUE;
        }

      if (emit_presence)
        g_signal_emit (manager, signals[ACCOUNT_PRESENCE_CHANGED], 0,
                       account, presence, old_p);

      if (emit_connection)
        g_signal_emit (manager, signals[ACCOUNT_CONNECTION_CHANGED], 0,
                       account, signal_data->reason, status, old_s);

      g_object_unref (account);
    }

  g_object_unref (signal_data->manager);
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

  data = g_slice_new0 (ChangedSignalData);
  data->status = status;
  data->presence = presence;
  data->reason = reason;
  data->unique_name = g_strdup (unique_name);
  data->manager = g_object_ref (manager);

  g_idle_add ((GSourceFunc) account_status_changed_idle_cb, data);
}

static void
empathy_account_manager_init (EmpathyAccountManager *manager)
{
  EmpathyAccountManagerPriv *priv;
  GList *mc_accounts, *l;
  AccountData *data;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
				      EMPATHY_TYPE_ACCOUNT_MANAGER, EmpathyAccountManagerPriv);
  manager->priv = priv;
  priv->monitor = mc_account_monitor_new ();
  priv->mc = empathy_mission_control_dup_singleton ();
  priv->connected = priv->connecting = 0;
  priv->dispose_run = FALSE;

  priv->accounts = g_hash_table_new_full (empathy_account_hash,
                                          empathy_account_equal,
                                          g_object_unref, 
                                          (GDestroyNotify) account_data_free);

  mc_accounts = mc_accounts_list ();

  for (l = mc_accounts; l; l = l->next)
    {
      data = account_data_new_default (priv->mc, l->data);

      account_created_cb (priv->monitor, (char *) mc_account_get_unique_name (l->data), manager);
    }

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

  g_hash_table_unref (priv->accounts);

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

static GObject*
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
                  1, MC_TYPE_ACCOUNT);

  signals[ACCOUNT_DELETED] =
    g_signal_new ("account-deleted",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, MC_TYPE_ACCOUNT);

  signals[ACCOUNT_ENABLED] =
    g_signal_new ("account-enabled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, MC_TYPE_ACCOUNT);

  signals[ACCOUNT_DISABLED] =
    g_signal_new ("account-disabled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, MC_TYPE_ACCOUNT);

  signals[ACCOUNT_CHANGED] =
    g_signal_new ("account-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, MC_TYPE_ACCOUNT);

  signals[ACCOUNT_CONNECTION_CHANGED] =
    g_signal_new ("account-connection-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _empathy_marshal_VOID__OBJECT_INT_UINT_UINT,
                  G_TYPE_NONE,
                  4, MC_TYPE_ACCOUNT,
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
                  3, MC_TYPE_ACCOUNT,
                  G_TYPE_INT,  /* actual presence */
                  G_TYPE_INT); /* previous presence */
  
  g_type_class_add_private (oclass, sizeof (EmpathyAccountManagerPriv));
}

/* public methods */

EmpathyAccountManager *
empathy_account_manager_dup_singleton (void)
{
  return g_object_new (EMPATHY_TYPE_ACCOUNT_MANAGER, NULL);
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

gboolean
empathy_account_manager_is_account_just_connected (EmpathyAccountManager *manager,
                                                   McAccount *account)
{
  EmpathyAccountManagerPriv *priv;
  AccountData *data;

  g_return_val_if_fail (EMPATHY_IS_ACCOUNT_MANAGER (manager), FALSE);

  priv = GET_PRIV (manager);
  data = g_hash_table_lookup (priv->accounts, account);

  g_assert (data);

  return (data->source_id > 0);
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
