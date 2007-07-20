/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Xavier Claessens <xclaesse@gmail.com>
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
 */

#ifndef __EMPATHY_TP_GROUP_H__
#define __EMPATHY_TP_GROUP_H__

#include <glib.h>

#include <libtelepathy/tp-chan.h>
#include <libmissioncontrol/mc-account.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_TP_GROUP         (empathy_tp_group_get_type ())
#define EMPATHY_TP_GROUP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_TP_GROUP, EmpathyTpGroup))
#define EMPATHY_TP_GROUP_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_TP_GROUP, EmpathyTpGroupClass))
#define EMPATHY_IS_TP_GROUP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_TP_GROUP))
#define EMPATHY_IS_TP_GROUP_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_TP_GROUP))
#define EMPATHY_TP_GROUP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_TP_GROUP, EmpathyTpGroupClass))

typedef struct _EmpathyTpGroup      EmpathyTpGroup;
typedef struct _EmpathyTpGroupClass EmpathyTpGroupClass;
typedef struct _EmpathyTpGroupPriv  EmpathyTpGroupPriv;

struct _EmpathyTpGroup {
	GObject      parent;
};

struct _EmpathyTpGroupClass {
	GObjectClass parent_class;
};

typedef struct {
	guint  member;
	guint  actor;
	guint  reason;
	gchar *message;
} EmpathyTpGroupInfo;

GType            empathy_tp_group_get_type                            (void) G_GNUC_CONST;
EmpathyTpGroup * empathy_tp_group_new                                 (McAccount       *account,
								       TpChan          *tp_chan);
void             empathy_tp_group_add_members                         (EmpathyTpGroup  *group,
								       GArray          *handles,
								       const gchar     *message);
void             empathy_tp_group_add_member                          (EmpathyTpGroup  *group,
								       guint            handle,
								       const gchar     *message);
void             empathy_tp_group_remove_members                      (EmpathyTpGroup  *group,
								       GArray          *handle,
								       const gchar     *message);
void             empathy_tp_group_remove_member                       (EmpathyTpGroup  *group,
								       guint            handle,
								       const gchar     *message);
GArray *         empathy_tp_group_get_members                         (EmpathyTpGroup  *group);
void             empathy_tp_group_get_all_members                     (EmpathyTpGroup  *group,
								       GArray         **members,
								       GArray         **local_pending,
								       GArray         **remote_pending);
GList *          empathy_tp_group_get_local_pending_members_with_info (EmpathyTpGroup  *group);
void             empathy_tp_group_info_list_free                      (GList           *infos);
const gchar *    empathy_tp_group_get_name                            (EmpathyTpGroup  *group);
guint            empathy_tp_group_get_self_handle                     (EmpathyTpGroup  *group);
const gchar *    empathy_tp_group_get_object_path                     (EmpathyTpGroup  *group);
gboolean         empathy_tp_group_is_member                           (EmpathyTpGroup  *group,
								       guint            handle);

G_END_DECLS

#endif /* __EMPATHY_TP_GROUP_H__ */
