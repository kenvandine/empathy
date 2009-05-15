/*
 * Copyright (C) 2004 Red Hat, Inc.
 * Copyright (C) 2007 The Free Software Foundation
 * Copyright (C) 2008 Marco Barisione <marco@barisione.org>
 *
 * Based on evince code (shell/ev-sidebar.h) by:
 * 	- Jonathan Blandford <jrb@alum.mit.edu>
 *
 * Base on eog code (src/eog-sidebar.c) by:
 *      - Lucas Rocha <lucasr@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __EMPATHY_SIDEBAR_H__
#define __EMPATHY_SIDEBAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _EmpathySidebar EmpathySidebar;
typedef struct _EmpathySidebarClass EmpathySidebarClass;
typedef struct _EmpathySidebarPrivate EmpathySidebarPrivate;

#define EMPATHY_TYPE_SIDEBAR            (empathy_sidebar_get_type())
#define EMPATHY_SIDEBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), EMPATHY_TYPE_SIDEBAR, EmpathySidebar))
#define EMPATHY_SIDEBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  EMPATHY_TYPE_SIDEBAR, EmpathySidebarClass))
#define EMPATHY_IS_SIDEBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), EMPATHY_TYPE_SIDEBAR))
#define EMPATHY_IS_SIDEBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  EMPATHY_TYPE_SIDEBAR))
#define EMPATHY_SIDEBAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  EMPATHY_TYPE_SIDEBAR, EmpathySidebarClass))

struct _EmpathySidebar
{
  GtkVBox base_instance;

  EmpathySidebarPrivate *priv;
};

struct _EmpathySidebarClass
{
  GtkVBoxClass base_class;

  void (* page_added)   (EmpathySidebar *sidebar,
                         GtkWidget *main_widget);

  void (* page_removed) (EmpathySidebar *sidebar,
                         GtkWidget *main_widget);
};

GType      empathy_sidebar_get_type     (void);

GtkWidget *empathy_sidebar_new          (void);

void       empathy_sidebar_add_page     (EmpathySidebar *sidebar,
                                            const gchar *title,
                                            GtkWidget *main_widget);

void       empathy_sidebar_remove_page  (EmpathySidebar *sidebar,
                                            GtkWidget *main_widget);

void       empathy_sidebar_set_page     (EmpathySidebar *sidebar,
                                            GtkWidget *main_widget);

gint       empathy_sidebar_get_n_pages  (EmpathySidebar *sidebar);

gboolean   empathy_sidebar_is_empty     (EmpathySidebar *sidebar);

G_END_DECLS

#endif /* __EMPATHY_SIDEBAR_H__ */


