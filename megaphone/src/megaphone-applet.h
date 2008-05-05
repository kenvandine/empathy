/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Raphaël Slinckx <raphael@slinckx.net>
 * Copyright (C) 2007 Collabora Ltd.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Raphaël Slinckx <raphael@slinckx.net>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __MEGAPHONE_APPLET_H__
#define __MEGAPHONE_APPLET_H__

#include <panel-applet.h>

G_BEGIN_DECLS

#define MEGAPHONE_TYPE_APPLET (megaphone_applet_get_type ())
#define MEGAPHONE_APPLET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), MEGAPHONE_TYPE_APPLET, MegaphoneApplet))
#define MEGAPHONE_APPLET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), MEGAPHONE_TYPE_APPLET, MegaphoneAppletClass))
#define MEGAPHONE_IS_APPLET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MEGAPHONE_TYPE_APPLET))
#define MEGAPHONE_IS_APPLET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MEGAPHONE_TYPE_APPLET))
#define MEGAPHONE_APPLET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), MEGAPHONE_TYPE_APPLET, MegaphoneAppletClass))

typedef struct _MegaphoneApplet      MegaphoneApplet;
typedef struct _MegaphoneAppletClass MegaphoneAppletClass;

struct _MegaphoneApplet {
	PanelApplet applet;
	gpointer priv;
};

struct _MegaphoneAppletClass {
	PanelAppletClass parent_class;
};

GType megaphone_applet_get_type (void);

G_END_DECLS

#endif /* __MEGAPHONE_APPLET_H__ */
