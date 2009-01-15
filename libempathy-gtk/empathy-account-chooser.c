/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
 * Authors: Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libmissioncontrol/mission-control.h>

#include <libempathy/empathy-account-manager.h>
#include <libempathy/empathy-utils.h>

#include "empathy-ui-utils.h"
#include "empathy-account-chooser.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyAccountChooser)
typedef struct {
	EmpathyAccountManager          *manager;
	gboolean                        set_active_item;
	gboolean                        has_all_option;
	EmpathyAccountChooserFilterFunc filter;
	gpointer                        filter_data;
} EmpathyAccountChooserPriv;

typedef struct {
	EmpathyAccountChooser *chooser;
	McAccount             *account;
	gboolean               set;
} SetAccountData;

enum {
	COL_ACCOUNT_IMAGE,
	COL_ACCOUNT_TEXT,
	COL_ACCOUNT_ENABLED, /* Usually tied to connected state */
	COL_ACCOUNT_POINTER,
	COL_ACCOUNT_COUNT
};

static void     account_chooser_finalize               (GObject                  *object);
static void     account_chooser_get_property           (GObject                  *object,
							guint                     param_id,
							GValue                   *value,
							GParamSpec               *pspec);
static void     account_chooser_set_property           (GObject                  *object,
							guint                     param_id,
							const GValue             *value,
							GParamSpec               *pspec);
static void     account_chooser_setup                  (EmpathyAccountChooser    *chooser);
static void     account_chooser_account_created_cb     (EmpathyAccountManager    *manager,
							McAccount                *account,
							EmpathyAccountChooser    *chooser);
static void     account_chooser_account_add_foreach    (McAccount                *account,
							EmpathyAccountChooser    *chooser);
static void     account_chooser_account_deleted_cb     (EmpathyAccountManager    *manager,
							McAccount                *account,
							EmpathyAccountChooser    *chooser);
static void     account_chooser_account_remove_foreach (McAccount                *account,
							EmpathyAccountChooser    *chooser);
static void     account_chooser_update_iter            (EmpathyAccountChooser    *chooser,
							GtkTreeIter              *iter);
static void     account_chooser_connection_changed_cb  (EmpathyAccountManager    *manager,
							McAccount                *account,
							TpConnectionStatusReason  reason,
							TpConnectionStatus        new_status,
							TpConnectionStatus        old_status,
							EmpathyAccountChooser    *chooser);
static gboolean account_chooser_separator_func         (GtkTreeModel             *model,
							GtkTreeIter              *iter,
							EmpathyAccountChooser    *chooser);
static gboolean account_chooser_set_account_foreach    (GtkTreeModel             *model,
							GtkTreePath              *path,
							GtkTreeIter              *iter,
							SetAccountData           *data);

enum {
	PROP_0,
	PROP_HAS_ALL_OPTION,
};

G_DEFINE_TYPE (EmpathyAccountChooser, empathy_account_chooser, GTK_TYPE_COMBO_BOX);

static void
empathy_account_chooser_class_init (EmpathyAccountChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = account_chooser_finalize;
	object_class->get_property = account_chooser_get_property;
	object_class->set_property = account_chooser_set_property;

	g_object_class_install_property (object_class,
					 PROP_HAS_ALL_OPTION,
					 g_param_spec_boolean ("has-all-option",
							       "Has All Option",
							       "Have a separate option in the list to mean ALL accounts",
							       FALSE,
							       G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EmpathyAccountChooserPriv));
}

static void
empathy_account_chooser_init (EmpathyAccountChooser *chooser)
{
	EmpathyAccountChooserPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (chooser,
		EMPATHY_TYPE_ACCOUNT_CHOOSER, EmpathyAccountChooserPriv);

	chooser->priv = priv;
	priv->set_active_item = FALSE;
	priv->filter = NULL;
	priv->filter_data = NULL;

	priv->manager = empathy_account_manager_dup_singleton ();

	g_signal_connect (priv->manager, "account-created",
			  G_CALLBACK (account_chooser_account_created_cb),
			  chooser);
	g_signal_connect (priv->manager, "account-deleted",
			  G_CALLBACK (account_chooser_account_deleted_cb),
			  chooser);
	g_signal_connect (priv->manager, "account-connection-changed",
			  G_CALLBACK (account_chooser_connection_changed_cb),
			  chooser);

	account_chooser_setup (EMPATHY_ACCOUNT_CHOOSER (chooser));
}

static void
account_chooser_finalize (GObject *object)
{
	EmpathyAccountChooserPriv *priv = GET_PRIV (object);

	g_signal_handlers_disconnect_by_func (priv->manager,
					      account_chooser_connection_changed_cb,
					      object);
	g_signal_handlers_disconnect_by_func (priv->manager,
					      account_chooser_account_created_cb,
					      object);
	g_signal_handlers_disconnect_by_func (priv->manager,
					      account_chooser_account_deleted_cb,
					      object);
	g_object_unref (priv->manager);

	G_OBJECT_CLASS (empathy_account_chooser_parent_class)->finalize (object);
}

static void
account_chooser_get_property (GObject    *object,
			      guint       param_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
	EmpathyAccountChooserPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_HAS_ALL_OPTION:
		g_value_set_boolean (value, priv->has_all_option);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

static void
account_chooser_set_property (GObject      *object,
			      guint         param_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
	EmpathyAccountChooserPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_HAS_ALL_OPTION:
		empathy_account_chooser_set_has_all_option (EMPATHY_ACCOUNT_CHOOSER (object),
							   g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	};
}

GtkWidget *
empathy_account_chooser_new (void)
{
	GtkWidget                *chooser;

	chooser = g_object_new (EMPATHY_TYPE_ACCOUNT_CHOOSER, NULL);

	return chooser;
}

McAccount *
empathy_account_chooser_get_account (EmpathyAccountChooser *chooser)
{
	EmpathyAccountChooserPriv *priv;
	McAccount                *account;
	GtkTreeModel             *model;
	GtkTreeIter               iter;

	g_return_val_if_fail (EMPATHY_IS_ACCOUNT_CHOOSER (chooser), NULL);

	priv = GET_PRIV (chooser);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (chooser));
	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (chooser), &iter)) {
		return NULL;
	}

	gtk_tree_model_get (model, &iter, COL_ACCOUNT_POINTER, &account, -1);

	return account;
}

gboolean
empathy_account_chooser_set_account (EmpathyAccountChooser *chooser,
				     McAccount             *account)
{
	GtkComboBox    *combobox;
	GtkTreeModel   *model;
	GtkTreeIter     iter;
	SetAccountData  data;

	g_return_val_if_fail (EMPATHY_IS_ACCOUNT_CHOOSER (chooser), FALSE);

	combobox = GTK_COMBO_BOX (chooser);
	model = gtk_combo_box_get_model (combobox);
	gtk_combo_box_get_active_iter (combobox, &iter);

	data.chooser = chooser;
	data.account = account;

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) account_chooser_set_account_foreach,
				&data);

	return data.set;
}

gboolean
empathy_account_chooser_get_has_all_option (EmpathyAccountChooser *chooser)
{
	EmpathyAccountChooserPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_ACCOUNT_CHOOSER (chooser), FALSE);

	priv = GET_PRIV (chooser);
	
	return priv->has_all_option;
}

void
empathy_account_chooser_set_has_all_option (EmpathyAccountChooser *chooser,
					   gboolean              has_all_option)
{
	EmpathyAccountChooserPriv *priv;
	GtkComboBox              *combobox;
	GtkListStore             *store;
	GtkTreeModel             *model;
	GtkTreeIter               iter;

	g_return_if_fail (EMPATHY_IS_ACCOUNT_CHOOSER (chooser));

	priv = GET_PRIV (chooser);

	if (priv->has_all_option == has_all_option) {
		return;
	}

	combobox = GTK_COMBO_BOX (chooser);
	model = gtk_combo_box_get_model (combobox);
	store = GTK_LIST_STORE (model);

	priv->has_all_option = has_all_option;

	/*
	 * The first 2 options are the ALL and separator
	 */

	if (has_all_option) {
		gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (chooser), 
						      (GtkTreeViewRowSeparatorFunc)
						      account_chooser_separator_func,
						      chooser, 
						      NULL);

		gtk_list_store_prepend (store, &iter);
		gtk_list_store_set (store, &iter, 
				    COL_ACCOUNT_TEXT, NULL,
				    COL_ACCOUNT_ENABLED, TRUE,
				    COL_ACCOUNT_POINTER, NULL,
				    -1);

		gtk_list_store_prepend (store, &iter);
		gtk_list_store_set (store, &iter, 
				    COL_ACCOUNT_TEXT, _("All"), 
				    COL_ACCOUNT_ENABLED, TRUE,
				    COL_ACCOUNT_POINTER, NULL,
				    -1);
	} else {
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			if (gtk_list_store_remove (GTK_LIST_STORE (model), &iter)) {
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
			}
		}

		gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (chooser), 
						      (GtkTreeViewRowSeparatorFunc)
						      NULL,
						      NULL, 
						      NULL);
	}

	g_object_notify (G_OBJECT (chooser), "has-all-option");
}

static void
account_chooser_setup (EmpathyAccountChooser *chooser)
{
	EmpathyAccountChooserPriv *priv;
	GList                    *accounts;
	GtkListStore             *store;
	GtkCellRenderer          *renderer;
	GtkComboBox              *combobox;

	priv = GET_PRIV (chooser);

	/* Set up combo box with new store */
	combobox = GTK_COMBO_BOX (chooser);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combobox));

	store = gtk_list_store_new (COL_ACCOUNT_COUNT,
				    G_TYPE_STRING,    /* Image */
				    G_TYPE_STRING,    /* Name */
				    G_TYPE_BOOLEAN,   /* Enabled */
				    MC_TYPE_ACCOUNT);

	gtk_combo_box_set_model (combobox, GTK_TREE_MODEL (store));

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
					"icon-name", COL_ACCOUNT_IMAGE,
					"sensitive", COL_ACCOUNT_ENABLED,
					NULL);
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_BUTTON, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
					"text", COL_ACCOUNT_TEXT,
					"sensitive", COL_ACCOUNT_ENABLED,
					NULL);

	/* Populate accounts */
	accounts = mc_accounts_list ();
	g_list_foreach (accounts,
			(GFunc) account_chooser_account_add_foreach,
			chooser);

	mc_accounts_list_free (accounts);
	g_object_unref (store);
}

static void
account_chooser_account_created_cb (EmpathyAccountManager *manager,
				    McAccount             *account,
				    EmpathyAccountChooser *chooser)
{
	account_chooser_account_add_foreach (account, chooser);
}

static void
account_chooser_account_add_foreach (McAccount             *account,
				     EmpathyAccountChooser *chooser)
{
	GtkListStore *store;
	GtkComboBox  *combobox;
	GtkTreeIter   iter;
	gint          position;

	combobox = GTK_COMBO_BOX (chooser);
	store = GTK_LIST_STORE (gtk_combo_box_get_model (combobox));

	position = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL);
	gtk_list_store_insert_with_values (store, &iter, position,
					   COL_ACCOUNT_POINTER, account,
					   -1);
	account_chooser_update_iter (chooser, &iter);
}

static void
account_chooser_account_deleted_cb (EmpathyAccountManager *manager,
				    McAccount             *account,
				    EmpathyAccountChooser *chooser)
{
	account_chooser_account_remove_foreach (account, chooser);
}

typedef struct {
	McAccount   *account;
	GtkTreeIter *iter;
	gboolean     found;
} FindAccountData;

static gboolean
account_chooser_find_account_foreach (GtkTreeModel *model,
				      GtkTreePath  *path,
				      GtkTreeIter  *iter,
				      gpointer      user_data)
{
	FindAccountData *data = user_data;
	McAccount       *account;

	gtk_tree_model_get (model, iter, COL_ACCOUNT_POINTER, &account, -1);

	if (empathy_account_equal (account, data->account)) {
		data->found = TRUE;
		*(data->iter) = *iter;
		g_object_unref (account);

		return TRUE;
	}

	g_object_unref (account);

	return FALSE;
}

static gboolean
account_chooser_find_account (EmpathyAccountChooser *chooser,
			      McAccount             *account,
			      GtkTreeIter           *iter)
{
	GtkListStore    *store;
	GtkComboBox     *combobox;
	FindAccountData  data;

	combobox = GTK_COMBO_BOX (chooser);
	store = GTK_LIST_STORE (gtk_combo_box_get_model (combobox));

	data.account = account;
	data.iter = iter;
	gtk_tree_model_foreach (GTK_TREE_MODEL (store),
				account_chooser_find_account_foreach,
				&data);

	return data.found;
}

static void
account_chooser_account_remove_foreach (McAccount            *account,
					EmpathyAccountChooser *chooser)
{
	GtkListStore *store;
	GtkComboBox  *combobox;
	GtkTreeIter   iter;

	combobox = GTK_COMBO_BOX (chooser);
	store = GTK_LIST_STORE (gtk_combo_box_get_model (combobox));

	if (account_chooser_find_account (chooser, account, &iter)) {
		gtk_list_store_remove (store, &iter);
	}
}

static void
account_chooser_update_iter (EmpathyAccountChooser *chooser,
			     GtkTreeIter           *iter)
{
	EmpathyAccountChooserPriv *priv;
	GtkListStore              *store;
	GtkComboBox               *combobox;
	McAccount                 *account;
	const gchar               *icon_name;
	gboolean                   is_enabled = TRUE;

	priv = GET_PRIV (chooser);

	combobox = GTK_COMBO_BOX (chooser);
	store = GTK_LIST_STORE (gtk_combo_box_get_model (combobox));

	gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
			    COL_ACCOUNT_POINTER, &account,
			    -1);

	icon_name = empathy_icon_name_from_account (account);
	if (priv->filter) {
		is_enabled = priv->filter (account, priv->filter_data);
	}

	gtk_list_store_set (store, iter,
			    COL_ACCOUNT_IMAGE, icon_name,
			    COL_ACCOUNT_TEXT, mc_account_get_display_name (account),
			    COL_ACCOUNT_ENABLED, is_enabled,
			    -1);

	/* set first connected account as active account */
	if (priv->set_active_item == FALSE && is_enabled) {
		priv->set_active_item = TRUE;
		gtk_combo_box_set_active_iter (combobox, iter);
	}

	g_object_unref (account);
}

static void
account_chooser_connection_changed_cb (EmpathyAccountManager   *manager,
				       McAccount               *account,
				       TpConnectionStatusReason reason,
				       TpConnectionStatus       new_status,
				       TpConnectionStatus       old_status,
				       EmpathyAccountChooser   *chooser)
{
	GtkTreeIter iter;

	if (account_chooser_find_account (chooser, account, &iter)) {
		account_chooser_update_iter (chooser, &iter);
	}
}

static gboolean
account_chooser_separator_func (GtkTreeModel         *model,
				GtkTreeIter          *iter,
				EmpathyAccountChooser *chooser)
{
	EmpathyAccountChooserPriv *priv;
	gchar                    *text;
	gboolean                  is_separator;

	priv = GET_PRIV (chooser);
	
	if (!priv->has_all_option) {
		return FALSE;
	}
	
	gtk_tree_model_get (model, iter, COL_ACCOUNT_TEXT, &text, -1);
	is_separator = text == NULL;
	g_free (text);

	return is_separator;
}

static gboolean
account_chooser_set_account_foreach (GtkTreeModel   *model,
				     GtkTreePath    *path,
				     GtkTreeIter    *iter,
				     SetAccountData *data)
{
	McAccount *account;
	gboolean   equal;

	gtk_tree_model_get (model, iter, COL_ACCOUNT_POINTER, &account, -1);

	/* Special case so we can make it possible to select the All option */
	if ((data->account == NULL) != (account == NULL)) {
		equal = FALSE;
	}
	else if (data->account == account) {
		equal = TRUE;
	} else {
		equal = empathy_account_equal (data->account, account);
	}

	if (account) {
		g_object_unref (account);
	}

	if (equal) {
		GtkComboBox *combobox;

		combobox = GTK_COMBO_BOX (data->chooser);
		gtk_combo_box_set_active_iter (combobox, iter);

		data->set = TRUE;
	}

	return equal;
}

static gboolean
account_chooser_filter_foreach (GtkTreeModel *model,
				GtkTreePath  *path,
				GtkTreeIter  *iter,
				gpointer      chooser)
{
	account_chooser_update_iter (chooser, iter);
	return FALSE;
}

void
empathy_account_chooser_set_filter (EmpathyAccountChooser           *chooser,
                                    EmpathyAccountChooserFilterFunc  filter,
                                    gpointer                         user_data)
{
	EmpathyAccountChooserPriv *priv;
	GtkTreeModel *model;

	g_return_if_fail (EMPATHY_IS_ACCOUNT_CHOOSER (chooser));

	priv = GET_PRIV (chooser);

	priv->filter = filter;
	priv->filter_data = user_data;

	/* Refilter existing data */
	priv->set_active_item = FALSE;
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (chooser));
	gtk_tree_model_foreach (model, account_chooser_filter_foreach, chooser);
}

gboolean
empathy_account_chooser_filter_is_connected (McAccount *account,
					     gpointer   user_data)
{
	MissionControl     *mc;
	TpConnectionStatus  status;

	g_return_val_if_fail (MC_IS_ACCOUNT (account), FALSE);

	mc = empathy_mission_control_dup_singleton ();
	status = mission_control_get_connection_status (mc, account, NULL);
	g_object_unref (mc);

	return status == TP_CONNECTION_STATUS_CONNECTED;
}

