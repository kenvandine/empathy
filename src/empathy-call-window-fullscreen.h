/*
 * empathy-call-window-fullscreen.h - Header for EmpathyCallWindowFullscreen
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
typedef struct _EmpathyCallWindowFullscreenClass
    EmpathyCallWindowFullscreenClass;

struct _EmpathyCallWindowFullscreenClass {
  GObjectClass parent_class;
};

struct _EmpathyCallWindowFullscreen {
  GObject parent;
  gboolean is_fullscreen;
  GtkWidget *leave_fullscreen_button;
};

GType empathy_call_window_fullscreen_get_type (void);

/* TYPE MACROS */
#define EMPATHY_TYPE_CALL_WINDOW_FULLSCREEN \
  (empathy_call_window_fullscreen_get_type ())
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

EmpathyCallWindowFullscreen *empathy_call_window_fullscreen_new (
    EmpathyCallWindow *parent);

void empathy_call_window_fullscreen_set_fullscreen (
    EmpathyCallWindowFullscreen *fs,
    gboolean set_fullscreen);
void empathy_call_window_fullscreen_set_video_widget (
    EmpathyCallWindowFullscreen *fs,
    GtkWidget *video_widget);
void empathy_call_window_fullscreen_show_popup (
    EmpathyCallWindowFullscreen *fs);

G_END_DECLS

#endif /* #ifndef __EMPATHY_CALL_WINDOW_FULLSCREEN_H__*/
