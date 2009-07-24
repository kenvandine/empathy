/*
 * empathy-account-priv.h - Private Header for EmpathyAccount
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

#ifndef __EMPATHY_ACCOUNT_PRIV_H__
#define __EMPATHY_ACCOUNT_PRIV_H__

#include <glib.h>

#include <libmissioncontrol/mc-account.h>
#include "empathy-account.h"

G_BEGIN_DECLS

EmpathyAccount *_empathy_account_new (McAccount *account);
void _empathy_account_set_status (EmpathyAccount *account,
    TpConnectionStatus status,
    TpConnectionStatusReason reason,
    TpConnectionPresenceType presence);
void _empathy_account_set_connection (EmpathyAccount *account,
    TpConnection *connection);
void _empathy_account_set_enabled (EmpathyAccount *account,
    gboolean enabled);
McAccount *_empathy_account_get_mc_account (EmpathyAccount *account);

G_END_DECLS

#endif /* #ifndef __EMPATHY_ACCOUNT_PRIV_H__*/
