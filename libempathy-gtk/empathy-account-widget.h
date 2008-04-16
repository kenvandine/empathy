/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2007 Imendio AB
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 *          Martyn Russell <martyn@imendio.com>
 */

#ifndef __EMPATHY_ACCOUNT_WIDGET_GENERIC_H__
#define __EMPATHY_ACCOUNT_WIDGET_GENERIC_H__

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libmissioncontrol/mc-account.h>

G_BEGIN_DECLS

void       empathy_account_widget_handle_params     (McAccount   *account,
						     GladeXML    *glade,
						     const gchar *first_widget_name,
						     ...);
void       empathy_account_widget_add_forget_button (McAccount   *account,
						     GladeXML    *glade,
						     const gchar *button,
						     const gchar *entry);
GtkWidget *empathy_account_widget_generic_new       (McAccount   *account);
GtkWidget *empathy_account_widget_salut_new         (McAccount   *account);
GtkWidget *empathy_account_widget_msn_new           (McAccount   *account);
GtkWidget *empathy_account_widget_jabber_new        (McAccount   *account);
GtkWidget *empathy_account_widget_icq_new           (McAccount   *account);
GtkWidget *empathy_account_widget_aim_new           (McAccount   *account);
GtkWidget *empathy_account_widget_yahoo_new         (McAccount   *account);
GtkWidget *empathy_account_widget_groupwise_new     (McAccount   *account);

G_END_DECLS

#endif /* __EMPATHY_ACCOUNT_WIDGET_GENERIC_H__ */
