/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Cosimo Cecchi <anarki@lilik.it>
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
 * Authors: Cosimo Cecchi <anarki@lilik.it>
 */
 
#include "config.h"

#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libmissioncontrol/mc-profile.h>

#include <libempathy/empathy-utils.h>

#include "empathy-account-widget-salut.h"
#include "empathy-ui-utils.h"

typedef struct {
	McAccount *account;
	
	GtkWidget *vbox_settings;
	GtkWidget *entry_nickname;
	GtkWidget *entry_published;
	GtkWidget *entry_first_name;
	GtkWidget *entry_last_name;
	GtkWidget *entry_email;
	GtkWidget *entry_jid;
} EmpathyAccountWidgetSalut;

static gboolean	account_widget_salut_entry_focus_cb (GtkWidget                 *widget,
						     GdkEventFocus             *event,
						     EmpathyAccountWidgetSalut *settings);
static void	account_widget_salut_destroy_cb	    (GtkWidget                 *widget,
						     EmpathyAccountWidgetSalut *settings);
static void	account_widget_salut_setup          (EmpathyAccountWidgetSalut *settings);



static gboolean
account_widget_salut_entry_focus_cb (GtkWidget               *widget,
				     GdkEventFocus           *event,
				     EmpathyAccountWidgetSalut *settings)
{
	const gchar *param;
	const gchar *str;
	
	if (widget == settings->entry_nickname) {
		param = "nickname";
	}
	else if (widget == settings->entry_published) {
		param = "published-name";
	}
	else if (widget == settings->entry_first_name) {
		param = "first-name";
	}
	else if (widget == settings->entry_last_name) {
		param = "last-name";
	}
	else if (widget == settings->entry_email) {
		param = "email";
	}
	else if (widget == settings->entry_jid) {
		param = "jid";
	}
	else {
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
account_widget_salut_destroy_cb (GtkWidget               *widget,
				 EmpathyAccountWidgetSalut *settings)
{
	g_object_unref (settings->account);
	g_free (settings);
}

static void
account_widget_salut_setup (EmpathyAccountWidgetSalut *settings)
{
	gchar *nickname = NULL;
	gchar *published_name = NULL;
	gchar *first_name = NULL;
	gchar *last_name = NULL;
	gchar *email = NULL;
	gchar *jid = NULL;

	mc_account_get_param_string (settings->account, "nickname", &nickname);
	mc_account_get_param_string (settings->account, "published-name", &published_name);
	mc_account_get_param_string (settings->account, "first-name", &first_name);
	mc_account_get_param_string (settings->account, "last-name", &last_name);
	mc_account_get_param_string (settings->account, "email", &email);
	mc_account_get_param_string (settings->account, "jid", &jid);
	
	gtk_entry_set_text (GTK_ENTRY (settings->entry_nickname), nickname ? nickname : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_published), published_name ? published_name : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_first_name), first_name ? first_name : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_last_name), last_name ? last_name : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_email), email ? email : "");
	gtk_entry_set_text (GTK_ENTRY (settings->entry_jid), jid ? jid : "");

	g_free (nickname);
	g_free (published_name);
	g_free (first_name);
	g_free (last_name);
	g_free (email);
	g_free (jid);
}


GtkWidget *
empathy_account_widget_salut_new (McAccount *account)
{
	EmpathyAccountWidgetSalut *settings;
	GladeXML		  *glade;

	settings = g_new0 (EmpathyAccountWidgetSalut, 1);
	settings->account = g_object_ref (account);

	glade = empathy_glade_get_file ("empathy-account-widget-salut.glade",
					"vbox_salut_settings",
					NULL,
					"vbox_salut_settings", &settings->vbox_settings,
					"entry_published", &settings->entry_published,
					"entry_nickname", &settings->entry_nickname,
					"entry_first_name", &settings->entry_first_name,
					"entry_last_name", &settings->entry_last_name,
					"entry_email", &settings->entry_email,
					"entry_jid", &settings->entry_jid,
					NULL);

	account_widget_salut_setup (settings);

	empathy_glade_connect (glade,
			       settings,
			       "vbox_salut_settings", "destroy", account_widget_salut_destroy_cb,
			       "entry_nickname", "focus-out-event", account_widget_salut_entry_focus_cb,
			       "entry_published", "focus-out-event", account_widget_salut_entry_focus_cb,
			       "entry_first_name", "focus-out-event", account_widget_salut_entry_focus_cb,
			       "entry_last_name", "focus-out-event", account_widget_salut_entry_focus_cb,
			       "entry_email", "focus-out-event", account_widget_salut_entry_focus_cb,
			       "entry_jid", "focus-out-event", account_widget_salut_entry_focus_cb,
			      NULL);

	g_object_unref (glade);

	gtk_widget_show (settings->vbox_settings);

	return settings->vbox_settings;
}
