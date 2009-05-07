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

#include "empathy-call-window-fullscreen.h"

#include <gtk/gtk.h>

#include <libempathy/empathy-utils.h>
#include <libempathy-gtk/empathy-ui-utils.h>

/* The number of seconds fo which the "leave fullscreen" popup should be shown */
#define FULLSCREEN_POPUP_TIMEOUT 5

G_DEFINE_TYPE (EmpathyCallWindowFullscreen, empathy_call_window_fullscreen, GTK_TYPE_WINDOW)

/* private structure */
typedef struct _EmpathyCallWindowFullscreenPriv EmpathyCallWindowFullscreenPriv;

struct _EmpathyCallWindowFullscreenPriv
{
  EmpathyCallWindow *parent_window;
    
  GtkWidget *leave_fullscreen_popup;
  GtkWidget *video_widget;

  gulong motion_handler_id;
  guint popup_timeout;
  gboolean popup_creation_in_progress;
  gboolean dispose_has_run;
};

#define GET_PRIV(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EMPATHY_TYPE_CALL_WINDOW_FULLSCREEN, \
    EmpathyCallWindowFullscreenPriv))

static void empathy_call_window_fullscreen_dispose (GObject *object);
static void empathy_call_window_fullscreen_finalize (GObject *object);

static gboolean empathy_call_window_fullscreen_motion_notify (GtkWidget *widget,
    GdkEventMotion *event, EmpathyCallWindowFullscreen *fs);
static gboolean empathy_call_window_fullscreen_hide_popup (EmpathyCallWindowFullscreen *fs);

static void
empathy_call_window_fullscreen_set_cursor_visible (EmpathyCallWindowFullscreen *fs,
    gboolean show_cursor)
{
  EmpathyCallWindowFullscreenPriv *priv = GET_PRIV (fs);

  if (priv->video_widget != NULL && !show_cursor)
    gdk_window_set_cursor (priv->video_widget->window, gdk_cursor_new (GDK_BLANK_CURSOR));
  else
    gdk_window_set_cursor (priv->video_widget->window, NULL);
}

static void
empathy_call_window_fullscreen_add_popup_timeout (EmpathyCallWindowFullscreen *self)
{
  EmpathyCallWindowFullscreenPriv *priv = GET_PRIV (self);

  if (priv->popup_timeout == 0)
    {
  	  priv->popup_timeout = g_timeout_add_seconds (FULLSCREEN_POPUP_TIMEOUT,
			    (GSourceFunc) empathy_call_window_fullscreen_hide_popup, self);
    }
}

static void
empathy_call_window_fullscreen_remove_popup_timeout (EmpathyCallWindowFullscreen *self)
{
  EmpathyCallWindowFullscreenPriv *priv = GET_PRIV (self);

  if (priv->popup_timeout != 0)
    {
		  g_source_remove (priv->popup_timeout);
		  priv->popup_timeout = 0;
  	}
}

static void
empathy_call_window_fullscreen_show_popup (EmpathyCallWindowFullscreen *self)
{
  g_assert (self->is_fullscreen);

  gint leave_fullscreen_width, leave_fullscreen_height;
	GdkScreen *screen;
	GdkRectangle fullscreen_rect;
	EmpathyCallWindowFullscreenPriv *priv = GET_PRIV (self);

  g_return_if_fail (priv->parent_window != NULL);

  if (priv->popup_creation_in_progress)
    return;

  if (!gtk_window_is_active (GTK_WINDOW (priv->parent_window)))
		return;

  priv->popup_creation_in_progress = TRUE;

  empathy_call_window_fullscreen_set_cursor_visible (self, TRUE);

	/* Obtaining the screen rectangle */
	screen = gtk_window_get_screen (GTK_WINDOW (priv->parent_window));
	gdk_screen_get_monitor_geometry (screen,
	    gdk_screen_get_monitor_at_window (screen, GTK_WIDGET (priv->parent_window)->window),
			    &fullscreen_rect);

	/* Getting the popup window sizes */
	gtk_window_get_size (GTK_WINDOW (priv->leave_fullscreen_popup),
			     &leave_fullscreen_width, &leave_fullscreen_height);

  /* Moving the popup to the top-left corner */
  gtk_window_move (GTK_WINDOW (priv->leave_fullscreen_popup),
      fullscreen_rect.width + fullscreen_rect.x - leave_fullscreen_width,
      fullscreen_rect.y);

  gtk_widget_show_all (priv->leave_fullscreen_popup);
  empathy_call_window_fullscreen_add_popup_timeout (self);
  
  priv->popup_creation_in_progress = FALSE;
}

static gboolean
empathy_call_window_fullscreen_hide_popup (EmpathyCallWindowFullscreen *fs)
{
  EmpathyCallWindowFullscreenPriv *priv = GET_PRIV (fs);

  if (priv->video_widget == NULL || !fs->is_fullscreen)
    return TRUE;

  gtk_widget_hide (priv->leave_fullscreen_popup);
  empathy_call_window_fullscreen_remove_popup_timeout (fs);

  empathy_call_window_fullscreen_set_cursor_visible (fs, FALSE);

  return FALSE;
}

static void
empathy_call_window_fullscreen_init (EmpathyCallWindowFullscreen *self)
{
  EmpathyCallWindowFullscreenPriv *priv = GET_PRIV (self);
  GtkBuilder *gui;
  gchar *filename;

  filename = empathy_file_lookup ("empathy-call-window-fullscreen.ui", "src");
  gui = empathy_builder_get_file (filename,
    "leave_fullscreen_window", &priv->leave_fullscreen_popup,
    "leave_fullscreen_button", &self->leave_fullscreen_button,
    NULL); 

  gtk_widget_add_events (priv->leave_fullscreen_popup, GDK_POINTER_MOTION_MASK);

  g_object_unref (gui);
  g_free (filename);
}

static void
empathy_call_window_fullscreen_class_init (
  EmpathyCallWindowFullscreenClass *empathy_call_window_fullscreen_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (empathy_call_window_fullscreen_class);

  g_type_class_add_private (empathy_call_window_fullscreen_class,
    sizeof (EmpathyCallWindowFullscreenPriv));

  object_class->dispose = empathy_call_window_fullscreen_dispose;
  object_class->finalize = empathy_call_window_fullscreen_finalize;
}

void
empathy_call_window_fullscreen_dispose (GObject *object)
{
  EmpathyCallWindowFullscreen *self = EMPATHY_CALL_WINDOW_FULLSCREEN (object);
  EmpathyCallWindowFullscreenPriv *priv = GET_PRIV (self);

  if (priv->dispose_has_run)
    return;

  if (priv->parent_window != NULL)
    g_object_unref (priv->parent_window);
  priv->parent_window = NULL;
  
  if (priv->leave_fullscreen_popup != NULL)
    gtk_widget_destroy (priv->leave_fullscreen_popup);
  priv->leave_fullscreen_popup = NULL;

  if (self->leave_fullscreen_button != NULL)
    gtk_widget_destroy (self->leave_fullscreen_button);
  self->leave_fullscreen_button = NULL;

  if (G_OBJECT_CLASS (empathy_call_window_fullscreen_parent_class)->dispose)
    G_OBJECT_CLASS (empathy_call_window_fullscreen_parent_class)->dispose (object);

  priv->dispose_has_run = TRUE;  
}

void
empathy_call_window_fullscreen_finalize (GObject *object)
{
  EmpathyCallWindowFullscreen *self = EMPATHY_CALL_WINDOW_FULLSCREEN (object);
  EmpathyCallWindowFullscreenPriv *priv = GET_PRIV (self);

  empathy_call_window_fullscreen_remove_popup_timeout (self);

  if (priv->motion_handler_id != 0)
    {
		  g_signal_handler_disconnect (G_OBJECT (priv->video_widget),
			    priv->motion_handler_id);
		  priv->motion_handler_id = 0;
	}

  G_OBJECT_CLASS (empathy_call_window_fullscreen_parent_class)->finalize (object);
}

static void
empathy_call_window_fullscreen_parent_window_notify (GtkWidget *parent_window,
    GParamSpec *property, EmpathyCallWindowFullscreen *fs)
{
  EmpathyCallWindowFullscreenPriv *priv = GET_PRIV (fs);

	if (!fs->is_fullscreen)
		return;

	if (parent_window == GTK_WIDGET (priv->parent_window) &&
	      gtk_window_is_active (GTK_WINDOW (parent_window)) == FALSE)
    {
  		empathy_call_window_fullscreen_hide_popup (fs);
      empathy_call_window_fullscreen_set_cursor_visible (fs, TRUE);
    }
}

EmpathyCallWindowFullscreen *
empathy_call_window_fullscreen_new (EmpathyCallWindow *parent_window)
{
  EmpathyCallWindowFullscreen *self = EMPATHY_CALL_WINDOW_FULLSCREEN (
    g_object_new (EMPATHY_TYPE_CALL_WINDOW_FULLSCREEN, NULL));
  EmpathyCallWindowFullscreenPriv *priv = GET_PRIV (self);

  priv->parent_window = parent_window;
  g_signal_connect (G_OBJECT (priv->parent_window), "notify::is-active",
			  G_CALLBACK (empathy_call_window_fullscreen_parent_window_notify), self);

  return self;
}

void
empathy_call_window_fullscreen_set_fullscreen (EmpathyCallWindowFullscreen *fs,
  gboolean sidebar_was_visible,
  gint original_width,
  gint original_height)
{
  g_return_if_fail (!fs->is_fullscreen);

  EmpathyCallWindowFullscreenPriv *priv = GET_PRIV (fs);
  
  empathy_call_window_fullscreen_remove_popup_timeout (fs);
  empathy_call_window_fullscreen_set_cursor_visible (fs, FALSE);
  
  fs->sidebar_was_visible = sidebar_was_visible;
  fs->original_width = original_width;
  fs->original_height = original_height;

  if (priv->motion_handler_id == 0 && priv->video_widget != NULL)
    {
	    priv->motion_handler_id = g_signal_connect (G_OBJECT (priv->video_widget),
        "motion-notify-event", G_CALLBACK (empathy_call_window_fullscreen_motion_notify),
        fs);
  	}
  
  fs->is_fullscreen = TRUE;
}

void
empathy_call_window_fullscreen_unset_fullscreen (EmpathyCallWindowFullscreen *fs)
{
  g_return_if_fail (fs->is_fullscreen);

  EmpathyCallWindowFullscreenPriv *priv = GET_PRIV (fs);
 
  empathy_call_window_fullscreen_hide_popup (fs);
  empathy_call_window_fullscreen_set_cursor_visible (fs, TRUE);

  if (priv->motion_handler_id != 0)
    {
		  g_signal_handler_disconnect (G_OBJECT (priv->video_widget),
          priv->motion_handler_id);
		  priv->motion_handler_id = 0;
    }

  fs->is_fullscreen = FALSE;
}

void
empathy_call_window_fullscreen_set_video_widget (EmpathyCallWindowFullscreen *fs,
    GtkWidget *video_widget)
{
  EmpathyCallWindowFullscreenPriv *priv = GET_PRIV (fs);

  priv->video_widget = video_widget;
 
	if (fs->is_fullscreen == TRUE && priv->motion_handler_id == 0) {
		  priv->motion_handler_id = g_signal_connect (G_OBJECT (priv->video_widget),
          "motion-notify-event", G_CALLBACK (empathy_call_window_fullscreen_motion_notify),
          fs);
	}
}

static gboolean
empathy_call_window_fullscreen_motion_notify (GtkWidget *widget,
    GdkEventMotion *event, EmpathyCallWindowFullscreen *self)
{
  empathy_call_window_fullscreen_show_popup (self);
  return FALSE;
}