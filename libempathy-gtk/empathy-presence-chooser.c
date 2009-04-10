/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
 * Copyright (C) 2009 Collabora Ltd.
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
 * Authors: Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 *          Davyd Madeley <davyd.madeley@collabora.co.uk>
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gdk/gdkkeysyms.h>

#include <telepathy-glib/util.h>
#include <libmissioncontrol/mc-enum-types.h>

#include <libempathy/empathy-idle.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-status-presets.h>

// FIXME - what's the correct debug flag?
#define DEBUG_FLAG EMPATHY_DEBUG_DISPATCHER
#include <libempathy/empathy-debug.h>

#include "empathy-ui-utils.h"
#include "empathy-images.h"
#include "empathy-presence-chooser.h"

/* Flashing delay for icons (milliseconds). */
#define FLASH_TIMEOUT 500

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyPresenceChooser)
typedef struct {
	EmpathyIdle *idle;

	gboolean     editing_status;
	int          block_set_editing;
	int          block_changed;

	McPresence   state;
	int          previous_type;

	McPresence   flash_state_1;
	McPresence   flash_state_2;
	guint        flash_timeout_id;
} EmpathyPresenceChooserPriv;

typedef struct {
	GtkWidget    *dialog;
	GtkWidget    *checkbutton_save;
	GtkWidget    *comboboxentry_message;
	GtkWidget    *entry_message;
	GtkWidget    *combobox_status;
	GtkTreeModel *model_status;
} CustomMessageDialog;

enum {
	COL_ICON,
	COL_LABEL,
	COL_PRESENCE,
	COL_COUNT
};

static CustomMessageDialog *message_dialog = NULL;
/* States to be listed in the menu.
 * Each state has a boolean telling if it can have custom message */
static guint states[] = {MC_PRESENCE_AVAILABLE, TRUE,
			 MC_PRESENCE_DO_NOT_DISTURB, TRUE,
			 MC_PRESENCE_AWAY, TRUE,
			 MC_PRESENCE_HIDDEN, FALSE,
			 MC_PRESENCE_OFFLINE, FALSE};

static void            presence_chooser_finalize               (GObject                    *object);
static void            presence_chooser_presence_changed_cb    (EmpathyPresenceChooser      *chooser);
static gboolean        presence_chooser_flash_timeout_cb       (EmpathyPresenceChooser      *chooser);
static void            presence_chooser_flash_start            (EmpathyPresenceChooser      *chooser,
								McPresence                  state_1,
								McPresence                  state_2);
static void            presence_chooser_flash_stop             (EmpathyPresenceChooser      *chooser,
								McPresence                  state);
static void            presence_chooser_menu_add_item          (GtkWidget                  *menu,
								const gchar                *str,
								McPresence                  state);
static void            presence_chooser_noncustom_activate_cb  (GtkWidget                  *item,
								gpointer                    user_data);
static void            presence_chooser_set_state              (McPresence                  state,
								const gchar                *status);
static void            presence_chooser_custom_activate_cb     (GtkWidget                  *item,
								gpointer                    user_data);
static void            presence_chooser_dialog_show            (GtkWindow                  *parent);

G_DEFINE_TYPE (EmpathyPresenceChooser, empathy_presence_chooser, GTK_TYPE_COMBO_BOX_ENTRY);

static void
empathy_presence_chooser_class_init (EmpathyPresenceChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = presence_chooser_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyPresenceChooserPriv));
}

enum
{
	COL_STATE_ICON_NAME,
	COL_STATE,
	COL_STATUS_TEXT,
	COL_DISPLAY_MARKUP,
	COL_STATUS_CUSTOMISABLE,
	COL_TYPE,
	N_COLUMNS
};

enum
{
	ENTRY_TYPE_BUILTIN,
	ENTRY_TYPE_SAVED,
	ENTRY_TYPE_CUSTOM,
	ENTRY_TYPE_SEPARATOR,
	ENTRY_TYPE_EDIT_CUSTOM,
};

static GtkTreeModel *
create_model (void)
{
	GtkListStore *store = gtk_list_store_new (N_COLUMNS,
			G_TYPE_STRING,		/* COL_STATE_ICON_NAME */
			MC_TYPE_PRESENCE,	/* COL_STATE */
			G_TYPE_STRING,		/* COL_STATUS_TEXT */
			G_TYPE_STRING,		/* COL_DISPLAY_MARKUP */
			G_TYPE_BOOLEAN,		/* COL_STATUS_CUSTOMISABLE */
			G_TYPE_INT);		/* COL_TYPE */
	
	GtkTreeIter iter;
	
	int i;
	for (i = 0; i < G_N_ELEMENTS (states); i += 2) {
		GList       *list, *l;

		const char *status = empathy_presence_get_default_message (states[i]);
		const char *icon_name = empathy_icon_name_for_presence (states[i]);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				COL_STATE_ICON_NAME, icon_name,
				COL_STATE, states[i],
				COL_STATUS_TEXT, status,
				COL_DISPLAY_MARKUP, status,
				COL_STATUS_CUSTOMISABLE, states[i+1],
				COL_TYPE, ENTRY_TYPE_BUILTIN,
				-1);

		if (states[i+1]) {
			/* Set custom messages if wanted */
			list = empathy_status_presets_get (states[i], 5);
			for (l = list; l; l = l->next) {
				gtk_list_store_append (store, &iter);
				gtk_list_store_set (store, &iter,
						COL_STATE_ICON_NAME, icon_name,
						COL_STATE, states[i],
						COL_STATUS_TEXT, l->data,
						COL_DISPLAY_MARKUP, l->data,
						COL_STATUS_CUSTOMISABLE, TRUE,
						COL_TYPE, ENTRY_TYPE_SAVED,
						-1);
			}
			g_list_free (list);
		
			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter,
					COL_STATE_ICON_NAME, icon_name,
					COL_STATE, states[i],
					COL_STATUS_TEXT, "",
					COL_DISPLAY_MARKUP, "<i>Custom Message...</i>",
					COL_STATUS_CUSTOMISABLE, TRUE,
					COL_TYPE, ENTRY_TYPE_CUSTOM,
					-1);
		}

	}
	
	/* add a separator */
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			COL_TYPE, ENTRY_TYPE_SEPARATOR,
			-1);
	
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			COL_STATE_ICON_NAME, GTK_STOCK_EDIT,
			COL_STATUS_TEXT, "",
			COL_DISPLAY_MARKUP, "Edit Custom Messages...",
			COL_TYPE, ENTRY_TYPE_EDIT_CUSTOM,
			-1);

	return GTK_TREE_MODEL (store);
}

static void
popup_shown_cb (GObject *self, GParamSpec *pspec, gpointer user_data)
{
	gboolean shown;
	g_object_get (self, "popup-shown", &shown, NULL);

	if (!shown) return;

	GtkTreeModel *model = create_model ();

	gtk_combo_box_set_model (GTK_COMBO_BOX (self), GTK_TREE_MODEL (model));
	
	g_object_unref (model);
}

static void
set_status_editing (EmpathyPresenceChooser *self, gboolean editing)
{
	EmpathyPresenceChooserPriv *priv = GET_PRIV (self);
	GtkWidget *entry = gtk_bin_get_child (GTK_BIN (self));

	if (priv->block_set_editing) return;

	if (editing)
	{
		priv->editing_status = TRUE;
		gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
				GTK_ENTRY_ICON_SECONDARY,
				GTK_STOCK_OK);
		gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
				GTK_ENTRY_ICON_SECONDARY,
				_("Set status"));
		gtk_entry_set_icon_sensitive (GTK_ENTRY (entry),
				GTK_ENTRY_ICON_PRIMARY,
				FALSE);
	}
	else
	{
		gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
				GTK_ENTRY_ICON_SECONDARY,
				NULL);
		gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
				GTK_ENTRY_ICON_SECONDARY,
				NULL);
		gtk_entry_set_icon_sensitive (GTK_ENTRY (entry),
				GTK_ENTRY_ICON_PRIMARY,
				TRUE);

		/* attempt to get the toplevel for this widget */
		GtkWidget *window = gtk_widget_get_toplevel (GTK_WIDGET (self));
		if (GTK_WIDGET_TOPLEVEL (window) && GTK_IS_WINDOW (window))
		{
			/* unset the focus */
			gtk_window_set_focus (GTK_WINDOW (window), NULL);
		}
		gtk_editable_set_position (GTK_EDITABLE (entry), 0);

		priv->editing_status = FALSE;
	}
}

static void
mc_set_custom_state (EmpathyPresenceChooser *self)
{
	EmpathyPresenceChooserPriv *priv = GET_PRIV (self);
	GtkWidget *entry = gtk_bin_get_child (GTK_BIN (self));

	/* update the status with MC */
	const char *status = gtk_entry_get_text (GTK_ENTRY (entry));
	DEBUG ("Sending state to MC-> %s (%s)\n",
			g_enum_get_value (g_type_class_peek (MC_TYPE_PRESENCE),
				priv->state)->value_name,
			status);
	empathy_idle_set_presence (priv->idle, priv->state, status);
}

static void
ui_set_custom_state (EmpathyPresenceChooser *self,
		           McPresence state,
			   const char *status)
{
	EmpathyPresenceChooserPriv *priv = GET_PRIV (self);
	GtkWidget *entry = gtk_bin_get_child (GTK_BIN (self));
	const char *icon_name;

	priv->block_set_editing++;
	priv->block_changed++;

	icon_name = empathy_icon_name_for_presence (state);
	gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
			GTK_ENTRY_ICON_PRIMARY,
			icon_name);
	gtk_entry_set_text (GTK_ENTRY (entry), status);

	priv->block_changed--;
	priv->block_set_editing--;
}

static void
reset_status (EmpathyPresenceChooser *self)
{
	/* recover the status that was unset */
	presence_chooser_presence_changed_cb (self);
}

static void
entry_icon_release_cb (EmpathyPresenceChooser	*self,
		       GtkEntryIconPosition	 icon_pos,
		       GdkEvent		*event,
		       GtkEntry		*entry)
{
	set_status_editing (self, FALSE);
	mc_set_custom_state (self);
}

static void
entry_activate_cb (EmpathyPresenceChooser	*self,
		   GtkEntry			*entry)
{
	set_status_editing (self, FALSE);
	mc_set_custom_state (self);
}

static gboolean
entry_key_press_event_cb (EmpathyPresenceChooser	*self,
		          GdkEventKey			*event,
			  GtkWidget			*entry)
{
	EmpathyPresenceChooserPriv *priv = GET_PRIV (self);

	if (priv->editing_status && event->keyval == GDK_Escape)
	{
		/* the user pressed Escape, undo the editing */
		set_status_editing (self, FALSE);
		reset_status (self);

		return TRUE;
	}
	else if (event->keyval == GDK_Up || event->keyval == GDK_Down)
	{
		/* ignore */
		return TRUE;
	}

	return FALSE; /* send this event elsewhere */
}

static gboolean
entry_button_press_event_cb (EmpathyPresenceChooser *self,
                             GdkEventButton         *event,
			     GtkWidget              *entry)
{
	EmpathyPresenceChooserPriv *priv = GET_PRIV (self);

	if (!priv->editing_status && event->button == 1)
	{
		set_status_editing (self, TRUE);
		gtk_widget_grab_focus (entry);
		gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);

		return TRUE;
	}

	return FALSE;
}

static void
text_changed_cb (EmpathyPresenceChooser *self, gpointer user_data)
{
	EmpathyPresenceChooserPriv *priv = GET_PRIV (self);

	if (priv->block_changed) return;

	/* the combo is being edited to a custom entry */
	if (!priv->editing_status)
	{
		set_status_editing (self, TRUE);
	}
}

static void
changed_cb (GtkComboBox *self, gpointer user_data)
{
	EmpathyPresenceChooserPriv *priv = GET_PRIV (self);

	if (priv->block_changed) return;

	GtkTreeIter iter;
	char *icon_name;
	McPresence new_state;
	gboolean customisable = TRUE;
	int type = -1;

	GtkTreeModel *model = gtk_combo_box_get_model (self);
	if (!gtk_combo_box_get_active_iter (self, &iter))
	{
		return;
	}

	gtk_tree_model_get (model, &iter,
			COL_STATE_ICON_NAME, &icon_name,
			COL_STATE, &new_state,
			COL_STATUS_CUSTOMISABLE, &customisable,
			COL_TYPE, &type,
			-1);

	GtkWidget *entry = gtk_bin_get_child (GTK_BIN (self));

	/* some types of status aren't editable, set the editability of the
	 * entry appropriately. Unless we're just about to reset it anyway,
	 * in which case, don't fiddle with it */
	if (type != ENTRY_TYPE_EDIT_CUSTOM)
	{
		gtk_editable_set_editable (GTK_EDITABLE (entry), customisable);
		priv->state = new_state;
	}

	if (type == ENTRY_TYPE_EDIT_CUSTOM)
	{
		reset_status (EMPATHY_PRESENCE_CHOOSER (self));

		/* attempt to get the toplevel for this widget */
		GtkWidget *window = gtk_widget_get_toplevel (GTK_WIDGET (self));
		if (!GTK_WIDGET_TOPLEVEL (window) || !GTK_IS_WINDOW (window))
		{
			window = NULL;
		}

		presence_chooser_dialog_show (GTK_WINDOW (window));
	}
	else if (type == ENTRY_TYPE_CUSTOM)
	{
		gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
				GTK_ENTRY_ICON_PRIMARY,
				icon_name);

		/* preseed the status */
		if (priv->previous_type == ENTRY_TYPE_BUILTIN)
		{
			gtk_entry_set_text (GTK_ENTRY (entry), "");
		}
		else
		{
			const char *status = empathy_idle_get_status (priv->idle);
			gtk_entry_set_text (GTK_ENTRY (entry), status);
		}

		/* grab the focus */
		gtk_widget_grab_focus (entry);
	}
	else
	{
		char *status;
		/* just in case we were setting a new status when
		 * things were changed */
		set_status_editing (EMPATHY_PRESENCE_CHOOSER (self), FALSE);

		gtk_tree_model_get (model, &iter,
				COL_STATUS_TEXT, &status,
				-1);

		empathy_idle_set_presence (priv->idle, priv->state, status);

		g_free (status);
	}

	if (type != ENTRY_TYPE_EDIT_CUSTOM)
	{
		priv->previous_type = type;
	}
	g_free (icon_name);
}

static gboolean
combo_row_separator_func (GtkTreeModel	*model,
			  GtkTreeIter	*iter,
			  gpointer	 data)
{
	int type;
	gtk_tree_model_get (model, iter,
			COL_TYPE, &type,
			-1);

	return (type == ENTRY_TYPE_SEPARATOR);
}

static gboolean
focus_out_cb (EmpathyPresenceChooser *chooser, GdkEventFocus *event,
              GtkEntry *entry)
{
	EmpathyPresenceChooserPriv *priv = GET_PRIV (chooser);

	if (priv->editing_status)
	{
		// entry_activate_cb (chooser, entry);
	}

	return FALSE;
}

static void
empathy_presence_chooser_init (EmpathyPresenceChooser *chooser)
{
	EmpathyPresenceChooserPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (chooser,
		EMPATHY_TYPE_PRESENCE_CHOOSER, EmpathyPresenceChooserPriv);

	chooser->priv = priv;
	
	GtkTreeModel *model = create_model ();

	gtk_combo_box_set_model (GTK_COMBO_BOX (chooser), GTK_TREE_MODEL (model));
	gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY (chooser),
			COL_STATUS_TEXT);
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (chooser),
			combo_row_separator_func,
			NULL, NULL);
	
	GtkWidget *entry = gtk_bin_get_child (GTK_BIN (chooser));
	gtk_entry_set_icon_activatable (GTK_ENTRY (entry),
			GTK_ENTRY_ICON_PRIMARY, FALSE);
	g_signal_connect_object (entry, "icon-release",
			G_CALLBACK (entry_icon_release_cb), chooser,
			G_CONNECT_SWAPPED);
	g_signal_connect_object (entry, "activate",
			G_CALLBACK (entry_activate_cb), chooser,
			G_CONNECT_SWAPPED);
	g_signal_connect_object (entry, "key-press-event",
			G_CALLBACK (entry_key_press_event_cb), chooser,
			G_CONNECT_SWAPPED);
	g_signal_connect_object (entry, "button-press-event",
			G_CALLBACK (entry_button_press_event_cb), chooser,
			G_CONNECT_SWAPPED);

	GtkCellRenderer *renderer;
	gtk_cell_layout_clear (GTK_CELL_LAYOUT (chooser));

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (chooser), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (chooser), renderer,
			"icon-name", COL_STATE_ICON_NAME,
			NULL);
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (chooser), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (chooser), renderer,
			"markup", COL_DISPLAY_MARKUP,
			NULL);

	g_object_unref (model);

	g_signal_connect (chooser, "notify::popup-shown",
			G_CALLBACK (popup_shown_cb), NULL);
	g_signal_connect (chooser, "changed",
			G_CALLBACK (changed_cb), NULL);
	g_signal_connect_swapped (entry, "changed",
			G_CALLBACK (text_changed_cb), chooser);
	g_signal_connect_swapped (entry, "focus-out-event",
			G_CALLBACK (focus_out_cb), chooser);

	priv->idle = empathy_idle_dup_singleton ();
	presence_chooser_presence_changed_cb (chooser);
	g_signal_connect_swapped (priv->idle, "notify",
				  G_CALLBACK (presence_chooser_presence_changed_cb),
				  chooser);

	g_object_set (chooser,
			// FIXME: this string sucks
			"tooltip-text", _("Set your presence and current status"),
			NULL);
}

static void
presence_chooser_finalize (GObject *object)
{
	EmpathyPresenceChooserPriv *priv;

	priv = GET_PRIV (object);

	if (priv->flash_timeout_id) {
		g_source_remove (priv->flash_timeout_id);
	}

	g_signal_handlers_disconnect_by_func (priv->idle,
					      presence_chooser_presence_changed_cb,
					      object);
	g_object_unref (priv->idle);

	G_OBJECT_CLASS (empathy_presence_chooser_parent_class)->finalize (object);
}

GtkWidget *
empathy_presence_chooser_new (void)
{
	GtkWidget *chooser;

	chooser = g_object_new (EMPATHY_TYPE_PRESENCE_CHOOSER, NULL);

	return chooser;
}

static void
presence_chooser_presence_changed_cb (EmpathyPresenceChooser *chooser)
{
	EmpathyPresenceChooserPriv *priv;
	McPresence                 state;
	McPresence                 flash_state;
	const gchar               *status;

	priv = GET_PRIV (chooser);

	priv->state = state = empathy_idle_get_state (priv->idle);
	status = empathy_idle_get_status (priv->idle);
	flash_state = empathy_idle_get_flash_state (priv->idle);

	/* look through the model and attempt to find a matching state */
	GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (chooser));
	GtkTreeIter iter;
	gboolean valid, match_state = FALSE, match = FALSE;
	for (valid = gtk_tree_model_get_iter_first (model, &iter);
	     valid;
	     valid = gtk_tree_model_iter_next (model, &iter))
	{
		int m_type;
		McPresence m_state;
		char *m_status;

		gtk_tree_model_get (model, &iter,
				COL_STATE, &m_state,
				COL_TYPE, &m_type,
				-1);

		if (m_type == ENTRY_TYPE_CUSTOM ||
		    m_type == ENTRY_TYPE_SEPARATOR ||
		    m_type == ENTRY_TYPE_EDIT_CUSTOM)
		{
			continue;
		}
		else if (!match_state && state == m_state)
		{
			/* we are now in the section that can contain our
			 * match */
			match_state = TRUE;
		}
		else if (match_state && state != m_state)
		{
			/* we have passed the section that can contain our
			 * match */
			break;
		}

		gtk_tree_model_get (model, &iter,
				COL_STATUS_TEXT, &m_status,
				-1);

		match = !strcmp (status, m_status);

		g_free (m_status);

		if (match) break;

	}

	if (match)
	{
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (chooser), &iter);
	}
	else
	{
		// FIXME - do we insert the match into the menu?
		ui_set_custom_state (chooser, state, status);
	}

	if (flash_state != MC_PRESENCE_UNSET) {
		presence_chooser_flash_start (chooser, state, flash_state);
	} else {
		presence_chooser_flash_stop (chooser, state);
	}
}

static gboolean
presence_chooser_flash_timeout_cb (EmpathyPresenceChooser *chooser)
{
	EmpathyPresenceChooserPriv *priv;
	McPresence                 state;
	static gboolean            on = FALSE;

	priv = GET_PRIV (chooser);

	if (on) {
		state = priv->flash_state_1;
	} else {
		state = priv->flash_state_2;
	}

	GtkWidget *entry = gtk_bin_get_child (GTK_BIN (chooser));
	gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
			GTK_ENTRY_ICON_PRIMARY,
			empathy_icon_name_for_presence (state));

	on = !on;

	return TRUE;
}

static void
presence_chooser_flash_start (EmpathyPresenceChooser *chooser,
			      McPresence             state_1,
			      McPresence             state_2)
{
	EmpathyPresenceChooserPriv *priv;

	g_return_if_fail (EMPATHY_IS_PRESENCE_CHOOSER (chooser));

	priv = GET_PRIV (chooser);

	priv->flash_state_1 = state_1;
	priv->flash_state_2 = state_2;

	if (!priv->flash_timeout_id) {
		priv->flash_timeout_id = g_timeout_add (FLASH_TIMEOUT,
							(GSourceFunc) presence_chooser_flash_timeout_cb,
							chooser);
	}
}

static void
presence_chooser_flash_stop (EmpathyPresenceChooser *chooser,
			     McPresence             state)
{
	EmpathyPresenceChooserPriv *priv;

	g_return_if_fail (EMPATHY_IS_PRESENCE_CHOOSER (chooser));

	priv = GET_PRIV (chooser);

	if (priv->flash_timeout_id) {
		g_source_remove (priv->flash_timeout_id);
		priv->flash_timeout_id = 0;
	}
	GtkWidget *entry = gtk_bin_get_child (GTK_BIN (chooser));
	
	gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
			GTK_ENTRY_ICON_PRIMARY,
			empathy_icon_name_for_presence (state));

	// FIXME - what does this do?
	// priv->last_state = state;
}

GtkWidget *
empathy_presence_chooser_create_menu (void)
{
	const gchar *status;
	GtkWidget   *menu;
	GtkWidget   *item;
	GtkWidget   *image;
	guint        i;

	menu = gtk_menu_new ();

	for (i = 0; i < G_N_ELEMENTS (states); i += 2) {
		GList       *list, *l;

		status = empathy_presence_get_default_message (states[i]);
		presence_chooser_menu_add_item (menu,
						status,
						states[i]);

		if (states[i+1]) {
			/* Set custom messages if wanted */
			list = empathy_status_presets_get (states[i], 5);
			for (l = list; l; l = l->next) {
				presence_chooser_menu_add_item (menu,
								l->data,
								states[i]);
			}
			g_list_free (list);
		}

	}

	/* Separator. */
	item = gtk_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	/* Custom messages */
	item = gtk_image_menu_item_new_with_label (_("Custom messages..."));
	image = gtk_image_new_from_stock (GTK_STOCK_EDIT, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (image);
	gtk_widget_show (item);

	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (presence_chooser_custom_activate_cb),
			  NULL);

	return menu;
}

static void
presence_chooser_menu_add_item (GtkWidget   *menu,
				const gchar *str,
				McPresence   state)
{
	GtkWidget   *item;
	GtkWidget   *image;
	const gchar *icon_name;

	item = gtk_image_menu_item_new_with_label (str);
	icon_name = empathy_icon_name_for_presence (state);

	g_signal_connect (item, "activate",
			  G_CALLBACK (presence_chooser_noncustom_activate_cb),
			  NULL);

	image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);

	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	gtk_widget_show (item);

	g_object_set_data_full (G_OBJECT (item),
				"status", g_strdup (str),
				(GDestroyNotify) g_free);

	g_object_set_data (G_OBJECT (item), "state", GINT_TO_POINTER (state));

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static void
presence_chooser_noncustom_activate_cb (GtkWidget *item,
					gpointer   user_data)
{
	McPresence   state;
	const gchar *status;

	status = g_object_get_data (G_OBJECT (item), "status");
	state = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "state"));

	presence_chooser_set_state (state, status);
}

static void
presence_chooser_set_state (McPresence   state,
			    const gchar *status)
{
	EmpathyIdle *idle;

	idle = empathy_idle_dup_singleton ();
	empathy_idle_set_presence (idle, state, status);
	g_object_unref (idle);
}

static void
presence_chooser_custom_activate_cb (GtkWidget *item,
				     gpointer   user_data)
{
	presence_chooser_dialog_show (NULL);
}

static McPresence
presence_chooser_dialog_get_selected (CustomMessageDialog *dialog)
{
	GtkTreeModel *model;
	GtkTreeIter   iter;
	McPresence    presence = LAST_MC_PRESENCE;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->combobox_status));
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->combobox_status), &iter)) {
		gtk_tree_model_get (model, &iter,
				    COL_PRESENCE, &presence,
				    -1);
	}

	return presence;
}

static void
presence_chooser_dialog_status_changed_cb (GtkWidget           *widget,
					   CustomMessageDialog *dialog)
{
	GtkListStore *store;
	GtkTreeIter   iter;
	McPresence    presence = LAST_MC_PRESENCE;
	GList        *messages, *l;

	presence = presence_chooser_dialog_get_selected (dialog);

	store = gtk_list_store_new (1, G_TYPE_STRING);
	messages = empathy_status_presets_get (presence, -1);
	for (l = messages; l; l = l->next) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, l->data, -1);
	}

	gtk_entry_set_text (GTK_ENTRY (dialog->entry_message),
			    messages ? messages->data : "");

	g_list_free (messages);

	gtk_combo_box_set_model (GTK_COMBO_BOX (dialog->comboboxentry_message),
				 GTK_TREE_MODEL (store));

	g_object_unref (store);
}

static void
presence_chooser_dialog_message_changed_cb (GtkWidget           *widget,
					    CustomMessageDialog *dialog)
{
	McPresence   presence;
	GList       *messages, *l;
	const gchar *text;
	gboolean     found = FALSE;

	presence = presence_chooser_dialog_get_selected (dialog);
	text = gtk_entry_get_text (GTK_ENTRY (dialog->entry_message));

	messages = empathy_status_presets_get (presence, -1);
	for (l = messages; l; l = l->next) {
		if (!tp_strdiff (text, l->data)) {
			found = TRUE;
			break;
		}
	}
	g_list_free (messages);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_save),
				      found);
}

static void
presence_chooser_dialog_save_toggled_cb (GtkWidget           *widget,
					 CustomMessageDialog *dialog)
{
	gboolean     active;
	McPresence   state;
	const gchar *text;

	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->checkbutton_save));
	state = presence_chooser_dialog_get_selected (dialog);
	text = gtk_entry_get_text (GTK_ENTRY (dialog->entry_message));

	if (active) {
		empathy_status_presets_set_last (state, text);
	} else {
		empathy_status_presets_remove (state, text);
	}
}

static void
presence_chooser_dialog_setup (CustomMessageDialog *dialog)
{
	GtkListStore    *store;
	GtkCellRenderer *renderer;
	GtkTreeIter      iter;
	guint            i;

	store = gtk_list_store_new (COL_COUNT,
				    G_TYPE_STRING,     /* Icon name */
				    G_TYPE_STRING,     /* Label     */
				    MC_TYPE_PRESENCE); /* Presence   */
	gtk_combo_box_set_model (GTK_COMBO_BOX (dialog->combobox_status),
				 GTK_TREE_MODEL (store));

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (dialog->combobox_status), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (dialog->combobox_status), renderer,
					"icon-name", COL_ICON,
					NULL);
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_BUTTON, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (dialog->combobox_status), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (dialog->combobox_status), renderer,
					"text", COL_LABEL,
					NULL);

	for (i = 0; i < G_N_ELEMENTS (states); i += 2) {
		if (!states[i+1]) {
			continue;
		}

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_ICON, empathy_icon_name_for_presence (states[i]),
				    COL_LABEL, empathy_presence_get_default_message (states[i]),
				    COL_PRESENCE, states[i],
				    -1);
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->combobox_status), 0);
}

static void
presence_chooser_dialog_response_cb (GtkWidget           *widget,
				     gint                 response,
				     CustomMessageDialog *dialog)
{
	if (response == GTK_RESPONSE_APPLY) {
		McPresence   state;
		const gchar *text;

		state = presence_chooser_dialog_get_selected (dialog);
		text = gtk_entry_get_text (GTK_ENTRY (dialog->entry_message));

		presence_chooser_set_state (state, text);
	}

	gtk_widget_destroy (widget);
}

static void
presence_chooser_dialog_destroy_cb (GtkWidget           *widget,
				    CustomMessageDialog *dialog)
{

	g_free (dialog);
	message_dialog = NULL;
}

static void
presence_chooser_dialog_show (GtkWindow *parent)
{
	GladeXML *glade;
	gchar    *filename;

	if (message_dialog) {
		gtk_window_present (GTK_WINDOW (message_dialog->dialog));
		return;
	}

	message_dialog = g_new0 (CustomMessageDialog, 1);

	filename = empathy_file_lookup ("empathy-presence-chooser.glade",
					"libempathy-gtk");
	glade = empathy_glade_get_file (filename,
				       "custom_message_dialog",
				       NULL,
				       "custom_message_dialog", &message_dialog->dialog,
				       "checkbutton_save", &message_dialog->checkbutton_save,
				       "comboboxentry_message", &message_dialog->comboboxentry_message,
				       "combobox_status", &message_dialog->combobox_status,
				       NULL);
	g_free (filename);

	empathy_glade_connect (glade,
			       message_dialog,
			       "custom_message_dialog", "destroy", presence_chooser_dialog_destroy_cb,
			       "custom_message_dialog", "response", presence_chooser_dialog_response_cb,
			       "combobox_status", "changed", presence_chooser_dialog_status_changed_cb,
			       "checkbutton_save", "toggled", presence_chooser_dialog_save_toggled_cb,
			       NULL);

	g_object_unref (glade);

	/* Setup the message combobox */
	message_dialog->entry_message = GTK_BIN (message_dialog->comboboxentry_message)->child;
	gtk_entry_set_activates_default (GTK_ENTRY (message_dialog->entry_message), TRUE);
	gtk_entry_set_width_chars (GTK_ENTRY (message_dialog->entry_message), 25);
	g_signal_connect (message_dialog->entry_message, "changed",
			  G_CALLBACK (presence_chooser_dialog_message_changed_cb),
			  message_dialog);

	presence_chooser_dialog_setup (message_dialog);

	gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY (message_dialog->comboboxentry_message), 0);

	if (parent)
	{
		gtk_window_set_transient_for (
				GTK_WINDOW (message_dialog->dialog),
				parent);
	}

	gtk_widget_show_all (message_dialog->dialog);
}

