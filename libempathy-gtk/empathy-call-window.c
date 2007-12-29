/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Elliot Fairweather
 * Copyright (C) 2007 Collabora Ltd.
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

#include "config.h"

#include <gtk/gtk.h>

#include <libempathy/empathy-debug.h>

#include "empathy-call-window.h"
#include "empathy-ui-utils.h"

#define DEBUG_DOMAIN "CallWindow"

typedef struct {
	GtkWidget     *window;
	GtkWidget     *input_volume_scale;
	GtkWidget     *output_volume_scale;
	GtkWidget     *input_mute_togglebutton;
	GtkWidget     *output_mute_togglebutton;
	GtkWidget     *preview_video_frame;
	GtkWidget     *output_video_frame;
	GtkWidget     *preview_video_socket;
	GtkWidget     *output_video_socket;
	GtkWidget     *send_video_checkbutton;

	EmpathyTpCall *call;
} EmpathyCallWindow;

static void
call_window_output_volume_changed_cb (GtkWidget         *scale,
				      EmpathyCallWindow *window)
{
	guint volume;

	volume = (guint) gtk_range_get_value (GTK_RANGE (scale));
	empathy_tp_call_set_output_volume (window->call, volume);
}


static void
call_window_output_mute_toggled_cb (GtkWidget         *button,
				    EmpathyCallWindow *window)
{
	gboolean is_muted;

	is_muted = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	empathy_tp_call_mute_output (window->call, is_muted);
}


static void
call_window_input_mute_toggled_cb (GtkWidget         *button,
				   EmpathyCallWindow *window)
{
	gboolean is_muted;

	is_muted = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	empathy_tp_call_mute_input (window->call, is_muted);
}


static void
call_window_send_video_toggled_cb (GtkWidget         *button,
				   EmpathyCallWindow *window)
{
	gboolean is_sending;

	is_sending = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	empathy_tp_call_send_video (window->call, is_sending);
}

static void
call_window_capabilities_notify_cb (EmpathyContact    *contact,
				    GParamSpec        *param,
				    EmpathyCallWindow *window)
{
	EmpathyCapabilities capabilities;

	capabilities = empathy_contact_get_capabilities (contact);
	empathy_tp_call_request_streams (window->call,
					 capabilities & EMPATHY_CAPABILITIES_AUDIO,
					 capabilities & EMPATHY_CAPABILITIES_VIDEO);
}

static void
call_window_status_notify_cb (EmpathyTpCall     *call,
			      GParamSpec        *param,
			      EmpathyCallWindow *window)
{
	guint status;

	status = empathy_tp_call_get_status (call);
	empathy_debug (DEBUG_DOMAIN, "Status changed to %d",
		       status);

	if (status == EMPATHY_TP_CALL_STATUS_RINGING) {
		if (empathy_tp_call_is_incoming (window->call)) {
			empathy_tp_call_accept (window->call);
		} else {
			EmpathyContact *contact;

			contact = empathy_tp_call_get_contact (call);
			g_signal_connect (contact, "notify::capabilities",
					  G_CALLBACK (call_window_capabilities_notify_cb),
					  window);
			call_window_capabilities_notify_cb (contact, NULL, window);
		}
	}

	if (status == EMPATHY_TP_CALL_STATUS_RUNNING) {
		empathy_tp_call_set_output_window (window->call,
			gtk_socket_get_id (GTK_SOCKET (window->output_video_socket)));
	}
}

static void
call_window_destroy_cb (GtkWidget         *widget,
			EmpathyCallWindow *window)
{
	g_object_unref (window->call);
	g_slice_free (EmpathyCallWindow, window);
}

void
empathy_call_window_show (EmpathyTpCall *call)
{
	EmpathyCallWindow *window;
	GladeXML          *glade;

	window = g_slice_new0 (EmpathyCallWindow);

	glade = empathy_glade_get_file ("empathy-call-window.glade",
					"window",
					NULL,
					"window", &window->window,
					"input_volume_scale", &window->input_volume_scale,
					"output_volume_scale", &window->output_volume_scale,
					"input_mute_togglebutton", &window->input_mute_togglebutton,
					"output_mute_togglebutton", &window->output_mute_togglebutton,
					"preview_video_frame", &window->preview_video_frame,
					"output_video_frame", &window->output_video_frame,
					"send_video_checkbutton", &window->send_video_checkbutton,
					NULL);

	empathy_glade_connect (glade,
			       window,
			       "window", "destroy", call_window_destroy_cb,
			       "input_mute_togglebutton", "toggled", call_window_input_mute_toggled_cb,
			       "output_mute_togglebutton", "toggled", call_window_output_mute_toggled_cb,
			       "output_volume_scale", "value-changed", call_window_output_volume_changed_cb,
			       "send_video_checkbutton", "toggled", call_window_send_video_toggled_cb,
			       NULL);
	g_object_unref (glade);

	/* Set output window socket */
	window->output_video_socket = gtk_socket_new ();
	gtk_widget_show (window->output_video_socket);
	gtk_container_add (GTK_CONTAINER (window->output_video_frame),
			   window->output_video_socket);

	/* Set preview window socket */
	window->preview_video_socket = gtk_socket_new ();
	gtk_widget_show (window->preview_video_socket);
	gtk_container_add (GTK_CONTAINER (window->preview_video_frame),
			   window->preview_video_socket);

	/* Setup TpCall */
	window->call = g_object_ref (call);
	empathy_tp_call_add_preview_window (window->call,
		gtk_socket_get_id (GTK_SOCKET (window->preview_video_socket)));
	g_signal_connect (window->call, "notify::status",
			  G_CALLBACK (call_window_status_notify_cb),
			  window);

	gtk_widget_show (window->window);
}

