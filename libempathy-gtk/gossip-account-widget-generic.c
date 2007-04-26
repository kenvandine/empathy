/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2007 Imendio AB
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
 *          Martyn Russell <martyn@imendio.com>
 */

#include <config.h>

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mc-protocol.h>

#include <libempathy-gtk/gossip-ui-utils.h>

#include "gossip-account-widget-generic.h"

typedef struct {
	McAccount     *account;

	GtkWidget     *sw;
	GtkWidget     *table_settings;
	GtkSizeGroup  *size_group;

	guint          n_rows;
} GossipAccountWidgetGeneric;

static gboolean account_widget_generic_entry_focus_cb         (GtkWidget                  *widget,
							       GdkEventFocus              *event,
							       GossipAccountWidgetGeneric *settings);
static void     account_widget_generic_int_changed_cb         (GtkWidget                  *widget,
							       GossipAccountWidgetGeneric *settings);
static void     account_widget_generic_checkbutton_toggled_cb (GtkWidget                  *widget,
							       GossipAccountWidgetGeneric *settings);
static gchar *  account_widget_generic_format_param_name      (const gchar                *param_name);
static void     account_widget_generic_setup_foreach          (McProtocolParam            *param,
							       GossipAccountWidgetGeneric *settings);
static void     account_widget_generic_destroy_cb             (GtkWidget                  *widget,
							       GossipAccountWidgetGeneric *settings);

static gboolean 
account_widget_generic_entry_focus_cb (GtkWidget                  *widget,
				       GdkEventFocus              *event,
				       GossipAccountWidgetGeneric *settings)
{
	const gchar *str;
	const gchar *param_name;

	str = gtk_entry_get_text (GTK_ENTRY (widget));
	param_name = g_object_get_data (G_OBJECT (widget), "param_name");

	mc_account_set_param_string (settings->account, param_name, str);

	return FALSE;
}

static void
account_widget_generic_int_changed_cb (GtkWidget                  *widget,
				       GossipAccountWidgetGeneric *settings)
{
	const gchar *param_name;
	gint         value;

	value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));
	param_name = g_object_get_data (G_OBJECT (widget), "param_name");

	mc_account_set_param_int (settings->account, param_name, value);
}

static void  
account_widget_generic_checkbutton_toggled_cb (GtkWidget                  *widget,
					       GossipAccountWidgetGeneric *settings)
{
	gboolean     active;
	const gchar *param_name;

	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	param_name = g_object_get_data (G_OBJECT (widget), "param_name");

	mc_account_set_param_boolean (settings->account, param_name, active);
}

static gchar *
account_widget_generic_format_param_name (const gchar *param_name)
{
	gchar *str;
	gchar *p;

	str = g_strdup (param_name);
	
	if (str && g_ascii_isalpha (str[0])) {
		str[0] = g_ascii_toupper (str[0]);
	}
	
	while ((p = strchr (str, '-')) != NULL) {
		if (p[1] != '\0' && g_ascii_isalpha (p[1])) {
			p[0] = ' ';
			p[1] = g_ascii_toupper (p[1]);
		}

		p++;
	}
	
	return str;
}

static void
account_widget_generic_setup_foreach (McProtocolParam            *param,
				      GossipAccountWidgetGeneric *settings)
{
	GtkWidget *widget;
	gchar     *param_name_formatted;

	param_name_formatted = account_widget_generic_format_param_name (param->name);

	gtk_table_resize (GTK_TABLE (settings->table_settings),
			  ++settings->n_rows,
			  2);

	if (param->signature[0] == 's') {
		gchar *str = NULL;

		str = g_strdup_printf (_("%s:"), param_name_formatted);
		widget = gtk_label_new (str);
		g_free (str);

		gtk_size_group_add_widget (settings->size_group, widget);
		gtk_table_attach (GTK_TABLE (settings->table_settings),
				  widget,
				  0, 1,
				  settings->n_rows - 1, settings->n_rows,
				  GTK_FILL, 0,
				  0, 0);

		str = NULL;
		widget = gtk_entry_new ();
		mc_account_get_param_string (settings->account,
					     param->name,
					     &str);
		if (str) {
			gtk_entry_set_text (GTK_ENTRY (widget), str);
			g_free (str);
		}

		if (strstr (param->name, "password")) {
			gtk_entry_set_visibility (GTK_ENTRY (widget), FALSE);
		}

		g_signal_connect (widget, "focus-out-event",
				  G_CALLBACK (account_widget_generic_entry_focus_cb),
				  settings);

		gtk_table_attach (GTK_TABLE (settings->table_settings),
				  widget,
				  1, 2,
				  settings->n_rows - 1, settings->n_rows,
				  GTK_FILL | GTK_EXPAND, 0,
				  0, 0);
	}
	else if (param->signature[0] == 'q' ||
		 param->signature[0] == 'n') {
		gchar *str = NULL;
		gint   value = 0;

		str = g_strdup_printf (_("%s:"), param_name_formatted);
		widget = gtk_label_new (str);
		g_free (str);

		gtk_size_group_add_widget (settings->size_group, widget);
		gtk_table_attach (GTK_TABLE (settings->table_settings),
				  widget,
				  0, 1,
				  settings->n_rows - 1, settings->n_rows,
				  GTK_FILL, 0,
				  0, 0);

		widget = gtk_spin_button_new_with_range (0, G_MAXINT, 1);
		mc_account_get_param_int (settings->account,
					  param->name,
					  &value);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

		g_signal_connect (widget, "value-changed",
				  G_CALLBACK (account_widget_generic_int_changed_cb),
				  settings);

		gtk_table_attach (GTK_TABLE (settings->table_settings),
				  widget,
				  1, 2,
				  settings->n_rows - 1, settings->n_rows,
				  GTK_FILL | GTK_EXPAND, 0,
				  0, 0);
	}
	else if (param->signature[0] == 'b') {
		gboolean value;

		mc_account_get_param_boolean (settings->account,
					      param->name,
					      &value);

		widget = gtk_check_button_new_with_label (param_name_formatted);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

		g_signal_connect (widget, "toggled",
				  G_CALLBACK (account_widget_generic_checkbutton_toggled_cb),
				  settings);

		gtk_table_attach (GTK_TABLE (settings->table_settings),
				  widget,
				  0, 2,
				  settings->n_rows - 1, settings->n_rows,
				  GTK_FILL | GTK_EXPAND, 0,
				  0, 0);
	} else {
		g_assert_not_reached ();
	}

	g_free (param_name_formatted);

	g_object_set_data_full (G_OBJECT (widget), "param_name", 
				g_strdup (param->name), g_free);
}

static void
accounts_widget_generic_setup (GossipAccountWidgetGeneric *settings)
{
	McProtocol *protocol;
	McProfile  *profile;
	GSList     *params;

	profile = mc_account_get_profile (settings->account);
	protocol = mc_profile_get_protocol (profile);
	params = mc_protocol_get_params (protocol);

	g_slist_foreach (params,
			 (GFunc) account_widget_generic_setup_foreach,
			 settings);

	g_slist_free (params);
}

static void
account_widget_generic_destroy_cb (GtkWidget                  *widget,
				   GossipAccountWidgetGeneric *settings)
{
	g_object_unref (settings->account);
	g_object_unref (settings->size_group);

	g_free (settings);
}

GtkWidget *
gossip_account_widget_generic_new (McAccount *account,
				   GtkWidget *label_name)
{
	GossipAccountWidgetGeneric *settings;

	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (GTK_IS_WIDGET (label_name), NULL);

	settings = g_new0 (GossipAccountWidgetGeneric, 1);

	settings->account = g_object_ref (account);

	settings->table_settings = gtk_table_new (0, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (settings->table_settings), 6);
	gtk_table_set_col_spacings (GTK_TABLE (settings->table_settings), 6);
	settings->sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (settings->sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (settings->sw),
					       settings->table_settings);

	settings->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	if (label_name) {
		gtk_size_group_add_widget (settings->size_group, label_name);
	}
	
	accounts_widget_generic_setup (settings);

	g_signal_connect (settings->sw, "destroy",
			  G_CALLBACK (account_widget_generic_destroy_cb),
			  settings);

	gtk_widget_show_all (settings->sw);

	return settings->sw;
}
