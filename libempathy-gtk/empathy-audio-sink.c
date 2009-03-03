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

#include <gst/farsight/fs-element-added-notifier.h>

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

enum {
  PROP_VOLUME = 1,
};

/* private structure */
typedef struct _EmpathyGstAudioSinkPrivate EmpathyGstAudioSinkPrivate;

struct _EmpathyGstAudioSinkPrivate
{
  gboolean dispose_has_run;
  GstElement *sink;
  GstElement *volume;
  FsElementAddedNotifier *notifier;
};

#define EMPATHY_GST_AUDIO_SINK_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EMPATHY_TYPE_GST_AUDIO_SINK, \
  EmpathyGstAudioSinkPrivate))

static void
empathy_audio_sink_element_added_cb (FsElementAddedNotifier *notifier,
  GstBin *bin, GstElement *element, EmpathyGstAudioSink *self)
{
  EmpathyGstAudioSinkPrivate *priv = EMPATHY_GST_AUDIO_SINK_GET_PRIVATE (self);

  if (g_object_class_find_property (G_OBJECT_GET_CLASS (element), "volume"))
    {
      gdouble volume;

      volume = empathy_audio_sink_get_volume (self);
      empathy_audio_sink_set_volume (self, 1.0);

      if (priv->volume != NULL)
        g_object_unref (priv->volume);
      priv->volume = g_object_ref (element);

      if (volume != 1.0)
        empathy_audio_sink_set_volume (self, volume);
    }
}

static void
empathy_audio_sink_init (EmpathyGstAudioSink *obj)
{
  EmpathyGstAudioSinkPrivate *priv = EMPATHY_GST_AUDIO_SINK_GET_PRIVATE (obj);
  GstElement *resample;
  GstPad *ghost, *sink;

  priv->notifier = fs_element_added_notifier_new ();
  g_signal_connect (priv->notifier, "element-added",
    G_CALLBACK (empathy_audio_sink_element_added_cb), obj);

  resample = gst_element_factory_make ("audioresample", NULL);

  priv->volume = gst_element_factory_make ("volume", NULL);
  g_object_ref (priv->volume);

  priv->sink = gst_element_factory_make ("gconfaudiosink", NULL);

  fs_element_added_notifier_add (priv->notifier, GST_BIN (priv->sink));

  gst_bin_add_many (GST_BIN (obj), resample, priv->volume, priv->sink, NULL);
  gst_element_link_many (resample, priv->volume, priv->sink, NULL);

  sink = gst_element_get_static_pad (resample, "sink");

  ghost = gst_ghost_pad_new ("sink", sink);
  gst_element_add_pad (GST_ELEMENT (obj), ghost);

  gst_object_unref (G_OBJECT (sink));
}

static void empathy_audio_sink_dispose (GObject *object);
static void empathy_audio_sink_finalize (GObject *object);

static void
empathy_audio_sink_set_property (GObject *object,
  guint property_id, const GValue *value, GParamSpec *pspec)
{
  switch (property_id)
    {
      case PROP_VOLUME:
        empathy_audio_sink_set_volume (EMPATHY_GST_AUDIO_SINK (object),
          g_value_get_double (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
empathy_audio_sink_get_property (GObject *object,
  guint property_id, GValue *value, GParamSpec *pspec)
{
  switch (property_id)
    {
      case PROP_VOLUME:
        g_value_set_double (value,
          empathy_audio_sink_get_volume (EMPATHY_GST_AUDIO_SINK (object)));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
empathy_audio_sink_class_init (EmpathyGstAudioSinkClass
  *empathy_audio_sink_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (empathy_audio_sink_class);
  GParamSpec *param_spec;

  g_type_class_add_private (empathy_audio_sink_class,
    sizeof (EmpathyGstAudioSinkPrivate));

  object_class->dispose = empathy_audio_sink_dispose;
  object_class->finalize = empathy_audio_sink_finalize;

  object_class->set_property = empathy_audio_sink_set_property;
  object_class->get_property = empathy_audio_sink_get_property;

  param_spec = g_param_spec_double ("volume", "Volume", "volume control",
    0.0, 5.0, 1.0,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_VOLUME, param_spec);
}

void
empathy_audio_sink_dispose (GObject *object)
{
  EmpathyGstAudioSink *self = EMPATHY_GST_AUDIO_SINK (object);
  EmpathyGstAudioSinkPrivate *priv = EMPATHY_GST_AUDIO_SINK_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->notifier != NULL)
    g_object_unref (priv->notifier);
  priv->notifier = NULL;

  if (priv->volume != NULL)
    g_object_unref (priv->volume);
  priv->volume = NULL;

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

void
empathy_audio_sink_set_volume (EmpathyGstAudioSink *sink, gdouble volume)
{
  EmpathyGstAudioSinkPrivate *priv = EMPATHY_GST_AUDIO_SINK_GET_PRIVATE (sink);

  g_object_set (G_OBJECT (priv->volume), "volume", volume, NULL);
}

gdouble
empathy_audio_sink_get_volume (EmpathyGstAudioSink *sink)
{
  EmpathyGstAudioSinkPrivate *priv = EMPATHY_GST_AUDIO_SINK_GET_PRIVATE (sink);
  gdouble volume;

  g_object_get (G_OBJECT (priv->volume), "volume", &volume, NULL);

  return volume;
}
