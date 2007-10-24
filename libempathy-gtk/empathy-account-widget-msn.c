/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 *          Cosimo Cecchi <anarki@lilik.it>
 */

#include "config.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libmissioncontrol/mc-profile.h>

#include <libempathy/empathy-utils.h>

#include "empathy-account-widget-msn.h"
#include "empathy-ui-utils.h"

typedef struct {
	McAccount *account;

	GtkWidget *vbox_settings;
	GtkWidget *button_forget;
	GtkWidget *entry_id;
	GtkWidget *entry_password;
	GtkWidget *entry_server;
	GtkWidget *spinbutton_port;
} EmpathyAccountWidgetMSN;

static gboolean account_widget_msn_entry_focus_cb            (GtkWidget               *widget,
							      GdkEventFocus           *event,
							      EmpathyAccountWidgetMSN *settings);
static void     account_widget_msn_entry_changed_cb          (GtkWidget               *widget,
							      EmpathyAccountWidgetMSN *settings);
static void	account_widget_msn_value_changed_cb 	     (GtkWidget		      *spinbutton,
							      EmpathyAccountWidgetMSN *settings);
static void     account_widget_msn_button_forget_clicked_cb  (GtkWidget               *button,
							      EmpathyAccountWidgetMSN *settings);
static void     account_widget_msn_destroy_cb                (GtkWidget               *widget,
							      EmpathyAccountWidgetMSN *settings);
static void     account_widget_msn_setup                     (EmpathyAccountWidgetMSN *settings);

static gboolean
account_widget_msn_entry_focus_cb (GtkWidget               *widget,
				   GdkEventFocus           *event,
				   EmpathyAccountWidgetMSN *settings)
{
	const gchar *param;
	const gchar *str;
	
	if (widget == settings->entry_password) {
		param = "password";
	}
	else if (widget == settings->entry_server) {
		param = "server";
	}
	else if (widget == settings->entry_id) {
		param = "account";
	} else {
		return FALSE;
	}
	
	str = gtk_entry_get_text (GTK_ENTRY (widget));
	
	if (G_STR_EMPTY (str)) {
		gchar *value = NULL;

		mc_account_get_param_string (settings->account, param, &value);
		gtk_entry_set_text (GTK_ENTRY (widget), value ? value : "");
		g_free (value);
	} else {
		mc_account_set_param_string (settings->account, param, str);
	}

	return FALSE;
}

static void
account_widget_msn_entry_changed_cb (GtkWidget               *widget,
				     EmpathyAccountWidgetMSN *settings)
{
	if (widget == settings->entry_password) {
		const gchar *str;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		gtk_widget_set_sensitive (settings->button_forget, !G_STR_EMPTY (str));
	}
}

static void
account_widget_msn_value_changed_cb (GtkWidget			*spinbutton,
				     EmpathyAccountWidgetMSN	*settings)
{
	if (spinbutton == settings->spinbutton_port) {
		gdouble value;

		value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spinbutton));
		mc_account_set_param_int (settings->account, "port", (gint) value);
	}
}

static void
account_widget_msn_button_forget_clicked_cb (GtkWidget               *button,
					     EmpathyAccountWidgetMSN *settings)
{
	mc_account_set_param_string (settings->account, "password", "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_password), "");
}

static void
account_widget_msn_destroy_cb (GtkWidget               *widget,
			       EmpathyAccountWidgetMSN *settings)
{
	g_object_unref (settings->account);
	g_free (settings);
}

static void
account_widget_msn_setup (EmpathyAccountWidgetMSN *settings)
{
	guint  port = 0;
	gchar *id = NULL;
	gchar *server = NULL;
	gchar *password = NULL;

	mc_account_get_param_int (settings->account, "port", &port);
	mc_account_get_param_string (settings->account, "account", &id);
	mc_account_get_param_string (settings->account, "server", &server);
	mc_account_get_param_string (settings->account, "password", &password);

	gtk_entry_set_text (GTK_ENTRY (settings->entry_id), id ? id : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_password), password ? password : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_server), server ? server : "");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (settings->spinbutton_port), port);

	gtk_widget_set_sensitive (settings->button_forget, !G_STR_EMPTY (password));

	g_free (id);
	g_free (server);
	g_free (password);
}

GtkWidget *
empathy_account_widget_msn_new (McAccount *account)
{
	EmpathyAccountWidgetMSN *settings;
	GladeXML                *glade;

	settings = g_new0 (EmpathyAccountWidgetMSN, 1);
	settings->account = g_object_ref (account);

	glade = empathy_glade_get_file ("empathy-account-widget-msn.glade",
				       "vbox_msn_settings",
				       NULL,
				       "vbox_msn_settings", &settings->vbox_settings,
				       "button_forget", &settings->button_forget,
				       "entry_id", &settings->entry_id,
				       "entry_password", &settings->entry_password,
				       "entry_server", &settings->entry_server,
				       "spinbutton_port", &settings->spinbutton_port,
				       NULL);

	account_widget_msn_setup (settings);

	empathy_glade_connect (glade, 
			      settings,
			      "vbox_msn_settings", "destroy", account_widget_msn_destroy_cb,
			      "button_forget", "clicked", account_widget_msn_button_forget_clicked_cb,
			      "entry_id", "changed", account_widget_msn_entry_changed_cb,
			      "entry_password", "changed", account_widget_msn_entry_changed_cb,
			      "entry_server", "changed", account_widget_msn_entry_changed_cb,
			      "entry_id", "focus-out-event", account_widget_msn_entry_focus_cb,
			      "entry_password", "focus-out-event", account_widget_msn_entry_focus_cb,
			      "entry_server", "focus-out-event", account_widget_msn_entry_focus_cb,
			      "spinbutton_port", "value-changed", account_widget_msn_value_changed_cb,
			      NULL);

	g_object_unref (glade);

	gtk_widget_show (settings->vbox_settings);

	return settings->vbox_settings;
}
