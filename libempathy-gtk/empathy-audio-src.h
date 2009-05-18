/*
 * empathy-gst-video-src.h - Header for EmpathyGstAudioSrc
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

#ifndef __EMPATHY_GST_AUDIO_SRC_H__
#define __EMPATHY_GST_AUDIO_SRC_H__

#include <glib-object.h>
#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _EmpathyGstAudioSrc EmpathyGstAudioSrc;
typedef struct _EmpathyGstAudioSrcClass EmpathyGstAudioSrcClass;

struct _EmpathyGstAudioSrcClass {
    GstBinClass parent_class;
};

struct _EmpathyGstAudioSrc {
    GstBin parent;
};

GType empathy_audio_src_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_GST_AUDIO_SRC \
  (empathy_audio_src_get_type ())
#define EMPATHY_GST_AUDIO_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), EMPATHY_TYPE_GST_AUDIO_SRC, \
    EmpathyGstAudioSrc))
#define EMPATHY_GST_AUDIO_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EMPATHY_TYPE_GST_AUDIO_SRC, \
    EmpathyGstAudioSrcClass))
#define EMPATHY_IS_GST_AUDIO_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), EMPATHY_TYPE_GST_AUDIO_SRC))
#define EMPATHY_IS_GST_AUDIO_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EMPATHY_TYPE_GST_AUDIO_SRC))
#define EMPATHY_GST_AUDIO_SRC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EMPATHY_TYPE_GST_AUDIO_SRC, \
    EmpathyGstAudioSrcClass))

GstElement *empathy_audio_src_new (void);

void empathy_audio_src_set_volume (EmpathyGstAudioSrc *src, gdouble volume);
gdouble empathy_audio_src_get_volume (EmpathyGstAudioSrc *src);

G_END_DECLS

#endif /* #ifndef __EMPATHY_GST_AUDIO_SRC_H__*/
