/*
 *  Copyright (C) 2007 Elliot Fairweather
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Authors: Elliot Fairweather <elliot.fairweather@collabora.co.uk>
 */

#include <string.h>

#include <libtelepathy/tp-chan.h>

#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mc-account-monitor.h>
#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-tp-call.h>
#include <libempathy/empathy-chandler.h>
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-utils.h>

#include <libempathy-gtk/empathy-call-window.h>
#include <libempathy-gtk/empathy-ui-utils.h>

#define DEBUG_DOMAIN "CallWindow"

typedef struct 
{
  GtkWidget *window;
  GtkWidget *status_label;
  GtkWidget *start_call_button;
  GtkWidget *end_call_button;
  GtkWidget *input_volume_scale;
  GtkWidget *output_volume_scale;
  GtkWidget *input_mute_button;
  GtkWidget *output_mute_button;
  GtkWidget *preview_video_frame;
  GtkWidget *output_video_frame;
  GtkWidget *preview_video_socket;
  GtkWidget *output_video_socket;
  GtkWidget *video_button;
  GtkWidget *output_video_label;

  EmpathyTpCall *call;

  GTimeVal start_time;
  guint timeout_event_id;

  gboolean is_drawing;
} EmpathyCallWindow;

static gboolean
call_window_update_timer (gpointer data)
{
  EmpathyCallWindow *window = data;
  GTimeVal current;
  gchar *str;
  glong now, then;
  glong time, seconds, minutes, hours;

  g_get_current_time (&current);

  now = current.tv_sec;
  then = (window->start_time).tv_sec;

  time = now - then;

  seconds = time % 60;
  time /= 60;
  minutes = time % 60;
  time /= 60;
  hours = time % 60;

  if (hours > 0)
    {
      str = g_strdup_printf ("Connected  -  %02ld : %02ld : %02ld", hours,
          minutes, seconds);
    }
  else
    {
      str = g_strdup_printf ("Connected  -  %02ld : %02ld", minutes, seconds);
    }

  gtk_label_set_text (GTK_LABEL (window->status_label), str);

  g_free (str);

  return TRUE;
}

static void
call_window_stop_timeout (EmpathyCallWindow *window)
{
  GMainContext *context;
  GSource *source;

  context = g_main_context_default ();

  empathy_debug (DEBUG_DOMAIN, "Timer stopped");

  if (window->timeout_event_id)
    {
      source = g_main_context_find_source_by_id (context,
          window->timeout_event_id);
      g_source_destroy (source);
      window->timeout_event_id = 0;
    }
}

static void
call_window_set_output_video_is_drawing (EmpathyCallWindow *window,
                                         gboolean is_drawing)
{
  GtkWidget* child;

  child = gtk_bin_get_child (GTK_BIN (window->output_video_frame));

  empathy_debug (DEBUG_DOMAIN,
      "Setting output video is drawing - %d", is_drawing);

  if (is_drawing)
    {
      if (!window->is_drawing)
        {
          if (child)
            {
              gtk_container_remove (GTK_CONTAINER (window->output_video_frame),
                  child);
            }
          gtk_container_add (GTK_CONTAINER (window->output_video_frame),
              window->output_video_socket);
          gtk_widget_show (window->output_video_socket);
          empathy_tp_call_add_output_video (window->call,
              gtk_socket_get_id (GTK_SOCKET (window->output_video_socket)));
          window->is_drawing = is_drawing;
        }
    }
  else
    {
      if (window->is_drawing)
        {
          empathy_tp_call_add_output_video (window->call, 0);
          if (child)
            {
              gtk_container_remove (GTK_CONTAINER (window->output_video_frame),
                  child);
            }
          gtk_container_add (GTK_CONTAINER (window->output_video_frame),
              window->output_video_label);
          gtk_widget_show (window->output_video_label);
          window->is_drawing = is_drawing;
        }
    }
}

static gboolean
call_window_delete_event_cb (GtkWidget *widget,
                             GdkEvent *event,
                             EmpathyCallWindow *window)
{
  GtkWidget *dialog;
  gint result;
  guint status;

  empathy_debug (DEBUG_DOMAIN, "Delete event occurred");

  g_object_get (G_OBJECT (window->call), "status", &status, NULL);

  if (status != EMPATHY_TP_CALL_STATUS_CLOSED)
    {
      dialog = gtk_message_dialog_new (GTK_WINDOW (window->window),
          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
          GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
          "This call will be ended. Continue?");

      result = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      switch (result)
        {
        case GTK_RESPONSE_YES:
          call_window_stop_timeout (window);
          call_window_set_output_video_is_drawing (window, FALSE);
          empathy_tp_call_close_channel (window->call);
          empathy_tp_call_remove_preview_video (window->call,
              gtk_socket_get_id (GTK_SOCKET (window->preview_video_socket)));
          return FALSE;
        default:
          return TRUE;
        }
    }
  else
    {
      empathy_tp_call_remove_preview_video (window->call,
          gtk_socket_get_id (GTK_SOCKET (window->preview_video_socket)));
      return FALSE;
    }
}

static void
call_window_video_button_toggled_cb (GtkWidget *button,
                                     EmpathyCallWindow *window)
{
  gboolean is_sending;

  is_sending = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  empathy_debug (DEBUG_DOMAIN, "Send video toggled - %d", is_sending);

  empathy_tp_call_request_video_stream_direction (window->call, is_sending);
}

static void
call_window_status_changed_cb (EmpathyTpCall *call,
                               EmpathyCallWindow *window)
{
  EmpathyContact *contact;
  guint status;
  guint stream_state;
  EmpathyTpCallStream *audio_stream;
  EmpathyTpCallStream *video_stream;
  gboolean is_incoming;
  gchar *title;

  g_object_get (G_OBJECT (window->call), "status", &status, NULL);
  g_object_get (G_OBJECT (window->call), "audio-stream", &audio_stream, NULL);
  g_object_get (G_OBJECT (window->call), "video-stream", &video_stream, NULL);

  if (video_stream->state > audio_stream->state)
    {
      stream_state = video_stream->state;
    }
  else
    {
      stream_state = audio_stream->state;
    }

  empathy_debug (DEBUG_DOMAIN, "Status changed - status: %d, stream state: %d",
      status, stream_state);

  if (window->timeout_event_id)
    {
      call_window_stop_timeout (window);
    }

  if (status == EMPATHY_TP_CALL_STATUS_CLOSED)
    {
      gtk_label_set_text (GTK_LABEL (window->status_label), "Closed");
      gtk_widget_set_sensitive (window->end_call_button, FALSE);
      gtk_widget_set_sensitive (window->start_call_button, FALSE);

      call_window_set_output_video_is_drawing (window, FALSE);
    }
  else if (stream_state == TP_MEDIA_STREAM_STATE_DISCONNECTED)
    {
      gtk_label_set_text (GTK_LABEL (window->status_label), "Disconnected");
    }
  else if (status == EMPATHY_TP_CALL_STATUS_PENDING)
    {
      g_object_get (G_OBJECT (window->call), "contact", &contact, NULL);

      title = g_strdup_printf ("%s - Empathy Call",
          empathy_contact_get_name (contact));
      gtk_window_set_title (GTK_WINDOW (window->window), title);

      gtk_label_set_text (GTK_LABEL (window->status_label), "Ringing");
      gtk_widget_set_sensitive (window->end_call_button, TRUE);
      gtk_widget_set_sensitive (window->video_button, TRUE);

      g_object_get (G_OBJECT (window->call), "is-incoming", &is_incoming, NULL);
      if (is_incoming)
        {
          gtk_widget_set_sensitive (window->start_call_button, TRUE);
        }
      else
        {
          g_signal_connect (GTK_OBJECT (window->video_button), "toggled",
              G_CALLBACK (call_window_video_button_toggled_cb),
              window);
        }
    }
  else if (status == EMPATHY_TP_CALL_STATUS_ACCEPTED)
    {
      if (stream_state == TP_MEDIA_STREAM_STATE_CONNECTING)
        {
          gtk_label_set_text (GTK_LABEL (window->status_label), "Connecting");
        }
      else if (stream_state == TP_MEDIA_STREAM_STATE_CONNECTED)
        {
          if ((window->start_time).tv_sec == 0)
            {
              g_get_current_time (&(window->start_time));
            }
          window->timeout_event_id = g_timeout_add (1000,
              call_window_update_timer, window);
          empathy_debug (DEBUG_DOMAIN, "Timer started");
        }
    }
}

static void
call_window_receiving_video_cb (EmpathyTpCall *call,
                                gboolean receiving_video,
                                EmpathyCallWindow *window)
{
  empathy_debug (DEBUG_DOMAIN, "Receiving video signal received");

  call_window_set_output_video_is_drawing (window, receiving_video);
}

static void
call_window_sending_video_cb (EmpathyTpCall *call,
                              gboolean sending_video,
                              EmpathyCallWindow *window)
{
  empathy_debug (DEBUG_DOMAIN, "Sending video signal received");

  g_signal_handlers_block_by_func (window->video_button,
      call_window_video_button_toggled_cb, window);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (window->video_button),
      sending_video);
  g_signal_handlers_unblock_by_func (window->video_button,
      call_window_video_button_toggled_cb, window);
}

static void
call_window_socket_realized_cb (GtkWidget *widget,
                                EmpathyCallWindow *window)
{
  if (widget == window->preview_video_socket)
    {
      empathy_debug (DEBUG_DOMAIN, "Preview socket realized");
      empathy_tp_call_add_preview_video (window->call,
          gtk_socket_get_id (GTK_SOCKET (window->preview_video_socket)));
    }
  else
    {
      empathy_debug (DEBUG_DOMAIN, "Output socket realized");
    }
}

static void
call_window_start_call_button_clicked_cb (GtkWidget *widget,
                                          EmpathyCallWindow *window)
{
  gboolean send_video;
  gboolean is_incoming;

  empathy_debug (DEBUG_DOMAIN, "Start call clicked");

  gtk_widget_set_sensitive (window->start_call_button, FALSE);
  g_object_get (G_OBJECT (window->call), "is-incoming", &is_incoming, NULL);
  if (is_incoming)
    {
      empathy_tp_call_accept_incoming_call (window->call);
      send_video = gtk_toggle_button_get_active
          (GTK_TOGGLE_BUTTON (window->video_button));
      empathy_tp_call_request_video_stream_direction (window->call, send_video);
      g_signal_connect (GTK_OBJECT (window->video_button), "toggled",
          G_CALLBACK (call_window_video_button_toggled_cb), window);
    }
}

static void
call_window_end_call_button_clicked_cb (GtkWidget *widget,
                                        EmpathyCallWindow *window)
{
  empathy_debug (DEBUG_DOMAIN, "End call clicked");

  call_window_set_output_video_is_drawing (window, FALSE);
  empathy_tp_call_close_channel (window->call);
  gtk_widget_set_sensitive (window->end_call_button, FALSE);
  gtk_widget_set_sensitive (window->start_call_button, FALSE);
}

static void
call_window_output_volume_changed_cb (GtkWidget *scale,
                                      EmpathyCallWindow *window)
{
  guint volume;

  volume = (guint) gtk_range_get_value (GTK_RANGE (scale));

  empathy_debug (DEBUG_DOMAIN, "Output volume changed - %u", volume);

  empathy_tp_call_set_output_volume (window->call, volume);
}

static void
call_window_output_mute_button_toggled_cb (GtkWidget *button,
                                           EmpathyCallWindow *window)
{
  gboolean is_muted;

  is_muted = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  empathy_debug (DEBUG_DOMAIN, "Mute output toggled - %d", is_muted);

  empathy_tp_call_mute_output (window->call, is_muted);
}

static void
call_window_input_mute_button_toggled_cb (GtkWidget *button,
                                          EmpathyCallWindow *window)
{
  gboolean is_muted;

  is_muted = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  empathy_debug (DEBUG_DOMAIN, "Mute input toggled - %d", is_muted);

  empathy_tp_call_mute_input (window->call, is_muted);
}

static void
call_window_destroy_cb (GtkWidget *widget,
                        EmpathyCallWindow *window)
{
  g_signal_handlers_disconnect_by_func (window->call,
      call_window_status_changed_cb, window);
  g_signal_handlers_disconnect_by_func (window->call,
      call_window_receiving_video_cb, window);
  g_signal_handlers_disconnect_by_func (window->call,
      call_window_sending_video_cb, window);

  g_object_unref (window->call);
  g_object_unref (window->output_video_socket);
  g_object_unref (window->preview_video_socket);
  g_object_unref (window->output_video_label);

  g_slice_free (EmpathyCallWindow, window);
}

GtkWidget *
empathy_call_window_new (EmpathyTpCall *call)
{
  EmpathyCallWindow *window;
  GladeXML *glade;
  guint status;
  gchar *filename;

  g_return_val_if_fail (EMPATHY_IS_TP_CALL (call), NULL);

  window = g_slice_new0 (EmpathyCallWindow);
  window->call = g_object_ref (call);

  filename = empathy_file_lookup ("empathy-call-window.glade", "src");
  glade = empathy_glade_get_file (filename,
      "window",
      NULL,
      "window", &window->window,
      "status_label", &window->status_label,
      "start_call_button", &window->start_call_button,
      "end_call_button", &window->end_call_button,
      "input_volume_scale", &window->input_volume_scale,
      "output_volume_scale", &window->output_volume_scale,
      "input_mute_button", &window->input_mute_button,
      "output_mute_button", &window->output_mute_button,
      "preview_video_frame", &window->preview_video_frame,
      "output_video_frame", &window->output_video_frame,
      "video_button", &window->video_button,
      NULL);
  g_free (filename);

  empathy_glade_connect (glade,
      window,
      "window", "destroy", call_window_destroy_cb,
      "window", "delete_event", call_window_delete_event_cb,
      "input_mute_button", "toggled", call_window_input_mute_button_toggled_cb,
      "output_mute_button", "toggled", call_window_output_mute_button_toggled_cb,
      "output_volume_scale", "value-changed", call_window_output_volume_changed_cb,
      "start_call_button", "clicked", call_window_start_call_button_clicked_cb,
      "end_call_button", "clicked", call_window_end_call_button_clicked_cb,
      NULL);

  g_object_unref (glade);

  /* Output video label */
  window->output_video_label = g_object_ref (gtk_label_new ("No video output"));
  gtk_container_add (GTK_CONTAINER (window->output_video_frame),
      window->output_video_label);
  gtk_widget_show (window->output_video_label);

  /* Output video socket */
  window->output_video_socket = g_object_ref (gtk_socket_new ());
  g_signal_connect (GTK_OBJECT (window->output_video_socket), "realize",
      G_CALLBACK (call_window_socket_realized_cb), window);
  gtk_widget_show (window->output_video_socket);

  /* Preview video socket */
  window->preview_video_socket = g_object_ref (gtk_socket_new ());
  g_signal_connect (GTK_OBJECT (window->preview_video_socket), "realize",
      G_CALLBACK (call_window_socket_realized_cb), window);
  gtk_container_add (GTK_CONTAINER (window->preview_video_frame),
      window->preview_video_socket);
  gtk_widget_show (window->preview_video_socket);

  g_signal_connect (G_OBJECT (window->call), "status-changed",
      G_CALLBACK (call_window_status_changed_cb),
      window);
  g_signal_connect (G_OBJECT (window->call), "receiving-video",
      G_CALLBACK (call_window_receiving_video_cb),
      window);
  g_signal_connect (G_OBJECT (window->call), "sending-video",
      G_CALLBACK (call_window_sending_video_cb),
      window);

  window->is_drawing = FALSE;

  g_object_get (G_OBJECT (window->call), "status", &status, NULL);

  if (status == EMPATHY_TP_CALL_STATUS_READYING)
    {
      gtk_window_set_title (GTK_WINDOW (window->window), "Empathy Call");
      gtk_label_set_text (GTK_LABEL (window->status_label), "Readying");
    }

  gtk_widget_show (window->window);

  return window->window;
}

