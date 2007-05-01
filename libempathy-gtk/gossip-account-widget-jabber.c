/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libempathy/gossip-utils.h>

#include "gossip-account-widget-jabber.h"
#include "gossip-ui-utils.h"

#define PORT_WITHOUT_SSL 5222
#define PORT_WITH_SSL 5223

typedef struct {
	McAccount *account;

	GtkWidget *vbox_settings;
	GtkWidget *button_forget;
	GtkWidget *entry_id;
	GtkWidget *entry_password;
	GtkWidget *entry_resource;
	GtkWidget *entry_server;
	GtkWidget *spinbutton_port;
	GtkWidget *checkbutton_ssl;
} GossipAccountWidgetJabber;

static gboolean account_widget_jabber_entry_focus_cb           (GtkWidget                 *widget,
								GdkEventFocus             *event,
								GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_entry_changed_cb         (GtkWidget                 *widget,
								GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_checkbutton_toggled_cb   (GtkWidget                 *widget,
								GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_value_changed_cb         (GtkWidget                 *spinbutton,
								GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_button_forget_clicked_cb (GtkWidget                 *button,
								GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_destroy_cb               (GtkWidget                 *widget,
								GossipAccountWidgetJabber *settings);
static void     account_widget_jabber_setup                    (GossipAccountWidgetJabber *settings);

static gboolean
account_widget_jabber_entry_focus_cb (GtkWidget                 *widget,
				      GdkEventFocus             *event,
				      GossipAccountWidgetJabber *settings)
{
	const gchar *param;
	const gchar *str;

	if (widget == settings->entry_password) {
		param = "password";
	}
	else if (widget == settings->entry_resource) {
		param = "resource";
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
		gchar *value;

		mc_account_get_param_string (settings->account, param, &value);
		gtk_entry_set_text (GTK_ENTRY (widget), value ? value : "");
		g_free (value);
	} else {
		mc_account_set_param_string (settings->account, param, str);
	}

	return FALSE;
}

static void
account_widget_jabber_entry_changed_cb (GtkWidget                 *widget,
					GossipAccountWidgetJabber *settings)
{
	if (widget == settings->entry_password) {
		const gchar *str;

		str = gtk_entry_get_text (GTK_ENTRY (widget));
		gtk_widget_set_sensitive (settings->button_forget, !G_STR_EMPTY (str));
	}
}

static void  
account_widget_jabber_checkbutton_toggled_cb (GtkWidget                 *widget,
					      GossipAccountWidgetJabber *settings)
{
	if (widget == settings->checkbutton_ssl) {
		gint     port;
		gboolean old_ssl;

		mc_account_get_param_int (settings->account, "port", &port);
		old_ssl = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

		if (old_ssl) {
			if (port == PORT_WITHOUT_SSL) {
				port = PORT_WITH_SSL;
			}
		} else {
			if (port == PORT_WITH_SSL) {
				port = PORT_WITHOUT_SSL;
			}
		}
		
		mc_account_set_param_int (settings->account, "port", port);
		mc_account_set_param_boolean (settings->account, "old-ssl", old_ssl);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (settings->spinbutton_port), port);
	}
}

static void
account_widget_jabber_value_changed_cb (GtkWidget                 *spinbutton,
					GossipAccountWidgetJabber *settings)
{
	if (spinbutton == settings->spinbutton_port) {
		gdouble value;

		value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spinbutton));
		mc_account_set_param_int (settings->account, "port", (gint) value);
	}
}

static void
account_widget_jabber_button_forget_clicked_cb (GtkWidget                 *button,
						GossipAccountWidgetJabber *settings)
{
	mc_account_set_param_string (settings->account, "password", "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_password), "");
}

static void
account_widget_jabber_destroy_cb (GtkWidget                 *widget,
				  GossipAccountWidgetJabber *settings)
{
	g_object_unref (settings->account);
	g_free (settings);
}

static void
account_widget_jabber_setup (GossipAccountWidgetJabber *settings)
{
	gint      port = 0;
	gchar    *id = NULL;
	gchar    *resource = NULL;
	gchar    *server = NULL;
	gchar    *password = NULL;
	gboolean  old_ssl = FALSE;

	mc_account_get_param_int (settings->account, "port", &port);
	mc_account_get_param_string (settings->account, "account", &id);
	mc_account_get_param_string (settings->account, "resource", &resource);
	mc_account_get_param_string (settings->account, "server", &server);
	mc_account_get_param_string (settings->account, "password", &password);
	mc_account_get_param_boolean (settings->account, "old-ssl", &old_ssl);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (settings->checkbutton_ssl), old_ssl);
	gtk_entry_set_text (GTK_ENTRY (settings->entry_id), id ? id : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_password), password ? password : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_resource), resource ? resource : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_server), server ? server : "");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (settings->spinbutton_port), port);

	gtk_widget_set_sensitive (settings->button_forget, !G_STR_EMPTY (password));

	g_free (id);
	g_free (resource);
	g_free (server);
	g_free (password);
}

GtkWidget *
gossip_account_widget_jabber_new (McAccount *account)
{
	GossipAccountWidgetJabber *settings;
	GladeXML                  *glade;
	GtkSizeGroup              *size_group;
	GtkWidget                 *label_id, *label_password;
	GtkWidget                 *label_server, *label_resource, *label_port; 

	settings = g_new0 (GossipAccountWidgetJabber, 1);
	settings->account = g_object_ref (account);

	glade = gossip_glade_get_file ("gossip-account-widget-jabber.glade",
				       "vbox_jabber_settings",
				       NULL,
				       "vbox_jabber_settings", &settings->vbox_settings,
				       "button_forget", &settings->button_forget,
				       "label_id", &label_id,
				       "label_password", &label_password,
				       "label_resource", &label_resource,
				       "label_server", &label_server,
				       "label_port", &label_port,
				       "entry_id", &settings->entry_id,
				       "entry_password", &settings->entry_password,
				       "entry_resource", &settings->entry_resource,
				       "entry_server", &settings->entry_server,
				       "spinbutton_port", &settings->spinbutton_port,
				       "checkbutton_ssl", &settings->checkbutton_ssl,
				       NULL);

	account_widget_jabber_setup (settings);

	gossip_glade_connect (glade, 
			      settings,
			      "vbox_jabber_settings", "destroy", account_widget_jabber_destroy_cb,
			      "button_forget", "clicked", account_widget_jabber_button_forget_clicked_cb,
			      "entry_password", "changed", account_widget_jabber_entry_changed_cb,
			      "spinbutton_port", "value-changed", account_widget_jabber_value_changed_cb,
			      "entry_id", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_password", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_resource", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "entry_server", "focus-out-event", account_widget_jabber_entry_focus_cb,
			      "checkbutton_ssl", "toggled", account_widget_jabber_checkbutton_toggled_cb,
			      NULL);

	g_object_unref (glade);

	/* Set up remaining widgets */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_size_group_add_widget (size_group, label_id);
	gtk_size_group_add_widget (size_group, label_password);
	gtk_size_group_add_widget (size_group, label_resource);
	gtk_size_group_add_widget (size_group, label_server);
	gtk_size_group_add_widget (size_group, label_port);

	g_object_unref (size_group);

	return settings->vbox_settings;
}

