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

#include <gtk/gtk.h>

#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-tp-call.h>
#include <libempathy/empathy-chandler.h>
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-utils.h>

#include <libempathy-gtk/empathy-call-window.h>

#define DEBUG_DOMAIN "CallChandler"

static guint nb_calls = 0;

static void
weak_notify (gpointer data,
             GObject *where_the_object_was)
{
  nb_calls--;
  if (nb_calls == 0)
    {
      empathy_debug (DEBUG_DOMAIN, "No more calls, leaving...");
      gtk_main_quit ();
    }
}

static void
new_channel_cb (EmpathyChandler *chandler,
                TpConn *connection,
                TpChan *channel,
                MissionControl *mc)
{
  EmpathyTpCall *call;

  call = empathy_tp_call_new (connection, channel);
  empathy_call_window_new (call);
  g_object_unref (call);

  nb_calls++;
  g_object_weak_ref (G_OBJECT (call), weak_notify, NULL);
}

int
main (int argc, char *argv[])
{
  MissionControl *mc;
  EmpathyChandler *chandler;

  gtk_init (&argc, &argv);

  mc = empathy_mission_control_new ();

  chandler = empathy_chandler_new ("org.gnome.Empathy.CallChandler",
      "/org/gnome/Empathy/CallChandler");
  g_signal_connect (chandler, "new-channel",
      G_CALLBACK (new_channel_cb), mc);

  empathy_debug (DEBUG_DOMAIN, "Ready to handle new streamed media channels");

  gtk_main ();

  g_object_unref (chandler);
  g_object_unref (mc);

  return EXIT_SUCCESS;
}
