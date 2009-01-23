/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2007 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
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
#include <glib/gi18n-lib.h>

#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mc-protocol.h>

#include <libempathy/empathy-utils.h>

#include "empathy-account-widget.h"
#include "empathy-ui-utils.h"

#define DEBUG_FLAG EMPATHY_DEBUG_ACCOUNT
#include <libempathy/empathy-debug.h>

static gboolean 
account_widget_entry_focus_cb (GtkWidget     *widget,
			       GdkEventFocus *event,
			       McAccount     *account)
{
	const gchar *str;
	const gchar *param_name;

	str = gtk_entry_get_text (GTK_ENTRY (widget));
	param_name = g_object_get_data (G_OBJECT (widget), "param_name");

	if (EMP_STR_EMPTY (str)) {
		gchar *value = NULL;

		mc_account_unset_param (account, param_name);
		mc_account_get_param_string (account, param_name, &value);
		DEBUG ("Unset %s and restore to %s", param_name, value);
		gtk_entry_set_text (GTK_ENTRY (widget), value ? value : "");
		g_free (value);
	} else {
		McProfile   *profile;
		const gchar *domain = NULL;
		gchar       *dup_str = NULL;

		profile = mc_account_get_profile (account);
		if (mc_profile_get_capabilities (profile) &
		    MC_PROFILE_CAPABILITY_SPLIT_ACCOUNT) {
			domain = mc_profile_get_default_account_domain (profile);
		}

		if (domain && !strstr (str, "@") &&
		    strcmp (param_name, "account") == 0) {
			DEBUG ("Adding @%s suffix to account", domain);
			str = dup_str = g_strconcat (str, "@", domain, NULL);
			gtk_entry_set_text (GTK_ENTRY (widget), str);
		}
		DEBUG ("Setting %s to %s", param_name,
			strstr (param_name, "password") ? "***" : str);
		mc_account_set_param_string (account, param_name, str);
		g_free (dup_str);
		g_object_unref (profile);
	}

	return FALSE;
}

static void
account_widget_int_changed_cb (GtkWidget *widget,
			       McAccount *account)
{
	const gchar *param_name;
	gint         value;

	value = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
	param_name = g_object_get_data (G_OBJECT (widget), "param_name");

	if (value == 0) {
		mc_account_unset_param (account, param_name);
		mc_account_get_param_int (account, param_name, &value);
		DEBUG ("Unset %s and restore to %d", param_name, value);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
	} else {
		DEBUG ("Setting %s to %d", param_name, value);
		mc_account_set_param_int (account, param_name, value);
	}
}

static void  
account_widget_checkbutton_toggled_cb (GtkWidget *widget,
				       McAccount *account)
{
	gboolean     value;
	gboolean     default_value;
	const gchar *param_name;

	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	param_name = g_object_get_data (G_OBJECT (widget), "param_name");

	/* FIXME: This is ugly! checkbox don't have a "not-set" value so we
	 * always unset the param and set the value if different from the
	 * default value. */
	mc_account_unset_param (account, param_name);
	mc_account_get_param_boolean (account, param_name, &default_value);

	if (default_value == value) {
		DEBUG ("Unset %s and restore to %d", param_name, default_value);
	} else {
		DEBUG ("Setting %s to %d", param_name, value);
		mc_account_set_param_boolean (account, param_name, value);
	}
}

static void
account_widget_forget_clicked_cb (GtkWidget *button,
				  GtkWidget *entry)
{
	McAccount   *account;
	const gchar *param_name;

	param_name = g_object_get_data (G_OBJECT (entry), "param_name");
	account = g_object_get_data (G_OBJECT (entry), "account");

	DEBUG ("Unset %s", param_name);
	mc_account_unset_param (account, param_name);
	gtk_entry_set_text (GTK_ENTRY (entry), "");
}

static void
account_widget_password_changed_cb (GtkWidget *entry,
				    GtkWidget *button)
{
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (entry));
	gtk_widget_set_sensitive (button, !EMP_STR_EMPTY (str));
}

static void  
account_widget_jabber_ssl_toggled_cb (GtkWidget *checkbutton_ssl,
				      GtkWidget *spinbutton_port)
{
	McAccount *account;
	gboolean   value;
	gint       port = 0;

	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton_ssl));
	account = g_object_get_data (G_OBJECT (spinbutton_port), "account");
	mc_account_get_param_int (account, "port", &port);

	if (value) {
		if (port == 5222 || port == 0) {
			port = 5223;
		}
	} else {
		if (port == 5223 || port == 0) {
			port = 5222;
		}
	}

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spinbutton_port), port);
}

static void
account_widget_setup_widget (GtkWidget   *widget,
			     McAccount   *account,
			     const gchar *param_name)
{
	g_object_set_data_full (G_OBJECT (widget), "param_name", 
				g_strdup (param_name), g_free);
	g_object_set_data_full (G_OBJECT (widget), "account", 
				g_object_ref (account), g_object_unref);

	if (GTK_IS_SPIN_BUTTON (widget)) {
		gint value = 0;

		mc_account_get_param_int (account, param_name, &value);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);

		g_signal_connect (widget, "value-changed",
				  G_CALLBACK (account_widget_int_changed_cb),
				  account);
	}
	else if (GTK_IS_ENTRY (widget)) {
		gchar *str = NULL;

		mc_account_get_param_string (account, param_name, &str);
		gtk_entry_set_text (GTK_ENTRY (widget), str ? str : "");
		g_free (str);

		if (strstr (param_name, "password")) {
			gtk_entry_set_visibility (GTK_ENTRY (widget), FALSE);
		}

		g_signal_connect (widget, "focus-out-event",
				  G_CALLBACK (account_widget_entry_focus_cb),
				  account);
	}
	else if (GTK_IS_TOGGLE_BUTTON (widget)) {
		gboolean value = FALSE;

		mc_account_get_param_boolean (account, param_name, &value);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);

		g_signal_connect (widget, "toggled",
				  G_CALLBACK (account_widget_checkbutton_toggled_cb),
				  account);
	} else {
		DEBUG ("Unknown type of widget for param %s", param_name);
	}
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
accounts_widget_generic_setup (McAccount *account,
			       GtkWidget *table_common_settings,
			       GtkWidget *table_advanced_settings)
{
	McProtocol *protocol;
	McProfile  *profile;
	GSList     *params, *l;

	profile = mc_account_get_profile (account);
	protocol = mc_profile_get_protocol (profile);

	if (!protocol) {
		/* The CM is not installed, MC shouldn't list them
		 * see SF bug #1688779
		 * FIXME: We should display something asking the user to 
		 * install the CM
		 */
		g_object_unref (profile);
		return;
	}

	params = mc_protocol_get_params (protocol);

	for (l = params; l; l = l->next) {
		McProtocolParam *param;
		GtkWidget       *table_settings;
		guint            n_rows = 0;
		GtkWidget       *widget = NULL;
		gchar           *param_name_formatted;

		param = l->data;
		if (param->flags & MC_PROTOCOL_PARAM_REQUIRED) {
			table_settings = table_common_settings;
		} else {
			table_settings = table_advanced_settings;
		}
		param_name_formatted = account_widget_generic_format_param_name (param->name);
		g_object_get (table_settings, "n-rows", &n_rows, NULL);
		gtk_table_resize (GTK_TABLE (table_settings), ++n_rows, 2);

		if (param->signature[0] == 's') {
			gchar *str;

			str = g_strdup_printf (_("%s:"), param_name_formatted);
			widget = gtk_label_new (str);
			gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
			g_free (str);

			gtk_table_attach (GTK_TABLE (table_settings),
					  widget,
					  0, 1,
					  n_rows - 1, n_rows,
					  GTK_FILL, 0,
					  0, 0);

			widget = gtk_entry_new ();
			gtk_table_attach (GTK_TABLE (table_settings),
					  widget,
					  1, 2,
					  n_rows - 1, n_rows,
					  GTK_FILL | GTK_EXPAND, 0,
					  0, 0);
		}
		/* int types: ynqiuxt. double type is 'd' */
		else if (param->signature[0] == 'y' ||
			 param->signature[0] == 'n' ||
			 param->signature[0] == 'q' ||
			 param->signature[0] == 'i' ||
			 param->signature[0] == 'u' ||
			 param->signature[0] == 'x' ||
			 param->signature[0] == 't' ||
			 param->signature[0] == 'd') {
			gchar   *str = NULL;
			gdouble  minint = 0;
			gdouble  maxint = 0;
			gdouble  step = 1;

			switch (param->signature[0]) {
			case 'y': minint = G_MININT8;  maxint = G_MAXINT8;   break;
			case 'n': minint = G_MININT16; maxint = G_MAXINT16;  break;
			case 'q': minint = 0;          maxint = G_MAXUINT16; break;
			case 'i': minint = G_MININT32; maxint = G_MAXINT32;  break;
			case 'u': minint = 0;          maxint = G_MAXUINT32; break;
			case 'x': minint = G_MININT64; maxint = G_MAXINT64;  break;
			case 't': minint = 0;          maxint = G_MAXUINT64; break;
			case 'd': minint = G_MININT32; maxint = G_MAXINT32; step = 0.1; break;
			}

			str = g_strdup_printf (_("%s:"), param_name_formatted);
			widget = gtk_label_new (str);
			gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
			g_free (str);

			gtk_table_attach (GTK_TABLE (table_settings),
					  widget,
					  0, 1,
					  n_rows - 1, n_rows,
					  GTK_FILL, 0,
					  0, 0);

			widget = gtk_spin_button_new_with_range (minint, maxint, step);
			gtk_table_attach (GTK_TABLE (table_settings),
					  widget,
					  1, 2,
					  n_rows - 1, n_rows,
					  GTK_FILL | GTK_EXPAND, 0,
					  0, 0);
		}
		else if (param->signature[0] == 'b') {
			widget = gtk_check_button_new_with_label (param_name_formatted);
			gtk_table_attach (GTK_TABLE (table_settings),
					  widget,
					  0, 2,
					  n_rows - 1, n_rows,
					  GTK_FILL | GTK_EXPAND, 0,
					  0, 0);
		} else {
			DEBUG ("Unknown signature for param %s: %s",
				param_name_formatted, param->signature);
		}

		if (widget) {
			account_widget_setup_widget (widget, account, param->name);
		}

		g_free (param_name_formatted);
	}

	g_slist_free (params);
	g_object_unref (profile);
	g_object_unref (protocol);
}

static void
account_widget_handle_params_valist (McAccount   *account,
				     GladeXML    *gui,
				     const gchar *first_widget_name,
				     va_list      args)
{
	GtkWidget   *widget;
	const gchar *widget_name;

	for (widget_name = first_widget_name; widget_name; widget_name = va_arg (args, gchar*)) {
		const gchar *param_name;

		param_name = va_arg (args, gchar*);

		widget = glade_xml_get_widget (gui, widget_name);

		if (!widget) {
			g_warning ("Glade is missing widget '%s'.", widget_name);
			continue;
		}

		account_widget_setup_widget (widget, account, param_name);
	}
}

void
empathy_account_widget_handle_params (McAccount   *account,
				      GladeXML    *gui,
				      const gchar *first_widget_name,
				      ...)
{
	va_list args;

	g_return_if_fail (MC_IS_ACCOUNT (account));

	va_start (args, first_widget_name);
	account_widget_handle_params_valist (account, gui,
					     first_widget_name,
					     args);
	va_end (args);
}

void
empathy_account_widget_add_forget_button (McAccount   *account,
					  GladeXML    *glade,
					  const gchar *button,
					  const gchar *entry)
{
	GtkWidget *button_forget;
	GtkWidget *entry_password;
	gchar     *password = NULL;
	
	button_forget = glade_xml_get_widget (glade, button);
	entry_password = glade_xml_get_widget (glade, entry);

	mc_account_get_param_string (account, "password", &password);
	gtk_widget_set_sensitive (button_forget, !EMP_STR_EMPTY (password));
	g_free (password);

	g_signal_connect (button_forget, "clicked",
			  G_CALLBACK (account_widget_forget_clicked_cb),
			  entry_password);
	g_signal_connect (entry_password, "changed",
			  G_CALLBACK (account_widget_password_changed_cb),
			  button_forget);
}

GtkWidget *
empathy_account_widget_generic_new (McAccount *account)
{
	GladeXML  *glade;
	GtkWidget *widget;
	GtkWidget *table_common_settings;
	GtkWidget *table_advanced_settings;
	gchar     *filename;

	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);

	filename = empathy_file_lookup ("empathy-account-widget-generic.glade",
					"libempathy-gtk");
	glade = empathy_glade_get_file (filename,
					"vbox_generic_settings",
					NULL,
					"vbox_generic_settings", &widget,
					"table_common_settings", &table_common_settings,
					"table_advanced_settings", &table_advanced_settings,
					NULL);
	g_free (filename);

	accounts_widget_generic_setup (account, table_common_settings, table_advanced_settings);

	g_object_unref (glade);

	gtk_widget_show_all (widget);

	return widget;
}

GtkWidget *
empathy_account_widget_salut_new (McAccount *account)
{
	GladeXML  *glade;
	GtkWidget *widget;
	gchar     *filename;

	filename = empathy_file_lookup ("empathy-account-widget-salut.glade",
					"libempathy-gtk");
	glade = empathy_glade_get_file (filename,
					"vbox_salut_settings",
					NULL,
					"vbox_salut_settings", &widget,
					NULL);
	g_free (filename);

	empathy_account_widget_handle_params (account, glade,
			"entry_published", "published-name",
			"entry_nickname", "nickname",
			"entry_first_name", "first-name",
			"entry_last_name", "last-name",
			"entry_email", "email",
			"entry_jid", "jid",
			NULL);

	g_object_unref (glade);

	gtk_widget_show (widget);

	return widget;
}

GtkWidget *
empathy_account_widget_msn_new (McAccount *account)
{
	GladeXML  *glade;
	GtkWidget *widget;
	gchar     *filename;

	filename = empathy_file_lookup ("empathy-account-widget-msn.glade",
					"libempathy-gtk");
	glade = empathy_glade_get_file (filename,
					"vbox_msn_settings",
					NULL,
					"vbox_msn_settings", &widget,
					NULL);
	g_free (filename);

	empathy_account_widget_handle_params (account, glade,
			"entry_id", "account",
			"entry_password", "password",
			"entry_server", "server",
			"spinbutton_port", "port",
			NULL);

	empathy_account_widget_add_forget_button (account, glade,
						  "button_forget",
						  "entry_password");

	g_object_unref (glade);

	gtk_widget_show (widget);

	return widget;
}

GtkWidget *
empathy_account_widget_jabber_new (McAccount *account)
{
	GladeXML  *glade;
	GtkWidget *widget;
	GtkWidget *spinbutton_port;
	GtkWidget *checkbutton_ssl;
	gchar     *filename;

	filename = empathy_file_lookup ("empathy-account-widget-jabber.glade",
					"libempathy-gtk");
	glade = empathy_glade_get_file (filename,
				        "vbox_jabber_settings",
				        NULL,
				        "vbox_jabber_settings", &widget,
				        "spinbutton_port", &spinbutton_port,
				        "checkbutton_ssl", &checkbutton_ssl,
				        NULL);
	g_free (filename);

	empathy_account_widget_handle_params (account, glade,
			"entry_id", "account",
			"entry_password", "password",
			"entry_resource", "resource",
			"entry_server", "server",
			"spinbutton_port", "port",
			"spinbutton_priority", "priority",
			"checkbutton_ssl", "old-ssl",
			"checkbutton_ignore_ssl_errors", "ignore-ssl-errors",
			"checkbutton_encryption", "require-encryption",
			NULL);

	empathy_account_widget_add_forget_button (account, glade,
						  "button_forget",
						  "entry_password");

	g_signal_connect (checkbutton_ssl, "toggled",
			  G_CALLBACK (account_widget_jabber_ssl_toggled_cb),
			  spinbutton_port);

	g_object_unref (glade);

	gtk_widget_show (widget);

	return widget;
}

GtkWidget *
empathy_account_widget_icq_new (McAccount *account)
{
	GladeXML  *glade;
	GtkWidget *widget;
	GtkWidget *spinbutton_port;
	gchar     *filename;

	filename = empathy_file_lookup ("empathy-account-widget-icq.glade",
					"libempathy-gtk");
	glade = empathy_glade_get_file (filename,
				        "vbox_icq_settings",
				        NULL,
				        "vbox_icq_settings", &widget,
				        "spinbutton_port", &spinbutton_port,
				        NULL);
	g_free (filename);

	empathy_account_widget_handle_params (account, glade,
			"entry_uin", "account",
			"entry_password", "password",
			"entry_server", "server",
			"spinbutton_port", "port",
			"entry_charset", "charset",
			NULL);

	empathy_account_widget_add_forget_button (account, glade,
						  "button_forget",
						  "entry_password");

	g_object_unref (glade);

	gtk_widget_show (widget);

	return widget;
}

GtkWidget *
empathy_account_widget_aim_new (McAccount *account)
{
	GladeXML  *glade;
	GtkWidget *widget;
	GtkWidget *spinbutton_port;
	gchar     *filename;

	filename = empathy_file_lookup ("empathy-account-widget-aim.glade",
					"libempathy-gtk");
	glade = empathy_glade_get_file (filename,
				        "vbox_aim_settings",
				        NULL,
				        "vbox_aim_settings", &widget,
				        "spinbutton_port", &spinbutton_port,
				        NULL);
	g_free (filename);

	empathy_account_widget_handle_params (account, glade,
			"entry_screenname", "account",
			"entry_password", "password",
			"entry_server", "server",
			"spinbutton_port", "port",
			NULL);

	empathy_account_widget_add_forget_button (account, glade,
						  "button_forget",
						  "entry_password");

	g_object_unref (glade);

	gtk_widget_show (widget);

	return widget;
}

GtkWidget *
empathy_account_widget_yahoo_new (McAccount *account)
{
	GladeXML  *glade;
	GtkWidget *widget;
	gchar     *filename;

	filename = empathy_file_lookup ("empathy-account-widget-yahoo.glade",
					"libempathy-gtk");
	glade = empathy_glade_get_file (filename,
					"vbox_yahoo_settings",
					NULL,
					"vbox_yahoo_settings", &widget,
					NULL);
	g_free (filename);

	empathy_account_widget_handle_params (account, glade,
			"entry_id", "account",
			"entry_password", "password",
			"entry_server", "server",
			"entry_locale", "room-list-locale",
			"entry_charset", "charset",
			"spinbutton_port", "port",
			"checkbutton_yahoojp", "yahoojp",
			"checkbutton_ignore_invites", "ignore-invites",
			NULL);

	empathy_account_widget_add_forget_button (account, glade,
						  "button_forget",
						  "entry_password");

	g_object_unref (glade);

	gtk_widget_show (widget);

	return widget;
}

GtkWidget *
empathy_account_widget_groupwise_new (McAccount *account)
{
	GladeXML  *glade;
	GtkWidget *widget;
	gchar     *filename;

	filename = empathy_file_lookup ("empathy-account-widget-groupwise.glade",
					"libempathy-gtk");
	glade = empathy_glade_get_file (filename,
					"vbox_groupwise_settings",
					NULL,
					"vbox_groupwise_settings", &widget,
					NULL);
	g_free (filename);

	empathy_account_widget_handle_params (account, glade,
			"entry_id", "account",
			"entry_password", "password",
			"entry_server", "server",
			"spinbutton_port", "port",
			NULL);

	empathy_account_widget_add_forget_button (account, glade,
						  "button_forget",
						  "entry_password");

	g_object_unref (glade);

	gtk_widget_show (widget);

	return widget;
}

