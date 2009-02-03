/*
 * empathy-call-window.c - Source for EmpathyCallWindow
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

#include <gst/gst.h>

#include <libempathy-gtk/empathy-video-widget.h>
#include <libempathy-gtk/empathy-audio-src.h>
#include <libempathy-gtk/empathy-audio-sink.h>
#include <libempathy-gtk/empathy-video-src.h>

#include "empathy-call-window.h"

G_DEFINE_TYPE(EmpathyCallWindow, empathy_call_window, GTK_TYPE_WINDOW)

/* signal enum */
#if 0
enum
{
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};
#endif

enum {
  PROP_CALL_HANDLER = 1,
};

/* private structure */
typedef struct _EmpathyCallWindowPriv EmpathyCallWindowPriv;

struct _EmpathyCallWindowPriv
{
  gboolean dispose_has_run;
  EmpathyCallHandler *handler;
  GtkWidget *video_output;
  GtkWidget *video_preview;
  GstElement *video_input;
  GstElement *audio_input;
  GstElement *audio_output;
  GstElement *pipeline;
  GstElement *video_tee;
};

#define GET_PRIV(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EMPATHY_TYPE_CALL_WINDOW, \
    EmpathyCallWindowPriv))

static void empathy_call_window_realized_cb (GtkWidget *widget,
  EmpathyCallWindow *window);

static void
empathy_call_window_init (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GtkWidget *hbox;
  GstBus *bus;

  priv->pipeline = gst_pipeline_new (NULL);

  hbox = gtk_hbox_new (TRUE, 3);
  gtk_container_add (GTK_CONTAINER (self), hbox);

  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));

  priv->video_output = empathy_video_widget_new (bus);

  priv->video_tee = gst_element_factory_make ("tee", NULL);
  priv->video_input = empathy_video_src_new ();
  priv->video_preview = empathy_video_widget_new (bus);

  priv->audio_input = empathy_audio_src_new ();
  priv->audio_output = empathy_audio_sink_new ();

/*
  gst_bin_add_many (GST_BIN (priv->pipeline),
    empathy_gtk_widget_get_element (
      EMPATHY_GST_GTK_WIDGET (priv->video_preview)),
    empathy_gtk_widget_get_element (
      EMPATHY_GST_GTK_WIDGET (priv->video_output)),
    priv->video_input,
    priv->audio_input,
    priv->audio_output,
    NULL);
  */

  g_object_unref (bus);

  gtk_box_pack_start (GTK_BOX (hbox), priv->video_preview, TRUE, TRUE, 3);
  gtk_box_pack_start (GTK_BOX (hbox), priv->video_output, TRUE, TRUE, 3);

  gtk_widget_show_all (hbox);

  g_signal_connect (G_OBJECT (self), "realize",
    G_CALLBACK (empathy_call_window_realized_cb), self);
}

static void empathy_call_window_dispose (GObject *object);
static void empathy_call_window_finalize (GObject *object);

static void
empathy_call_window_set_property (GObject *object,
  guint property_id, const GValue *value, GParamSpec *pspec)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (object);

  switch (property_id)
    {
      case PROP_CALL_HANDLER:
        priv->handler = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
empathy_call_window_get_property (GObject *object,
  guint property_id, GValue *value, GParamSpec *pspec)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (object);

  switch (property_id)
    {
      case PROP_CALL_HANDLER:
        g_value_set_object (value, priv->handler);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
empathy_call_window_class_init (
  EmpathyCallWindowClass *empathy_call_window_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (empathy_call_window_class);
  GParamSpec *param_spec;

  g_type_class_add_private (empathy_call_window_class,
    sizeof (EmpathyCallWindowPriv));

  object_class->set_property = empathy_call_window_set_property;
  object_class->get_property = empathy_call_window_get_property;

  object_class->dispose = empathy_call_window_dispose;
  object_class->finalize = empathy_call_window_finalize;

  param_spec = g_param_spec_object ("handler",
    "handler", "The call handler",
    EMPATHY_TYPE_CALL_HANDLER,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
    PROP_CALL_HANDLER, param_spec);

}

void
empathy_call_window_dispose (GObject *object)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (object);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->handler != NULL)
    g_object_unref (priv->handler);

  priv->handler = NULL;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (empathy_call_window_parent_class)->dispose)
    G_OBJECT_CLASS (empathy_call_window_parent_class)->dispose (object);
}

void
empathy_call_window_finalize (GObject *object)
{
  //EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (object);
  //EmpathyCallWindowPriv *priv = GET_PRIV (self);

  /* free any data held directly by the object here */

  G_OBJECT_CLASS (empathy_call_window_parent_class)->finalize (object);
}


EmpathyCallWindow *
empathy_call_window_new (EmpathyCallHandler *handler)
{
  return EMPATHY_CALL_WINDOW (
    g_object_new (EMPATHY_TYPE_CALL_WINDOW, "handler", handler, NULL));
}

static void
empathy_call_window_conference_added_cb (EmpathyCallHandler *handler,
  GstElement *conference, gpointer user_data)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (user_data);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  gst_bin_add (GST_BIN (priv->pipeline), conference);

  gst_element_set_state (conference, GST_STATE_PLAYING);

  printf ("ADDING CONFERENCE\n");
}

static void
empathy_call_window_src_added_cb (EmpathyCallHandler *handler,
  GstPad *src, guint media_type, gpointer user_data)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (user_data);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  GstPad *pad;
  GstElement *element;

  printf ("ADDING SRC PAD %d\n", media_type);

  switch (media_type)
    {
      case TP_MEDIA_STREAM_TYPE_AUDIO:
        element = priv->audio_output;
        break;
      case TP_MEDIA_STREAM_TYPE_VIDEO:
        element =
          empathy_video_widget_get_element (
            EMPATHY_VIDEO_WIDGET (priv->video_output));
        break;
      default:
        g_assert_not_reached ();
    }

  gst_bin_add (GST_BIN (priv->pipeline), element);

  pad = gst_element_get_static_pad (element, "sink");
  gst_element_set_state (element, GST_STATE_PLAYING);

  gst_pad_link (src, pad);
}

static void
empathy_call_window_sink_added_cb (EmpathyCallHandler *handler,
  GstPad *sink, guint media_type, gpointer user_data)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (user_data);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GstPad *pad;

  printf ("ADDING SINK PAD %d\n", media_type);

  switch (media_type)
    {
      case TP_MEDIA_STREAM_TYPE_AUDIO:
        gst_bin_add (GST_BIN (priv->pipeline), priv->audio_input);
        gst_element_set_state (priv->audio_input, GST_STATE_PLAYING);

        pad = gst_element_get_static_pad (priv->audio_input, "src");
        gst_pad_link (pad, sink);
        break;
      case TP_MEDIA_STREAM_TYPE_VIDEO:
        pad =  gst_element_get_request_pad (priv->video_tee, "src%d");
        gst_pad_link (pad, sink);
        break;
      default:
        g_assert_not_reached ();
    }

}

static void
empathy_call_window_realized_cb (GtkWidget *widget, EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);
  GstElement *preview;
  GstBus *bus;

  g_signal_connect (priv->handler, "conference-added",
    G_CALLBACK (empathy_call_window_conference_added_cb), window);
  g_signal_connect (priv->handler, "src-pad-added",
    G_CALLBACK (empathy_call_window_src_added_cb), window);
  g_signal_connect (priv->handler, "sink-pad-added",
    G_CALLBACK (empathy_call_window_sink_added_cb), window);

  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));
  empathy_call_handler_set_bus (priv->handler, bus);
  empathy_call_handler_start_call (priv->handler);

  preview = empathy_video_widget_get_element (
    EMPATHY_VIDEO_WIDGET (priv->video_preview));

  gst_bin_add_many (GST_BIN (priv->pipeline), priv->video_input,
    priv->video_tee, preview, NULL);
  gst_element_link_many (priv->video_input, priv->video_tee,
    preview, NULL);

  gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);

  g_object_unref (bus);
}

