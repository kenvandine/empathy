/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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
 */

#include <config.h>

#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libempathy/empathy-debug.h>

#include "empathy-ui-utils.h"
#include "empathy-main-window.h"
#include "empathy-theme-utils.h"
#include "empathy-theme-boxes.h"

#define DEBUG_DOMAIN "FancyTheme"

#define MARGIN 4
#define HEADER_PADDING 2

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EMPATHY_TYPE_THEME_BOXES, EmpathyThemeBoxesPriv))

typedef struct _EmpathyThemeBoxesPriv EmpathyThemeBoxesPriv;

struct _EmpathyThemeBoxesPriv {
	gchar *header_foreground;
	gchar *header_background;
	gchar *header_line_background;
	gchar *text_foreground;
	gchar *text_background;
	gchar *action_foreground;
	gchar *highlight_foreground;
	gchar *time_foreground;
	gchar *event_foreground;
	gchar *invite_foreground;
	gchar *link_foreground;
};

static void     theme_boxes_finalize          (GObject            *object);
static void     theme_boxes_get_property      (GObject            *object,
					       guint               param_id,
					       GValue             *value,
					       GParamSpec         *pspec);
static void     theme_boxes_set_property      (GObject            *object,
					       guint               param_id,
					       const GValue       *value,
					       GParamSpec         *pspec);
static void     theme_boxes_define_theme_tags (EmpathyTheme        *theme,
					       EmpathyChatView     *view);
static EmpathyThemeContext *
theme_boxes_setup_with_view                   (EmpathyTheme        *theme,
					       EmpathyChatView     *view);
static void     theme_boxes_detach_from_view  (EmpathyTheme        *theme,
					       EmpathyThemeContext *context,
					       EmpathyChatView     *view);
static void     theme_boxes_view_cleared      (EmpathyTheme        *theme,
					       EmpathyThemeContext *context,
					       EmpathyChatView     *view);

static void     theme_boxes_append_message    (EmpathyTheme        *theme,
					       EmpathyThemeContext *context,
					       EmpathyChatView     *view,
					       EmpathyMessage      *message);
static void     theme_boxes_append_event      (EmpathyTheme        *theme,
					       EmpathyThemeContext *context,
					       EmpathyChatView     *view,
					       const gchar        *str);
static void     theme_boxes_append_timestamp  (EmpathyTheme        *theme,
					       EmpathyThemeContext *context,
					       EmpathyChatView     *view,
					       EmpathyMessage      *message,
					       gboolean            show_date,
					       gboolean            show_time);
static void     theme_boxes_append_spacing    (EmpathyTheme        *theme,
					       EmpathyThemeContext *context,
					       EmpathyChatView     *view);

enum {
	PROP_0,
	PROP_HEADER_FOREGROUND,
	PROP_HEADER_BACKGROUND,
	PROP_HEADER_LINE_BACKGROUND,
	PROP_TEXT_FOREGROUND,
	PROP_TEXT_BACKGROUND,
	PROP_ACTION_FOREGROUND,
	PROP_HIGHLIGHT_FOREGROUND,
	PROP_TIME_FOREGROUND,
	PROP_EVENT_FOREGROUND,
	PROP_INVITE_FOREGROUND,
	PROP_LINK_FOREGROUND
};

enum {
	PROP_FLOP,
	PROP_MY_PROP
};

G_DEFINE_TYPE (EmpathyThemeBoxes, empathy_theme_boxes, EMPATHY_TYPE_THEME);

static void
empathy_theme_boxes_class_init (EmpathyThemeBoxesClass *class)
{
	GObjectClass     *object_class;
	EmpathyThemeClass *theme_class;

	object_class = G_OBJECT_CLASS (class);
	theme_class  = EMPATHY_THEME_CLASS (class);

	object_class->finalize       = theme_boxes_finalize;
	object_class->get_property   = theme_boxes_get_property;
	object_class->set_property   = theme_boxes_set_property;

	theme_class->setup_with_view  = theme_boxes_setup_with_view;
	theme_class->detach_from_view = theme_boxes_detach_from_view;
	theme_class->view_cleared     = theme_boxes_view_cleared;
	theme_class->append_message   = theme_boxes_append_message;
	theme_class->append_event     = theme_boxes_append_event;
	theme_class->append_timestamp = theme_boxes_append_timestamp;
	theme_class->append_spacing   = theme_boxes_append_spacing;

	g_object_class_install_property (object_class,
					 PROP_HEADER_FOREGROUND,
					 g_param_spec_string ("header-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_HEADER_BACKGROUND,
					 g_param_spec_string ("header-background",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_HEADER_LINE_BACKGROUND,
					 g_param_spec_string ("header-line-background",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));


	g_object_class_install_property (object_class,
					 PROP_TEXT_FOREGROUND,
					 g_param_spec_string ("text-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_TEXT_BACKGROUND,
					 g_param_spec_string ("text-background",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_ACTION_FOREGROUND,
					 g_param_spec_string ("action-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_HIGHLIGHT_FOREGROUND,
					 g_param_spec_string ("highlight-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_TIME_FOREGROUND,
					 g_param_spec_string ("time-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_EVENT_FOREGROUND,
					 g_param_spec_string ("event-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_INVITE_FOREGROUND,
					 g_param_spec_string ("invite-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_LINK_FOREGROUND,
					 g_param_spec_string ("link-foreground",
							      "",
							      "",
							      NULL,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EmpathyThemeBoxesPriv));
}

static void
empathy_theme_boxes_init (EmpathyThemeBoxes *theme)
{
	EmpathyThemeBoxesPriv *priv;

	priv = GET_PRIV (theme);
}

static void
theme_boxes_finalize (GObject *object)
{
	EmpathyThemeBoxesPriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->header_foreground);
	g_free (priv->header_background);
	g_free (priv->header_line_background);
	g_free (priv->text_foreground);
	g_free (priv->text_background);
	g_free (priv->action_foreground);
	g_free (priv->highlight_foreground);
	g_free (priv->time_foreground);
	g_free (priv->event_foreground);
	g_free (priv->invite_foreground);
	g_free (priv->link_foreground);
	
	(G_OBJECT_CLASS (empathy_theme_boxes_parent_class)->finalize) (object);
}

static void
theme_boxes_get_property (GObject    *object,
			  guint       param_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
	EmpathyThemeBoxesPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_HEADER_FOREGROUND:
		g_value_set_string (value, priv->header_foreground);
		break;
	case PROP_HEADER_BACKGROUND:
		g_value_set_string (value, priv->header_background);
		break;
	case PROP_HEADER_LINE_BACKGROUND:
		g_value_set_string (value, priv->header_line_background);
		break;
	case PROP_TEXT_FOREGROUND:
		g_value_set_string (value, priv->text_foreground);
		break;
	case PROP_TEXT_BACKGROUND:
		g_value_set_string (value, priv->text_background);
		break;
	case PROP_ACTION_FOREGROUND:
		g_value_set_string (value, priv->action_foreground);
		break;
	case PROP_HIGHLIGHT_FOREGROUND:
		g_value_set_string (value, priv->highlight_foreground);
		break;
	case PROP_TIME_FOREGROUND:
		g_value_set_string (value, priv->time_foreground);
		break;
	case PROP_EVENT_FOREGROUND:
		g_value_set_string (value, priv->event_foreground);
		break;
	case PROP_INVITE_FOREGROUND:
		g_value_set_string (value, priv->invite_foreground);
		break;
	case PROP_LINK_FOREGROUND:
		g_value_set_string (value, priv->link_foreground);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}
static void
theme_boxes_set_property (GObject      *object,
		    guint         param_id,
		    const GValue *value,
		    GParamSpec   *pspec)
{
	EmpathyThemeBoxesPriv *priv;

	priv = GET_PRIV (object);

	switch (param_id) {
	case PROP_HEADER_FOREGROUND:
		g_free (priv->header_foreground);
		priv->header_foreground = g_value_dup_string (value);
		break;
	case PROP_HEADER_BACKGROUND:
		g_free (priv->header_background);
		priv->header_background = g_value_dup_string (value);
		break;
	case PROP_HEADER_LINE_BACKGROUND:
		g_free (priv->header_line_background);
		priv->header_line_background = g_value_dup_string (value);
		break;
	case PROP_TEXT_FOREGROUND:
		g_free (priv->text_foreground);
		priv->text_foreground = g_value_dup_string (value);
		break;
	case PROP_TEXT_BACKGROUND:
		g_free (priv->text_background);
		priv->text_background = g_value_dup_string (value);
		break;
	case PROP_ACTION_FOREGROUND:
		g_free (priv->action_foreground);
		priv->action_foreground = g_value_dup_string (value);
		break;
	case PROP_HIGHLIGHT_FOREGROUND:
		g_free (priv->highlight_foreground);
		priv->highlight_foreground = g_value_dup_string (value);
		break;
	case PROP_TIME_FOREGROUND:
		g_free (priv->time_foreground);
		priv->time_foreground = g_value_dup_string (value);
		break;
	case PROP_EVENT_FOREGROUND:
		g_free (priv->event_foreground);
		priv->event_foreground = g_value_dup_string (value);
		break;
	case PROP_INVITE_FOREGROUND:
		g_free (priv->invite_foreground);
		priv->invite_foreground = g_value_dup_string (value);
		break;
	case PROP_LINK_FOREGROUND:
		g_free (priv->link_foreground);
		priv->link_foreground = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
theme_boxes_define_theme_tags (EmpathyTheme *theme, EmpathyChatView *view)
{
	EmpathyThemeBoxesPriv *priv;
	GtkTextBuffer   *buffer;
	GtkTextTagTable *table;
	GtkTextTag      *tag;

	priv = GET_PRIV (theme);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	table = gtk_text_buffer_get_tag_table (buffer);

	tag = empathy_theme_utils_init_tag_by_name (table, "fancy-spacing");
	g_object_set (tag,
		      "size", 3000,
		      "pixels-above-lines", 8,
		      NULL);
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, 
						   "fancy-header");
	g_object_set (tag,
		      "weight", PANGO_WEIGHT_BOLD,
		      "pixels-above-lines", HEADER_PADDING,
		      "pixels-below-lines", HEADER_PADDING,
		      NULL);
	if (priv->header_foreground) {
		g_object_set (tag,
			      "foreground", priv->header_foreground,
			      "paragraph-background", priv->header_background,
			      NULL);
	}
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "fancy-header-line");
	g_object_set (tag,
		      "size", 1,
		      NULL);
	if (priv->header_line_background) {
		g_object_set (tag,
			      "paragraph-background", priv->header_line_background,
			      NULL);
	}

	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "fancy-body");
	g_object_set (tag,
		      "pixels-above-lines", 4,
		      NULL);
	if (priv->text_background) {
		g_object_set (tag,
			      "paragraph-background", priv->text_background,
			      NULL);
	}

	if (priv->text_foreground) {
		g_object_set (tag,
			      "foreground", priv->text_foreground,
			      NULL);
	}
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "fancy-action");
	g_object_set (tag,
		      "style", PANGO_STYLE_ITALIC,
		      "pixels-above-lines", 4,
		      NULL);

	if (priv->text_background) {
		g_object_set (tag,
			      "paragraph-background", priv->text_background,
			      NULL);
	}

	if (priv->action_foreground) {
		g_object_set (tag,
			      "foreground", priv->action_foreground,
			      NULL);
	} 

	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table,
						   "fancy-highlight");
	g_object_set (tag,
		      "weight", PANGO_WEIGHT_BOLD,
		      "pixels-above-lines", 4,
		      NULL);
	if (priv->text_background) {
		g_object_set (tag,
			      "paragraph-background", priv->text_background,
			      NULL);
	}


	if (priv->highlight_foreground) {
		g_object_set (tag,
			      "foreground", priv->highlight_foreground,
			      NULL);
	}
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "fancy-time");
	g_object_set (tag,
		      "justification", GTK_JUSTIFY_CENTER,
		      NULL);
	if (priv->time_foreground) {
		g_object_set (tag,
			      "foreground", priv->time_foreground,
			      NULL);
	}
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "fancy-event");
	g_object_set (tag,
		      "justification", GTK_JUSTIFY_LEFT,
		      NULL);
	if (priv->event_foreground) {
		g_object_set (tag,
			      "foreground", priv->event_foreground,
			      NULL);
	}
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "invite");
	if (priv->invite_foreground) {
		g_object_set (tag,
			      "foreground", priv->invite_foreground,
			      NULL);
	}
	empathy_theme_utils_add_tag (table, tag);

	tag = empathy_theme_utils_init_tag_by_name (table, "fancy-link");
	g_object_set (tag,
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL);
	if (priv->link_foreground) {
		g_object_set (tag,
			      "foreground", priv->link_foreground,
			      NULL);
	} 
	empathy_theme_utils_add_tag (table, tag);
}

static void
theme_boxes_fixup_tag_table (EmpathyTheme *theme, EmpathyChatView *view)
{
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	/* "Fancy" style tags. */
	empathy_theme_utils_ensure_tag_by_name (buffer, "fancy-header");
	empathy_theme_utils_ensure_tag_by_name (buffer, "fancy-header-line");
	empathy_theme_utils_ensure_tag_by_name (buffer, "fancy-body");
	empathy_theme_utils_ensure_tag_by_name (buffer, "fancy-action");
	empathy_theme_utils_ensure_tag_by_name (buffer, "fancy-highlight");
	empathy_theme_utils_ensure_tag_by_name (buffer, "fancy-spacing");
	empathy_theme_utils_ensure_tag_by_name (buffer, "fancy-time");
	empathy_theme_utils_ensure_tag_by_name (buffer, "fancy-event");
	empathy_theme_utils_ensure_tag_by_name (buffer, "fancy-link");
}

typedef struct {
	BlockType last_block_type;
	time_t    last_timestamp;
} FancyContext;

static EmpathyThemeContext *
theme_boxes_setup_with_view (EmpathyTheme *theme, EmpathyChatView *view)
{
	EmpathyThemeBoxesPriv *priv;

	g_return_val_if_fail (EMPATHY_IS_THEME_BOXES (theme), NULL);

	priv = GET_PRIV (theme);

	theme_boxes_fixup_tag_table (theme, view);

	theme_boxes_define_theme_tags (theme, view);
	
	empathy_chat_view_set_margin (view, MARGIN);

	return NULL;
}

static void
theme_boxes_detach_from_view (EmpathyTheme        *theme,
			      EmpathyThemeContext *context,
			      EmpathyChatView     *view)
{
	/* FIXME: Free the context */
}

static void 
theme_boxes_view_cleared (EmpathyTheme        *theme,
			  EmpathyThemeContext *context,
			  EmpathyChatView     *view)
{
	/* FIXME: clear the context data */
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
theme_boxes_maybe_append_header (EmpathyTheme        *theme,
				 EmpathyThemeContext *context,
				 EmpathyChatView     *view,
				 EmpathyMessage      *msg)
{
	EmpathyThemeBoxesPriv *priv;
	EmpathyContact        *contact;
	GdkPixbuf            *avatar = NULL;
	GtkTextBuffer        *buffer;
	const gchar          *name;
	gboolean              header;
	GtkTextIter           iter;
	GtkWidget            *label1, *label2;
	GtkTextChildAnchor   *anchor;
	GtkWidget            *box;
	gchar                *str;
	time_t                time;
	gchar                *tmp;
	GtkTextIter           start;
	GdkColor              color;
	gboolean              parse_success;
	gboolean              from_self;

	priv = GET_PRIV (theme);

	contact = empathy_message_get_sender (msg);
	from_self = empathy_contact_is_user (contact);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	empathy_debug (DEBUG_DOMAIN, "Maybe add fancy header");

	name = empathy_contact_get_name (contact);

	header = FALSE;

	/* Only insert a header if the previously inserted block is not the same
	 * as this one. This catches all the different cases:
	 */
	if (empathy_chat_view_get_last_block_type (view) != BLOCK_TYPE_SELF &&
	    empathy_chat_view_get_last_block_type (view) != BLOCK_TYPE_OTHER) {
		header = TRUE;
	}
	else if (from_self &&
		 empathy_chat_view_get_last_block_type (view) == BLOCK_TYPE_OTHER) {
		header = TRUE;
	}
	else if (!from_self && 
		 empathy_chat_view_get_last_block_type (view) == BLOCK_TYPE_SELF) {
		header = TRUE;
	}
	else if (!from_self &&
		 (!empathy_chat_view_get_last_contact (view) ||
		  !empathy_contact_equal (contact, empathy_chat_view_get_last_contact (view)))) {
		header = TRUE;
	}

	if (!header) {
		return;
	}

	empathy_theme_append_spacing (theme, context, view);

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  "\n",
						  -1,
						  "fancy-header-line",
						  NULL);

	gtk_text_buffer_get_end_iter (buffer, &iter);
	anchor = gtk_text_buffer_create_child_anchor (buffer, &iter);

	box = gtk_hbox_new (FALSE, 0);


	avatar = empathy_chat_view_get_avatar_pixbuf_with_cache (contact);
	if (avatar && empathy_theme_get_show_avatars (theme)) {
		GtkWidget *image;

		image = gtk_image_new_from_pixbuf (avatar);

		gtk_box_pack_start (GTK_BOX (box), image,
				    FALSE, TRUE, 2);

	}

	g_signal_connect_object (view, "size-allocate",
				 G_CALLBACK (table_size_allocate_cb),
				 box, 0);

	str = g_strdup_printf ("<b>%s</b>", name);

	label1 = g_object_new (GTK_TYPE_LABEL,
			       "label", str,
			       "use-markup", TRUE,
			       "xalign", 0.0,
			       NULL);

	parse_success = gdk_color_parse (priv->header_foreground, &color);

	if (parse_success) {
		gtk_widget_modify_fg (label1, GTK_STATE_NORMAL, &color);
	}

	g_free (str);

	time = empathy_message_get_timestamp (msg);

	tmp = empathy_time_to_string_local (time, 
					   EMPATHY_TIME_FORMAT_DISPLAY_SHORT);
	str = g_strdup_printf ("<i>%s</i>", tmp);
	g_free (tmp);

	label2 = g_object_new (GTK_TYPE_LABEL,
			       "label", str,
			       "use-markup", TRUE,
			       "xalign", 1.0,
			       NULL);
	
	if (parse_success) {
		gtk_widget_modify_fg (label2, GTK_STATE_NORMAL, &color);
	}

	g_free (str);

	gtk_misc_set_alignment (GTK_MISC (label1), 0.0, 0.5);
	gtk_misc_set_alignment (GTK_MISC (label2), 1.0, 0.5);

	gtk_box_pack_start (GTK_BOX (box), label1, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (box), label2, TRUE, TRUE, 0);

	gtk_text_view_add_child_at_anchor (GTK_TEXT_VIEW (view),
					   box,
					   anchor);

	gtk_widget_show_all (box);

	gtk_text_buffer_get_end_iter (buffer, &iter);
	start = iter;
	gtk_text_iter_backward_char (&start);
	gtk_text_buffer_apply_tag_by_name (buffer,
					   "fancy-header",
					   &start, &iter);

	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  "\n",
						  -1,
						  "fancy-header",
						  NULL);

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  "\n",
						  -1,
						  "fancy-header-line",
						  NULL);
}

static void
theme_boxes_append_message (EmpathyTheme        *theme,
			    EmpathyThemeContext *context,
			    EmpathyChatView     *view,
			    EmpathyMessage      *message)
{
	EmpathyContact *sender;

	empathy_theme_maybe_append_date_and_time (theme, context, view, message);
	theme_boxes_maybe_append_header (theme, context, view, message);

	sender = empathy_message_get_sender (message);

	if (empathy_message_get_type (message) == EMPATHY_MESSAGE_TYPE_ACTION) {
		gchar *body;

		body = g_strdup_printf (" * %s %s", 
					empathy_contact_get_name (sender),
					empathy_message_get_body (message));
		empathy_theme_append_text (theme, context, view, body,
					   "fancy-action", "fancy-link");
	} else {
		empathy_theme_append_text (theme, context, view,
					   empathy_message_get_body (message),
					   "fancy-body", "fancy-link");
	}
	
	if (empathy_contact_is_user (sender)) {
		empathy_chat_view_set_last_block_type (view, BLOCK_TYPE_SELF);
		empathy_chat_view_set_last_contact (view, NULL);
	} else {
		empathy_chat_view_set_last_block_type (view, BLOCK_TYPE_OTHER);
		empathy_chat_view_set_last_contact (view, sender);
	}
}

static void
theme_boxes_append_event (EmpathyTheme        *theme,
			  EmpathyThemeContext *context,
			  EmpathyChatView     *view,
			  const gchar        *str)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;
	gchar         *msg;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	empathy_theme_maybe_append_date_and_time (theme, context, view, NULL);

	gtk_text_buffer_get_end_iter (buffer, &iter);

	msg = g_strdup_printf (" - %s\n", str);

	gtk_text_buffer_insert_with_tags_by_name (buffer, &iter,
						  msg, -1,
						  "fancy-event",
						  NULL);
	g_free (msg);

	empathy_chat_view_set_last_block_type (view, BLOCK_TYPE_EVENT);
}

static void
theme_boxes_append_timestamp (EmpathyTheme        *theme,
			      EmpathyThemeContext *context,
			      EmpathyChatView     *view,
			      EmpathyMessage      *message,
			      gboolean            show_date,
			      gboolean            show_time)
{
	GtkTextBuffer *buffer;
	time_t         timestamp;
	GDate         *date;
	GtkTextIter    iter;
	GString       *str;
	
	if (!show_date) {
		return;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	date = empathy_message_get_date_and_time (message, &timestamp);

	str = g_string_new (NULL);

	if (show_time || show_date) {
		empathy_theme_append_spacing (theme, 
					     context,
					     view);

		g_string_append (str, "- ");
	}

	if (show_date) {
		gchar buf[256];

		g_date_strftime (buf, 256, _("%A %d %B %Y"), date);
		g_string_append (str, buf);

		if (show_time) {
			g_string_append (str, ", ");
		}
	}

	g_date_free (date);

	if (show_time) {
		gchar *tmp;

		tmp = empathy_time_to_string_local (timestamp, EMPATHY_TIME_FORMAT_DISPLAY_SHORT);
		g_string_append (str, tmp);
		g_free (tmp);
	}

	if (show_time || show_date) {
		g_string_append (str, " -\n");

		gtk_text_buffer_get_end_iter (buffer, &iter);
		gtk_text_buffer_insert_with_tags_by_name (buffer,
							  &iter,
							  str->str, -1,
							  "fancy-time",
							  NULL);

		empathy_chat_view_set_last_block_type (view, BLOCK_TYPE_TIME);
		empathy_chat_view_set_last_timestamp (view, timestamp);
	}

	g_string_free (str, TRUE);
	
}

static void
theme_boxes_append_spacing (EmpathyTheme        *theme,
			    EmpathyThemeContext *context,
			    EmpathyChatView     *view)
{
	GtkTextBuffer *buffer;
	GtkTextIter    iter;

	g_return_if_fail (EMPATHY_IS_THEME (theme));
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (buffer,
						  &iter,
						  "\n",
						  -1,
						  "cut",
						  "fancy-spacing",
						  NULL);
}

static void
theme_boxes_setup_clean (EmpathyTheme *theme)
{
	g_object_set (theme,
		      "header-foreground", "black",
		      "header-background", "#efefdf",
		      "header_line_background", "#e3e3d3",
		      "action_foreground", "brown4",
		      "time_foreground", "darkgrey",
		      "event_foreground", "darkgrey",
		      "invite_foreground", "sienna",
		      "link_foreground","#49789e",
		      NULL);
}

static void
theme_boxes_gdk_color_to_hex (GdkColor *gdk_color, gchar *str_color)
{
	g_snprintf (str_color, 10, 
		    "#%02x%02x%02x", 
		    gdk_color->red >> 8, 
		    gdk_color->green >> 8, 
		    gdk_color->blue >> 8);
}

static void
theme_boxes_setup_themed (EmpathyTheme *theme)
{
	EmpathyThemeBoxesPriv *priv;
	GtkWidget            *widget;
	GtkStyle             *style;
	gchar                 color[10];

	priv = GET_PRIV (theme);

	widget = gtk_entry_new ();
	style = gtk_widget_get_style (widget);
	gtk_widget_destroy (widget);

	theme_boxes_gdk_color_to_hex (&style->base[GTK_STATE_SELECTED], color);

	g_object_set (theme,
		      "action-foreground", color,
		      "link-foreground", color,
		      NULL);

	theme_boxes_gdk_color_to_hex (&style->bg[GTK_STATE_SELECTED], color);

	g_object_set (theme,
		      "header-background", color,
		      NULL);

	theme_boxes_gdk_color_to_hex (&style->dark[GTK_STATE_SELECTED], color);

	g_object_set (theme,
		      "header_line-background", color,
		      NULL);

	theme_boxes_gdk_color_to_hex (&style->fg[GTK_STATE_SELECTED], color);

	g_object_set (theme,
		      "header-foreground", color,
		      NULL);
}

static void
theme_boxes_theme_changed_cb (GtkWidget *widget,
			      GtkStyle  *previous_style,
			      gpointer   user_data)
{
	theme_boxes_setup_themed (EMPATHY_THEME (user_data));

	g_signal_emit_by_name (G_OBJECT (user_data), "updated");
}

static void
theme_boxes_setup_blue (EmpathyTheme *theme)
{
	g_object_set (theme,
		      "header_foreground", "black",
		      "header_background", "#88a2b4",
		      "header_line_background", "#7f96a4",
		      "text_foreground", "black",
		      "text_background", "#adbdc8",
		      "highlight_foreground", "black",
		      "action_foreground", "brown4",
		      "time_foreground", "darkgrey",
		      "event_foreground", "#7f96a4",
		      "invite_foreground", "sienna",
		      "link_foreground", "#49789e",
		      NULL);
}

EmpathyTheme *
empathy_theme_boxes_new (const gchar *name)
{
	EmpathyTheme          *theme;
	EmpathyThemeBoxesPriv *priv;

	theme = g_object_new (EMPATHY_TYPE_THEME_BOXES, NULL);
	priv  = GET_PRIV (theme);

	if (strcmp (name, "clean") == 0) {
		theme_boxes_setup_clean (theme);
	}
	else if (strcmp (name, "simple") == 0) {
		/* FIXME: Make an actual accessor function */
		g_signal_connect (empathy_main_window_show (),
				  "style-set",
				  G_CALLBACK (theme_boxes_theme_changed_cb),
				  theme);

		theme_boxes_setup_themed (theme);
	}
	else if (strcmp (name, "blue") == 0) {
		theme_boxes_setup_blue (theme);
	}

	return theme;
}


