/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Xavier Claessens <xclaesse@gmail.com>
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

#ifndef __EMPATHY_TP_GROUP_H__
#define __EMPATHY_TP_GROUP_H__

#include <glib.h>

#include <telepathy-glib/channel.h>

#include "empathy-contact.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_TP_GROUP         (empathy_tp_group_get_type ())
#define EMPATHY_TP_GROUP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_TP_GROUP, EmpathyTpGroup))
#define EMPATHY_TP_GROUP_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_TP_GROUP, EmpathyTpGroupClass))
#define EMPATHY_IS_TP_GROUP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_TP_GROUP))
#define EMPATHY_IS_TP_GROUP_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_TP_GROUP))
#define EMPATHY_TP_GROUP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_TP_GROUP, EmpathyTpGroupClass))

typedef struct _EmpathyTpGroup      EmpathyTpGroup;
typedef struct _EmpathyTpGroupClass EmpathyTpGroupClass;

struct _EmpathyTpGroup {
	GObject parent;
	gpointer priv;
};

struct _EmpathyTpGroupClass {
	GObjectClass parent_class;
};

typedef struct {
	EmpathyContact *member;
	EmpathyContact *actor;
	gchar          *message;
	guint           reason;
} EmpathyPendingInfo;

GType               empathy_tp_group_get_type            (void) G_GNUC_CONST;
EmpathyTpGroup *    empathy_tp_group_new                 (TpChannel          *channel);
void                empathy_tp_group_close               (EmpathyTpGroup     *group);
void                empathy_tp_group_add_members         (EmpathyTpGroup     *group,
							  GList              *contacts,
							  const gchar        *message);
void                empathy_tp_group_add_member          (EmpathyTpGroup     *group,
							  EmpathyContact     *contact,
							  const gchar        *message);
void                empathy_tp_group_remove_members      (EmpathyTpGroup     *group,
							  GList              *contacts,
							  const gchar        *message);
void                empathy_tp_group_remove_member       (EmpathyTpGroup     *group,
							  EmpathyContact     *contact,
							  const gchar        *message);
GList *             empathy_tp_group_get_members         (EmpathyTpGroup     *group);
GList *             empathy_tp_group_get_local_pendings  (EmpathyTpGroup     *group);
GList *             empathy_tp_group_get_remote_pendings (EmpathyTpGroup     *group);
const gchar *       empathy_tp_group_get_name            (EmpathyTpGroup     *group);
EmpathyContact *    empathy_tp_group_get_self_contact    (EmpathyTpGroup     *group);
gboolean            empathy_tp_group_is_member           (EmpathyTpGroup     *group,
							  EmpathyContact     *contact);
gboolean            empathy_tp_group_is_ready            (EmpathyTpGroup     *group);
EmpathyPendingInfo *empathy_tp_group_get_invitation      (EmpathyTpGroup     *group,
							  EmpathyContact    **remote_contact);
EmpathyPendingInfo *empathy_pending_info_new             (EmpathyContact     *member,
							  EmpathyContact     *actor,
							  const gchar        *message);
void                empathy_pending_info_free            (EmpathyPendingInfo *info);
TpChannelGroupFlags empathy_tp_group_get_flags           (EmpathyTpGroup     *group);

G_END_DECLS

#endif /* __EMPATHY_TP_GROUP_H__ */
