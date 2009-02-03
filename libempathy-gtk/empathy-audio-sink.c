/*
 * empathy-gst-audio-sink.c - Source for EmpathyGstAudioSink
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

#include "empathy-audio-sink.h"

G_DEFINE_TYPE(EmpathyGstAudioSink, empathy_audio_sink, GST_TYPE_BIN)

/* signal enum */
#if 0
enum
{
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};
#endif

/* private structure */
typedef struct _EmpathyGstAudioSinkPrivate EmpathyGstAudioSinkPrivate;

struct _EmpathyGstAudioSinkPrivate
{
  gboolean dispose_has_run;
  GstElement *sink;
};

#define EMPATHY_GST_AUDIO_SINK_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EMPATHY_TYPE_GST_AUDIO_SINK, \
  EmpathyGstAudioSinkPrivate))

static void
empathy_audio_sink_init (EmpathyGstAudioSink *obj)
{
  EmpathyGstAudioSinkPrivate *priv = EMPATHY_GST_AUDIO_SINK_GET_PRIVATE (obj);
  GstElement *resample;
  GstPad *ghost, *sink;

  /* allocate any data required by the object here */
  resample = gst_element_factory_make ("audioresample", NULL);

  priv->sink = gst_element_factory_make ("gconfaudiosink", NULL);

  gst_bin_add_many (GST_BIN (obj), resample, priv->sink, NULL);
  gst_element_link_many (resample, priv->sink, NULL);

  sink = gst_element_get_static_pad (resample, "sink");

  ghost = gst_ghost_pad_new ("sink", sink);
  gst_element_add_pad (GST_ELEMENT (obj), ghost);

  gst_object_unref (G_OBJECT (sink));
}

static void empathy_audio_sink_dispose (GObject *object);
static void empathy_audio_sink_finalize (GObject *object);

static void
empathy_audio_sink_class_init (EmpathyGstAudioSinkClass
  *empathy_audio_sink_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (empathy_audio_sink_class);

  g_type_class_add_private (empathy_audio_sink_class,
    sizeof (EmpathyGstAudioSinkPrivate));

  object_class->dispose = empathy_audio_sink_dispose;
  object_class->finalize = empathy_audio_sink_finalize;
}

void
empathy_audio_sink_dispose (GObject *object)
{
  EmpathyGstAudioSink *self = EMPATHY_GST_AUDIO_SINK (object);
  EmpathyGstAudioSinkPrivate *priv = EMPATHY_GST_AUDIO_SINK_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (empathy_audio_sink_parent_class)->dispose)
    G_OBJECT_CLASS (empathy_audio_sink_parent_class)->dispose (object);
}

void
empathy_audio_sink_finalize (GObject *object)
{
  //EmpathyGstAudioSink *self = EMPATHY_GST_AUDIO_SINK (object);
  //EmpathyGstAudioSinkPrivate *priv =
  //  EMPATHY_GST_AUDIO_SINK_GET_PRIVATE (self);

  /* free any data held directly by the object here */

  G_OBJECT_CLASS (empathy_audio_sink_parent_class)->finalize (object);
}

GstElement *
empathy_audio_sink_new (void)
{
  return GST_ELEMENT (g_object_new (EMPATHY_TYPE_GST_AUDIO_SINK, NULL));
}
