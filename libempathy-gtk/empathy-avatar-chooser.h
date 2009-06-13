/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2007 Imendio AB.
 * Copyright (C) 2007-2008 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Based on Novell's e-image-chooser.
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_AVATAR_CHOOSER_H__
#define __EMPATHY_AVATAR_CHOOSER_H__

#include <gtk/gtk.h>

#include <libempathy/empathy-contact.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_AVATAR_CHOOSER	       (empathy_avatar_chooser_get_type ())
#define EMPATHY_AVATAR_CHOOSER(obj)	       (G_TYPE_CHECK_INSTANCE_CAST ((obj), EMPATHY_TYPE_AVATAR_CHOOSER, EmpathyAvatarChooser))
#define EMPATHY_AVATAR_CHOOSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EMPATHY_TYPE_AVATAR_CHOOSER, EmpathyAvatarChooserClass))
#define EMPATHY_IS_AVATAR_CHOOSER(obj)	       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EMPATHY_TYPE_AVATAR_CHOOSER))
#define EMPATHY_IS_AVATAR_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EMPATHY_TYPE_AVATAR_CHOOSER))

typedef struct _EmpathyAvatarChooser        EmpathyAvatarChooser;
typedef struct _EmpathyAvatarChooserClass   EmpathyAvatarChooserClass;

struct _EmpathyAvatarChooser {
	GtkButton parent;

	/*<private>*/
	gpointer priv;
};

struct _EmpathyAvatarChooserClass {
	GtkButtonClass parent_class;
};

GType      empathy_avatar_chooser_get_type       (void);
GtkWidget *empathy_avatar_chooser_new            (void);
void       empathy_avatar_chooser_set            (EmpathyAvatarChooser *chooser,
						  EmpathyAvatar        *avatar);
void       empathy_avatar_chooser_get_image_data (EmpathyAvatarChooser *chooser,
						  const gchar         **data,
						  gsize                *data_size,
						  const gchar         **mime_type);

#endif /* __EMPATHY_AVATAR_CHOOSER_H__ */
