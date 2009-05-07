/*
 * empathy-call-window.c - Source for EmpathyCallWindow
 * Copyright (C) 2009 Collabora Ltd.
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

#ifndef __EMPATHY_CALL_WINDOW_FULLSCREEN_H__
#define __EMPATHY_CALL_WINDOW_FULLSCREEN_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include "empathy-call-window.h"

G_BEGIN_DECLS

typedef struct _EmpathyCallWindowFullscreen EmpathyCallWindowFullscreen;
typedef struct _EmpathyCallWindowFullscreenClass EmpathyCallWindowFullscreenClass;

struct _EmpathyCallWindowFullscreenClass {
  GtkWindowClass parent_class;
};

struct _EmpathyCallWindowFullscreen {
  GtkWindow parent;
  gboolean is_fullscreen;

  GtkWidget *leave_fullscreen_button;

  /* Those fields represent the state of the parent empathy_call_window before 
     it actually was in fullscreen mode. */
  gboolean sidebar_was_visible;
  gint original_width;
  gint original_height;
};

GType empathy_call_window_fullscreen_get_type(void);

/* TYPE MACROS */
#define EMPATHY_TYPE_CALL_WINDOW_FULLSCREEN \
  (empathy_call_window_fullscreen_get_type())
#define EMPATHY_CALL_WINDOW_FULLSCREEN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), EMPATHY_TYPE_CALL_WINDOW_FULLSCREEN, \
    EmpathyCallWindowFullscreen))
#define EMPATHY_CALL_WINDOW_FULLSCREEN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EMPATHY_TYPE_CALL_WINDOW_FULLSCREEN, \
    EmpathyCallWindowClassFullscreen))
#define EMPATHY_IS_CALL_WINDOW_FULLSCREEN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), EMPATHY_TYPE_CALL_WINDOW_FULLSCREEN))
#define EMPATHY_IS_CALL_WINDOW_FULLSCREEN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EMPATHY_TYPE_CALL_WINDOW_FULLSCREEN))
#define EMPATHY_CALL_WINDOW_FULLSCREEN_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EMPATHY_TYPE_CALL_WINDOW_FULLSCREEN, \
    EmpathyCallWindowFullscreenClass))

EmpathyCallWindowFullscreen *
empathy_call_window_fullscreen_new (EmpathyCallWindow *parent);

void empathy_call_window_fullscreen_set_fullscreen (EmpathyCallWindowFullscreen *fs,
    gboolean sidebar_was_visible,
    gint original_width,
    gint original_height);
void empathy_call_window_fullscreen_unset_fullscreen (EmpathyCallWindowFullscreen *fs);
void empathy_call_window_fullscreen_set_video_widget (EmpathyCallWindowFullscreen *fs,
    GtkWidget *video_widget);

G_END_DECLS

#endif /* #ifndef __EMPATHY_CALL_WINDOW_FULLSCREEN_H__*/