/*
 * empathy-call-window.c - Source for EmpathyCallWindow
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

#include <math.h>

#include <gst/gst.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <telepathy-farsight/channel.h>

#include <libempathy/empathy-utils.h>
#include <libempathy-gtk/empathy-video-widget.h>
#include <libempathy-gtk/empathy-audio-src.h>
#include <libempathy-gtk/empathy-audio-sink.h>
#include <libempathy-gtk/empathy-video-src.h>
#include <libempathy-gtk/empathy-ui-utils.h>

#include "empathy-call-window.h"

#include "empathy-sidebar.h"

#define BUTTON_ID "empathy-call-dtmf-button-id"

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

  gboolean connected;

  GtkWidget *video_output;
  GtkWidget *video_preview;
  GtkWidget *sidebar;
  GtkWidget *sidebar_button;
  GtkWidget *statusbar;
  GtkWidget *volume_button;
  GtkWidget *mic_button;
  GtkWidget *camera_button;

  gdouble volume;
  GtkAdjustment *audio_input_adj;

  GtkWidget *dtmf_panel;

  GstElement *video_input;
  GstElement *audio_input;
  GstElement *audio_output;
  GstElement *pipeline;
  GstElement *video_tee;

  GstElement *funnel;
  GstElement *liveadder;

  GladeXML *glade;
  guint context_id;

  GTimer *timer;
  guint timer_id;

  GtkWidget *video_contrast;
  GtkWidget *video_brightness;
  GtkWidget *video_gamma;

  GMutex *lock;
  gboolean call_started;
};

#define GET_PRIV(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EMPATHY_TYPE_CALL_WINDOW, \
    EmpathyCallWindowPriv))

static void empathy_call_window_realized_cb (GtkWidget *widget,
  EmpathyCallWindow *window);

static gboolean empathy_call_window_delete_cb (GtkWidget *widget,
  GdkEvent*event, EmpathyCallWindow *window);

static void empathy_call_window_sidebar_toggled_cb (GtkToggleButton *toggle,
  EmpathyCallWindow *window);

static void empathy_call_window_camera_toggled_cb (GtkToggleToolButton *toggle,
  EmpathyCallWindow *window);

static void empathy_call_window_mic_toggled_cb (
  GtkToggleToolButton *toggle, EmpathyCallWindow *window);

static void empathy_call_window_sidebar_hidden_cb (EmpathySidebar *sidebar,
  EmpathyCallWindow *window);

static void empathy_call_window_hangup (EmpathyCallWindow *window);

static void empathy_call_window_status_message (EmpathyCallWindow *window,
  gchar *message);

static gboolean empathy_call_window_bus_message (GstBus *bus,
  GstMessage *message, gpointer user_data);

static void
empathy_call_window_volume_changed_cb (GtkScaleButton *button,
  gdouble value, EmpathyCallWindow *window);

static void
empathy_call_window_setup_menubar (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GtkWidget *hangup;

  hangup = glade_xml_get_widget (priv->glade, "menuhangup");
  g_signal_connect_swapped (G_OBJECT (hangup), "activate",
    G_CALLBACK (empathy_call_window_hangup), self);
}

static void
empathy_call_window_setup_toolbar (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GtkWidget *hangup;
  GtkWidget *mic;
  GtkWidget *camera;
  GtkWidget *toolbar;
  GtkToolItem *tool_item;

  hangup = glade_xml_get_widget (priv->glade, "hangup");

  g_signal_connect_swapped (G_OBJECT (hangup), "clicked",
    G_CALLBACK (empathy_call_window_hangup), self);

  mic = glade_xml_get_widget (priv->glade, "microphone");
  gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (mic), TRUE);

  priv->mic_button = mic;
  g_signal_connect (G_OBJECT (priv->mic_button), "toggled",
    G_CALLBACK (empathy_call_window_mic_toggled_cb), self);

  toolbar = glade_xml_get_widget (priv->glade, "toolbar");

  /* Add an empty expanded GtkToolItem so the volume button is at the end of
   * the toolbar. */
  tool_item = gtk_tool_item_new ();
  gtk_tool_item_set_expand (tool_item, TRUE);
  gtk_widget_show (GTK_WIDGET (tool_item));
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), tool_item, -1);

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
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), tool_item, -1);

  camera = glade_xml_get_widget (priv->glade, "camera");
  priv->camera_button = camera;
  gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (camera), FALSE);
  gtk_widget_set_sensitive (priv->camera_button, FALSE);

  g_signal_connect (G_OBJECT (camera), "toggled",
    G_CALLBACK (empathy_call_window_camera_toggled_cb), self);
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
  gdouble level, GtkProgressBar *bar)
{
  gdouble value;

  value = CLAMP (pow (10, level / 20), 0.0, 1.0);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (bar), value);
}

static GtkWidget *
empathy_call_window_create_audio_input (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GtkWidget *hbox, *vbox, *scale, *progress, *label;
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

  progress = gtk_progress_bar_new ();
  gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (progress),
    GTK_PROGRESS_BOTTOM_TO_TOP);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress), 0);

  g_signal_connect (priv->audio_input, "peak-level-changed",
    G_CALLBACK (empathy_call_window_audio_input_level_changed_cb), progress);

  gtk_box_pack_start (GTK_BOX (hbox), progress, FALSE, FALSE, 3);

  return hbox;
}

static void
empathy_call_window_init (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);
  GtkWidget *vbox, *top_vbox;
  GtkWidget *hbox, *h;
  GtkWidget *arrow;
  GtkWidget *page;
  GstBus *bus;
  gchar *filename;
  GtkWidget *pane;

  filename = empathy_file_lookup ("empathy-call-window.glade", "src");

  priv->glade = empathy_glade_get_file (filename, "call_window", NULL,
    "call_window_vbox", &top_vbox,
    "pane", &pane,
    "statusbar", &priv->statusbar,
    NULL);

  priv->lock = g_mutex_new ();

  gtk_widget_reparent (top_vbox, GTK_WIDGET (self));

  empathy_call_window_setup_menubar (self);
  empathy_call_window_setup_toolbar (self);

  priv->pipeline = gst_pipeline_new (NULL);

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
  gtk_paned_pack1 (GTK_PANED(pane), hbox, TRUE, FALSE);

  bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));

  gst_bus_add_watch (bus, empathy_call_window_bus_message, self);

  priv->video_output = empathy_video_widget_new (bus);
  gtk_box_pack_start (GTK_BOX (hbox), priv->video_output, TRUE, TRUE, 3);

  priv->video_tee = gst_element_factory_make ("tee", NULL);
  gst_object_ref (priv->video_tee);
  gst_object_sink (priv->video_tee);

  vbox = gtk_vbox_new (FALSE, 3);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 3);

  priv->video_preview = empathy_video_widget_new_with_size (bus, 160, 120);
  g_object_set (priv->video_preview, "sync", FALSE, "async", TRUE, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), priv->video_preview, FALSE, FALSE, 0);

  priv->video_input = empathy_video_src_new ();
  gst_object_ref (priv->video_input);
  gst_object_sink (priv->video_input);

  priv->audio_input = empathy_audio_src_new ();
  gst_object_ref (priv->audio_input);
  gst_object_sink (priv->audio_input);

  priv->audio_output = empathy_audio_sink_new ();
  gst_object_ref (priv->audio_output);
  gst_object_sink (priv->audio_output);

  g_object_unref (bus);

  priv->sidebar_button = gtk_toggle_button_new_with_mnemonic (_("_Sidebar"));
  arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
  g_signal_connect (G_OBJECT (priv->sidebar_button), "toggled",
    G_CALLBACK (empathy_call_window_sidebar_toggled_cb), self);

  gtk_button_set_image (GTK_BUTTON (priv->sidebar_button), arrow);

  h = gtk_hbox_new (FALSE, 3);
  gtk_box_pack_end (GTK_BOX (vbox), h, FALSE, FALSE, 3);
  gtk_box_pack_end (GTK_BOX (h), priv->sidebar_button, FALSE, FALSE, 3);

  priv->sidebar = empathy_sidebar_new ();
  g_signal_connect (G_OBJECT (priv->sidebar),
    "hide", G_CALLBACK (empathy_call_window_sidebar_hidden_cb),
    self);
  gtk_paned_pack2 (GTK_PANED(pane), priv->sidebar, FALSE, FALSE);

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

  g_signal_connect (G_OBJECT (self), "realize",
    G_CALLBACK (empathy_call_window_realized_cb), self);

  g_signal_connect (G_OBJECT (self), "delete-event",
    G_CALLBACK (empathy_call_window_delete_cb), self);

  empathy_call_window_status_message (self, _("Connecting..."));

  priv->timer = g_timer_new ();
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

  object_class->set_property = empathy_call_window_set_property;
  object_class->get_property = empathy_call_window_get_property;

  object_class->dispose = empathy_call_window_dispose;
  object_class->finalize = empathy_call_window_finalize;

  param_spec = g_param_spec_object ("handler",
    "handler", "The call handler",
    EMPATHY_TYPE_CALL_HANDLER,
    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
    PROP_CALL_HANDLER, param_spec);

}

void
empathy_call_window_dispose (GObject *object)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (object);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

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

  /* release any references held by the object here */
  if (G_OBJECT_CLASS (empathy_call_window_parent_class)->dispose)
    G_OBJECT_CLASS (empathy_call_window_parent_class)->dispose (object);
}

void
empathy_call_window_finalize (GObject *object)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (object);
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

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

static void
empathy_call_window_disconnected (EmpathyCallWindow *self)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (self);

  g_mutex_lock (priv->lock);

  g_timer_stop (priv->timer);

  if (priv->timer_id != 0)
    g_source_remove (priv->timer_id);
  priv->timer_id = 0;

  g_mutex_unlock (priv->lock);

  empathy_call_window_status_message (self, _("Disconnected"));

  gtk_widget_set_sensitive (priv->camera_button, FALSE);
}


static void
empathy_call_window_channel_closed_cb (TfChannel *channel, gpointer user_data)
{
  EmpathyCallWindow *self = EMPATHY_CALL_WINDOW (user_data);

  empathy_call_window_disconnected (self);
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
  str = g_strdup_printf (_("Connected -- %d:%02dm"), (int) time / 60,
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

  if (empathy_tp_call_has_dtmf (call))
    gtk_widget_set_sensitive (priv->dtmf_panel, TRUE);

  if (priv->video_input != NULL)
    gtk_widget_set_sensitive (priv->camera_button, TRUE);

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
            pad =  gst_element_get_request_pad (priv->video_tee, "src%d");
            gst_pad_link (pad, sink);
          }
        break;
      default:
        g_assert_not_reached ();
    }

}

static gboolean
empathy_gst_bin_has_child (GstBin *bin, GstElement *element)
{
  GstIterator *it;
  gboolean ret = FALSE;
  GstElement *item;

  it = gst_bin_iterate_recurse (bin);

  for (;;)
    {
      switch (gst_iterator_next (it, (gpointer *)&item))
       {
         case GST_ITERATOR_OK:
           if (item == element)
            {
              gst_object_unref (GST_OBJECT (item));
              ret = TRUE;
              goto out;
            }
           gst_object_unref (GST_OBJECT (item));
           break;
         case GST_ITERATOR_RESYNC:
           gst_iterator_resync (it);
           break;
        case GST_ITERATOR_ERROR:
           g_assert_not_reached ();
           /* fallthrough */
        case GST_ITERATOR_DONE:
           goto out;
           break;
      }
    }
    gst_iterator_free (it);

out:
  return ret;
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
          GError *error;
          gchar *debug;

          gst_message_parse_error (message, &error, &debug);

          g_message ("Element error: %s -- %s\n", error->message, debug);

          if (priv->video_input != NULL &&
              empathy_gst_bin_has_child (GST_BIN (priv->video_input),
                GST_ELEMENT (GST_MESSAGE_SRC (message))))
            {
              /* Remove the video input and continue */
              empathy_call_window_remove_video_input (self);
              gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
            }
          else
            {
              gst_element_set_state (priv->pipeline, GST_STATE_NULL);
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
empathy_call_window_realized_cb (GtkWidget *widget, EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);
  GstElement *preview;

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


  preview = empathy_video_widget_get_element (
    EMPATHY_VIDEO_WIDGET (priv->video_preview));

  gst_bin_add_many (GST_BIN (priv->pipeline), priv->video_input,
    priv->video_tee, preview, NULL);
  gst_element_link_many (priv->video_input, priv->video_tee,
    preview, NULL);

  gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
}

static gboolean
empathy_call_window_delete_cb (GtkWidget *widget, GdkEvent*event,
  EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  gst_element_set_state (priv->pipeline, GST_STATE_NULL);

  return FALSE;
}

static void
empathy_call_window_sidebar_toggled_cb (GtkToggleButton *toggle,
  EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);
  GtkWidget *arrow;
  GtkWidget *pane;
  int w,h, handle_size;

  w = GTK_WIDGET (window)->allocation.width;
  h = GTK_WIDGET (window)->allocation.height;

  pane = glade_xml_get_widget (priv->glade, "pane");
  gtk_widget_style_get (pane, "handle_size", &handle_size, NULL);

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
empathy_call_window_camera_toggled_cb (GtkToggleToolButton *toggle,
  EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);
  gboolean active;
  EmpathyTpCall *call;

  active = (gtk_toggle_tool_button_get_active (toggle));

  g_object_get (priv->handler, "tp-call", &call, NULL);

  empathy_tp_call_request_video_stream_direction (call, active);

  g_object_unref (call);
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
empathy_call_window_hangup (EmpathyCallWindow *window)
{
  EmpathyCallWindowPriv *priv = GET_PRIV (window);

  gst_element_set_state (priv->pipeline, GST_STATE_NULL);
  gtk_widget_destroy (GTK_WIDGET (window));
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
