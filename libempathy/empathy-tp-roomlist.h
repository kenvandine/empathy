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

#ifndef __EMPATHY_TP_ROOMLIST_H__
#define __EMPATHY_TP_ROOMLIST_H__

#include <glib.h>

#include <telepathy-glib/connection.h>
#include <libmissioncontrol/mc-account.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_TP_ROOMLIST         (empathy_tp_roomlist_get_type ())
#define EMPATHY_TP_ROOMLIST(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_TP_ROOMLIST, EmpathyTpRoomlist))
#define EMPATHY_TP_ROOMLIST_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_TP_ROOMLIST, EmpathyTpRoomlistClass))
#define EMPATHY_IS_TP_ROOMLIST(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_TP_ROOMLIST))
#define EMPATHY_IS_TP_ROOMLIST_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_TP_ROOMLIST))
#define EMPATHY_TP_ROOMLIST_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_TP_ROOMLIST, EmpathyTpRoomlistClass))

typedef struct _EmpathyTpRoomlist      EmpathyTpRoomlist;
typedef struct _EmpathyTpRoomlistClass EmpathyTpRoomlistClass;

struct _EmpathyTpRoomlist {
	GObject parent;
	gpointer priv;
};

struct _EmpathyTpRoomlistClass {
	GObjectClass parent_class;
};

GType              empathy_tp_roomlist_get_type   (void) G_GNUC_CONST;
EmpathyTpRoomlist *empathy_tp_roomlist_new        (McAccount *account);
gboolean           empathy_tp_roomlist_is_listing (EmpathyTpRoomlist *list);
void               empathy_tp_roomlist_start      (EmpathyTpRoomlist *list);
void               empathy_tp_roomlist_stop       (EmpathyTpRoomlist *list);

G_END_DECLS

#endif /* __EMPATHY_TP_ROOMLIST_H__ */
