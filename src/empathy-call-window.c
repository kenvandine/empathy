/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Elliot Fairweather
 * Copyright (C) 2007-2008 Collabora Ltd.
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
 *
 * Authors: Elliot Fairweather <elliot.fairweather@collabora.co.uk>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include <string.h>

#include <glade/glade.h>
#include <glib/gi18n.h>

#include <telepathy-glib/enums.h>

#include <libempathy/empathy-contact.h>
#include <libempathy/empathy-tp-call.h>
#include <libempathy/empathy-utils.h>
#include <libempathy-gtk/empathy-ui-utils.h>

#include "empathy-call-window.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

typedef struct 
{
  EmpathyTpCall *call;
  GTimeVal start_time;
  guint timeout_event_id;
  gboolean is_drawing;
  guint status; 

  GtkWidget *window;
  GtkWidget *main_hbox;
  GtkWidget *controls_vbox;
  GtkWidget *volume_hbox;
  GtkWidget *status_label;
  GtkWidget *input_volume_button;
  GtkWidget *output_volume_button;
  GtkWidget *preview_video_socket;
  GtkWidget *output_video_socket;
  GtkWidget *video_button;
  GtkWidget *hang_up_button;
  GtkWidget *confirmation_dialog;
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
      str = g_strdup_printf ("Connected  -  %02ld : %02ld : %02ld", hours,
          minutes, seconds);
  else
      str = g_strdup_printf ("Connected  -  %02ld : %02ld", minutes, seconds);

  gtk_label_set_text (GTK_LABEL (window->status_label), str);

  g_free (str);

  return TRUE;
}

static void
call_window_stop_timeout (EmpathyCallWindow *window)
{
  DEBUG ("Timer stopped");

  if (window->timeout_event_id)
    {
      g_source_remove (window->timeout_event_id);
      window->timeout_event_id = 0;
    }
}

static void
call_window_set_output_video_is_drawing (EmpathyCallWindow *window,
                                         gboolean is_drawing)
{
  DEBUG ("Setting output video is drawing - %d", is_drawing);

  if (is_drawing && !window->is_drawing)
    {
      gtk_window_set_resizable (GTK_WINDOW (window->window), TRUE);
      gtk_box_pack_end (GTK_BOX (window->main_hbox),
          window->output_video_socket, TRUE, TRUE, 0);
      empathy_tp_call_add_output_video (window->call,
          gtk_socket_get_id (GTK_SOCKET (window->output_video_socket)));
    }
  if (!is_drawing && window->is_drawing)
    {
      gtk_window_set_resizable (GTK_WINDOW (window->window), FALSE);
      empathy_tp_call_add_output_video (window->call, 0);
      gtk_container_remove (GTK_CONTAINER (window->main_hbox),
          window->output_video_socket);
    }

  window->is_drawing = is_drawing;
}

static void
call_window_finalize (EmpathyCallWindow *window)
{
  gtk_label_set_text (GTK_LABEL (window->status_label), _("Closed"));
  gtk_widget_set_sensitive (window->hang_up_button, FALSE);
  gtk_widget_set_sensitive (window->video_button, FALSE);
  gtk_widget_set_sensitive (window->output_volume_button, FALSE);
  gtk_widget_set_sensitive (window->input_volume_button, FALSE);

  if (window->call)
    { 
      call_window_stop_timeout (window);
      call_window_set_output_video_is_drawing (window, FALSE);
      empathy_tp_call_remove_preview_video (window->call,
          gtk_socket_get_id (GTK_SOCKET (window->preview_video_socket)));
      g_object_unref (window->call);
      window->call = NULL;
    }

  if (window->confirmation_dialog)
      gtk_widget_destroy (window->confirmation_dialog);
}

static void
call_window_socket_realized_cb (GtkWidget *widget,
                                EmpathyCallWindow *window)
{
  if (widget == window->preview_video_socket)
    {
      DEBUG ("Preview socket realized");
      empathy_tp_call_add_preview_video (window->call,
          gtk_socket_get_id (GTK_SOCKET (window->preview_video_socket)));
    }
  else
      DEBUG ("Output socket realized");
}

static void
call_window_video_button_toggled_cb (GtkWidget *button,
                                     EmpathyCallWindow *window)
{
  gboolean is_sending;
  guint status;

  is_sending = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  DEBUG ("Send video toggled - %d", is_sending);

  g_object_get (window->call, "status", &status, NULL);
  if (status == EMPATHY_TP_CALL_STATUS_ACCEPTED)
      empathy_tp_call_request_video_stream_direction (window->call, is_sending);
}

static void
call_window_hang_up_button_clicked_cb (GtkWidget *widget,
                                       EmpathyCallWindow *window)
{
  DEBUG ("Call clicked, end call");
  call_window_finalize (window);
}

static void
call_window_output_volume_changed_cb (GtkScaleButton *button,
                                      gdouble value,
                                      EmpathyCallWindow *window)
{
  if (!window->call)
      return;

  if (value <= 0)
      empathy_tp_call_mute_output (window->call, TRUE);
  else
    {
      empathy_tp_call_mute_output (window->call, FALSE);
      empathy_tp_call_set_output_volume (window->call, value * 100);
    }
}

static void
call_window_input_volume_changed_cb (GtkScaleButton    *button,
                                     gdouble            value,
                                     EmpathyCallWindow *window)
{
  if (!window->call)
      return;

  if (value <= 0)
      empathy_tp_call_mute_input (window->call, TRUE);
  else
    {
      empathy_tp_call_mute_input (window->call, FALSE);
      /* FIXME: Not implemented?
      empathy_tp_call_set_input_volume (window->call, value * 100);*/
    }
}

static gboolean
call_window_delete_event_cb (GtkWidget *widget,
                             GdkEvent *event,
                             EmpathyCallWindow *window)
{
  GtkWidget *dialog;
  gint result;
  guint status = EMPATHY_TP_CALL_STATUS_CLOSED;

  DEBUG ("Delete event occurred");

  if (window->call)
      g_object_get (window->call, "status", &status, NULL);

  if (status == EMPATHY_TP_CALL_STATUS_ACCEPTED)
    {
      dialog = gtk_message_dialog_new (GTK_WINDOW (window->window),
          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
          GTK_MESSAGE_WARNING, GTK_BUTTONS_CANCEL, _("End this call?"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
          _("Closing this window will end the call in progress."));
      gtk_dialog_add_button (GTK_DIALOG (dialog), _("_End Call"), GTK_RESPONSE_OK);
      gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

      result = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      if (result != GTK_RESPONSE_OK)
          return TRUE;
    }

  return FALSE;
}

static void
call_window_destroy_cb (GtkWidget *widget,
                        EmpathyCallWindow *window)
{
  call_window_finalize (window);
  g_object_unref (window->output_video_socket);
  g_object_unref (window->preview_video_socket);
  g_slice_free (EmpathyCallWindow, window);
}

static void
call_window_confirmation_dialog_response_cb (GtkDialog *dialog,
                                             gint response,
                                             EmpathyCallWindow *window)
{
  if (response == GTK_RESPONSE_OK && window->call)
      empathy_tp_call_accept_incoming_call (window->call);
  else
      call_window_finalize (window);

  gtk_widget_destroy (window->confirmation_dialog);
  window->confirmation_dialog = NULL;
}

static void
call_window_show_confirmation_dialog (EmpathyCallWindow *window)
{
  EmpathyContact *contact;
  GtkWidget *button;
  GtkWidget *image;

  if (window->confirmation_dialog)
      return;

  g_object_get (window->call, "contact", &contact, NULL);

  window->confirmation_dialog = gtk_message_dialog_new (GTK_WINDOW (window->window),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, _("Incoming call"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (window->confirmation_dialog),
      _("%s is calling you, do you want to answer?"),
      empathy_contact_get_name (contact));
  gtk_dialog_set_default_response (GTK_DIALOG (window->confirmation_dialog),
      GTK_RESPONSE_OK);

  button = gtk_dialog_add_button (GTK_DIALOG (window->confirmation_dialog),
      _("_Reject"), GTK_RESPONSE_CANCEL);
  image = gtk_image_new_from_icon_name (GTK_STOCK_CANCEL, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);

  button = gtk_dialog_add_button (GTK_DIALOG (window->confirmation_dialog),
      _("_Answer"), GTK_RESPONSE_OK);
  image = gtk_image_new_from_icon_name (GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);

  g_signal_connect (window->confirmation_dialog, "response",
      G_CALLBACK (call_window_confirmation_dialog_response_cb),
      window);

  gtk_widget_show (window->confirmation_dialog);
  g_object_unref (contact);
}

static void
call_window_update (EmpathyCallWindow *window)
{
  EmpathyContact *contact;
  guint stream_state;
  EmpathyTpCallStream *audio_stream;
  EmpathyTpCallStream *video_stream;
  gboolean is_incoming;
  gchar *title;

  g_object_get (window->call,
      "status", &window->status,
      "audio-stream", &audio_stream,
      "video-stream", &video_stream,
      "contact", &contact,
      "is-incoming", &is_incoming,
      NULL);

  if (video_stream->state > audio_stream->state)
      stream_state = video_stream->state;
  else
      stream_state = audio_stream->state;

  DEBUG ("Status changed - status: %d, stream state: %d, "
      "is-incoming: %d video-stream direction: %d",
      window->status, stream_state, is_incoming, video_stream->direction);

  /* Depending on the status we have to set:
   * - window's title
   * - status's label
   * - sensibility of all buttons
   * */
  if (window->status == EMPATHY_TP_CALL_STATUS_READYING)
    {
      gtk_window_set_title (GTK_WINDOW (window->window), _("Empathy Call"));
      gtk_label_set_text (GTK_LABEL (window->status_label), _("Readying"));
      gtk_widget_set_sensitive (window->video_button, FALSE);
      gtk_widget_set_sensitive (window->output_volume_button, FALSE);
      gtk_widget_set_sensitive (window->input_volume_button, FALSE);
      gtk_widget_set_sensitive (window->hang_up_button, FALSE);
    }
  else if (window->status == EMPATHY_TP_CALL_STATUS_PENDING)
    {
      title = g_strdup_printf (_("%s - Empathy Call"),
          empathy_contact_get_name (contact));

      gtk_window_set_title (GTK_WINDOW (window->window), title);
      gtk_label_set_text (GTK_LABEL (window->status_label), _("Ringing"));
      gtk_widget_set_sensitive (window->hang_up_button, TRUE);
      if (is_incoming)
          call_window_show_confirmation_dialog (window);
    }
  else if (window->status == EMPATHY_TP_CALL_STATUS_ACCEPTED)
    {
      gboolean receiving_video;
      gboolean sending_video;

      if (stream_state == TP_MEDIA_STREAM_STATE_DISCONNECTED)
          gtk_label_set_text (GTK_LABEL (window->status_label), _("Disconnected"));
      if (stream_state == TP_MEDIA_STREAM_STATE_CONNECTING)
          gtk_label_set_text (GTK_LABEL (window->status_label), _("Connecting"));
      else if (stream_state == TP_MEDIA_STREAM_STATE_CONNECTED &&
               window->timeout_event_id == 0)
        {
          /* The call started, launch the timer */
          g_get_current_time (&(window->start_time));
          window->timeout_event_id = g_timeout_add_seconds (1,
              call_window_update_timer, window);
          call_window_update_timer (window);
        }

      receiving_video = video_stream->direction & TP_MEDIA_STREAM_DIRECTION_RECEIVE;
      sending_video = video_stream->direction & TP_MEDIA_STREAM_DIRECTION_SEND;
      call_window_set_output_video_is_drawing (window, receiving_video);
      g_signal_handlers_block_by_func (window->video_button,
          call_window_video_button_toggled_cb, window);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (window->video_button),
          sending_video);
      g_signal_handlers_unblock_by_func (window->video_button,
          call_window_video_button_toggled_cb, window);

      gtk_widget_set_sensitive (window->video_button, TRUE);
      gtk_widget_set_sensitive (window->output_volume_button, TRUE);
      gtk_widget_set_sensitive (window->input_volume_button, TRUE);
      gtk_widget_set_sensitive (window->hang_up_button, TRUE);
    }
  else if (window->status == EMPATHY_TP_CALL_STATUS_CLOSED)
      call_window_finalize (window);

  if (contact)
      g_object_unref (contact);
}

static gboolean
call_window_dtmf_button_release_event_cb (GtkWidget *widget,
                                          GdkEventButton *event,
                                          EmpathyCallWindow *window)
{
  empathy_tp_call_stop_tone (window->call);
  return FALSE;
}

static gboolean
call_window_dtmf_button_press_event_cb (GtkWidget *widget,
                                        GdkEventButton *event,
                                        EmpathyCallWindow *window)
{
  TpDTMFEvent dtmf_event;

  dtmf_event = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget), "code"));
  empathy_tp_call_start_tone (window->call, dtmf_event);
  return FALSE;
}

static void
call_window_dtmf_connect (GladeXML *glade,
                          EmpathyCallWindow *window,
                          const gchar *name,
                          TpDTMFEvent event)
{
  GtkWidget *widget;

  widget = glade_xml_get_widget (glade, name);
  g_object_set_data (G_OBJECT (widget), "code", GUINT_TO_POINTER (event));
  g_signal_connect (widget, "button-press-event",
      G_CALLBACK (call_window_dtmf_button_press_event_cb), window);
  g_signal_connect (widget, "button-release-event",
      G_CALLBACK (call_window_dtmf_button_release_event_cb), window);
  /* FIXME: Connect "key-[press/release]-event" to*/
}

GtkWidget *
empathy_call_window_new (EmpathyTpCall *call)
{
  EmpathyCallWindow *window;
  GladeXML *glade;
  gchar *filename;
  const gchar *icons[] = {"audio-input-microphone", NULL};

  g_return_val_if_fail (EMPATHY_IS_TP_CALL (call), NULL);

  window = g_slice_new0 (EmpathyCallWindow);
  window->call = g_object_ref (call);

  filename = empathy_file_lookup ("empathy-call-window.glade", "src");
  glade = empathy_glade_get_file (filename,
      "window",
      NULL,
      "window", &window->window,
      "main_hbox", &window->main_hbox,
      "controls_vbox", &window->controls_vbox,
      "volume_hbox", &window->volume_hbox,
      "status_label", &window->status_label,
      "video_button", &window->video_button,
      "hang_up_button", &window->hang_up_button,
      NULL);
  g_free (filename);

  empathy_glade_connect (glade,
      window,
      "window", "destroy", call_window_destroy_cb,
      "window", "delete_event", call_window_delete_event_cb,
      "hang_up_button", "clicked", call_window_hang_up_button_clicked_cb,
      "video_button", "toggled", call_window_video_button_toggled_cb,
      NULL);

  /* Setup DTMF buttons */
  call_window_dtmf_connect (glade, window, "button_0", TP_DTMF_EVENT_DIGIT_0);
  call_window_dtmf_connect (glade, window, "button_1", TP_DTMF_EVENT_DIGIT_1);
  call_window_dtmf_connect (glade, window, "button_2", TP_DTMF_EVENT_DIGIT_2);
  call_window_dtmf_connect (glade, window, "button_3", TP_DTMF_EVENT_DIGIT_3);
  call_window_dtmf_connect (glade, window, "button_4", TP_DTMF_EVENT_DIGIT_4);
  call_window_dtmf_connect (glade, window, "button_5", TP_DTMF_EVENT_DIGIT_5);
  call_window_dtmf_connect (glade, window, "button_6", TP_DTMF_EVENT_DIGIT_6);
  call_window_dtmf_connect (glade, window, "button_7", TP_DTMF_EVENT_DIGIT_7);
  call_window_dtmf_connect (glade, window, "button_8", TP_DTMF_EVENT_DIGIT_8);
  call_window_dtmf_connect (glade, window, "button_9", TP_DTMF_EVENT_DIGIT_9);
  call_window_dtmf_connect (glade, window, "button_asterisk", TP_DTMF_EVENT_ASTERISK);
  call_window_dtmf_connect (glade, window, "button_hash", TP_DTMF_EVENT_HASH);

  g_object_unref (glade);

  /* Output volume button */
  window->output_volume_button = gtk_volume_button_new ();
  gtk_scale_button_set_value (GTK_SCALE_BUTTON (window->output_volume_button), 1);
  gtk_box_pack_start (GTK_BOX (window->volume_hbox),
      window->output_volume_button, FALSE, FALSE, 0);
  gtk_widget_show (window->output_volume_button);
  g_signal_connect (window->output_volume_button, "value-changed",
      G_CALLBACK (call_window_output_volume_changed_cb), window);

  /* Input volume button */
  window->input_volume_button = gtk_volume_button_new ();
  gtk_scale_button_set_icons (GTK_SCALE_BUTTON (window->input_volume_button),
      icons);
  gtk_scale_button_set_value (GTK_SCALE_BUTTON (window->input_volume_button), 1);
  gtk_box_pack_start (GTK_BOX (window->volume_hbox),
      window->input_volume_button, FALSE, FALSE, 0);
  gtk_widget_show (window->input_volume_button);
  g_signal_connect (window->input_volume_button, "value-changed",
      G_CALLBACK (call_window_input_volume_changed_cb), window);

  /* Output video socket */
  window->output_video_socket = g_object_ref (gtk_socket_new ());
  gtk_widget_set_size_request (window->output_video_socket, 400, 300);
  g_signal_connect (GTK_OBJECT (window->output_video_socket), "realize",
      G_CALLBACK (call_window_socket_realized_cb), window);
  gtk_widget_show (window->output_video_socket);

  /* Preview video socket */
  window->preview_video_socket = g_object_ref (gtk_socket_new ());
  gtk_widget_set_size_request (window->preview_video_socket, 176, 144);
  g_signal_connect (GTK_OBJECT (window->preview_video_socket), "realize",
      G_CALLBACK (call_window_socket_realized_cb), window);
  gtk_widget_show (window->preview_video_socket);

  /* FIXME: We shouldn't do this if there is no video input */
  gtk_box_pack_start (GTK_BOX (window->controls_vbox),
      window->preview_video_socket, FALSE, FALSE, 0);
  gtk_box_reorder_child (GTK_BOX (window->controls_vbox),
      window->preview_video_socket, 0);

  g_signal_connect_swapped (G_OBJECT (window->call), "notify",
      G_CALLBACK (call_window_update),
      window);

  call_window_update (window);
  gtk_widget_show (window->window);

  return window->window;
}

