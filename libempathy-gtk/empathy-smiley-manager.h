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
 * Authors: Dafydd Harrie <dafydd.harries@collabora.co.uk>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_SMILEY_MANAGER__H__
#define __EMPATHY_SMILEY_MANAGER_H__

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_SMILEY_MANAGER         (empathy_smiley_manager_get_type ())
#define EMPATHY_SMILEY_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_SMILEY_MANAGER, EmpathySmileyManager))
#define EMPATHY_SMILEY_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_SMILEY_MANAGER, EmpathySmileyManagerClass))
#define EMPATHY_IS_SMILEY_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_SMILEY_MANAGER))
#define EMPATHY_IS_SMILEY_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_SMILEY_MANAGER))
#define EMPATHY_SMILEY_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_SMILEY_MANAGER, EmpathySmileyManagerClass))

typedef struct _EmpathySmileyManager      EmpathySmileyManager;
typedef struct _EmpathySmileyManagerClass EmpathySmileyManagerClass;

struct _EmpathySmileyManager {
	GObject parent;
	gpointer priv;
};

struct _EmpathySmileyManagerClass {
	GObjectClass parent_class;
};

typedef struct {
	GdkPixbuf *pixbuf;
	gchar     *str;
} EmpathySmiley;

typedef void (*EmpathySmileyMenuFunc) (EmpathySmileyManager *manager,
				       EmpathySmiley        *smiley,
				       gpointer              user_data);

GType                 empathy_smiley_manager_get_type        (void) G_GNUC_CONST;
EmpathySmileyManager *empathy_smiley_manager_dup_singleton   (void);
void                  empathy_smiley_manager_load            (EmpathySmileyManager *manager);
void                  empathy_smiley_manager_add             (EmpathySmileyManager *manager,
							      const gchar          *icon_name,
							      const gchar          *first_str,
							      ...);
void                  empathy_smiley_manager_add_from_pixbuf (EmpathySmileyManager *manager,
							      GdkPixbuf            *smiley,
							      const gchar          *first_str,
							      ...);
GSList *              empathy_smiley_manager_get_all         (EmpathySmileyManager *manager);
GSList *              empathy_smiley_manager_parse           (EmpathySmileyManager *manager,
							      const gchar          *text);
GtkWidget *           empathy_smiley_menu_new                (EmpathySmileyManager *manager,
							      EmpathySmileyMenuFunc func,
							      gpointer              user_data);
void                  empathy_smiley_free                    (EmpathySmiley        *smiley);

G_END_DECLS

#endif /* __EMPATHY_SMILEY_MANAGER_H__ */

