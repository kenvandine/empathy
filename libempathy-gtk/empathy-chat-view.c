/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Collabora Ltd.
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

#include <telepathy-glib/util.h>

#include "empathy-chat-view.h"
#include "empathy-smiley-manager.h"
#include "empathy-ui-utils.h"

static void chat_view_base_init (gpointer klass);

GType
empathy_chat_view_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo type_info = {
			sizeof (EmpathyChatViewIface),
			chat_view_base_init,
			NULL,
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EmpathyChatView",
					       &type_info, 0);
		
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}
	
	return type;
}

static void
chat_view_base_init (gpointer klass)
{
	static gboolean initialized = FALSE;
	
	if (!initialized) {
		initialized = TRUE;
	}
}

void
empathy_chat_view_scroll_down (EmpathyChatView *view)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->scroll_down) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->scroll_down (view);
	}
}

void
empathy_chat_view_append_message (EmpathyChatView *view,
				  EmpathyMessage  *msg)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->append_message) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->append_message (view, 
									 msg);
	}
}

void
empathy_chat_view_append_event (EmpathyChatView *view,
				const gchar    *str)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->append_event) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->append_event (view,
								       str);
	}
}

void
empathy_chat_view_append_button (EmpathyChatView *view,
				 const gchar    *message,
				 GtkWidget      *button1,
				 GtkWidget      *button2)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->append_button) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->append_button (view, 
									message,
									button1,
									button2);
	}
}

void
empathy_chat_view_scroll (EmpathyChatView *view,
			  gboolean        allow_scrolling)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->scroll) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->scroll (view, 
								 allow_scrolling);
	}
}

gboolean
empathy_chat_view_get_selection_bounds (EmpathyChatView *view,
					GtkTextIter    *start,
					GtkTextIter    *end)
{
	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), FALSE);
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->get_selection_bounds) {
		return EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->get_selection_bounds (view, 
										      start, 
										      end);
	}
	return FALSE;
}

void
empathy_chat_view_clear (EmpathyChatView *view)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->clear) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->clear (view);
	}
}

gboolean
empathy_chat_view_find_previous (EmpathyChatView *view,
				 const gchar    *search_criteria,
				 gboolean        new_search)
{
	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), FALSE);
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->find_previous) {
		return EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->find_previous (view, 
									       search_criteria, 
									       new_search);
	}
	return FALSE;
}

gboolean
empathy_chat_view_find_next (EmpathyChatView *view,
			     const gchar    *search_criteria,
			     gboolean        new_search)
{
	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), FALSE);
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->find_next) {
		return EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->find_next (view, 
									   search_criteria, 
									   new_search);
	}
	return FALSE;
}


void
empathy_chat_view_find_abilities (EmpathyChatView *view,
				  const gchar    *search_criteria,
				  gboolean       *can_do_previous,
				  gboolean       *can_do_next)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->find_abilities) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->find_abilities (view, 
									 search_criteria, 
									 can_do_previous, 
									 can_do_next);
	}
}

void
empathy_chat_view_highlight (EmpathyChatView *view,
			     const gchar     *text)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->highlight) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->highlight (view, text);
	}
}

void
empathy_chat_view_copy_clipboard (EmpathyChatView *view)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->copy_clipboard) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->copy_clipboard (view);
	}
}

EmpathyTheme *
empathy_chat_view_get_theme (EmpathyChatView *view)
{
	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), NULL);
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->get_theme) {
		return EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->get_theme (view);
	}
	return NULL;
}

void
empathy_chat_view_set_theme (EmpathyChatView *view, EmpathyTheme *theme)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->set_theme) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->set_theme (view, theme);
	}
}

void
empathy_chat_view_set_margin (EmpathyChatView *view,
			      gint            margin)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->set_margin) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->set_margin (view, margin);
	}
}

time_t
empathy_chat_view_get_last_timestamp (EmpathyChatView *view)
{
	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), 0);
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->get_last_timestamp) {
		return EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->get_last_timestamp (view);
	}
	return 0;
}

void
empathy_chat_view_set_last_timestamp (EmpathyChatView *view,
				      time_t          timestamp)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->set_last_timestamp) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->set_last_timestamp (view, timestamp);
	}
}

EmpathyContact *
empathy_chat_view_get_last_contact (EmpathyChatView *view)
{
	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), NULL);
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->get_last_contact) {
		return EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->get_last_contact (view);
	}
	return NULL;
}

/* Pads a pixbuf to the specified size, by centering it in a larger transparent
 * pixbuf. Returns a new ref.
 */
static GdkPixbuf *
chat_view_pad_to_size (GdkPixbuf *pixbuf,
		       gint       width,
		       gint       height,
		       gint       extra_padding_right)
{
	gint       src_width, src_height;
	GdkPixbuf *padded;
	gint       x_offset, y_offset;

	src_width = gdk_pixbuf_get_width (pixbuf);
	src_height = gdk_pixbuf_get_height (pixbuf);

	x_offset = (width - src_width) / 2;
	y_offset = (height - src_height) / 2;

	padded = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (pixbuf),
				 TRUE, /* alpha */
				 gdk_pixbuf_get_bits_per_sample (pixbuf),
				 width + extra_padding_right,
				 height);

	gdk_pixbuf_fill (padded, 0);

	gdk_pixbuf_copy_area (pixbuf,
			      0, /* source coords */
			      0,
			      src_width,
			      src_height,
			      padded,
			      x_offset, /* dest coords */
			      y_offset);

	return padded;
}

typedef struct {
	GdkPixbuf *pixbuf;
	gchar     *token;
} AvatarData;

static void
chat_view_avatar_cache_data_free (gpointer ptr)
{
	AvatarData *data = ptr;

	g_object_unref (data->pixbuf);
	g_free (data->token);
	g_slice_free (AvatarData, data);
}

GdkPixbuf *
empathy_chat_view_get_avatar_pixbuf_with_cache (EmpathyContact *contact)
{
	AvatarData        *data;
	EmpathyAvatar     *avatar;
	GdkPixbuf         *tmp_pixbuf;
	GdkPixbuf         *pixbuf = NULL;

	/* Check if avatar is in cache and if it's up to date */
	avatar = empathy_contact_get_avatar (contact);
	data = g_object_get_data (G_OBJECT (contact), "chat-view-avatar-cache");
	if (data) {
		if (avatar && !tp_strdiff (avatar->token, data->token)) {
			/* We have the avatar in cache */
			return data->pixbuf;
		}
	}

	/* Avatar not in cache, create pixbuf */
	tmp_pixbuf = empathy_pixbuf_avatar_from_contact_scaled (contact, 32, 32);
	if (tmp_pixbuf) {
		pixbuf = chat_view_pad_to_size (tmp_pixbuf, 32, 32, 6);
		g_object_unref (tmp_pixbuf);
	}
	if (!pixbuf) {
		return NULL;
	}

	/* Insert new pixbuf in cache */
	data = g_slice_new0 (AvatarData);
	data->token = g_strdup (avatar->token);
	data->pixbuf = pixbuf;

	g_object_set_data_full (G_OBJECT (contact), "chat-view-avatar-cache",
				data, chat_view_avatar_cache_data_free);

	return data->pixbuf;
}

GtkWidget *
empathy_chat_view_get_smiley_menu (GCallback    callback,
				   gpointer     user_data)
{
	EmpathySmileyManager *smiley_manager;
	GSList               *smileys, *l;
	GtkWidget            *menu;
	gint                  x = 0;
	gint                  y = 0;

	g_return_val_if_fail (callback != NULL, NULL);

	menu = gtk_menu_new ();

	smiley_manager = empathy_smiley_manager_new ();
	smileys = empathy_smiley_manager_get_all (smiley_manager);
	for (l = smileys; l; l = l->next) {
		EmpathySmiley *smiley;
		GtkWidget     *item;
		GtkWidget     *image;

		smiley = l->data;
		image = gtk_image_new_from_pixbuf (smiley->pixbuf);

		item = gtk_image_menu_item_new_with_label ("");
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

		gtk_menu_attach (GTK_MENU (menu), item,
				 x, x + 1, y, y + 1);

		gtk_widget_set_tooltip_text (item, smiley->str);

		g_object_set_data  (G_OBJECT (item), "smiley_text", smiley->str);
		g_signal_connect (item, "activate", callback, user_data);

		if (x > 3) {
			y++;
			x = 0;
		} else {
			x++;
		}
	}
	g_object_unref (smiley_manager);

	gtk_widget_show_all (menu);

	return menu;
}

