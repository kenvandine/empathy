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
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <libempathy/empathy-utils.h>
#include "empathy-avatar-image.h"
#include "empathy-ui-utils.h"

#define MAX_SMALL 64
#define MAX_LARGE 400

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyAvatarImage)
typedef struct {
	GtkWidget   *image;
	GtkWidget   *popup;
	GdkPixbuf   *pixbuf;
} EmpathyAvatarImagePriv;

static void     avatar_image_finalize                (GObject           *object);
static void     avatar_image_add_filter              (EmpathyAvatarImage *avatar_image);
static void     avatar_image_remove_filter           (EmpathyAvatarImage *avatar_image);
static gboolean avatar_image_button_press_event      (GtkWidget         *widget,
						      GdkEventButton    *event);
static gboolean avatar_image_button_release_event    (GtkWidget         *widget,
						      GdkEventButton    *event);

G_DEFINE_TYPE (EmpathyAvatarImage, empathy_avatar_image, GTK_TYPE_EVENT_BOX);

static void
empathy_avatar_image_class_init (EmpathyAvatarImageClass *klass)
{
	GObjectClass   *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = avatar_image_finalize;

	widget_class->button_press_event   = avatar_image_button_press_event;
	widget_class->button_release_event = avatar_image_button_release_event;

	g_type_class_add_private (object_class, sizeof (EmpathyAvatarImagePriv));
}

static void
empathy_avatar_image_init (EmpathyAvatarImage *avatar_image)
{
	EmpathyAvatarImagePriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (avatar_image,
		EMPATHY_TYPE_AVATAR_IMAGE, EmpathyAvatarImagePriv);

	avatar_image->priv = priv;
	priv->image = gtk_image_new ();
	gtk_container_add (GTK_CONTAINER (avatar_image), priv->image);
	empathy_avatar_image_set (avatar_image, NULL);
	gtk_widget_show (priv->image);

	avatar_image_add_filter (avatar_image);
}

static void
avatar_image_finalize (GObject *object)
{
	EmpathyAvatarImagePriv *priv;

	priv = GET_PRIV (object);

	avatar_image_remove_filter (EMPATHY_AVATAR_IMAGE (object));

	if (priv->popup) {
		gtk_widget_destroy (priv->popup);
	}

	if (priv->pixbuf) {
		g_object_unref (priv->pixbuf);
	}

	G_OBJECT_CLASS (empathy_avatar_image_parent_class)->finalize (object);
}

static GdkFilterReturn
avatar_image_filter_func (GdkXEvent  *gdkxevent,
			  GdkEvent   *event,
			  gpointer    data)
{
	XEvent                *xevent = gdkxevent;
	Atom                   atom;
	EmpathyAvatarImagePriv *priv;

	priv = GET_PRIV (data);

	switch (xevent->type) {
	case PropertyNotify:
		atom = gdk_x11_get_xatom_by_name ("_NET_CURRENT_DESKTOP");
		if (xevent->xproperty.atom == atom) {
			if (priv->popup) {
				gtk_widget_destroy (priv->popup);
				priv->popup = NULL;
			}
		}
		break;
	}

	return GDK_FILTER_CONTINUE;
}

static void
avatar_image_add_filter (EmpathyAvatarImage *avatar_image)
{
	Window     window;
	GdkWindow *gdkwindow;
	gint       mask;

	mask = PropertyChangeMask;

	window = GDK_ROOT_WINDOW ();
	gdkwindow = gdk_xid_table_lookup (window);

	gdk_error_trap_push ();
	if (gdkwindow) {
		XWindowAttributes attrs;
		XGetWindowAttributes (gdk_display, window, &attrs);
		mask |= attrs.your_event_mask;
	}

	XSelectInput (gdk_display, window, mask);

	gdk_error_trap_pop ();

	gdk_window_add_filter (NULL, avatar_image_filter_func, avatar_image);
}

static void
avatar_image_remove_filter (EmpathyAvatarImage *avatar_image)
{
	gdk_window_remove_filter (NULL, avatar_image_filter_func, avatar_image);
}

static gboolean
avatar_image_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	EmpathyAvatarImagePriv *priv;
	GtkWidget             *popup;
	GtkWidget             *frame;
	GtkWidget             *image;
	gint                   x, y;
	gint                   popup_width, popup_height;
	gint                   width, height;
	GdkPixbuf             *pixbuf;

	priv = GET_PRIV (widget);

	if (priv->popup) {
		gtk_widget_destroy (priv->popup);
		priv->popup = NULL;
	}

	if (event->button != 1 || event->type != GDK_BUTTON_PRESS || !priv->pixbuf) {
		return FALSE;
	}

	popup_width = gdk_pixbuf_get_width (priv->pixbuf);
	popup_height = gdk_pixbuf_get_height (priv->pixbuf);

	width = priv->image->allocation.width;
	height = priv->image->allocation.height;

	/* Don't show a popup if the popup is smaller then the currently avatar
	 * image.
	 */
	if (popup_height <= height && popup_width <= width) {
		return TRUE;
	}

	pixbuf = empathy_pixbuf_scale_down_if_necessary (priv->pixbuf, MAX_LARGE);
	popup_width = gdk_pixbuf_get_width (pixbuf);
	popup_height = gdk_pixbuf_get_height (pixbuf);

	popup = gtk_window_new (GTK_WINDOW_POPUP);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);

	gtk_container_add (GTK_CONTAINER (popup), frame);

	image = gtk_image_new ();
	gtk_container_add (GTK_CONTAINER (frame), image);

	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
	g_object_unref (pixbuf);

	gdk_window_get_origin (priv->image->window, &x, &y);

	x = x - (popup_width - width) / 2;
	y = y - (popup_height - height) / 2;

	gtk_window_move (GTK_WINDOW (popup), x, y);

	priv->popup = popup;

	gtk_widget_show_all (popup);

	return TRUE;
}

static gboolean
avatar_image_button_release_event (GtkWidget *widget, GdkEventButton *event)
{
	EmpathyAvatarImagePriv *priv;

	priv = GET_PRIV (widget);

	if (event->button != 1 || event->type != GDK_BUTTON_RELEASE) {
		return FALSE;
	}

	if (!priv->popup) {
		return TRUE;
	}

	gtk_widget_destroy (priv->popup);
	priv->popup = NULL;

	return TRUE;
}

GtkWidget *
empathy_avatar_image_new (void)
{
	EmpathyAvatarImage *avatar_image;

	avatar_image = g_object_new (EMPATHY_TYPE_AVATAR_IMAGE, NULL);

	return GTK_WIDGET (avatar_image);
}

void
empathy_avatar_image_set (EmpathyAvatarImage *avatar_image,
			  EmpathyAvatar      *avatar)
{
	EmpathyAvatarImagePriv *priv = GET_PRIV (avatar_image);
	GdkPixbuf              *scaled_pixbuf;

	g_return_if_fail (EMPATHY_IS_AVATAR_IMAGE (avatar_image));

	if (priv->pixbuf) {
		g_object_unref (priv->pixbuf);
		priv->pixbuf = NULL;
	}

	if (avatar) {
		priv->pixbuf = empathy_pixbuf_from_data (avatar->data, avatar->len);
	}

	if (!priv->pixbuf) {
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      "stock_person",
					      GTK_ICON_SIZE_DIALOG);
		return;
	}

	scaled_pixbuf = empathy_pixbuf_scale_down_if_necessary (priv->pixbuf, MAX_SMALL);
	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->image), scaled_pixbuf);

	if (scaled_pixbuf != priv->pixbuf) {
		gtk_widget_set_tooltip_text (GTK_WIDGET (avatar_image),
					     _("Click to enlarge"));
	} else {
		gtk_widget_set_tooltip_text (GTK_WIDGET (avatar_image),
					     NULL);
	}

	g_object_unref (scaled_pixbuf);
}

