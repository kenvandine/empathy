/*
 * Copyright (C) 2007-2008 Guillaume Desmottes
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
 * Authors: Frederic Peters <fpeters@0d.be>
 */

#ifndef __EMPATHY_ACCOUNT_WIDGET_SIP_H__
#define __EMPATHY_ACCOUNT_WIDGET_SIP_H__

#include <gtk/gtkwidget.h>
#include <libmissioncontrol/mc-account.h>

G_BEGIN_DECLS

GtkWidget * empathy_account_widget_sip_new (McAccount *account);

G_END_DECLS

#endif /* __EMPATHY_ACCOUNT_WIDGET_SIP_H__ */
