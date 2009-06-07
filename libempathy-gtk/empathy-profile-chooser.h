/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 * Copyright (C) 2007-2009 Collabora Ltd.
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
 *          Jonny Lamb <jonny.lamb@collabora.co.uk
 */

#ifndef __EMPATHY_PROFILE_CHOOSER_H__
#define __EMPATHY_PROFILE_CHOOSER_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include <libmissioncontrol/mc-profile.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_PROFILE_CHOOSER (empathy_profile_chooser_get_type ())
#define EMPATHY_PROFILE_CHOOSER(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), \
    EMPATHY_TYPE_PROFILE_CHOOSER, EmpathyProfileChooser))
#define EMPATHY_PROFILE_CHOOSER_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), \
    EMPATHY_TYPE_PROFILE_CHOOSER, EmpathyProfileChooserClass))
#define EMPATHY_IS_PROFILE_CHOOSER(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), \
    EMPATHY_TYPE_PROFILE_CHOOSER))
#define EMPATHY_IS_PROFILE_CHOOSER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), \
    EMPATHY_TYPE_PROFILE_CHOOSER))
#define EMPATHY_PROFILE_CHOOSER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), \
    EMPATHY_TYPE_PROFILE_CHOOSER, EmpathyProfileChooserClass))

typedef struct _EmpathyProfileChooser EmpathyProfileChooser;
typedef struct _EmpathyProfileChooserClass EmpathyProfileChooserClass;

struct _EmpathyProfileChooser
{
  GtkComboBox parent;

  /*<private>*/
  gpointer priv;
};

struct _EmpathyProfileChooserClass
{
  GtkComboBoxClass parent_class;
};

GType empathy_profile_chooser_get_type (void) G_GNUC_CONST;
GtkWidget * empathy_profile_chooser_new (void);
McProfile * empathy_profile_chooser_dup_selected (
    EmpathyProfileChooser *profile_chooser);
gint empathy_profile_chooser_n_profiles (
    EmpathyProfileChooser *profile_chooser);

G_END_DECLS
#endif /*  __EMPATHY_PROFILE_CHOOSER_H__ */
