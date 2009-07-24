/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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

#ifndef __EMPATHY_ACCOUNT_MANAGER_H__
#define __EMPATHY_ACCOUNT_MANAGER_H__

#include <glib-object.h>

#include "empathy-account.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_ACCOUNT_MANAGER         (empathy_account_manager_get_type ())
#define EMPATHY_ACCOUNT_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_ACCOUNT_MANAGER, EmpathyAccountManager))
#define EMPATHY_ACCOUNT_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_ACCOUNT_MANAGER, EmpathyAccountManagerClass))
#define EMPATHY_IS_ACCOUNT_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_ACCOUNT_MANAGER))
#define EMPATHY_IS_ACCOUNT_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_ACCOUNT_MANAGER))
#define EMPATHY_ACCOUNT_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_ACCOUNT_MANAGER, EmpathyAccountManagerClass))

typedef struct _EmpathyAccountManager      EmpathyAccountManager;
typedef struct _EmpathyAccountManagerClass EmpathyAccountManagerClass;

struct _EmpathyAccountManager {
  GObject parent;
  gpointer priv;
};

struct _EmpathyAccountManagerClass {
  GObjectClass parent_class;
};

GType empathy_account_manager_get_type (void);

/* public methods */

EmpathyAccountManager * empathy_account_manager_dup_singleton (void);
EmpathyAccount *        empathy_account_manager_create
                                (EmpathyAccountManager *manager,
                                 McProfile *profile);
int                     empathy_account_manager_get_connected_accounts
                                (EmpathyAccountManager *manager);
int                     empathy_account_manager_get_connecting_accounts
                                (EmpathyAccountManager *manager);
int                     empathy_account_manager_get_count
                                (EmpathyAccountManager *manager);
EmpathyAccount *        empathy_account_manager_get_account
                                (EmpathyAccountManager *manager,
                                 TpConnection          *connection);
EmpathyAccount *        empathy_account_manager_lookup
                                (EmpathyAccountManager *manager,
                                 const gchar *unique_name);
GList *                 empathy_account_manager_dup_accounts
                                (EmpathyAccountManager *manager);
GList *                 empathy_account_manager_dup_connections
                                (EmpathyAccountManager *manager);
void                    empathy_account_manager_remove (
                                 EmpathyAccountManager *manager,
                                 EmpathyAccount *account);

G_END_DECLS

#endif /* __EMPATHY_ACCOUNT_MANAGER_H__ */

