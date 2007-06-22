/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Imendio AB
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

#ifndef __EMPATHY_PRESENCE_H__
#define __EMPATHY_PRESENCE_H__

#include <glib-object.h>
#include <libmissioncontrol/mission-control.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_PRESENCE         (empathy_presence_get_type ())
#define EMPATHY_PRESENCE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_PRESENCE, EmpathyPresence))
#define EMPATHY_PRESENCE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_PRESENCE, EmpathyPresenceClass))
#define EMPATHY_IS_PRESENCE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_PRESENCE))
#define EMPATHY_IS_PRESENCE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_PRESENCE))
#define EMPATHY_PRESENCE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_PRESENCE, EmpathyPresenceClass))

typedef struct _EmpathyPresence      EmpathyPresence;
typedef struct _EmpathyPresenceClass EmpathyPresenceClass;

struct _EmpathyPresence {
	GObject parent;
};

struct _EmpathyPresenceClass {
	GObjectClass parent_class;
};

GType               empathy_presence_get_type                 (void) G_GNUC_CONST;

EmpathyPresence *    empathy_presence_new                      (void);
EmpathyPresence *    empathy_presence_new_full                 (McPresence      state,
							      const gchar    *status);
McPresence          empathy_presence_get_state                (EmpathyPresence *presence);
const gchar *       empathy_presence_get_status               (EmpathyPresence *presence);
void                empathy_presence_set_state                (EmpathyPresence *presence,
							      McPresence      state);
void                empathy_presence_set_status               (EmpathyPresence *presence,
							      const gchar    *status);
gint                empathy_presence_sort_func                (gconstpointer   a,
							      gconstpointer   b);
const gchar *       empathy_presence_state_get_default_status (McPresence      state);
const gchar *       empathy_presence_state_to_str             (McPresence      state);
McPresence          empathy_presence_state_from_str           (const gchar    *str);

G_END_DECLS

#endif /* __EMPATHY_PRESENCE_H__ */

