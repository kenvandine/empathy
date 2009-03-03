/*
 * empathy-gst-audio-src.c - Source for EmpathyGstAudioSrc
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
#include "empathy-audio-src.h"

G_DEFINE_TYPE(EmpathyGstAudioSrc, empathy_audio_src, GST_TYPE_BIN)

/* signal enum */
enum
{
    PEAK_LEVEL_CHANGED,
    RMS_LEVEL_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

enum {
    PROP_VOLUME = 1,
    PROP_RMS_LEVEL,
    PROP_PEAK_LEVEL,
};

/* private structure */
typedef struct _EmpathyGstAudioSrcPrivate EmpathyGstAudioSrcPrivate;

struct _EmpathyGstAudioSrcPrivate
{
  gboolean dispose_has_run;
  GstElement *src;
  GstElement *volume;
  GstElement *level;
  FsElementAddedNotifier *notifier;

  gdouble peak_level;
  gdouble rms_level;

  GMutex *lock;
  guint idle_id;
};

#define EMPATHY_GST_AUDIO_SRC_GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EMPATHY_TYPE_GST_AUDIO_SRC, \
  EmpathyGstAudioSrcPrivate))

static void
empathy_audio_src_element_added_cb (FsElementAddedNotifier *notifier,
  GstBin *bin, GstElement *element, EmpathyGstAudioSrc *self)
{
  EmpathyGstAudioSrcPrivate *priv = EMPATHY_GST_AUDIO_SRC_GET_PRIVATE (self);

  if (g_object_class_find_property (G_OBJECT_GET_CLASS (element), "volume"))
    {
      gdouble volume;

      volume = empathy_audio_src_get_volume (self);
      empathy_audio_src_set_volume (self, 1.0);

      if (priv->volume != NULL)
        g_object_unref (priv->volume);
      priv->volume = g_object_ref (element);

      if (volume != 1.0)
        empathy_audio_src_set_volume (self, volume);
    }
}

static void
empathy_audio_src_init (EmpathyGstAudioSrc *obj)
{
  EmpathyGstAudioSrcPrivate *priv = EMPATHY_GST_AUDIO_SRC_GET_PRIVATE (obj);
  GstPad *ghost, *src;

  priv->peak_level = -G_MAXDOUBLE;
  priv->lock = g_mutex_new ();

  priv->notifier = fs_element_added_notifier_new ();
  g_signal_connect (priv->notifier, "element-added",
    G_CALLBACK (empathy_audio_src_element_added_cb), obj);

  priv->src = gst_element_factory_make ("gconfaudiosrc", NULL);
  gst_bin_add (GST_BIN (obj), priv->src);

  fs_element_added_notifier_add (priv->notifier, GST_BIN (priv->src));

  priv->volume = gst_element_factory_make ("volume", NULL);
  g_object_ref (priv->volume);

  gst_bin_add (GST_BIN (obj), priv->volume);
  gst_element_link (priv->src, priv->volume);

  priv->level = gst_element_factory_make ("level", NULL);
  gst_bin_add (GST_BIN (obj), priv->level);
  gst_element_link (priv->volume, priv->level);

  src = gst_element_get_static_pad (priv->level, "src");

  ghost = gst_ghost_pad_new ("src", src);
  gst_element_add_pad (GST_ELEMENT (obj), ghost);

  gst_object_unref (G_OBJECT (src));
}

static void empathy_audio_src_dispose (GObject *object);
static void empathy_audio_src_finalize (GObject *object);
static void empathy_audio_src_handle_message (GstBin *bin,
  GstMessage *message);

static gboolean empathy_audio_src_levels_updated (gpointer user_data);

static void
empathy_audio_src_set_property (GObject *object,
  guint property_id, const GValue *value, GParamSpec *pspec)
{
  switch (property_id)
    {
      case PROP_VOLUME:
        empathy_audio_src_set_volume (EMPATHY_GST_AUDIO_SRC (object),
          g_value_get_double (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
empathy_audio_src_get_property (GObject *object,
  guint property_id, GValue *value, GParamSpec *pspec)
{
  EmpathyGstAudioSrc *self = EMPATHY_GST_AUDIO_SRC (object);
  EmpathyGstAudioSrcPrivate *priv = EMPATHY_GST_AUDIO_SRC_GET_PRIVATE (self);

  switch (property_id)
    {
      case PROP_VOLUME:
        g_value_set_double (value,
          empathy_audio_src_get_volume (self));
        break;
      case PROP_PEAK_LEVEL:
        g_mutex_lock (priv->lock);
        g_value_set_double (value, priv->peak_level);
        g_mutex_unlock (priv->lock);
        break;
      case PROP_RMS_LEVEL:
        g_mutex_lock (priv->lock);
        g_value_set_double (value, priv->rms_level);
        g_mutex_unlock (priv->lock);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
empathy_audio_src_class_init (EmpathyGstAudioSrcClass
  *empathy_audio_src_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (empathy_audio_src_class);
  GstBinClass *gstbin_class = GST_BIN_CLASS (empathy_audio_src_class);
  GParamSpec *param_spec;

  g_type_class_add_private (empathy_audio_src_class,
    sizeof (EmpathyGstAudioSrcPrivate));

  object_class->dispose = empathy_audio_src_dispose;
  object_class->finalize = empathy_audio_src_finalize;

  object_class->set_property = empathy_audio_src_set_property;
  object_class->get_property = empathy_audio_src_get_property;

  gstbin_class->handle_message =
    GST_DEBUG_FUNCPTR (empathy_audio_src_handle_message);

  param_spec = g_param_spec_double ("volume", "Volume", "volume contol",
    0.0, 5.0, 1.0,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_VOLUME, param_spec);

  param_spec = g_param_spec_double ("peak-level", "peak level", "peak level",
    -G_MAXDOUBLE, G_MAXDOUBLE, 0,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_VOLUME, param_spec);

  signals[PEAK_LEVEL_CHANGED] = g_signal_new ("peak-level-changed",
    G_TYPE_FROM_CLASS (empathy_audio_src_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__DOUBLE,
    G_TYPE_NONE, 1, G_TYPE_DOUBLE);

  param_spec = g_param_spec_double ("rms-level", "RMS level", "RMS level",
    -G_MAXDOUBLE, G_MAXDOUBLE, 0,
    G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_VOLUME, param_spec);


  signals[RMS_LEVEL_CHANGED] = g_signal_new ("rms-level-changed",
    G_TYPE_FROM_CLASS (empathy_audio_src_class),
    G_SIGNAL_RUN_LAST,
    0,
    NULL, NULL,
    g_cclosure_marshal_VOID__DOUBLE,
    G_TYPE_NONE, 1, G_TYPE_DOUBLE);
}

void
empathy_audio_src_dispose (GObject *object)
{
  EmpathyGstAudioSrc *self = EMPATHY_GST_AUDIO_SRC (object);
  EmpathyGstAudioSrcPrivate *priv = EMPATHY_GST_AUDIO_SRC_GET_PRIVATE (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->idle_id != 0)
    g_source_remove (priv->idle_id);

  priv->idle_id = 0;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (empathy_audio_src_parent_class)->dispose)
    G_OBJECT_CLASS (empathy_audio_src_parent_class)->dispose (object);
}

void
empathy_audio_src_finalize (GObject *object)
{
  EmpathyGstAudioSrc *self = EMPATHY_GST_AUDIO_SRC (object);
  EmpathyGstAudioSrcPrivate *priv = EMPATHY_GST_AUDIO_SRC_GET_PRIVATE (self);

  /* free any data held directly by the object here */
  g_mutex_free (priv->lock);

  G_OBJECT_CLASS (empathy_audio_src_parent_class)->finalize (object);
}

static gboolean
empathy_audio_src_levels_updated (gpointer user_data)
{
  EmpathyGstAudioSrc *self = EMPATHY_GST_AUDIO_SRC (user_data);
  EmpathyGstAudioSrcPrivate *priv = EMPATHY_GST_AUDIO_SRC_GET_PRIVATE (self);

  g_mutex_lock (priv->lock);

  g_signal_emit (self, signals[PEAK_LEVEL_CHANGED], 0, priv->peak_level);
  g_signal_emit (self, signals[RMS_LEVEL_CHANGED], 0, priv->rms_level);
  priv->idle_id = 0;

  g_mutex_unlock (priv->lock);

  return FALSE;
}

static void
empathy_audio_src_handle_message (GstBin *bin, GstMessage *message)
{
  EmpathyGstAudioSrc *self = EMPATHY_GST_AUDIO_SRC (bin);
  EmpathyGstAudioSrcPrivate *priv = EMPATHY_GST_AUDIO_SRC_GET_PRIVATE (self);

  if  (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ELEMENT &&
        GST_MESSAGE_SRC (message) == GST_OBJECT (priv->level))
    {
      const GstStructure *s;
      const gchar *name;
      const GValue *list;
      guint i, len;
      gdouble peak = -G_MAXDOUBLE;
      gdouble rms = -G_MAXDOUBLE;

      s = gst_message_get_structure (message);
      name = gst_structure_get_name (s);

      if (g_strcmp0 ("level", name) != 0)
        goto out;

      list = gst_structure_get_value (s, "peak");
      len = gst_value_list_get_size (list);

      for (i =0 ; i < len; i++)
        {
          const GValue *value;
          gdouble db;

          value = gst_value_list_get_value (list, i);
          db = g_value_get_double (value);
          peak = MAX (db, peak);
        }

      list = gst_structure_get_value (s, "rms");
      len = gst_value_list_get_size (list);

      for (i =0 ; i < len; i++)
        {
          const GValue *value;
          gdouble db;

          value = gst_value_list_get_value (list, i);
          db = g_value_get_double (value);
          rms = MAX (db, rms);
        }

      g_mutex_lock (priv->lock);

      priv->peak_level = peak;
      priv->rms_level = rms;
      if (priv->idle_id == 0)
        priv->idle_id = g_idle_add (empathy_audio_src_levels_updated, self);

      g_mutex_unlock (priv->lock);
    }

out:
   GST_BIN_CLASS (empathy_audio_src_parent_class)->handle_message (bin,
    message);
}

GstElement *
empathy_audio_src_new (void)
{
  return GST_ELEMENT (g_object_new (EMPATHY_TYPE_GST_AUDIO_SRC, NULL));
}

void
empathy_audio_src_set_volume (EmpathyGstAudioSrc *src, gdouble volume)
{
  EmpathyGstAudioSrcPrivate *priv = EMPATHY_GST_AUDIO_SRC_GET_PRIVATE (src);
  GParamSpec *pspec;
  GParamSpecDouble *pspec_double;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (priv->volume),
    "volume");

  g_assert (pspec != NULL);

  pspec_double = G_PARAM_SPEC_DOUBLE (pspec);

  volume = CLAMP (volume, pspec_double->minimum, pspec_double->maximum);

  g_object_set (G_OBJECT (priv->volume), "volume", volume, NULL);
}

gdouble
empathy_audio_src_get_volume (EmpathyGstAudioSrc *src)
{
  EmpathyGstAudioSrcPrivate *priv = EMPATHY_GST_AUDIO_SRC_GET_PRIVATE (src);
  gdouble volume;

  g_object_get (G_OBJECT (priv->volume), "volume", &volume, NULL);

  return volume;
}
