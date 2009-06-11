/*
 * empathy-call-window.c - Source for EmpathyCallWindow
 * Copyright (C) 2008-2009 Collabora Ltd.
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

#include <math.h>

#include <gdk/gdkkeysyms.h>
#include <gst/gst.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <telepathy-farsight/channel.h>

#include <libempathy/empathy-tp-contact-factory.h>
#include <libempathy/empathy-call-factory.h>
#include <libempathy/empathy-utils.h>
#include <libempathy-gtk/empathy-avatar-image.h>
#include <libempathy-gtk/empathy-video-widget.h>
#include <libempathy-gtk/empathy-audio-src.h>
#include <libempathy-gtk/empathy-audio-sink.h>
#include <libempathy-gtk/empathy-video-src.h>
#include <libempathy-gtk/empathy-ui-utils.h>

#include "empathy-call-window.h"

#include "empathy-call-window-fullscreen.h"
#include "empathy-sidebar.h"

#define BUTTON_ID "empathy-call-dtmf-button-id"

#define CONTENT_HBOX_BORDER_WIDTH 6
#define CONTENT_HBOX_SPACING 3
#define CONTENT_HBOX_CHILDREN_PACKING_PADDING 3

#define SELF_VIDEO_SECTION_WIDTH 160
#define SELF_VIDEO_SECTION_HEIGTH 120

/* The avatar's default width and height are set to the same value because we
   want a square icon. */
#define REMOTE_CONTACT_AVATAR_DEFAULT_WIDTH EMPATHY_VIDEO_WIDGET_DEFAULT_HEIGHT
#define REMOTE_CONTACT_AVATAR_DEFAULT_HEIGHT EMPATHY_VIDEO_WIDGET_DEFAULT_HEIGHT

#define CONNECTING_STATUS_TEXT _("Connecting...")

/* If an video input error occurs, the error message will start with "v4l" */
#define VIDEO_INPUT_ERROR_PREFIX "v4l"

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
  EmpathyContact *contact;

  gboolean connected;

  GtkUIManager *ui_manager;
  GtkWidget *video_output;
  GtkWidget *video_preview;
  GtkWidget *remote_user_avatar_widget;
  GtkWidget *self_user_avatar_widget;
  GtkWidget *sidebar;
  GtkWidget *sidebar_button;
  GtkWidget *statusbar;
  GtkWidget *volume_button;
  GtkWidget *redial_button;
  GtkWidget *mic_button;
  GtkWidget *camera_button;
  GtkWidget *toolbar;
  GtkWidget *pane;
  GtkAction *show_preview;
  GtkAction *send_video;
  GtkAction *redial;
  GtkAction *menu_fullscreen;

  /* The frames and boxes that contain self and remote avatar and video
     input/output. When we redial, we destroy and re-create the boxes */
  GtkWidget *remote_user_output_frame;
  GtkWidget *self_user_output_frame;
  GtkWidget *remote_user_output_hbox;
  GtkWidget *self_user_output_hbox;

  /* We keep a reference on the hbox which contains the main content so we can
     easilly repack everything when toggling fullscreen */
  GtkWidget *content_hbox;

  /* This vbox is contained in the content_hbox and it contains the
     self_user_output_frame and the sidebar button. When toggling fullscreen,
     it needs to be repacked. We keep a reference on it for easier access. */
  GtkWidget *vbox;

  gulong video_output_motion_handler_id;

  gdouble volume;
  GtkWidget *volume_progress_bar;
  GtkAdjustment *audio_input_adj;

  GtkWidget *dtmf_panel;

  GstElement *video_input;
  GstElement *audio_input;
  GstElement *audio_output;
  GstElement *pipeline;
  GstElement *video_tee;

  GstElement *funnel;
  GstElement *liveadder;

  guint context_id;

  GTimer *timer;
  guint timer_id;

  GtkWidget *video_contrast;
  GtkWidget *video_brightness;
  GtkWidget *video_gamma;

  GMutex *lock;
  gboolean call_started;
  gboolean sending_video;

  EmpathyCallWindowFullscreen *fullscreen;
  gboolean is_fullscreen;

  /* Those fields represent the state of the window before it actually was in
     fullscreen mode. */
  gboolean sidebar_was_visible_before_fs;
  gint original_width_before_fs;
  gint original_height_before_fs;

  /* Used to indicate if we are currently redialing. If we are, as soon as the
     channel is closed, the call is automatically re-initiated.*/
  gboolean redialing;
};

#define GET_PRIV(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EMPATHY_TYPE_CALL_WINDOW, \
    EmpathyCallWindowPriv))

static void empathy_call_window_realized_cb (GtkWidget *widget,
  EmpathyCallWindow *window);

static gboolean empathy_call_window_delete_cb (GtkWidget *widget,
  GdkEvent *event, EmpathyCallWindow *window);

static gboolean empathy_call_window_state_event_cb (GtkWidget *widget,
  GdkEventWindowState *event, EmpathyCallWindow *window);

static void empathy_call_window_sidebar_toggled_cb (GtkToggleButton *toggle,
  EmpathyCallWindow *window);

static void empathy_call_window_camera_toggled_cb (GtkToggleToolButton *toggle,
  EmpathyCallWindow *window);

static void empathy_call_window_set_send_video (EmpathyCallWindow *window,
  gboolean send);

static void empathy_call_window_send_video_toggled_cb (GtkToggleAction *toggle,
  EmpathyCallWindow *window);

static void empathy_call_window_show_preview_toggled_cb (GtkToggleAction *toggle,
  EmpathyCallWindow *window);

static void empathy_call_window_mic_toggled_cb (
  GtkToggleToolButton *toggle, EmpathyCallWindow *window);

static void empathy_call_window_sidebar_hidden_cb (EmpathySidebar *sidebar,
  EmpathyCallWindow *window);

static void empathy_call_window_sidebar_shown_cb (EmpathySidebar *sidebar,
  EmpathyCallWindow *window);

static void empathy_call_window_hangup_cb (gpointer object,
  EmpathyCallWindow *window);

static void empathy_call_window_fullscreen_cb (gpointer object,
  EmpathyCallWindow *window);

static void empathy_call_window_fullscreen_toggle (EmpathyCallWindow *window);

static gboolean empathy_call_window_video_button_press_cb (GtkWidget *video_output,
  GdkEventButton *event, EmpathyCallWindow *window);

static gboolean empathy_call_window_key_press_cb (GtkWidget *video_output,
  GdkEventKey *event, EmpathyCallWindow *window);

static gboolean empathy_call_window_video_output_motion_notify (GtkWidget *widget,
  GdkEventMotion *event, EmpathyCallWindow *window);

static void empathy_call_window_video_menu_popup (EmpathyCallWindow *window,
  guint button);

static void empathy_call_window_redial_cb (gpointer object,
  EmpathyCallWindow *window);

static void empathy_call_window_restart_call (EmpathyCallWindow *window);

static void empathy_call_window_status_message (EmpathyCallWindow *window,
  gchar *message);

static void empathy_call_window_update_avatars_visibility (EmpathyTpCall *call,
  EmpathyCallWindow *window);

static gboolean empathy_call_window_bus_message (GstBus *bus,
  GstMessage *message, gpointer user_data);

static void
empathy_call_window_volume_changed_cb (GtkScaleButton *button,
  gdouble value, EmpathyCallWindow *window);

static void
empathy_call_window_setup_toolbar (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GtkToolItem *tool_item;

  /* Add an empty expanded GtkToolItem so the volume button is at the end of
   * the toolbar. */
  tool_item = gtk_tool_item_new ();
  gtk_tool_item_set_expand (tool_item, TRUE);
  gtk_widget_show (GTK_WIDGET (tool_item));
  gtk_toolbar_insert (GTK_TOOLBAR (priv->toolbar), tool_item, -1);

  priv->volume_button = gtk_volume_button_new ();
  /* FIXME listen to the audiosinks signals and update the button according to
   * that, for now starting out at 1.0 and assuming only the app changes the
   * volume will do */
  gtk_scale_button_set_value (GTK_SCALE_BUTTON (priv->volume_button), 1.0);
  g_signal_connect (G_OBJECT (priv->volume_button), "value-changed",
    G_CALLBACK (empathy_call_window_volume_changed_cb), self);

  tool_item = gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (tool_item), priv->volume_button);
  gtk_widget_show_all (GTK_WIDGET (tool_item));
  gtk_toolbar_insert (GTK_TOOLBAR (priv->toolbar), tool_item, -1);
}

static void
dtmf_button_pressed_cb (GtkButton *button, EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);
  EmpathyTpCall *call;
  GQuark button_quark;
  TpDTMFEvent event;

  g_object_get (priv->handler, "tp-call", &call, NULL);

  button_quark = g_quark_from_static_string (BUTTON_ID);
  event = GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (button),
    button_quark));

  empathy_tp_call_start_tone (call, event);

  g_object_unref (call);
}

static void
dtmf_button_released_cb (GtkButton *button, EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);
  EmpathyTpCall *call;

  g_object_get (priv->handler, "tp-call", &call, NULL);

  empathy_tp_call_stop_tone (call);

  g_object_unref (call);
}

static GtkWidget *
empathy_call_window_create_dtmf (EmpathyCallWindow *self)
{
  GtkWidget *table;
  int i;
  GQuark button_quark;
  struct {
    gchar *label;
    TpDTMFEvent event;
  } dtmfbuttons[] = { { "1", TP_DTMF_EVENT_DIGIT_1 },
                      { "2", TP_DTMF_EVENT_DIGIT_2 },
                      { "3", TP_DTMF_EVENT_DIGIT_3 },
                      { "4", TP_DTMF_EVENT_DIGIT_4 },
                      { "5", TP_DTMF_EVENT_DIGIT_5 },
                      { "6", TP_DTMF_EVENT_DIGIT_6 },
                      { "7", TP_DTMF_EVENT_DIGIT_7 },
                      { "8", TP_DTMF_EVENT_DIGIT_8 },
                      { "9", TP_DTMF_EVENT_DIGIT_9 },
                      { "#", TP_DTMF_EVENT_HASH },
                      { "0", TP_DTMF_EVENT_DIGIT_0 },
                      { "*", TP_DTMF_EVENT_ASTERISK },
                      { NULL, } };

  button_quark = g_quark_from_static_string (BUTTON_ID);

  table = gtk_table_new (4, 3, TRUE);

  for (i = 0; dtmfbuttons[i].label != NULL; i++)
    {
      GtkWidget *button = gtk_button_new_with_label (dtmfbuttons[i].label);
      gtk_table_attach (GTK_TABLE (table), button, i % 3, i % 3 + 1,
        i/3, i/3 + 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 1, 1);

      g_object_set_qdata (G_OBJECT (button), button_quark,
        GUINT_TO_POINTER (dtmfbuttons[i].event));

      g_signal_connect (G_OBJECT (button), "pressed",
        G_CALLBACK (dtmf_button_pressed_cb), self);
      g_signal_connect (G_OBJECT (button), "released",
        G_CALLBACK (dtmf_button_released_cb), self);
    }

  return table;
}

static GtkWidget *
empathy_call_window_create_video_input_add_slider (EmpathyCallWindow *self,
  gchar *label_text, GtkWidget *bin)
{
   GtkWidget *vbox = gtk_vbox_new (FALSE, 2);
   GtkWidget *scale = gtk_vscale_new_with_range (0, 100, 10);
   GtkWidget *label = gtk_label_new (label_text);

   gtk_widget_set_sensitive (scale, FALSE);

   gtk_container_add (GTK_CONTAINER (bin), vbox);

   gtk_range_set_inverted (GTK_RANGE (scale), TRUE);
   gtk_box_pack_start (GTK_BOX (vbox), scale, TRUE, TRUE, 0);
   gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

   return scale;
}

static void
empathy_call_window_video_contrast_changed_cb (GtkAdjustment *adj,
  EmpathyCallWindow *self)

{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  empathy_video_src_set_channel (priv->video_input,
    EMPATHY_GST_VIDEO_SRC_CHANNEL_CONTRAST, gtk_adjustment_get_value (adj));
}

static void
empathy_call_window_video_brightness_changed_cb (GtkAdjustment *adj,
  EmpathyCallWindow *self)

{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  empathy_video_src_set_channel (priv->video_input,
    EMPATHY_GST_VIDEO_SRC_CHANNEL_BRIGHTNESS, gtk_adjustment_get_value (adj));
}

static void
empathy_call_window_video_gamma_changed_cb (GtkAdjustment *adj,
  EmpathyCallWindow *self)

{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  empathy_video_src_set_channel (priv->video_input,
    EMPATHY_GST_VIDEO_SRC_CHANNEL_GAMMA, gtk_adjustment_get_value (adj));
}


static GtkWidget *
empathy_call_window_create_video_input (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GtkWidget *hbox;

  hbox = gtk_hbox_new (TRUE, 3);

  priv->video_contrast = empathy_call_window_create_video_input_add_slider (
    self,  _("Contrast"), hbox);

  priv->video_brightness = empathy_call_window_create_video_input_add_slider (
    self,  _("Brightness"), hbox);

  priv->video_gamma = empathy_call_window_create_video_input_add_slider (
    self,  _("Gamma"), hbox);

  return hbox;
}

static void
empathy_call_window_setup_video_input (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  guint supported;
  GtkAdjustment *adj;

  supported = empathy_video_src_get_supported_channels (priv->video_input);

  if (supported & EMPATHY_GST_VIDEO_SRC_SUPPORTS_CONTRAST)
    {
      adj = gtk_range_get_adjustment (GTK_RANGE (priv->video_contrast));

      gtk_adjustment_set_value (adj,
        empathy_video_src_get_channel (priv->video_input,
          EMPATHY_GST_VIDEO_SRC_CHANNEL_CONTRAST));

      g_signal_connect (G_OBJECT (adj), "value-changed",
        G_CALLBACK (empathy_call_window_video_contrast_changed_cb), self);

      gtk_widget_set_sensitive (priv->video_contrast, TRUE);
    }

  if (supported & EMPATHY_GST_VIDEO_SRC_SUPPORTS_BRIGHTNESS)
    {
      adj = gtk_range_get_adjustment (GTK_RANGE (priv->video_brightness));

      gtk_adjustment_set_value (adj,
        empathy_video_src_get_channel (priv->video_input,
          EMPATHY_GST_VIDEO_SRC_CHANNEL_BRIGHTNESS));

      g_signal_connect (G_OBJECT (adj), "value-changed",
        G_CALLBACK (empathy_call_window_video_brightness_changed_cb), self);
      gtk_widget_set_sensitive (priv->video_brightness, TRUE);
    }

  if (supported & EMPATHY_GST_VIDEO_SRC_SUPPORTS_GAMMA)
    {
      adj = gtk_range_get_adjustment (GTK_RANGE (priv->video_gamma));

      gtk_adjustment_set_value (adj,
        empathy_video_src_get_channel (priv->video_input,
          EMPATHY_GST_VIDEO_SRC_CHANNEL_GAMMA));

      g_signal_connect (G_OBJECT (adj), "value-changed",
        G_CALLBACK (empathy_call_window_video_gamma_changed_cb), self);
      gtk_widget_set_sensitive (priv->video_gamma, TRUE);
    }
}

static void
empathy_call_window_mic_volume_changed_cb (GtkAdjustment *adj,
  EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  gdouble volume;

  volume =  gtk_adjustment_get_value (adj)/100.0;

  /* Don't store the volume because of muting */
  if (volume > 0 || gtk_toggle_tool_button_get_active (
        GTK_TOGGLE_TOOL_BUTTON (priv->mic_button)))
    priv->volume = volume;

  /* Ensure that the toggle button is active if the volume is > 0 and inactive
   * if it's smaller then 0 */
  if ((volume > 0) != gtk_toggle_tool_button_get_active (
        GTK_TOGGLE_TOOL_BUTTON (priv->mic_button)))
    gtk_toggle_tool_button_set_active (
      GTK_TOGGLE_TOOL_BUTTON (priv->mic_button), volume > 0);

  empathy_audio_src_set_volume (EMPATHY_GST_AUDIO_SRC (priv->audio_input),
    volume);
}

static void
empathy_call_window_audio_input_level_changed_cb (EmpathyGstAudioSrc *src,
  gdouble level, EmpathyCallWindow *window)
{
  gdouble value;
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  value = CLAMP (pow (10, level / 20), 0.0, 1.0);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->volume_progress_bar), value);
}

static GtkWidget *
empathy_call_window_create_audio_input (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GtkWidget *hbox, *vbox, *scale, *label;
  GtkAdjustment *adj;

  hbox = gtk_hbox_new (TRUE, 3);

  vbox = gtk_vbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 3);

  scale = gtk_vscale_new_with_range (0, 150, 100);
  gtk_range_set_inverted (GTK_RANGE (scale), TRUE);
  label = gtk_label_new (_("Volume"));

  priv->audio_input_adj = adj = gtk_range_get_adjustment (GTK_RANGE (scale));
  priv->volume =  empathy_audio_src_get_volume (EMPATHY_GST_AUDIO_SRC
    (priv->audio_input));
  gtk_adjustment_set_value (adj, priv->volume * 100);

  g_signal_connect (G_OBJECT (adj), "value-changed",
    G_CALLBACK (empathy_call_window_mic_volume_changed_cb), self);

  gtk_box_pack_start (GTK_BOX (vbox), scale, TRUE, TRUE, 3);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 3);

  priv->volume_progress_bar = gtk_progress_bar_new ();
  gtk_progress_bar_set_orientation (
      GTK_PROGRESS_BAR (priv->volume_progress_bar), GTK_PROGRESS_BOTTOM_TO_TOP);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->volume_progress_bar),
      0);

  gtk_box_pack_start (GTK_BOX (hbox), priv->volume_progress_bar, FALSE, FALSE,
      3);

  return hbox;
}

static void
empathy_call_window_setup_remote_frame (GstBus *bus, EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  /* Initializing all the content (UI and output gst elements) related to the
     remote contact */
  priv->remote_user_output_hbox = gtk_hbox_new (FALSE, 0);

  priv->remote_user_avatar_widget = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (priv->remote_user_output_hbox),
      priv->remote_user_avatar_widget, TRUE, TRUE, 0);

  priv->video_output = empathy_video_widget_new (bus);
  gtk_box_pack_start (GTK_BOX (priv->remote_user_output_hbox),
      priv->video_output, TRUE, TRUE, 0);

  gtk_widget_add_events (priv->video_output,
      GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);
  g_signal_connect (G_OBJECT (priv->video_output), "button-press-event",
      G_CALLBACK (empathy_call_window_video_button_press_cb), self);

  gtk_container_add (GTK_CONTAINER (priv->remote_user_output_frame),
      priv->remote_user_output_hbox);

  priv->audio_output = empathy_audio_sink_new ();
  gst_object_ref (priv->audio_output);
  gst_object_sink (priv->audio_output);
}

static void
empathy_call_window_setup_self_frame (GstBus *bus, EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  /* Initializing all the content (UI and input gst elements) related to the
     self contact, except for the video preview widget. This widget is only
     initialized when the "show video preview" option is activated */
  priv->self_user_output_hbox = gtk_hbox_new (FALSE, 0);

  priv->self_user_avatar_widget = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (priv->self_user_output_hbox),
      priv->self_user_avatar_widget, TRUE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (priv->self_user_output_frame),
      priv->self_user_output_hbox);

  priv->video_input = empathy_video_src_new ();
  gst_object_ref (priv->video_input);
  gst_object_sink (priv->video_input);

  priv->audio_input = empathy_audio_src_new ();
  gst_object_ref (priv->audio_input);
  gst_object_sink (priv->audio_input);

  g_signal_connect (priv->audio_input, "peak-level-changed",
    G_CALLBACK (empathy_call_window_audio_input_level_changed_cb), self);
}

static void
empathy_call_window_setup_video_preview (EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  if (priv->video_preview != NULL)
    {
      /* Since the video preview and the video tee are initialized and freed
         at the same time, if one is initialized, then the other one should
         be too. */
      g_assert (priv->video_tee != NULL);
      return;
    }

    GstElement *preview;
    GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));

    priv->video_tee = gst_element_factory_make ("tee", NULL);
    gst_object_ref (priv->video_tee);
    gst_object_sink (priv->video_tee);

    priv->video_preview = empathy_video_widget_new_with_size (bus,
        SELF_VIDEO_SECTION_WIDTH, SELF_VIDEO_SECTION_HEIGTH);
    g_object_set (priv->video_preview, "sync", FALSE, "async", TRUE, NULL);
    gtk_box_pack_start (GTK_BOX (priv->self_user_output_hbox),
        priv->video_preview, TRUE, TRUE, 0);

    preview = empathy_video_widget_get_element (
        EMPATHY_VIDEO_WIDGET (priv->video_preview));
    gst_bin_add_many (GST_BIN (priv->pipeline), priv->video_input,
        priv->video_tee, preview, NULL);
    gst_element_link_many (priv->video_input, priv->video_tee,
        preview, NULL);

    g_object_unref (bus);

    gst_element_set_state (preview, GST_STATE_PLAYING);
    gst_element_set_state (priv->video_input, GST_STATE_PLAYING);
    gst_element_set_state (priv->video_tee, GST_STATE_PLAYING);
}

static void
empathy_call_window_init (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GtkBuilder *gui;
  GtkWidget *top_vbox;
  GtkWidget *h;
  GtkWidget *arrow;
  GtkWidget *page;
  GstBus *bus;
  gchar *filename;

  filename = empathy_file_lookup ("empathy-call-window.ui", "src");
  gui = empathy_builder_get_file (filename,
    "call_window_vbox", &top_vbox,
    "pane", &priv->pane,
    "statusbar", &priv->statusbar,
    "redial", &priv->redial_button,
    "microphone", &priv->mic_button,
    "camera", &priv->camera_button,
    "toolbar", &priv->toolbar,
    "send_video", &priv->send_video,
    "menuredial", &priv->redial,
    "show_preview", &priv->show_preview,
    "ui_manager", &priv->ui_manager,
    "menufullscreen", &priv->menu_fullscreen,
    NULL);

  empathy_builder_connect (gui, self,
    "menuhangup", "activate", empathy_call_window_hangup_cb,
    "hangup", "clicked", empathy_call_window_hangup_cb,
    "menuredial", "activate", empathy_call_window_redial_cb,
    "redial", "clicked", empathy_call_window_redial_cb,
    "microphone", "toggled", empathy_call_window_mic_toggled_cb,
    "camera", "toggled", empathy_call_window_camera_toggled_cb,
    "send_video", "toggled", empathy_call_window_send_video_toggled_cb,
    "show_preview", "toggled", empathy_call_window_show_preview_toggled_cb,
    "menufullscreen", "activate", empathy_call_window_fullscreen_cb,
    NULL);

  priv->lock = g_mutex_new ();

  gtk_container_add (GTK_CONTAINER (self), top_vbox);

  empathy_call_window_setup_toolbar (self);

  priv->content_hbox = gtk_hbox_new (FALSE, CONTENT_HBOX_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (priv->content_hbox),
                                  CONTENT_HBOX_BORDER_WIDTH);
  gtk_paned_pack1 (GTK_PANED (priv->pane), priv->content_hbox, TRUE, FALSE);

  priv->pipeline = gst_pipeline_new (NULL);
  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));
  gst_bus_add_watch (bus, empathy_call_window_bus_message, self);

  priv->remote_user_output_frame = gtk_frame_new (NULL);
  gtk_widget_set_size_request (priv->remote_user_output_frame,
      EMPATHY_VIDEO_WIDGET_DEFAULT_WIDTH, EMPATHY_VIDEO_WIDGET_DEFAULT_HEIGHT);
  gtk_box_pack_start (GTK_BOX (priv->content_hbox),
      priv->remote_user_output_frame, TRUE, TRUE,
      CONTENT_HBOX_CHILDREN_PACKING_PADDING);
  empathy_call_window_setup_remote_frame (bus, self);

  priv->self_user_output_frame = gtk_frame_new (NULL);
  gtk_widget_set_size_request (priv->self_user_output_frame,
      SELF_VIDEO_SECTION_WIDTH, SELF_VIDEO_SECTION_HEIGTH);

  priv->vbox = gtk_vbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (priv->content_hbox), priv->vbox,
      FALSE, FALSE, CONTENT_HBOX_CHILDREN_PACKING_PADDING);
  gtk_box_pack_start (GTK_BOX (priv->vbox), priv->self_user_output_frame, FALSE,
      FALSE, 0);
  empathy_call_window_setup_self_frame (bus, self);

  g_object_unref (bus);

  priv->sidebar_button = gtk_toggle_button_new_with_mnemonic (_("_Sidebar"));
  arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
  g_signal_connect (G_OBJECT (priv->sidebar_button), "toggled",
    G_CALLBACK (empathy_call_window_sidebar_toggled_cb), self);

  gtk_button_set_image (GTK_BUTTON (priv->sidebar_button), arrow);

  h = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_end (GTK_BOX (priv->vbox), h, FALSE, FALSE, 3);
  gtk_box_pack_end (GTK_BOX (h), priv->sidebar_button, FALSE, FALSE, 3);

  priv->sidebar = empathy_sidebar_new ();
  g_signal_connect (G_OBJECT (priv->sidebar),
    "hide", G_CALLBACK (empathy_call_window_sidebar_hidden_cb), self);
  g_signal_connect (G_OBJECT (priv->sidebar),
    "show", G_CALLBACK (empathy_call_window_sidebar_shown_cb), self);
  gtk_paned_pack2 (GTK_PANED (priv->pane), priv->sidebar, FALSE, FALSE);

  priv->dtmf_panel = empathy_call_window_create_dtmf (self);
  empathy_sidebar_add_page (EMPATHY_SIDEBAR (priv->sidebar), _("Dialpad"),
    priv->dtmf_panel);

  gtk_widget_set_sensitive (priv->dtmf_panel, FALSE);

  page = empathy_call_window_create_audio_input (self);
  empathy_sidebar_add_page (EMPATHY_SIDEBAR (priv->sidebar), _("Audio input"),
    page);

  page = empathy_call_window_create_video_input (self);
  empathy_sidebar_add_page (EMPATHY_SIDEBAR (priv->sidebar), _("Video input"),
    page);

  gtk_widget_show_all (top_vbox);

  gtk_widget_hide (priv->sidebar);

  priv->fullscreen = empathy_call_window_fullscreen_new (self);
  empathy_call_window_fullscreen_set_video_widget (priv->fullscreen, priv->video_output);
  g_signal_connect (G_OBJECT (priv->fullscreen->leave_fullscreen_button),
      "clicked", G_CALLBACK (empathy_call_window_fullscreen_cb), self);

  g_signal_connect (G_OBJECT (self), "realize",
    G_CALLBACK (empathy_call_window_realized_cb), self);

  g_signal_connect (G_OBJECT (self), "delete-event",
    G_CALLBACK (empathy_call_window_delete_cb), self);

  g_signal_connect (G_OBJECT (self), "window-state-event",
    G_CALLBACK (empathy_call_window_state_event_cb), self);

  g_signal_connect (G_OBJECT (self), "key-press-event",
      G_CALLBACK (empathy_call_window_key_press_cb), self);

  empathy_call_window_status_message (self, CONNECTING_STATUS_TEXT);

  priv->timer = g_timer_new ();

  g_object_ref (priv->ui_manager);
  g_object_unref (gui);
  g_free (filename);
}

/* Instead of specifying a width and a height, we specify only one size. That's
   because we want a square avatar icon.  */
static void
init_contact_avatar_with_size (EmpathyContact *contact, GtkWidget *image_widget,
    gint size)
{

  GdkPixbuf *pixbuf_avatar = NULL;

  if (contact != NULL)
    {
      pixbuf_avatar = empathy_pixbuf_avatar_from_contact_scaled (contact,
        size, size);
    }

  if (pixbuf_avatar == NULL)
    {
      pixbuf_avatar = empathy_pixbuf_from_icon_name_sized ("stock_person",
          size);
    }

  gtk_image_set_from_pixbuf (GTK_IMAGE (image_widget), pixbuf_avatar);
}

static void
set_window_title (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  gchar *tmp;

  tmp = g_strdup_printf (_("Call with %s"),
      empathy_contact_get_name (priv->contact));
  gtk_window_set_title (GTK_WINDOW (self), tmp);
  g_free (tmp);
}

static void
contact_name_changed_cb (EmpathyContact *contact,
    GParamSpec *pspec, EmpathyCallWindow *self)
{
  set_window_title (self);
}

static void
contact_avatar_changed_cb (EmpathyContact *contact,
    GParamSpec *pspec, GtkWidget *avatar_widget)
{
  init_contact_avatar_with_size (contact, avatar_widget,
      avatar_widget->allocation.height);
}

static void
empathy_call_window_got_self_contact_cb (EmpathyTpContactFactory *factory,
    EmpathyContact *contact, const GError *error, gpointer user_data,
    GObject *weak_object)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (user_data);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  init_contact_avatar_with_size (contact, priv->self_user_avatar_widget,
      MIN (SELF_VIDEO_SECTION_WIDTH, SELF_VIDEO_SECTION_HEIGTH));

  g_signal_connect (contact, "notify::avatar",
      G_CALLBACK (contact_avatar_changed_cb), priv->self_user_avatar_widget);
}

static void
empathy_call_window_setup_avatars (EmpathyCallWindow *self,
    EmpathyCallHandler *handler)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  g_object_get (handler, "contact", &(priv->contact), NULL);

  if (priv->contact != NULL)
    {
      TpConnection *connection;
      EmpathyTpContactFactory *factory;

      set_window_title (self);

      g_signal_connect (priv->contact, "notify::name",
          G_CALLBACK (contact_name_changed_cb), self);
      g_signal_connect (priv->contact, "notify::avatar",
          G_CALLBACK (contact_avatar_changed_cb),
          priv->remote_user_avatar_widget);

      /* Retreiving the self avatar */
      connection = empathy_contact_get_connection (priv->contact);
      factory = empathy_tp_contact_factory_dup_singleton (connection);
      empathy_tp_contact_factory_get_from_handle (factory,
          tp_connection_get_self_handle (connection),
          empathy_call_window_got_self_contact_cb, self, NULL, NULL);

      g_object_unref (factory);
    }
  else
    {
      g_warning ("call handler doesn't have a contact");
      gtk_window_set_title (GTK_WINDOW (self), _("Call"));

      /* Since we can't access the remote contact, we can't get a connection
         to it and can't get the self contact (and its avatar). This means
         that we have to manually set the self avatar. */
      init_contact_avatar_with_size (NULL, priv->self_user_avatar_widget,
          MIN (SELF_VIDEO_SECTION_WIDTH, SELF_VIDEO_SECTION_HEIGTH));
    }

  init_contact_avatar_with_size (priv->contact,
      priv->remote_user_avatar_widget, MIN (REMOTE_CONTACT_AVATAR_DEFAULT_WIDTH,
      REMOTE_CONTACT_AVATAR_DEFAULT_HEIGHT));

  /* The remote avatar is shown by default and will be hidden when we receive
     video from the remote side. */
  gtk_widget_hide (priv->video_output);
  gtk_widget_show (priv->remote_user_avatar_widget);
}

static void
empathy_call_window_setup_video_preview_visibility (EmpathyCallWindow *self,
    EmpathyCallHandler *handler)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  gboolean initial_video = empathy_call_handler_has_initial_video (priv->handler);

  if (initial_video)
    {
      empathy_call_window_setup_video_preview (self);
      gtk_widget_hide (priv->self_user_avatar_widget);

      if (priv->video_preview != NULL)
        gtk_widget_show (priv->video_preview);
    }
  else
    {
      gtk_widget_show (priv->self_user_avatar_widget);
    }

  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (priv->show_preview),
      initial_video);
}

static void
empathy_call_window_constructed (GObject *object)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (object);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  g_assert (priv->handler != NULL);
  empathy_call_window_setup_avatars (self, priv->handler);
  empathy_call_window_setup_video_preview_visibility (self, priv->handler);
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

  object_class->constructed = empathy_call_window_constructed;
  object_class->set_property = empathy_call_window_set_property;
  object_class->get_property = empathy_call_window_get_property;

  object_class->dispose = empathy_call_window_dispose;
  object_class->finalize = empathy_call_window_finalize;

  param_spec = g_param_spec_object ("handler",
    "handler", "The call handler",
    EMPATHY_TYPE_CALL_HANDLER,
    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
    PROP_CALL_HANDLER, param_spec);

}

static void
empathy_call_window_video_stream_changed_cb (EmpathyTpCall *call,
    GParamSpec *property, EmpathyCallWindow *self)
{
  empathy_call_window_update_avatars_visibility (call, self);
}

void
empathy_call_window_dispose (GObject *object)
{
  EmpathyTpCall *call;
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (object);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  g_object_get (priv->handler, "tp-call", &call, NULL);

  if (call != NULL)
    {
      g_signal_handlers_disconnect_by_func (call,
        empathy_call_window_video_stream_changed_cb, object);
    }

  g_object_unref (call);

  if (priv->handler != NULL)
    g_object_unref (priv->handler);

  priv->handler = NULL;

  if (priv->pipeline != NULL)
    g_object_unref (priv->pipeline);
  priv->pipeline = NULL;

  if (priv->video_input != NULL)
    g_object_unref (priv->video_input);
  priv->video_input = NULL;

  if (priv->audio_input != NULL)
    g_object_unref (priv->audio_input);
  priv->audio_input = NULL;

  if (priv->audio_output != NULL)
    g_object_unref (priv->audio_output);
  priv->audio_output = NULL;

  if (priv->video_tee != NULL)
    g_object_unref (priv->video_tee);
  priv->video_tee = NULL;

  if (priv->timer_id != 0)
    g_source_remove (priv->timer_id);
  priv->timer_id = 0;

  if (priv->ui_manager != NULL)
    g_object_unref (priv->ui_manager);
  priv->ui_manager = NULL;

  if (priv->contact != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->contact,
          contact_name_changed_cb, self);
      g_object_unref (priv->contact);
      priv->contact = NULL;
    }

  /* release any references held by the object here */
  if (G_OBJECT_CLASS (empathy_call_window_parent_class)->dispose)
    G_OBJECT_CLASS (empathy_call_window_parent_class)->dispose (object);
}

void
empathy_call_window_finalize (GObject *object)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (object);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  if (priv->video_output_motion_handler_id != 0)
    {
      g_signal_handler_disconnect (G_OBJECT (priv->video_output),
          priv->video_output_motion_handler_id);
      priv->video_output_motion_handler_id = 0;
    }

  /* free any data held directly by the object here */
  g_mutex_free (priv->lock);

  g_timer_destroy (priv->timer);

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
}

static gboolean
empathy_call_window_request_resource_cb (EmpathyCallHandler *handler,
  FsMediaType type, FsStreamDirection direction, gpointer user_data)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (user_data);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  if (type != TP_MEDIA_STREAM_TYPE_VIDEO)
    return TRUE;

  if (direction == FS_DIRECTION_RECV)
    return TRUE;

  /* video and direction is send */
  return priv->video_input != NULL;
}

static gboolean
empathy_call_window_reset_pipeline (EmpathyCallWindow *self)
{
  GstStateChangeReturn state_change_return;
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  if (priv->pipeline == NULL)
    return TRUE;

  state_change_return = gst_element_set_state (priv->pipeline, GST_STATE_NULL);

  if (state_change_return == GST_STATE_CHANGE_SUCCESS ||
        state_change_return == GST_STATE_CHANGE_NO_PREROLL)
    {
      if (priv->pipeline != NULL)
        g_object_unref (priv->pipeline);
      priv->pipeline = NULL;

      if (priv->video_input != NULL)
        g_object_unref (priv->video_input);
      priv->video_input = NULL;

      if (priv->audio_input != NULL)
        g_object_unref (priv->audio_input);
      priv->audio_input = NULL;

      if (priv->audio_output != NULL)
        g_object_unref (priv->audio_output);
      priv->audio_output = NULL;

      if (priv->video_tee != NULL)
        g_object_unref (priv->video_tee);
      priv->video_tee = NULL;

      if (priv->video_preview != NULL)
        gtk_widget_destroy (priv->video_preview);
      priv->video_preview = NULL;

      priv->liveadder = NULL;
      priv->funnel = NULL;

      return TRUE;
    }
  else
    {
      g_message ("Error: could not destroy pipeline. Closing call window");
      gtk_widget_destroy (GTK_WIDGET (self));

      return FALSE;
    }
}

static gboolean
empathy_call_window_disconnected (EmpathyCallWindow *self)
{
  gboolean could_disconnect = FALSE;
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  gboolean could_reset_pipeline = empathy_call_window_reset_pipeline (self);

  if (could_reset_pipeline)
    {
      g_mutex_lock (priv->lock);

      g_timer_stop (priv->timer);

      if (priv->timer_id != 0)
        g_source_remove (priv->timer_id);
      priv->timer_id = 0;

      g_mutex_unlock (priv->lock);

      empathy_call_window_status_message (self, _("Disconnected"));

      gtk_action_set_sensitive (priv->redial, TRUE);
      gtk_widget_set_sensitive (priv->redial_button, TRUE);
      gtk_widget_set_sensitive (priv->camera_button, FALSE);
      gtk_action_set_sensitive (priv->send_video, FALSE);
      priv->sending_video = FALSE;
      priv->connected = FALSE;
      priv->call_started = FALSE;

      could_disconnect = TRUE;
    }

  return could_disconnect;
}


static void
empathy_call_window_channel_closed_cb (TfChannel *channel, gpointer user_data)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (user_data);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  if (empathy_call_window_disconnected (self) && priv->redialing)
    {
      empathy_call_window_restart_call (self);
      priv->redialing = FALSE;
    }
}

/* Called with global lock held */
static GstPad *
empathy_call_window_get_video_sink_pad (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GstPad *pad;

  if (priv->funnel == NULL)
    {
      GstElement *output;

      output = empathy_video_widget_get_element (EMPATHY_VIDEO_WIDGET
        (priv->video_output));

      priv->funnel = gst_element_factory_make ("fsfunnel", NULL);

      gst_bin_add (GST_BIN (priv->pipeline), priv->funnel);
      gst_bin_add (GST_BIN (priv->pipeline), output);

      gst_element_link (priv->funnel, output);

      gst_element_set_state (priv->funnel, GST_STATE_PLAYING);
      gst_element_set_state (output, GST_STATE_PLAYING);
    }

  pad = gst_element_get_request_pad (priv->funnel, "sink%d");

  return pad;
}

/* Called with global lock held */
static GstPad *
empathy_call_window_get_audio_sink_pad (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GstPad *pad;

  if (priv->liveadder == NULL)
    {
      priv->liveadder = gst_element_factory_make ("liveadder", NULL);

      gst_bin_add (GST_BIN (priv->pipeline), priv->liveadder);
      gst_bin_add (GST_BIN (priv->pipeline), priv->audio_output);

      gst_element_link (priv->liveadder, priv->audio_output);

      gst_element_set_state (priv->liveadder, GST_STATE_PLAYING);
      gst_element_set_state (priv->audio_output, GST_STATE_PLAYING);
    }

  pad = gst_element_get_request_pad (priv->liveadder, "sink%d");

  return pad;
}

static gboolean
empathy_call_window_update_timer (gpointer user_data)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (user_data);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  gchar *str;
  gdouble time;

  time = g_timer_elapsed (priv->timer, NULL);

  /* Translators: number of minutes:seconds the caller has been connected */
  str = g_strdup_printf (_("Connected — %d:%02dm"), (int) time / 60,
    (int) time % 60);
  empathy_call_window_status_message (self, str);
  g_free (str);

  return TRUE;
}

static gboolean
empathy_call_window_connected (gpointer user_data)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (user_data);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  EmpathyTpCall *call;

  g_object_get (priv->handler, "tp-call", &call, NULL);

  g_signal_connect (call, "notify::video-stream",
    G_CALLBACK (empathy_call_window_video_stream_changed_cb), self);

  if (empathy_tp_call_has_dtmf (call))
    gtk_widget_set_sensitive (priv->dtmf_panel, TRUE);

  if (priv->video_input == NULL)
      empathy_call_window_set_send_video (self, FALSE);

  priv->sending_video = empathy_tp_call_is_sending_video (call);

  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (priv->show_preview),
      priv->sending_video);
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (priv->send_video),
      priv->sending_video && priv->video_input != NULL);
  gtk_toggle_tool_button_set_active (
      GTK_TOGGLE_TOOL_BUTTON (priv->camera_button),
      priv->sending_video && priv->video_input != NULL);
  gtk_widget_set_sensitive (priv->camera_button, priv->video_input != NULL);
  gtk_action_set_sensitive (priv->send_video, priv->video_input != NULL);

  gtk_action_set_sensitive (priv->redial, FALSE);
  gtk_widget_set_sensitive (priv->redial_button, FALSE);

  empathy_call_window_update_avatars_visibility (call, self);

  g_object_unref (call);

  g_mutex_lock (priv->lock);

  priv->timer_id = g_timeout_add_seconds (1,
    empathy_call_window_update_timer, self);

  g_mutex_unlock (priv->lock);

  empathy_call_window_update_timer (self);

  return FALSE;
}


/* Called from the streaming thread */
static void
empathy_call_window_src_added_cb (EmpathyCallHandler *handler,
  GstPad *src, guint media_type, gpointer user_data)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (user_data);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  GstPad *pad;

  g_mutex_lock (priv->lock);

  if (priv->connected == FALSE)
    {
      g_timer_start (priv->timer);
      priv->timer_id = g_idle_add  (empathy_call_window_connected, self);
      priv->connected = TRUE;
    }

  switch (media_type)
    {
      case TP_MEDIA_STREAM_TYPE_AUDIO:
        pad = empathy_call_window_get_audio_sink_pad (self);
        break;
      case TP_MEDIA_STREAM_TYPE_VIDEO:
        gtk_widget_hide (priv->remote_user_avatar_widget);
        gtk_widget_show (priv->video_output);
        pad = empathy_call_window_get_video_sink_pad (self);
        break;
      default:
        g_assert_not_reached ();
    }

  gst_pad_link (src, pad);
  gst_object_unref (pad);

  g_mutex_unlock (priv->lock);
}

/* Called from the streaming thread */
static void
empathy_call_window_sink_added_cb (EmpathyCallHandler *handler,
  GstPad *sink, guint media_type, gpointer user_data)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (user_data);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GstPad *pad;

  switch (media_type)
    {
      case TP_MEDIA_STREAM_TYPE_AUDIO:
        gst_bin_add (GST_BIN (priv->pipeline), priv->audio_input);

        pad = gst_element_get_static_pad (priv->audio_input, "src");
        gst_pad_link (pad, sink);

        gst_element_set_state (priv->audio_input, GST_STATE_PLAYING);
        break;
      case TP_MEDIA_STREAM_TYPE_VIDEO:
        if (priv->video_input != NULL)
          {
            EmpathyTpCall *call;
            g_object_get (priv->handler, "tp-call", &call, NULL);

            if (empathy_tp_call_is_sending_video (call))
              {
                empathy_call_window_setup_video_preview (self);

                gtk_toggle_action_set_active (
                    GTK_TOGGLE_ACTION (priv->show_preview), TRUE);

                if (priv->video_preview != NULL)
                  gtk_widget_show (priv->video_preview);
                gtk_widget_hide (priv->self_user_avatar_widget);
              }

            g_object_unref (call);

            if (priv->video_tee != NULL)
              {
                pad = gst_element_get_request_pad (priv->video_tee, "src%d");
                gst_pad_link (pad, sink);
              }
          }
        break;
      default:
        g_assert_not_reached ();
    }

}

static void
empathy_call_window_remove_video_input (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GstElement *preview;

  preview = empathy_video_widget_get_element (
    EMPATHY_VIDEO_WIDGET (priv->video_preview));

  gst_element_set_state (priv->video_input, GST_STATE_NULL);
  gst_element_set_state (priv->video_tee, GST_STATE_NULL);
  gst_element_set_state (preview, GST_STATE_NULL);

  gst_bin_remove_many (GST_BIN (priv->pipeline), priv->video_input,
    priv->video_tee, preview, NULL);

  g_object_unref (priv->video_input);
  priv->video_input = NULL;
  g_object_unref (priv->video_tee);
  priv->video_tee = NULL;
  gtk_widget_destroy (priv->video_preview);
  priv->video_preview = NULL;

  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (priv->send_video), FALSE);
  gtk_toggle_tool_button_set_active (
      GTK_TOGGLE_TOOL_BUTTON (priv->camera_button), FALSE);
  gtk_widget_set_sensitive (priv->camera_button, FALSE);
  gtk_action_set_sensitive (priv->send_video, FALSE);

  gtk_widget_show (priv->self_user_avatar_widget);
}


static gboolean
empathy_call_window_bus_message (GstBus *bus, GstMessage *message,
  gpointer user_data)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (user_data);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GstState newstate;

  empathy_call_handler_bus_message (priv->handler, bus, message);

  switch (GST_MESSAGE_TYPE (message))
    {
      case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC (message) == GST_OBJECT (priv->video_input))
          {
            gst_message_parse_state_changed (message, NULL, &newstate, NULL);
            if (newstate == GST_STATE_PAUSED)
                empathy_call_window_setup_video_input (self);
          }
        if (GST_MESSAGE_SRC (message) == GST_OBJECT (priv->pipeline) &&
            !priv->call_started)
          {
            gst_message_parse_state_changed (message, NULL, &newstate, NULL);
            if (newstate == GST_STATE_PAUSED)
              {
                priv->call_started = TRUE;
                empathy_call_handler_start_call (priv->handler);
                gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
              }
          }
        break;
      case GST_MESSAGE_ERROR:
        {
          GError *error = NULL;
          GstElement *gst_error;
          gchar *debug;

          gst_message_parse_error (message, &error, &debug);
          gst_error = GST_ELEMENT (GST_MESSAGE_SRC (message));

          g_message ("Element error: %s -- %s\n", error->message, debug);

          if (g_str_has_prefix (gst_element_get_name (gst_error),
                VIDEO_INPUT_ERROR_PREFIX))
            {
              /* Remove the video input and continue */
              if (priv->video_input != NULL)
                empathy_call_window_remove_video_input (self);
              gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
            }
          else
            {
              empathy_call_window_disconnected (self);
            }
          g_error_free (error);
          g_free (debug);
        }
      default:
        break;
    }

  return TRUE;
}

static void
empathy_call_window_update_avatars_visibility (EmpathyTpCall *call,
    EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  if (empathy_tp_call_is_receiving_video (call))
    {
      gtk_widget_hide (priv->remote_user_avatar_widget);
      gtk_widget_show (priv->video_output);
    }
  else
    {
      gtk_widget_hide (priv->video_output);
      gtk_widget_show (priv->remote_user_avatar_widget);
    }

  if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (priv->show_preview)))
    {
      if (priv->video_preview != NULL
          && empathy_tp_call_is_sending_video (call))
        {
          gtk_widget_hide (priv->self_user_avatar_widget);
          gtk_widget_show (priv->video_preview);
        }
      else
        {
          if (priv->video_preview != NULL)
            gtk_widget_hide (priv->video_preview);

          gtk_widget_show (priv->self_user_avatar_widget);
        }
    }
}

static void
empathy_call_window_realized_cb (GtkWidget *widget, EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  g_signal_connect (priv->handler, "conference-added",
    G_CALLBACK (empathy_call_window_conference_added_cb), window);
  g_signal_connect (priv->handler, "request-resource",
    G_CALLBACK (empathy_call_window_request_resource_cb), window);
  g_signal_connect (priv->handler, "closed",
    G_CALLBACK (empathy_call_window_channel_closed_cb), window);
  g_signal_connect (priv->handler, "src-pad-added",
    G_CALLBACK (empathy_call_window_src_added_cb), window);
  g_signal_connect (priv->handler, "sink-pad-added",
    G_CALLBACK (empathy_call_window_sink_added_cb), window);

  gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
}

static gboolean
empathy_call_window_delete_cb (GtkWidget *widget, GdkEvent*event,
  EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  if (priv->pipeline != NULL)
    gst_element_set_state (priv->pipeline, GST_STATE_NULL);

  return FALSE;
}

static void
show_controls (EmpathyCallWindow *window, gboolean set_fullscreen)
{
  GtkWidget *menu;
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  menu = gtk_ui_manager_get_widget (priv->ui_manager,
            "/menubar1");

  if (set_fullscreen)
    {
      gtk_widget_hide (priv->sidebar);
      gtk_widget_hide (menu);
      gtk_widget_hide (priv->vbox);
      gtk_widget_hide (priv->statusbar);
      gtk_widget_hide (priv->toolbar);
    }
  else
    {
      if (priv->sidebar_was_visible_before_fs)
        gtk_widget_show (priv->sidebar);

      gtk_widget_show (menu);
      gtk_widget_show (priv->vbox);
      gtk_widget_show (priv->statusbar);
      gtk_widget_show (priv->toolbar);

      gtk_window_resize (GTK_WINDOW (window), priv->original_width_before_fs,
          priv->original_height_before_fs);
    }
}

static void
show_borders (EmpathyCallWindow *window, gboolean set_fullscreen)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  gtk_container_set_border_width (GTK_CONTAINER (priv->content_hbox),
      set_fullscreen ? 0 : CONTENT_HBOX_BORDER_WIDTH);
  gtk_box_set_spacing (GTK_BOX (priv->content_hbox),
      set_fullscreen ? 0 : CONTENT_HBOX_SPACING);
  gtk_box_set_child_packing (GTK_BOX (priv->content_hbox),
      priv->video_output, TRUE, TRUE,
      set_fullscreen ? 0 : CONTENT_HBOX_CHILDREN_PACKING_PADDING,
      GTK_PACK_START);
  gtk_box_set_child_packing (GTK_BOX (priv->content_hbox),
      priv->vbox, TRUE, TRUE,
      set_fullscreen ? 0 : CONTENT_HBOX_CHILDREN_PACKING_PADDING,
      GTK_PACK_START);
}

static gboolean
empathy_call_window_state_event_cb (GtkWidget *widget,
  GdkEventWindowState *event, EmpathyCallWindow *window)
{
  if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
      EmpathyCallWindowPriv *priv = GET_PRIV (window);
      gboolean set_fullscreen = event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;

      if (set_fullscreen)
        {
          gboolean sidebar_was_visible;
          gint original_width = GTK_WIDGET (window)->allocation.width;
          gint original_height = GTK_WIDGET (window)->allocation.height;

          g_object_get (priv->sidebar, "visible", &sidebar_was_visible, NULL);

          priv->sidebar_was_visible_before_fs = sidebar_was_visible;
          priv->original_width_before_fs = original_width;
          priv->original_height_before_fs = original_height;

          if (priv->video_output_motion_handler_id == 0 &&
                priv->video_output != NULL)
            {
              priv->video_output_motion_handler_id = g_signal_connect (
                  G_OBJECT (priv->video_output), "motion-notify-event",
                  G_CALLBACK (empathy_call_window_video_output_motion_notify), window);
            }
        }
      else
        {
          if (priv->video_output_motion_handler_id != 0)
            {
              g_signal_handler_disconnect (G_OBJECT (priv->video_output),
                  priv->video_output_motion_handler_id);
              priv->video_output_motion_handler_id = 0;
            }
        }

      empathy_call_window_fullscreen_set_fullscreen (priv->fullscreen,
          set_fullscreen);
      show_controls (window, set_fullscreen);
      show_borders (window, set_fullscreen);
      gtk_action_set_stock_id (priv->menu_fullscreen,
          (set_fullscreen ? "gtk-leave-fullscreen" : "gtk-fullscreen"));
      priv->is_fullscreen = set_fullscreen;
  }

  return FALSE;
}

static void
empathy_call_window_sidebar_toggled_cb (GtkToggleButton *toggle,
  EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);
  GtkWidget *arrow;
  int w,h, handle_size;

  w = GTK_WIDGET (window)->allocation.width;
  h = GTK_WIDGET (window)->allocation.height;

  gtk_widget_style_get (priv->pane, "handle_size", &handle_size, NULL);

  if (gtk_toggle_button_get_active (toggle))
    {
      arrow = gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_NONE);
      gtk_widget_show (priv->sidebar);
      w += priv->sidebar->allocation.width + handle_size;
    }
  else
    {
      arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
      w -= priv->sidebar->allocation.width + handle_size;
      gtk_widget_hide (priv->sidebar);
    }

  gtk_button_set_image (GTK_BUTTON (priv->sidebar_button), arrow);

  if (w > 0 && h > 0)
    gtk_window_resize (GTK_WINDOW (window), w, h);
}

static void
empathy_call_window_set_send_video (EmpathyCallWindow *window,
  gboolean send)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);
  EmpathyTpCall *call;

  priv->sending_video = send;

  /* When we start sending video, we want to show the video preview by
     default. */
  if (send)
    {
      empathy_call_window_setup_video_preview (window);
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (priv->show_preview),
          TRUE);
    }

  g_object_get (priv->handler, "tp-call", &call, NULL);
  empathy_tp_call_request_video_stream_direction (call, send);
  g_object_unref (call);
}

static void
empathy_call_window_camera_toggled_cb (GtkToggleToolButton *toggle,
  EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);
  gboolean active;

  active = (gtk_toggle_tool_button_get_active (toggle));

  if (priv->sending_video == active)
    return;

  empathy_call_window_set_send_video (window, active);
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (priv->send_video), active);
}

static void
empathy_call_window_send_video_toggled_cb (GtkToggleAction *toggle,
  EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);
  gboolean active;

  active = (gtk_toggle_action_get_active (toggle));

  if (priv->sending_video == active)
    return;

  empathy_call_window_set_send_video (window, active);
  gtk_toggle_tool_button_set_active (
      GTK_TOGGLE_TOOL_BUTTON (priv->camera_button), active);
}

static void
empathy_call_window_show_preview_toggled_cb (GtkToggleAction *toggle,
  EmpathyCallWindow *window)
{
  gboolean show_preview_toggled;
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  show_preview_toggled = gtk_toggle_action_get_active (toggle);

  if (show_preview_toggled)
    gtk_widget_show (priv->self_user_output_frame);
  else
    gtk_widget_hide (priv->self_user_output_frame);
}

static void
empathy_call_window_mic_toggled_cb (GtkToggleToolButton *toggle,
  EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);
  gboolean active;

  active = (gtk_toggle_tool_button_get_active (toggle));

  if (active)
    {
      empathy_audio_src_set_volume (EMPATHY_GST_AUDIO_SRC (priv->audio_input),
        priv->volume);
      gtk_adjustment_set_value (priv->audio_input_adj, priv->volume * 100);
    }
  else
    {
      /* TODO, Instead of setting the input volume to 0 we should probably
       * stop sending but this would cause the audio call to drop if both
       * sides mute at the same time on certain CMs AFAIK. Need to revisit this
       * in the future. GNOME #574574
       */
      empathy_audio_src_set_volume (EMPATHY_GST_AUDIO_SRC (priv->audio_input),
        0);
      gtk_adjustment_set_value (priv->audio_input_adj, 0);
    }
}

static void
empathy_call_window_sidebar_hidden_cb (EmpathySidebar *sidebar,
  EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->sidebar_button),
    FALSE);
}

static void
empathy_call_window_sidebar_shown_cb (EmpathySidebar *sidebar,
  EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->sidebar_button),
    TRUE);
}

static void
empathy_call_window_hangup_cb (gpointer object,
                               EmpathyCallWindow *window)
{
  if (empathy_call_window_disconnected (window))
    gtk_widget_destroy (GTK_WIDGET (window));
}

static void
empathy_call_window_restart_call (EmpathyCallWindow *window)
{
  GstBus *bus;
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  gtk_widget_destroy (priv->remote_user_output_hbox);
  gtk_widget_destroy (priv->self_user_output_hbox);

  priv->pipeline = gst_pipeline_new (NULL);
  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));
  gst_bus_add_watch (bus, empathy_call_window_bus_message, window);

  empathy_call_window_setup_remote_frame (bus, window);
  empathy_call_window_setup_self_frame (bus, window);

  g_object_unref (bus);

  gtk_widget_show_all (priv->content_hbox);

  empathy_call_window_status_message (window, CONNECTING_STATUS_TEXT);
  priv->call_started = TRUE;
  empathy_call_handler_start_call (priv->handler);
  gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);

  gtk_action_set_sensitive (priv->redial, FALSE);
  gtk_widget_set_sensitive (priv->redial_button, FALSE);
}

static void
empathy_call_window_redial_cb (gpointer object,
    EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  if (priv->connected)
    priv->redialing = TRUE;

  empathy_call_handler_stop_call (priv->handler);

  if (!priv->connected)
    empathy_call_window_restart_call (window);
}

static void
empathy_call_window_fullscreen_cb (gpointer object,
                                   EmpathyCallWindow *window)
{
  empathy_call_window_fullscreen_toggle (window);
}

static void
empathy_call_window_fullscreen_toggle (EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  if (priv->is_fullscreen)
    gtk_window_unfullscreen (GTK_WINDOW (window));
  else
    gtk_window_fullscreen (GTK_WINDOW (window));
}

static gboolean
empathy_call_window_video_button_press_cb (GtkWidget *video_output,
  GdkEventButton *event, EmpathyCallWindow *window)
{
  if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
      empathy_call_window_video_menu_popup (window, event->button);
      return TRUE;
    }

  return FALSE;
}

static gboolean
empathy_call_window_key_press_cb (GtkWidget *video_output,
  GdkEventKey *event, EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  if (priv->is_fullscreen && event->keyval == GDK_Escape)
    {
      /* Since we are in fullscreen mode, toggling will bring us back to
         normal mode. */
      empathy_call_window_fullscreen_toggle (window);
      return TRUE;
    }

  return FALSE;
}

static gboolean
empathy_call_window_video_output_motion_notify (GtkWidget *widget,
    GdkEventMotion *event, EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  if (priv->is_fullscreen)
    {
      empathy_call_window_fullscreen_show_popup (priv->fullscreen);
      return TRUE;
    }
  return FALSE;
}

static void
empathy_call_window_video_menu_popup (EmpathyCallWindow *window,
  guint button)
{
  GtkWidget *menu;
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  menu = gtk_ui_manager_get_widget (priv->ui_manager,
            "/video-popup");
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
      button, gtk_get_current_event_time ());
  gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
}

static void
empathy_call_window_status_message (EmpathyCallWindow *window,
  gchar *message)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  if (priv->context_id == 0)
    {
      priv->context_id = gtk_statusbar_get_context_id (
        GTK_STATUSBAR (priv->statusbar), "voip call status messages");
    }
  else
    {
      gtk_statusbar_pop (GTK_STATUSBAR (priv->statusbar), priv->context_id);
    }

  gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar), priv->context_id,
    message);
}

static void
empathy_call_window_volume_changed_cb (GtkScaleButton *button,
  gdouble value, EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  empathy_audio_sink_set_volume (EMPATHY_GST_AUDIO_SINK (priv->audio_output),
    value);
}
