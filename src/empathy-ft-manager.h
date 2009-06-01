/*
 * Copyright (C) 2007 Marco Barisione <marco@barisione.org>
 * Copyright (C) 2008-2009 Collabora Ltd.
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Marco Barisione <marco@barisione.org>
 *          Jonny Lamb <jonny.lamb@collabora.co.uk>
 *          Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
 */

#ifndef __EMPATHY_FT_MANAGER_H__
#define __EMPATHY_FT_MANAGER_H__

#include <gtk/gtk.h>
#include <glib-object.h>
#include <glib.h>

#include <libempathy/empathy-ft-handler.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_FT_MANAGER \
  (empathy_ft_manager_get_type ())
#define EMPATHY_FT_MANAGER(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_FT_MANAGER, EmpathyFTManager))
#define EMPATHY_FT_MANAGER_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_FT_MANAGER, EmpathyFTManagerClass))
#define EMPATHY_IS_FT_MANAGER(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_FT_MANAGER))
#define EMPATHY_IS_FT_MANAGER_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_FT_MANAGER))
#define EMPATHY_FT_MANAGER_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_FT_MANAGER, EmpathyFTManagerClass))

typedef struct _EmpathyFTManager EmpathyFTManager;
typedef struct _EmpathyFTManagerClass EmpathyFTManagerClass;

struct _EmpathyFTManager {
  GObject parent;
  gpointer priv;
};

struct _EmpathyFTManagerClass {
  GObjectClass parent_class;
};

GType empathy_ft_manager_get_type (void);

/* public methods */
void empathy_ft_manager_add_handler (EmpathyFTHandler *handler);
void empathy_ft_manager_display_error (EmpathyFTHandler *handler,
  const GError *error);
void empathy_ft_manager_show (void);

G_END_DECLS

#endif /* __EMPATHY_FT_MANAGER_H__ */
