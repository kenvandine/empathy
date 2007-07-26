/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
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
 */

#ifndef __EMPATHY_CONTACT_WIDGET_H__
#define __EMPATHY_CONTACT_WIDGET_H__

#include <gtk/gtk.h>

#include <libempathy/empathy-contact.h>

G_BEGIN_DECLS

typedef enum {
	CONTACT_WIDGET_TYPE_SHOW, /* used to show contact information */
	CONTACT_WIDGET_TYPE_EDIT, /* used to edit contact information */
	CONTACT_WIDGET_TYPE_ADD,  /* used to add a new contact */
	CONTACT_WIDGET_TYPE_SUBSCRIPTION /* used to accepte/reject a new contact */
} EmpathyContactWidgetType;

GtkWidget *     empathy_contact_widget_new         (EmpathyContact           *contact,
						    EmpathyContactWidgetType  type);
EmpathyContact *empathy_contact_widget_get_contact (GtkWidget                *widget);

G_END_DECLS
#endif /*  __EMPATHY_CONTACT_WIDGET_H__ */
