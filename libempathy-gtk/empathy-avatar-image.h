/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2007 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_AVATAR_IMAGE_H__
#define __EMPATHY_AVATAR_IMAGE_H__

#include <gtk/gtk.h>

#include <libempathy/empathy-contact.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_AVATAR_IMAGE         (empathy_avatar_image_get_type ())
#define EMPATHY_AVATAR_IMAGE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_AVATAR_IMAGE, EmpathyAvatarImage))
#define EMPATHY_AVATAR_IMAGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_AVATAR_IMAGE, EmpathyAvatarImageClass))
#define EMPATHY_IS_AVATAR_IMAGE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_AVATAR_IMAGE))
#define EMPATHY_IS_AVATAR_IMAGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_AVATAR_IMAGE))
#define EMPATHY_AVATAR_IMAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_AVATAR_IMAGE, EmpathyAvatarImageClass))

typedef struct _EmpathyAvatarImage      EmpathyAvatarImage;
typedef struct _EmpathyAvatarImageClass EmpathyAvatarImageClass;

struct _EmpathyAvatarImage {
	GtkEventBox parent;

	/*<private>*/
	gpointer priv;
};

struct _EmpathyAvatarImageClass {
	GtkEventBoxClass parent_class;
};

GType       empathy_avatar_image_get_type (void) G_GNUC_CONST;
GtkWidget * empathy_avatar_image_new      (void);
void        empathy_avatar_image_set      (EmpathyAvatarImage *avatar_image,
					   EmpathyAvatar      *avatar);

G_END_DECLS

#endif /* __EMPATHY_AVATAR_IMAGE_H__ */
