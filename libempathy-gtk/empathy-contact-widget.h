/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007-2008 Collabora Ltd.
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

#ifndef __EMPATHY_CONTACT_WIDGET_H__
#define __EMPATHY_CONTACT_WIDGET_H__

#include <gtk/gtk.h>

#include <libempathy/empathy-contact.h>
#include "empathy-account-chooser.h"

G_BEGIN_DECLS

typedef enum
{
  EMPATHY_CONTACT_WIDGET_EDIT_NONE    = 0,
  EMPATHY_CONTACT_WIDGET_EDIT_ALIAS   = 1 << 0,
  EMPATHY_CONTACT_WIDGET_EDIT_AVATAR  = 1 << 1,
  EMPATHY_CONTACT_WIDGET_EDIT_ACCOUNT = 1 << 2,
  EMPATHY_CONTACT_WIDGET_EDIT_ID      = 1 << 3,
  EMPATHY_CONTACT_WIDGET_EDIT_GROUPS  = 1 << 4,
  EMPATHY_CONTACT_WIDGET_FOR_TOOLTIP  = 1 << 5,
} EmpathyContactWidgetFlags;

GtkWidget * empathy_contact_widget_new (EmpathyContact *contact,
    EmpathyContactWidgetFlags flags);
EmpathyContact *empathy_contact_widget_get_contact (GtkWidget *widget);
void empathy_contact_widget_set_contact (GtkWidget *widget,
    EmpathyContact *contact);
void empathy_contact_widget_set_account_filter (GtkWidget *widget,
    EmpathyAccountChooserFilterFunc filter, gpointer user_data);

G_END_DECLS

#endif /*  __EMPATHY_CONTACT_WIDGET_H__ */
