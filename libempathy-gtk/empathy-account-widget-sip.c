/*
 * Copyright (C) 2007-2008 Guillaume Desmottes
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
 * Authors: Guillaume Desmottes <gdesmott@gnome.org>
 *          Frederic Peters <fpeters@0d.be>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <libmissioncontrol/mc-protocol.h>

#include <libempathy/empathy-utils.h>

#include "empathy-account-widget.h"
#include "empathy-account-widget-sip.h"
#include "empathy-ui-utils.h"

typedef struct {
  EmpathyAccount *account;

  GtkWidget *vbox_settings;

  GtkWidget *entry_stun_server;
  GtkWidget *spinbutton_stun_part;
  GtkWidget *checkbutton_discover_stun;
} EmpathyAccountWidgetSip;

static void
account_widget_sip_destroy_cb (GtkWidget *widget,
                               EmpathyAccountWidgetSip *settings)
{
  g_object_unref (settings->account);
  g_slice_free (EmpathyAccountWidgetSip, settings);
}

static void
account_widget_sip_discover_stun_toggled_cb (
    GtkWidget *checkbox,
    EmpathyAccountWidgetSip *settings)
{
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox));
  gtk_widget_set_sensitive (settings->entry_stun_server, !active);
  gtk_widget_set_sensitive (settings->spinbutton_stun_part, !active);
}

/**
 * empathy_account_widget_sip_new:
 * @account: the #EmpathyAccount to configure
 *
 * Creates a new SIP account widget to configure a given #EmpathyAccount
 *
 * Returns: The toplevel container of the configuration widget
 */
GtkWidget *
empathy_account_widget_sip_new (EmpathyAccount *account)
{
  EmpathyAccountWidgetSip *settings;
  GtkBuilder *gui;
  gchar *filename;

  settings = g_slice_new0 (EmpathyAccountWidgetSip);
  settings->account = g_object_ref (account);

  filename = empathy_file_lookup ("empathy-account-widget-sip.ui",
      "libempathy-gtk");
  gui = empathy_builder_get_file (filename,
      "vbox_sip_settings", &settings->vbox_settings,
      "entry_stun-server", &settings->entry_stun_server,
      "spinbutton_stun-port", &settings->spinbutton_stun_part,
      "checkbutton_discover-stun", &settings->checkbutton_discover_stun,
      NULL);
  g_free (filename);

  empathy_account_widget_handle_params (account, gui,
      "entry_userid", "account",
      "entry_password", "password",
      "checkbutton_discover-stun", "discover-stun",
      "entry_stun-server", "stun-server",
      "spinbutton_stun-port", "stun-port",
      NULL);

  empathy_account_widget_add_forget_button (account, gui,
                                            "button_forget",
                                            "entry_password");

  account_widget_sip_discover_stun_toggled_cb (settings->checkbutton_discover_stun,
                                               settings);

  empathy_builder_connect (gui, settings,
      "vbox_sip_settings", "destroy", account_widget_sip_destroy_cb,
      "checkbutton_discover-stun", "toggled", account_widget_sip_discover_stun_toggled_cb,
      NULL);

  empathy_account_widget_set_default_focus (gui, "entry_userid");

  return empathy_builder_unref_and_keep_widget (gui, settings->vbox_settings);
}
