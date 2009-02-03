/*
 * empathy-gst-gtk-widget.c - Source for EmpathyVideoWidget
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


#include <stdio.h>
#include <stdlib.h>

#include <gdk/gdkx.h>
#include <gst/interfaces/xoverlay.h>
#include <gst/farsight/fs-element-added-notifier.h>

#include "empathy-video-widget.h"

G_DEFINE_TYPE(EmpathyVideoWidget, empathy_video_widget,
  GTK_TYPE_DRAWING_AREA)

static void empathy_video_widget_element_added_cb (
  FsElementAddedNotifier *notifier, GstBin *bin, GstElement *element,
  EmpathyVideoWidget *self);

static void empathy_video_widget_sync_message_cb (
  GstBus *bus, GstMessage *message, EmpathyVideoWidget *self);

/* signal enum */
#if 0
enum
{
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};
#endif

enum {
  PROP_GST_ELEMENT = 1,
  PROP_GST_BUS,
};

/* private structure */
typedef struct _EmpathyVideoWidgetPriv EmpathyVideoWidgetPriv;

struct _EmpathyVideoWidgetPriv
{
  gboolean dispose_has_run;
  GstBus *bus;
  GstElement *videosink;
  GstPad *sink_pad;
  GstElement *overlay;
  FsElementAddedNotifier *notifier;
};

#define GET_PRIV(o)     (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
  EMPATHY_TYPE_GST_GTK_WIDGET, EmpathyVideoWidgetPriv))

static void
empathy_video_widget_init (EmpathyVideoWidget *obj)
{
  EmpathyVideoWidgetPriv *priv = GET_PRIV (obj);

  priv->notifier = fs_element_added_notifier_new ();
  g_signal_connect (priv->notifier, "element-added",
    G_CALLBACK (empathy_video_widget_element_added_cb),
    obj);

  gtk_widget_set_size_request (GTK_WIDGET (obj), 320, 240);

  GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (obj), GTK_DOUBLE_BUFFERED);
}

static void
empathy_video_widget_constructed (GObject *object)
{
  EmpathyVideoWidgetPriv *priv = GET_PRIV (object);

  priv->videosink = gst_element_factory_make ("gconfvideosink", NULL);
  priv->sink_pad = gst_element_get_static_pad (priv->videosink, "sink");

  fs_element_added_notifier_add (priv->notifier, GST_BIN (priv->videosink));
  gst_bus_enable_sync_message_emission (priv->bus);

  g_signal_connect (priv->bus, "sync-message",
    G_CALLBACK (empathy_video_widget_sync_message_cb), object);

}

static void empathy_video_widget_dispose (GObject *object);
static void empathy_video_widget_finalize (GObject *object);
static gboolean empathy_video_widget_expose_event (GtkWidget *widget,
  GdkEventExpose *event);

static void
empathy_video_widget_set_property (GObject *object,
  guint property_id, const GValue *value, GParamSpec *pspec)
{
  EmpathyVideoWidgetPriv *priv = GET_PRIV (object);

  switch (property_id)
    {
      case PROP_GST_BUS:
        priv->bus = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
empathy_video_widget_get_property (GObject *object,
  guint property_id, GValue *value, GParamSpec *pspec)
{
  EmpathyVideoWidgetPriv *priv = GET_PRIV (object);

  switch (property_id)
    {
      case PROP_GST_ELEMENT:
        g_value_set_object (value, priv->videosink);
        break;
      case PROP_GST_BUS:
        g_value_set_object (value, priv->bus);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}


static void
empathy_video_widget_class_init (
  EmpathyVideoWidgetClass *empathy_video_widget_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (empathy_video_widget_class);
  GtkWidgetClass *widget_class =
    GTK_WIDGET_CLASS (empathy_video_widget_class);
  GParamSpec *param_spec;

  g_type_class_add_private (empathy_video_widget_class,
    sizeof (EmpathyVideoWidgetPriv));

  object_class->dispose = empathy_video_widget_dispose;
  object_class->finalize = empathy_video_widget_finalize;
  object_class->constructed = empathy_video_widget_constructed;

  object_class->set_property = empathy_video_widget_set_property;
  object_class->get_property = empathy_video_widget_get_property;

  widget_class->expose_event = empathy_video_widget_expose_event;

  param_spec = g_param_spec_object ("gst-element",
    "gst-element", "The underlaying gstreamer element",
    GST_TYPE_ELEMENT,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_GST_ELEMENT, param_spec);

  param_spec = g_param_spec_object ("gst-bus",
    "gst-bus",
    "The toplevel bus from the pipeline in which this bin will be added",
    GST_TYPE_BUS,
    G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_GST_BUS, param_spec);
}

void
empathy_video_widget_dispose (GObject *object)
{
  EmpathyVideoWidget *self = EMPATHY_VIDEO_WIDGET (object);
  EmpathyVideoWidgetPriv *priv = GET_PRIV (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->bus != NULL)
    g_object_unref (priv->bus);

  priv->bus = NULL;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (empathy_video_widget_parent_class)->dispose)
    G_OBJECT_CLASS (empathy_video_widget_parent_class)->dispose (object);
}

void
empathy_video_widget_finalize (GObject *object)
{
  //EmpathyVideoWidget *self = EMPATHY_VIDEO_WIDGET (object);
  //EmpathyVideoWidgetPriv *priv = GET_PRIV (self);

  /* free any data held directly by the object here */

  G_OBJECT_CLASS (empathy_video_widget_parent_class)->finalize (object);
}

static void
empathy_video_widget_element_added_cb (FsElementAddedNotifier *notifier,
  GstBin *bin, GstElement *element, EmpathyVideoWidget *self)
{
  EmpathyVideoWidgetPriv *priv = GET_PRIV (self);

  if (priv->overlay == NULL && GST_IS_X_OVERLAY (element))
    {
      priv->overlay = element;
      gst_x_overlay_expose (GST_X_OVERLAY (priv->overlay));
    }

  if (g_object_class_find_property (
      G_OBJECT_GET_CLASS (element), "force-aspect-ratio"))
    {
      g_object_set (G_OBJECT (element), "force-aspect-ratio", TRUE, NULL);
    }
}

static void
empathy_video_widget_sync_message_cb (GstBus *bus, GstMessage *message,
  EmpathyVideoWidget *self)
{
  EmpathyVideoWidgetPriv *priv = GET_PRIV (self);
  const GstStructure *s;

  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
    return;

  if (GST_MESSAGE_SRC (message) != (GstObject *) priv->overlay)
    return;

  s = gst_message_get_structure (message);

  if (gst_structure_has_name (s, "prepare-xwindow-id"))
    {
      gst_x_overlay_set_xwindow_id (GST_X_OVERLAY (priv->overlay),
        GDK_WINDOW_XID (GTK_WIDGET (self)->window));
    }
}

static gboolean
empathy_video_widget_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  EmpathyVideoWidget *self = EMPATHY_VIDEO_WIDGET (widget);
  EmpathyVideoWidgetPriv *priv = GET_PRIV (self);

  if (event != NULL && event->count > 0)
    return TRUE;

  if (priv->overlay == NULL)
    return TRUE;

  gst_x_overlay_set_xwindow_id (GST_X_OVERLAY (priv->overlay),
    GDK_WINDOW_XID (widget->window));

  gst_x_overlay_expose (GST_X_OVERLAY (priv->overlay));

  return TRUE;
}


GtkWidget *
empathy_video_widget_new (GstBus *bus)
{
  g_return_val_if_fail (bus != NULL, NULL);

  return GTK_WIDGET (g_object_new (EMPATHY_TYPE_GST_GTK_WIDGET,
    "gst-bus", bus,
    NULL));
}

GstPad *
empathy_video_widget_get_sink (EmpathyVideoWidget *widget)
{
  EmpathyVideoWidgetPriv *priv = GET_PRIV (widget);

  return priv->sink_pad;
}

GstElement *
empathy_video_widget_get_element (EmpathyVideoWidget *widget)
{
  EmpathyVideoWidgetPriv *priv = GET_PRIV (widget);

  return priv->videosink;
}
