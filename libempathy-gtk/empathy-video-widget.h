/*
 * empathy-gst-gtk-widget.h - Header for EmpathyVideoWidget
 * Copyright (C) 2008 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.co.uk>
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
 */

#ifndef __EMPATHY_VIDEO_WIDGET_H__
#define __EMPATHY_VIDEO_WIDGET_H__

#define EMPATHY_VIDEO_WIDGET_DEFAULT_WIDTH 320
#define EMPATHY_VIDEO_WIDGET_DEFAULT_HEIGHT 240

#include <glib-object.h>
#include <gst/gst.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _EmpathyVideoWidget EmpathyVideoWidget;
typedef struct _EmpathyVideoWidgetClass EmpathyVideoWidgetClass;

struct _EmpathyVideoWidgetClass {
    GtkDrawingAreaClass parent_class;
};

struct _EmpathyVideoWidget {
    GtkDrawingArea parent;
};

GType empathy_video_widget_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_VIDEO_WIDGET \
  (empathy_video_widget_get_type ())
#define EMPATHY_VIDEO_WIDGET(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), EMPATHY_TYPE_VIDEO_WIDGET, \
    EmpathyVideoWidget))
#define EMPATHY_VIDEO_WIDGET_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EMPATHY_TYPE_VIDEO_WIDGET, \
  EmpathyVideoWidgetClass))
#define EMPATHY_IS_VIDEO_WIDGET(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), EMPATHY_TYPE_VIDEO_WIDGET))
#define EMPATHY_IS_VIDEO_WIDGET_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EMPATHY_TYPE_VIDEO_WIDGET))
#define EMPATHY_VIDEO_WIDGET_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EMPATHY_TYPE_VIDEO_WIDGET, \
  EmpathyVideoWidgetClass))

GtkWidget *empathy_video_widget_new (GstBus *bus);
GtkWidget *empathy_video_widget_new_with_size (GstBus *bus,
  gint width, gint height);

GstElement *empathy_video_widget_get_element (EmpathyVideoWidget *widget);
GstPad *empathy_video_widget_get_sink (EmpathyVideoWidget *widget);

G_END_DECLS

#endif /* #ifndef __EMPATHY_VIDEO_WIDGET_H__*/
