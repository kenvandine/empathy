/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Marco Barisione <marco@barisione.org>
 * Copyright (C) 2008 Collabora Ltd.
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
 * Authors: Marco Barisione <marco@barisione.org>
 *          Jonny Lamb <jonny.lamb@collabora.co.uk>
 */

#ifndef __EMPATHY_FT_MANAGER_H__
#define __EMPATHY_FT_MANAGER_H__

#include <gtk/gtk.h>
#include <glib-object.h>
#include <glib.h>

#include <libempathy/empathy-tp-file.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_FT_MANAGER (empathy_ft_manager_get_type ())
#define EMPATHY_FT_MANAGER(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_FT_MANAGER, EmpathyFTManager))
#define EMPATHY_FT_MANAGER_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_FT_MANAGER, EmpathyFTManagerClass))
#define EMPATHY_IS_FT_MANAGER(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_FT_MANAGER))
#define EMPATHY_IS_FT_MANAGER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_FT_MANAGER))
#define EMPATHY_FT_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_FT_MANAGER, EmpathyFTManagerClass))

typedef struct _EmpathyFTManager EmpathyFTManager;
typedef struct _EmpathyFTManagerPriv EmpathyFTManagerPriv;
typedef struct _EmpathyFTManagerClass EmpathyFTManagerClass;

struct _EmpathyFTManager
{
  GObject parent;

  EmpathyFTManagerPriv *priv;
};

struct _EmpathyFTManagerClass
{
  GObjectClass parent_class;
};

GType empathy_ft_manager_get_type (void);

EmpathyFTManager *empathy_ft_manager_dup_singleton (void);
void empathy_ft_manager_add_tp_file (EmpathyFTManager *ft_manager, EmpathyTpFile *tp_file);
GtkWidget *empathy_ft_manager_get_dialog (EmpathyFTManager *ft_manager);

G_END_DECLS

#endif /* __EMPATHY_FT_MANAGER_H__ */
