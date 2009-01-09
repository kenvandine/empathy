/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <telepathy-glib/util.h>
#include <libmissioncontrol/mc-enum-types.h>

#include <libempathy/empathy-idle.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-status-presets.h>

#include "empathy-ui-utils.h"
#include "empathy-images.h"
#include "empathy-presence-chooser.h"

/* Flashing delay for icons (milliseconds). */
#define FLASH_TIMEOUT 500

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyPresenceChooser)
typedef struct {
	EmpathyIdle *idle;

	GtkWidget   *hbox;
	GtkWidget   *image;
	GtkWidget   *label;
	GtkWidget   *menu;

	McPresence   last_state;

	McPresence   flash_state_1;
	McPresence   flash_state_2;
	guint        flash_timeout_id;

	/* The handle the kind of unnessecary scroll support. */
	guint        scroll_timeout_id;
	McPresence   scroll_state;
	gchar       *scroll_status;
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

typedef struct {
	McPresence   state;
	const gchar *status;
} StateAndStatus;

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
static void            presence_chooser_reset_scroll_timeout   (EmpathyPresenceChooser      *chooser);
static gboolean        presence_chooser_scroll_timeout_cb      (EmpathyPresenceChooser      *chooser);
static gboolean        presence_chooser_scroll_event_cb        (EmpathyPresenceChooser      *chooser,
								GdkEventScroll             *event,
								gpointer                    user_data);
static GList *         presence_chooser_get_presets            (EmpathyPresenceChooser      *chooser);
static StateAndStatus *presence_chooser_state_and_status_new   (McPresence                  state,
								const gchar                *status);
static gboolean        presence_chooser_flash_timeout_cb       (EmpathyPresenceChooser      *chooser);
static void            presence_chooser_flash_start            (EmpathyPresenceChooser      *chooser,
								McPresence                  state_1,
								McPresence                  state_2);
static void            presence_chooser_flash_stop             (EmpathyPresenceChooser      *chooser,
								McPresence                  state);
static gboolean        presence_chooser_button_press_event_cb  (GtkWidget                  *chooser,
								GdkEventButton             *event,
								gpointer                    user_data);
static void            presence_chooser_toggled_cb             (GtkWidget                  *chooser,
								gpointer                    user_data);
static void            presence_chooser_menu_popup             (EmpathyPresenceChooser      *chooser);
static void            presence_chooser_menu_popdown           (EmpathyPresenceChooser      *chooser);
static void            presence_chooser_menu_selection_done_cb (GtkMenuShell               *menushell,
								EmpathyPresenceChooser      *chooser);
static void            presence_chooser_menu_destroy_cb        (GtkWidget                  *menu,
								EmpathyPresenceChooser      *chooser);
static void            presence_chooser_menu_detach            (GtkWidget                  *attach_widget,
								GtkMenu                    *menu);
static void            presence_chooser_menu_align_func        (GtkMenu                    *menu,
								gint                       *x,
								gint                       *y,
								gboolean                   *push_in,
								GtkWidget                  *widget);
static void            presence_chooser_menu_add_item          (GtkWidget                  *menu,
								const gchar                *str,
								McPresence                  state);
static void            presence_chooser_noncustom_activate_cb  (GtkWidget                  *item,
								gpointer                    user_data);
static void            presence_chooser_set_state              (McPresence                  state,
								const gchar                *status);
static void            presence_chooser_custom_activate_cb     (GtkWidget                  *item,
								gpointer                    user_data);
static void            presence_chooser_dialog_show            (void);

G_DEFINE_TYPE (EmpathyPresenceChooser, empathy_presence_chooser, GTK_TYPE_TOGGLE_BUTTON);

static void
empathy_presence_chooser_class_init (EmpathyPresenceChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = presence_chooser_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyPresenceChooserPriv));
}

static void
empathy_presence_chooser_init (EmpathyPresenceChooser *chooser)
{
	GtkWidget                  *arrow;
	GtkWidget                  *alignment;
	EmpathyPresenceChooserPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (chooser,
		EMPATHY_TYPE_PRESENCE_CHOOSER, EmpathyPresenceChooserPriv);

	chooser->priv = priv;
	gtk_button_set_relief (GTK_BUTTON (chooser), GTK_RELIEF_NONE);
	gtk_button_set_focus_on_click (GTK_BUTTON (chooser), FALSE);

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_widget_show (alignment);
	gtk_container_add (GTK_CONTAINER (chooser), alignment);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 1, 0);

	priv->hbox = gtk_hbox_new (FALSE, 1);
	gtk_widget_show (priv->hbox);
	gtk_container_add (GTK_CONTAINER (alignment), priv->hbox);

	priv->image = gtk_image_new ();
	gtk_widget_show (priv->image);
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->image, FALSE, TRUE, 0);

	priv->label = gtk_label_new (NULL);
	gtk_widget_show (priv->label);
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->label, TRUE, TRUE, 0);
	gtk_label_set_ellipsize (GTK_LABEL (priv->label), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (priv->label), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (priv->label), 4, 1);

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_widget_show (alignment);
	gtk_box_pack_start (GTK_BOX (priv->hbox), alignment, FALSE, FALSE, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show (arrow);
	gtk_container_add (GTK_CONTAINER (alignment), arrow);

	g_signal_connect (chooser, "toggled",
			  G_CALLBACK (presence_chooser_toggled_cb),
			  NULL);
	g_signal_connect (chooser, "button-press-event",
			  G_CALLBACK (presence_chooser_button_press_event_cb),
			  NULL);
	g_signal_connect (chooser, "scroll-event",
			  G_CALLBACK (presence_chooser_scroll_event_cb),
			  NULL);

	priv->idle = empathy_idle_dup_singleton ();
	presence_chooser_presence_changed_cb (chooser);
	g_signal_connect_swapped (priv->idle, "notify",
				  G_CALLBACK (presence_chooser_presence_changed_cb),
				  chooser);
}

static void
presence_chooser_finalize (GObject *object)
{
	EmpathyPresenceChooserPriv *priv;

	priv = GET_PRIV (object);

	if (priv->flash_timeout_id) {
		g_source_remove (priv->flash_timeout_id);
	}

	if (priv->scroll_timeout_id) {
		g_source_remove (priv->scroll_timeout_id);
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

	state = empathy_idle_get_state (priv->idle);
	status = empathy_idle_get_status (priv->idle);
	flash_state = empathy_idle_get_flash_state (priv->idle);

	presence_chooser_reset_scroll_timeout (chooser);
	gtk_label_set_text (GTK_LABEL (priv->label), status);

	if (flash_state != MC_PRESENCE_UNSET) {
		presence_chooser_flash_start (chooser, state, flash_state);
	} else {
		presence_chooser_flash_stop (chooser, state);
	}
}

static void
presence_chooser_reset_scroll_timeout (EmpathyPresenceChooser *chooser)
{
	EmpathyPresenceChooserPriv *priv;

	priv = GET_PRIV (chooser);

	if (priv->scroll_timeout_id) {
		g_source_remove (priv->scroll_timeout_id);
		priv->scroll_timeout_id = 0;
	}

	g_free (priv->scroll_status);
	priv->scroll_status = NULL;
}

static gboolean
presence_chooser_scroll_timeout_cb (EmpathyPresenceChooser *chooser)
{
	EmpathyPresenceChooserPriv *priv;

	priv = GET_PRIV (chooser);

	priv->scroll_timeout_id = 0;

	empathy_idle_set_presence (priv->idle,
				   priv->scroll_state,
				   priv->scroll_status);

	g_free (priv->scroll_status);
	priv->scroll_status = NULL;

	return FALSE;
}

static gboolean
presence_chooser_scroll_event_cb (EmpathyPresenceChooser *chooser,
				  GdkEventScroll        *event,
				  gpointer               user_data)
{
	EmpathyPresenceChooserPriv *priv;
	GList                     *list, *l;
	const gchar               *current_status;
	StateAndStatus            *sas;
	gboolean                   match;

	priv = GET_PRIV (chooser);

	switch (event->direction) {
	case GDK_SCROLL_UP:
		break;
	case GDK_SCROLL_DOWN:
		break;
	default:
		return FALSE;
	}

	current_status = gtk_label_get_text (GTK_LABEL (priv->label));

	/* Get the list of presets, which in this context means all the items
	 * without a trailing "...".
	 */
	list = presence_chooser_get_presets (chooser);
	sas = NULL;
	match = FALSE;
	for (l = list; l; l = l->next) {
		sas = l->data;

		if (sas->state == priv->last_state &&
		    strcmp (sas->status, current_status) == 0) {
			sas = NULL;
			match = TRUE;
			if (event->direction == GDK_SCROLL_UP) {
				if (l->prev) {
					sas = l->prev->data;
				}
			}
			else if (event->direction == GDK_SCROLL_DOWN) {
				if (l->next) {
					sas = l->next->data;
				}
			}
			break;
		}

		sas = NULL;
	}

	if (sas) {
		presence_chooser_reset_scroll_timeout (chooser);

		priv->scroll_status = g_strdup (sas->status);
		priv->scroll_state = sas->state;

		priv->scroll_timeout_id =
			g_timeout_add_seconds (1,
					       (GSourceFunc) presence_chooser_scroll_timeout_cb,
					       chooser);

		presence_chooser_flash_stop (chooser, sas->state);
		gtk_label_set_text (GTK_LABEL (priv->label), sas->status);	
	}
	else if (!match) {
		const gchar *status;
		/* If we didn't get any match at all, it means the last state
		 * was a custom one. Just switch to the first one.
		 */
		status = empathy_presence_get_default_message (states[0]);

		presence_chooser_reset_scroll_timeout (chooser);
		empathy_idle_set_presence (priv->idle, states[0], status);
	}

	g_list_foreach (list, (GFunc) g_free, NULL);
	g_list_free (list);

	return TRUE;
}

static GList *
presence_chooser_get_presets (EmpathyPresenceChooser *chooser)
{
	GList      *list = NULL;
	guint       i;

	for (i = 0; i < G_N_ELEMENTS (states); i += 2) {
		GList          *presets, *p;
		StateAndStatus *sas;
		const gchar    *status;

		status = empathy_presence_get_default_message (states[i]);
		sas = presence_chooser_state_and_status_new (states[i], status);
		list = g_list_prepend (list, sas);

		/* Go to next state if we don't want messages for that state */
		if (!states[i+1]) {
			continue;
		}

		presets = empathy_status_presets_get (states[i], 5);
		for (p = presets; p; p = p->next) {
			sas = presence_chooser_state_and_status_new (states[i], p->data);
			list = g_list_prepend (list, sas);
		}
		g_list_free (presets);
	}
	list = g_list_reverse (list);

	return list;
}

static StateAndStatus *
presence_chooser_state_and_status_new (McPresence   state,
				       const gchar *status)
{
	StateAndStatus *sas;

	sas = g_new0 (StateAndStatus, 1);

	sas->state = state;
	sas->status = status;

	return sas;
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

	gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
				      empathy_icon_name_for_presence (state),
				      GTK_ICON_SIZE_MENU);

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

	gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
				      empathy_icon_name_for_presence (state),
				      GTK_ICON_SIZE_MENU);

	priv->last_state = state;
}

static gboolean
presence_chooser_button_press_event_cb (GtkWidget      *chooser,
					GdkEventButton *event,
					gpointer        user_data)
{
	if (event->button != 1 || event->type != GDK_BUTTON_PRESS) {
		return FALSE;
	}

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chooser))) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chooser), TRUE);
			return TRUE;
		}

	return FALSE;
}

static void
presence_chooser_toggled_cb (GtkWidget *chooser,
			     gpointer   user_data)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chooser))) {
		presence_chooser_menu_popup (EMPATHY_PRESENCE_CHOOSER (chooser));
	} else {
		presence_chooser_menu_popdown (EMPATHY_PRESENCE_CHOOSER (chooser));
	}
}

static void
presence_chooser_menu_popup (EmpathyPresenceChooser *chooser)
{
	EmpathyPresenceChooserPriv *priv;
	GtkWidget                 *menu;

	priv = GET_PRIV (chooser);

	if (priv->menu) {
		return;
	}

	menu = empathy_presence_chooser_create_menu ();

	g_signal_connect_after (menu, "selection-done",
				G_CALLBACK (presence_chooser_menu_selection_done_cb),
				chooser);

	g_signal_connect (menu, "destroy",
			  G_CALLBACK (presence_chooser_menu_destroy_cb),
			  chooser);

	gtk_menu_attach_to_widget (GTK_MENU (menu),
				   GTK_WIDGET (chooser),
				   presence_chooser_menu_detach);

	gtk_menu_popup (GTK_MENU (menu),
			NULL, NULL,
			(GtkMenuPositionFunc) presence_chooser_menu_align_func,
			chooser,
			1,
			gtk_get_current_event_time ());

	priv->menu = menu;
}

static void
presence_chooser_menu_popdown (EmpathyPresenceChooser *chooser)
{
	EmpathyPresenceChooserPriv *priv;

	priv = GET_PRIV (chooser);

	if (priv->menu) {
		gtk_widget_destroy (priv->menu);
	}
}

static void
presence_chooser_menu_selection_done_cb (GtkMenuShell          *menushell,
					 EmpathyPresenceChooser *chooser)
{
	gtk_widget_destroy (GTK_WIDGET (menushell));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chooser), FALSE);
}

static void
presence_chooser_menu_destroy_cb (GtkWidget             *menu,
				  EmpathyPresenceChooser *chooser)
{
	EmpathyPresenceChooserPriv *priv;

	priv = GET_PRIV (chooser);

	priv->menu = NULL;
}

static void
presence_chooser_menu_detach (GtkWidget *attach_widget,
			      GtkMenu   *menu)
{
	/* We don't need to do anything, but attaching the menu means
	 * we don't own the ref count and it is cleaned up properly.
	 */
}

static void
presence_chooser_menu_align_func (GtkMenu   *menu,
				  gint      *x,
				  gint      *y,
				  gboolean  *push_in,
				  GtkWidget *widget)
{
	GtkRequisition  req;
	GdkScreen      *screen;
	gint            screen_height;

	gtk_widget_size_request (GTK_WIDGET (menu), &req);

	gdk_window_get_origin (widget->window, x, y);

	*x += widget->allocation.x + 1;
	*y += widget->allocation.y;

	screen = gtk_widget_get_screen (GTK_WIDGET (menu));
	screen_height = gdk_screen_get_height (screen);

	if (req.height > screen_height) {
		/* Too big for screen height anyway. */
		*y = 0;
		return;
	}

	if ((*y + req.height + widget->allocation.height) > screen_height) {
		/* Can't put it below the button. */
		*y -= req.height;
		*y += 1;
	} else {
		/* Put menu below button. */
		*y += widget->allocation.height;
		*y -= 1;
	}

	*push_in = FALSE;
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
	presence_chooser_dialog_show ();
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
presence_chooser_dialog_show (void)
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

	/* FIXME: Set transian for a window ? */

	gtk_widget_show_all (message_dialog->dialog);
}

