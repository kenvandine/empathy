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
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libempathy/empathy-idle.h>
#include <libempathy/gossip-utils.h>
#include <libempathy/gossip-debug.h>
#include <libempathy/empathy-marshal.h>

#include "gossip-ui-utils.h"
#include "empathy-images.h"
#include "gossip-presence-chooser.h"
#include "gossip-status-presets.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GOSSIP_TYPE_PRESENCE_CHOOSER, GossipPresenceChooserPriv))

#define DEBUG_DOMAIN "PresenceChooser"

/* Flashing delay for icons (milliseconds). */
#define FLASH_TIMEOUT 500

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
} GossipPresenceChooserPriv;

typedef struct {
	McPresence   state;
	const gchar *status;
} StateAndStatus;

/* States to be listed in the menu */
static McPresence states[] = {MC_PRESENCE_AVAILABLE,
			      MC_PRESENCE_DO_NOT_DISTURB,
			      MC_PRESENCE_AWAY};

static void            gossip_presence_chooser_class_init      (GossipPresenceChooserClass *klass);
static void            gossip_presence_chooser_init            (GossipPresenceChooser      *chooser);
static void            presence_chooser_finalize               (GObject                    *object);
static void            presence_chooser_presence_changed_cb    (GossipPresenceChooser      *chooser);
static void            presence_chooser_reset_scroll_timeout   (GossipPresenceChooser      *chooser);
static gboolean        presence_chooser_scroll_timeout_cb      (GossipPresenceChooser      *chooser);
static gboolean        presence_chooser_scroll_event_cb        (GossipPresenceChooser      *chooser,
								GdkEventScroll             *event,
								gpointer                    user_data);
static GList *         presence_chooser_get_presets            (GossipPresenceChooser      *chooser);
static StateAndStatus *presence_chooser_state_and_status_new   (McPresence                  state,
								const gchar                *status);
static gboolean        presence_chooser_flash_timeout_cb       (GossipPresenceChooser      *chooser);
static void            presence_chooser_flash_start            (GossipPresenceChooser      *chooser,
								McPresence                  state_1,
								McPresence                  state_2);
static void            presence_chooser_flash_stop             (GossipPresenceChooser      *chooser,
								McPresence                  state);
static gboolean        presence_chooser_button_press_event_cb  (GtkWidget                  *chooser,
								GdkEventButton             *event,
								gpointer                    user_data);
static void            presence_chooser_toggled_cb             (GtkWidget                  *chooser,
								gpointer                    user_data);
static void            presence_chooser_menu_popup             (GossipPresenceChooser      *chooser);
static void            presence_chooser_menu_popdown           (GossipPresenceChooser      *chooser);
static void            presence_chooser_menu_selection_done_cb (GtkMenuShell               *menushell,
								GossipPresenceChooser      *chooser);
static void            presence_chooser_menu_destroy_cb        (GtkWidget                  *menu,
								GossipPresenceChooser      *chooser);
static void            presence_chooser_menu_detach            (GtkWidget                  *attach_widget,
								GtkMenu                    *menu);
static void            presence_chooser_menu_align_func        (GtkMenu                    *menu,
								gint                       *x,
								gint                       *y,
								gboolean                   *push_in,
								GtkWidget                  *widget);
static void            presence_chooser_menu_add_item          (GtkWidget                  *menu,
								const gchar                *str,
								McPresence                  state,
								gboolean                    custom);
static void            presence_chooser_clear_activate_cb      (GtkWidget                  *item,
								gpointer                    user_data);
static void            presence_chooser_clear_response_cb      (GtkWidget                  *widget,
								gint                        response,
								gpointer                    user_data);
static void            presence_chooser_noncustom_activate_cb  (GtkWidget                  *item,
								gpointer                    user_data);
static void            presence_chooser_set_state              (McPresence                  state,
								const gchar                *status,
								gboolean                    save);
static void            presence_chooser_custom_activate_cb     (GtkWidget                  *item,
								gpointer                    user_data);
static void            presence_chooser_show_dialog            (McPresence                  state);
static void            presence_chooser_dialog_response_cb     (GtkWidget                  *dialog,
								gint                        response,
								gpointer                    user_data);

G_DEFINE_TYPE (GossipPresenceChooser, gossip_presence_chooser, GTK_TYPE_TOGGLE_BUTTON);

static void
gossip_presence_chooser_class_init (GossipPresenceChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = presence_chooser_finalize;

	g_type_class_add_private (object_class, sizeof (GossipPresenceChooserPriv));
}

static void
gossip_presence_chooser_init (GossipPresenceChooser *chooser)
{
	GossipPresenceChooserPriv *priv;
	GtkWidget                 *arrow;
	GtkWidget                 *alignment;

	priv = GET_PRIV (chooser);

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

	priv->idle = empathy_idle_new ();
	presence_chooser_presence_changed_cb (chooser);
	g_signal_connect_swapped (priv->idle, "notify",
				  G_CALLBACK (presence_chooser_presence_changed_cb),
				  chooser);
}

static void
presence_chooser_finalize (GObject *object)
{
	GossipPresenceChooserPriv *priv;

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

	G_OBJECT_CLASS (gossip_presence_chooser_parent_class)->finalize (object);
}

GtkWidget *
gossip_presence_chooser_new (void)
{
	GtkWidget *chooser;

	chooser = g_object_new (GOSSIP_TYPE_PRESENCE_CHOOSER, NULL);

	return chooser;
}

static void
presence_chooser_presence_changed_cb (GossipPresenceChooser *chooser)
{
	GossipPresenceChooserPriv *priv;
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
presence_chooser_reset_scroll_timeout (GossipPresenceChooser *chooser)
{
	GossipPresenceChooserPriv *priv;

	priv = GET_PRIV (chooser);

	if (priv->scroll_timeout_id) {
		g_source_remove (priv->scroll_timeout_id);
		priv->scroll_timeout_id = 0;
	}

	g_free (priv->scroll_status);
	priv->scroll_status = NULL;
}

static gboolean
presence_chooser_scroll_timeout_cb (GossipPresenceChooser *chooser)
{
	GossipPresenceChooserPriv *priv;

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
presence_chooser_scroll_event_cb (GossipPresenceChooser *chooser,
				  GdkEventScroll        *event,
				  gpointer               user_data)
{
	GossipPresenceChooserPriv *priv;
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
			g_timeout_add (500,
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
		status = gossip_presence_state_get_default_status (states[0]);

		presence_chooser_reset_scroll_timeout (chooser);
		empathy_idle_set_presence (priv->idle, states[0], status);
	}

	g_list_foreach (list, (GFunc) g_free, NULL);
	g_list_free (list);

	return TRUE;
}

static GList *
presence_chooser_get_presets (GossipPresenceChooser *chooser)
{
	GList      *list = NULL;
	guint       i;

	for (i = 0; i < G_N_ELEMENTS (states); i++) {
		GList          *presets, *p;
		StateAndStatus *sas;
		const gchar    *status;

		status = gossip_presence_state_get_default_status (states[i]);
		sas = presence_chooser_state_and_status_new (states[i], status);
		list = g_list_append (list, sas);
	
		presets = gossip_status_presets_get (states[i], 5);
		for (p = presets; p; p = p->next) {
			sas = presence_chooser_state_and_status_new (states[i], p->data);
			list = g_list_append (list, sas);
		}
		g_list_free (presets);
	}

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
presence_chooser_flash_timeout_cb (GossipPresenceChooser *chooser)
{
	GossipPresenceChooserPriv *priv;
	McPresence                 state;
	static gboolean            on = FALSE;

	priv = GET_PRIV (chooser);

	if (on) {
		state = priv->flash_state_1;
	} else {
		state = priv->flash_state_2;
	}

	gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
				      gossip_icon_name_for_presence_state (state),
				      GTK_ICON_SIZE_MENU);

	on = !on;

	return TRUE;
}

static void
presence_chooser_flash_start (GossipPresenceChooser *chooser,
			      McPresence             state_1,
			      McPresence             state_2)
{
	GossipPresenceChooserPriv *priv;

	g_return_if_fail (GOSSIP_IS_PRESENCE_CHOOSER (chooser));

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
presence_chooser_flash_stop (GossipPresenceChooser *chooser,
			     McPresence             state)
{
	GossipPresenceChooserPriv *priv;

	g_return_if_fail (GOSSIP_IS_PRESENCE_CHOOSER (chooser));

	priv = GET_PRIV (chooser);

	if (priv->flash_timeout_id) {
		g_source_remove (priv->flash_timeout_id);
		priv->flash_timeout_id = 0;
	}

	gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
				      gossip_icon_name_for_presence_state (state),
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
		presence_chooser_menu_popup (GOSSIP_PRESENCE_CHOOSER (chooser));
	} else {
		presence_chooser_menu_popdown (GOSSIP_PRESENCE_CHOOSER (chooser));
	}
}

static void
presence_chooser_menu_popup (GossipPresenceChooser *chooser)
{
	GossipPresenceChooserPriv *priv;
	GtkWidget                 *menu;

	priv = GET_PRIV (chooser);

	if (priv->menu) {
		return;
	}

	menu = gossip_presence_chooser_create_menu ();

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
presence_chooser_menu_popdown (GossipPresenceChooser *chooser)
{
	GossipPresenceChooserPriv *priv;

	priv = GET_PRIV (chooser);

	if (priv->menu) {
		gtk_widget_destroy (priv->menu);
	}
}

static void
presence_chooser_menu_selection_done_cb (GtkMenuShell          *menushell,
					 GossipPresenceChooser *chooser)
{
	gtk_widget_destroy (GTK_WIDGET (menushell));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chooser), FALSE);
}

static void
presence_chooser_menu_destroy_cb (GtkWidget             *menu,
				  GossipPresenceChooser *chooser)
{
	GossipPresenceChooserPriv *priv;

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
gossip_presence_chooser_create_menu (void)
{
	const gchar *status;
	GtkWidget   *menu;
	GtkWidget   *item;
	GtkWidget   *image;
	guint        i;

	menu = gtk_menu_new ();

	for (i = 0; i < G_N_ELEMENTS (states); i++) {
		GList       *list, *l;

		status = gossip_presence_state_get_default_status (states[i]);
		presence_chooser_menu_add_item (menu,
						status,
						states[i],
						FALSE);

		list = gossip_status_presets_get (states[i], 5);
		for (l = list; l; l = l->next) {
			presence_chooser_menu_add_item (menu,
							l->data,
							states[i],
							FALSE);
		}
		g_list_free (list);

		presence_chooser_menu_add_item (menu,
						_("Custom message..."),
						states[i],
						TRUE);

		/* Separator. */
		item = gtk_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);
	}

	/* Offline to disconnect */
	status = gossip_presence_state_get_default_status (MC_PRESENCE_OFFLINE);
	presence_chooser_menu_add_item (menu,
					status,
					MC_PRESENCE_OFFLINE,
					FALSE);
	/* Separator. */
	item = gtk_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	/* Clear list */
	item = gtk_image_menu_item_new_with_label (_("Clear List..."));
	image = gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (image);
	gtk_widget_show (item);

	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (presence_chooser_clear_activate_cb),
			  NULL);

	return menu;
}

static void
presence_chooser_menu_add_item (GtkWidget   *menu,
				const gchar *str,
				McPresence   state,
				gboolean     custom)
{
	GtkWidget   *item;
	GtkWidget   *image;
	const gchar *icon_name;

	item = gtk_image_menu_item_new_with_label (str);
	icon_name = gossip_icon_name_for_presence_state (state);

	if (custom) {
		g_signal_connect (item, "activate",
				  G_CALLBACK (presence_chooser_custom_activate_cb),
				  NULL);
	} else {
		g_signal_connect (item, "activate",
				  G_CALLBACK (presence_chooser_noncustom_activate_cb),
				  NULL);
	}

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
presence_chooser_clear_activate_cb (GtkWidget *item,
				    gpointer   user_data)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;
	GtkWindow *parent = NULL;

	toplevel = gtk_widget_get_toplevel (item);
	if (GTK_WIDGET_TOPLEVEL (toplevel) &&
	    GTK_IS_WINDOW (toplevel)) {
		GtkWindow *window;
		gboolean   visible;

		window = GTK_WINDOW (toplevel);
		visible = gossip_window_get_is_visible (window);

		if (visible) {
			parent = window;
		}
	}

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
					 0,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 _("Are you sure you want to clear the list?"));

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (dialog),
		_("This will remove any custom messages you have "
		  "added to the list of preset status messages."));

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				_("Clear List"), GTK_RESPONSE_OK,
				NULL);

	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (presence_chooser_clear_response_cb),
			  NULL);

	gtk_widget_show (dialog);
}

static void
presence_chooser_clear_response_cb (GtkWidget *widget,
				    gint       response,
				    gpointer   user_data)
{
	if (response == GTK_RESPONSE_OK) {
		gossip_status_presets_reset ();
	}

	gtk_widget_destroy (widget);
}

static void
presence_chooser_noncustom_activate_cb (GtkWidget *item,
					gpointer   user_data)
{
	McPresence   state;
	const gchar *status;

	status = g_object_get_data (G_OBJECT (item), "status");
	state = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "state"));

	presence_chooser_set_state (state, status, FALSE);
}

static void
presence_chooser_set_state (McPresence   state,
			    const gchar *status,
			    gboolean     save)
{
	EmpathyIdle *idle;

	if (!G_STR_EMPTY (status)) {
		const gchar *default_status;

		/* Only store the value if it differs from the default ones. */
		default_status = gossip_presence_state_get_default_status (state);
		if (save && strcmp (status, default_status) != 0) {
			gossip_status_presets_set_last (state, status);
		}
	}

	idle = empathy_idle_new ();
	empathy_idle_set_presence (idle, state, status);
	g_object_unref (idle);
}

static void
presence_chooser_custom_activate_cb (GtkWidget *item,
				     gpointer   user_data)
{
	McPresence state;

	state = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "state"));

	presence_chooser_show_dialog (state);
}

static void
presence_chooser_show_dialog (McPresence state)
{
	static GtkWidget    *dialog = NULL;
	static GtkListStore *store[LAST_MC_PRESENCE];
	GladeXML            *glade;
	GtkWidget           *image;
	GtkWidget           *combo;
	GtkWidget           *entry;
	GtkWidget           *checkbutton;
	const gchar         *default_status;

	if (dialog) {
		gtk_widget_destroy (dialog);
		dialog = NULL;
	} else {
		guint i;

		for (i = 0; i < LAST_MC_PRESENCE; i++) {
			store[i] = NULL;
		}
	}

	glade = gossip_glade_get_file ("gossip-presence-chooser.glade",
				       "status_message_dialog",
				       NULL,
				       "status_message_dialog", &dialog,
				       "comboentry_status", &combo,
				       "image_status", &image,
				       "checkbutton_add", &checkbutton,
				       NULL);

	g_object_unref (glade);

	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &dialog);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (presence_chooser_dialog_response_cb),
			  NULL);

	gtk_image_set_from_icon_name (GTK_IMAGE (image),
				      gossip_icon_name_for_presence_state (state),
				      GTK_ICON_SIZE_MENU);

	if (!store[state]) {
		GList       *presets, *l;
		GtkTreeIter  iter;

		store[state] = gtk_list_store_new (1, G_TYPE_STRING);

		presets = gossip_status_presets_get (state, -1);
		for (l = presets; l; l = l->next) {
			gtk_list_store_append (store[state], &iter);
			gtk_list_store_set (store[state], &iter, 0, l->data, -1);
		}

		g_list_free (presets);
	}

	default_status = gossip_presence_state_get_default_status (state);

	entry = GTK_BIN (combo)->child;
	gtk_entry_set_text (GTK_ENTRY (entry), default_status);
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	gtk_entry_set_width_chars (GTK_ENTRY (entry), 25);

	gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (store[state]));
	gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY (combo), 0);

	/* FIXME: Set transian for a window ? */

	g_object_set_data (G_OBJECT (dialog), "store", store[state]);
	g_object_set_data (G_OBJECT (dialog), "entry", entry);
	g_object_set_data (G_OBJECT (dialog), "checkbutton", checkbutton);
	g_object_set_data (G_OBJECT (dialog), "state", GINT_TO_POINTER (state));

	gtk_widget_show_all (dialog);
}

static void
presence_chooser_dialog_response_cb (GtkWidget *dialog,
				     gint       response,
				     gpointer   user_data)
{
	if (response == GTK_RESPONSE_OK) {
		GtkWidget           *entry;
		GtkWidget           *checkbutton;
		GtkListStore        *store;
		GtkTreeModel        *model;
		GtkTreeIter          iter;
		McPresence           state;
		const gchar         *status;
		gboolean             save;
		gboolean             duplicate = FALSE;
		gboolean             has_next;

		entry = g_object_get_data (G_OBJECT (dialog), "entry");
		status = gtk_entry_get_text (GTK_ENTRY (entry));
		store = g_object_get_data (G_OBJECT (dialog), "store");
		model = GTK_TREE_MODEL (store);

		has_next = gtk_tree_model_get_iter_first (model, &iter);
		while (has_next) {
			gchar *str;

			gtk_tree_model_get (model, &iter,
					    0, &str,
					    -1);

			if (strcmp (status, str) == 0) {
				g_free (str);
				duplicate = TRUE;
				break;
			}

			g_free (str);

			has_next = gtk_tree_model_iter_next (model, &iter);
		}

		if (!duplicate) {
			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter, 0, status, -1);
		}

		checkbutton = g_object_get_data (G_OBJECT (dialog), "checkbutton");
		save = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton));
		state = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog), "state"));

		presence_chooser_set_state (state, status, save);
	}

	gtk_widget_destroy (dialog);
}

