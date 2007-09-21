/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Raphaël Slinckx <raphael@slinckx.net>
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
 */

#ifndef __NOTHERE_APPLET_H__
#define __NOTHERE_APPLET_H__

#include <panel-applet.h>

G_BEGIN_DECLS

#define NOTHERE_TYPE_APPLET (nothere_applet_get_type ())
#define NOTHERE_APPLET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOTHERE_TYPE_APPLET, NotHereApplet))
#define NOTHERE_APPLET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NOTHERE_TYPE_APPLET, NotHereAppletClass))
#define NOTHERE_IS_APPLET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOTHERE_TYPE_APPLET))
#define NOTHERE_IS_APPLET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NOTHERE_TYPE_APPLET))
#define NOTHERE_APPLET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), NOTHERE_TYPE_APPLET, NotHereAppletClass))

typedef struct _NotHereApplet      NotHereApplet;
typedef struct _NotHereAppletClass NotHereAppletClass;

struct _NotHereApplet {
	PanelApplet  applet;
	GtkWidget   *presence_chooser;
};

struct _NotHereAppletClass {
	PanelAppletClass parent_class;
};

GType nothere_applet_get_type (void);

G_END_DECLS

#endif /* __NOTHERE_APPLET_H__ */
