/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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

#include <config.h>

#include <string.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <telepathy-glib/util.h>

#include <libempathy/empathy-utils.h>
#include "empathy-theme-boxes.h"
#include "empathy-ui-utils.h"
#include "empathy-conf.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

#define MARGIN 4
#define HEADER_PADDING 2

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyThemeBoxes)
typedef struct {
	gboolean show_avatars;
	guint    notify_show_avatars_id;
} EmpathyThemeBoxesPriv;

G_DEFINE_TYPE (EmpathyThemeBoxes, empathy_theme_boxes, EMPATHY_TYPE_CHAT_TEXT_VIEW);

static void
theme_boxes_create_tags (EmpathyThemeBoxes *theme)
{
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (theme));

	gtk_text_buffer_create_tag (buffer, EMPATHY_THEME_BOXES_TAG_HEADER,
				    "pixels-above-lines", HEADER_PADDING,
				    "pixels-below-lines", HEADER_PADDING,
				    NULL);

	gtk_text_buffer_create_tag (buffer, EMPATHY_THEME_BOXES_TAG_HEADER_LINE, NULL);
}

/* Pads a pixbuf to the specified size, by centering it in a larger transparent
 * pixbuf. Returns a new ref.
 */
static GdkPixbuf *
theme_boxes_pad_to_size (GdkPixbuf *pixbuf,
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
theme_boxes_avatar_cache_data_free (gpointer ptr)
{
	AvatarData *data = ptr;

	g_object_unref (data->pixbuf);
	g_free (data->token);
	g_slice_free (AvatarData, data);
}

static GdkPixbuf *
theme_boxes_get_avatar_pixbuf_with_cache (EmpathyContact *contact)
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
		pixbuf = theme_boxes_pad_to_size (tmp_pixbuf, 32, 32, 6);
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
				data, theme_boxes_avatar_cache_data_free);

	return data->pixbuf;
}

static void
table_size_allocate_cb (GtkWidget     *view,
			GtkAllocation *allocation,
			GtkWidget     *box)
{
	gint width, height;

        gtk_widget_get_size_request (box, NULL, &height);

	width = allocation->width;
	
	width -= \
		gtk_text_view_get_right_margin (GTK_TEXT_VIEW (view)) - \
		gtk_text_view_get_left_margin (GTK_TEXT_VIEW (view));
	width -= 2 * MARGIN;
	width -= 2 * HEADER_PADDING;

        gtk_widget_set_size_request (box, width, height);
}

static void
theme_boxes_maybe_append_header (EmpathyThemeBoxes *theme,
				 EmpathyMessage    *msg)
{
	EmpathyChatTextView  *view = EMPATHY_CHAT_TEXT_VIEW (theme);
	EmpathyThemeBoxesPriv*priv = GET_PRIV (theme);
	EmpathyContact       *contact;
	EmpathyContact       *last_contact;
	GdkPixbuf            *avatar = NULL;
	GtkTextBuffer        *buffer;
	const gchar          *name;
	GtkTextIter           iter;
	GtkWidget            *label1, *label2;
	GtkTextChildAnchor   *anchor;
	GtkWidget            *box;
	gchar                *str;
	time_t                time;
	gchar                *tmp;
	GtkTextIter           start;
	gboolean              color_set;
	GtkTextTagTable      *table;
	GtkTextTag           *tag;

	contact = empathy_message_get_sender (msg);
	name = empathy_contact_get_name (contact);
	last_contact = empathy_chat_text_view_get_last_contact (view);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (theme));

	DEBUG ("Maybe add fancy header");

	/* Only insert a header if the previously inserted block is not the same
	 * as this one. This catches all the different cases:
	 */
	if (last_contact && empathy_contact_equal (last_contact, contact)) {
		return;
	}

	empathy_chat_text_view_append_spacing (view);

	/* Insert header line */
	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  "\n",
						  -1,
						  EMPATHY_THEME_BOXES_TAG_HEADER_LINE,
						  NULL);

	gtk_text_buffer_get_end_iter (buffer, &iter);
	anchor = gtk_text_buffer_create_child_anchor (buffer, &iter);

	/* Create a hbox for the header and resize it when the view allocation
	 * changes */
	box = gtk_hbox_new (FALSE, 0);
	g_signal_connect_object (view, "size-allocate",
				 G_CALLBACK (table_size_allocate_cb),
				 box, 0);

	/* Add avatar to the box if needed */
	if (priv->show_avatars) {
		avatar = theme_boxes_get_avatar_pixbuf_with_cache (contact);
		if (avatar) {
			GtkWidget *image;

			image = gtk_image_new_from_pixbuf (avatar);

			gtk_box_pack_start (GTK_BOX (box), image,
					    FALSE, TRUE, 2);
		}
	}

	/* Add contact alias */
	str = g_markup_printf_escaped ("<b>%s</b>", name);
	label1 = g_object_new (GTK_TYPE_LABEL,
			       "label", str,
			       "use-markup", TRUE,
			       "xalign", 0.0,
			       NULL);
	g_free (str);

	/* Add the message receive time */
	time = empathy_message_get_timestamp (msg);
	tmp = empathy_time_to_string_local (time, 
					   EMPATHY_TIME_FORMAT_DISPLAY_SHORT);
	str = g_strdup_printf ("<i>%s</i>", tmp);
	label2 = g_object_new (GTK_TYPE_LABEL,
			       "label", str,
			       "use-markup", TRUE,
			       "xalign", 1.0,
			       NULL);
	g_free (tmp);
	g_free (str);
	
	/* Set foreground color of labels to the same color than the header tag. */
	table = gtk_text_buffer_get_tag_table (buffer);
	tag = gtk_text_tag_table_lookup (table, EMPATHY_THEME_BOXES_TAG_HEADER);
	g_object_get (tag, "foreground-set", &color_set, NULL);
	if (color_set) {
		GdkColor *color;

		g_object_get (tag, "foreground-gdk", &color, NULL);
		gtk_widget_modify_fg (label1, GTK_STATE_NORMAL, color);
		gtk_widget_modify_fg (label2, GTK_STATE_NORMAL, color);
		gdk_color_free (color);
	}

	/* Pack labels into the box */
	gtk_misc_set_alignment (GTK_MISC (label1), 0.0, 0.5);
	gtk_misc_set_alignment (GTK_MISC (label2), 1.0, 0.5);
	gtk_box_pack_start (GTK_BOX (box), label1, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (box), label2, TRUE, TRUE, 0);

	/* Add the header box to the text view */
	gtk_text_view_add_child_at_anchor (GTK_TEXT_VIEW (view),
					   box,
					   anchor);
	gtk_widget_show_all (box);

	/* Insert a header line */
	gtk_text_buffer_get_end_iter (buffer, &iter);
	start = iter;
	gtk_text_iter_backward_char (&start);
	gtk_text_buffer_apply_tag_by_name (buffer,
					   EMPATHY_THEME_BOXES_TAG_HEADER,
					   &start, &iter);
	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  "\n",
						  -1,
						  EMPATHY_THEME_BOXES_TAG_HEADER,
						  NULL);
	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  "\n",
						  -1,
						  EMPATHY_THEME_BOXES_TAG_HEADER_LINE,
						  NULL);
}

static void
theme_boxes_append_message (EmpathyChatTextView *view,
			    EmpathyMessage      *message)
{
	EmpathyContact *sender;

	theme_boxes_maybe_append_header (EMPATHY_THEME_BOXES (view), message);

	sender = empathy_message_get_sender (message);
	if (empathy_message_get_tptype (message) ==
	    TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION) {
		gchar *body;

		body = g_strdup_printf (" * %s %s", 
					empathy_contact_get_name (sender),
					empathy_message_get_body (message));
		empathy_chat_text_view_append_body (EMPATHY_CHAT_TEXT_VIEW (view),
						    body,
						    EMPATHY_CHAT_TEXT_VIEW_TAG_ACTION);
	} else {
		empathy_chat_text_view_append_body (EMPATHY_CHAT_TEXT_VIEW (view),
						    empathy_message_get_body (message),
						    EMPATHY_CHAT_TEXT_VIEW_TAG_BODY);
	}
}

static void
theme_boxes_notify_show_avatars_cb (EmpathyConf *conf,
				    const gchar *key,
				    gpointer     user_data)
{
	EmpathyThemeBoxesPriv *priv = GET_PRIV (user_data);
	
	empathy_conf_get_bool (conf, key, &priv->show_avatars);
}

static void
theme_boxes_finalize (GObject *object)
{
	EmpathyThemeBoxesPriv *priv = GET_PRIV (object);

	empathy_conf_notify_remove (empathy_conf_get (),
				    priv->notify_show_avatars_id);

	G_OBJECT_CLASS (empathy_theme_boxes_parent_class)->finalize (object);
}

static void
empathy_theme_boxes_class_init (EmpathyThemeBoxesClass *class)
{
	GObjectClass             *object_class;
	EmpathyChatTextViewClass *chat_text_view_class;

	object_class = G_OBJECT_CLASS (class);
	chat_text_view_class  = EMPATHY_CHAT_TEXT_VIEW_CLASS (class);

	object_class->finalize = theme_boxes_finalize;
	chat_text_view_class->append_message = theme_boxes_append_message;

	g_type_class_add_private (object_class, sizeof (EmpathyThemeBoxesPriv));
}

static void
empathy_theme_boxes_init (EmpathyThemeBoxes *theme)
{
	EmpathyThemeBoxesPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (theme,
		EMPATHY_TYPE_THEME_BOXES, EmpathyThemeBoxesPriv);

	theme->priv = priv;

	theme_boxes_create_tags (theme);

	priv->notify_show_avatars_id =
		empathy_conf_notify_add (empathy_conf_get (),
					 EMPATHY_PREFS_UI_SHOW_AVATARS,
					 theme_boxes_notify_show_avatars_cb,
					 theme);

	empathy_conf_get_bool (empathy_conf_get (),
			       EMPATHY_PREFS_UI_SHOW_AVATARS,
			       &priv->show_avatars);

	/* Define margin */
	g_object_set (theme,
		      "left-margin", MARGIN,
		      "right-margin", MARGIN,
		      NULL);
}

EmpathyThemeBoxes *
empathy_theme_boxes_new (void)
{
	return g_object_new (EMPATHY_TYPE_THEME_BOXES,
			     "only-if-date", TRUE,
			     NULL);
}

