/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Authors: Mikael Hallendal <micke@imendio.com>
 */

#include "config.h"

#include <string.h>

#include <libempathy/empathy-utils.h>
#include "empathy-cell-renderer-text.h"

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyCellRendererText)
typedef struct {
	gchar    *name;
	gchar    *status;
	gboolean  is_group;

	gboolean  is_valid;
	gboolean  is_selected;

	gboolean  show_status;
} EmpathyCellRendererTextPriv;

static void cell_renderer_text_finalize          (GObject                     *object);
static void cell_renderer_text_get_property      (GObject                     *object,
						  guint                        param_id,
						  GValue                      *value,
						  GParamSpec                  *pspec);
static void cell_renderer_text_set_property      (GObject                     *object,
						  guint                        param_id,
						  const GValue                *value,
						  GParamSpec                  *pspec);
static void cell_renderer_text_get_size          (GtkCellRenderer             *cell,
						  GtkWidget                   *widget,
						  GdkRectangle                *cell_area,
						  gint                        *x_offset,
						  gint                        *y_offset,
						  gint                        *width,
						  gint                        *height);
static void cell_renderer_text_render            (GtkCellRenderer             *cell,
						  GdkDrawable                 *window,
						  GtkWidget                   *widget,
						  GdkRectangle                *background_area,
						  GdkRectangle                *cell_area,
						  GdkRectangle                *expose_area,
						  GtkCellRendererState         flags);
static void cell_renderer_text_update_text       (EmpathyCellRendererText      *cell,
						  GtkWidget                   *widget,
						  gboolean                     selected);

/* Properties */
enum {
	PROP_0,
	PROP_NAME,
	PROP_STATUS,
	PROP_IS_GROUP,
	PROP_SHOW_STATUS,
};

G_DEFINE_TYPE (EmpathyCellRendererText, empathy_cell_renderer_text, GTK_TYPE_CELL_RENDERER_TEXT);

static void
empathy_cell_renderer_text_class_init (EmpathyCellRendererTextClass *klass)
{
	GObjectClass         *object_class;
	GtkCellRendererClass *cell_class;

	object_class = G_OBJECT_CLASS (klass);
	cell_class = GTK_CELL_RENDERER_CLASS (klass);

	object_class->finalize = cell_renderer_text_finalize;

	object_class->get_property = cell_renderer_text_get_property;
	object_class->set_property = cell_renderer_text_set_property;

	cell_class->get_size = cell_renderer_text_get_size;
	cell_class->render = cell_renderer_text_render;

	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Name",
							      "Contact name",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_STATUS,
					 g_param_spec_string ("status",
							      "Status",
							      "Contact status string",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_IS_GROUP,
					 g_param_spec_boolean ("is_group",
							       "Is group",
							       "Whether this cell is a group",
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SHOW_STATUS,
					 g_param_spec_boolean ("show-status",
							       "Show status",
							       "Whether to show the status line",
							       TRUE,
							       G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EmpathyCellRendererTextPriv));
}

static void
empathy_cell_renderer_text_init (EmpathyCellRendererText *cell)
{
	EmpathyCellRendererTextPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (cell,
		EMPATHY_TYPE_CELL_RENDERER_TEXT, EmpathyCellRendererTextPriv);

	cell->priv = priv;
	g_object_set (cell,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	priv->name = g_strdup ("");
	priv->status = g_strdup ("");
	priv->show_status = TRUE;
}

static void
cell_renderer_text_finalize (GObject *object)
{
	EmpathyCellRendererText     *cell;
	EmpathyCellRendererTextPriv *priv;

	cell = EMPATHY_CELL_RENDERER_TEXT (object);
	priv = GET_PRIV (cell);

	g_free (priv->name);
	g_free (priv->status);

	(G_OBJECT_CLASS (empathy_cell_renderer_text_parent_class)->finalize) (object);
}

static void
cell_renderer_text_get_property (GObject    *object,
				 guint       param_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	EmpathyCellRendererText     *cell;
	EmpathyCellRendererTextPriv *priv;

	cell = EMPATHY_CELL_RENDERER_TEXT (object);
	priv = GET_PRIV (cell);

	switch (param_id) {
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_STATUS:
		g_value_set_string (value, priv->status);
		break;
	case PROP_IS_GROUP:
		g_value_set_boolean (value, priv->is_group);
		break;
	case PROP_SHOW_STATUS:
		g_value_set_boolean (value, priv->show_status);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
cell_renderer_text_set_property (GObject      *object,
				 guint         param_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	EmpathyCellRendererText     *cell;
	EmpathyCellRendererTextPriv *priv;
	const gchar                *str;

	cell = EMPATHY_CELL_RENDERER_TEXT (object);
	priv = GET_PRIV (cell);

	switch (param_id) {
	case PROP_NAME:
		g_free (priv->name);
		str = g_value_get_string (value);
		priv->name = g_strdup (str ? str : "");
		g_strdelimit (priv->name, "\n\r\t", ' ');
		priv->is_valid = FALSE;
		break;
	case PROP_STATUS:
		g_free (priv->status);
		str = g_value_get_string (value);
		priv->status = g_strdup (str ? str : "");
		g_strdelimit (priv->status, "\n\r\t", ' ');
		priv->is_valid = FALSE;
		break;
	case PROP_IS_GROUP:
		priv->is_group = g_value_get_boolean (value);
		priv->is_valid = FALSE;
		break;
	case PROP_SHOW_STATUS:
		priv->show_status = g_value_get_boolean (value);
		priv->is_valid = FALSE;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
cell_renderer_text_get_size (GtkCellRenderer *cell,
			     GtkWidget       *widget,
			     GdkRectangle    *cell_area,
			     gint            *x_offset,
			     gint            *y_offset,
			     gint            *width,
			     gint            *height)
{
	EmpathyCellRendererText     *celltext;
	EmpathyCellRendererTextPriv *priv;

	celltext = EMPATHY_CELL_RENDERER_TEXT (cell);
	priv = GET_PRIV (cell);

	/* Only update if not already valid so we get the right size. */
	cell_renderer_text_update_text (celltext, widget, priv->is_selected);

	(GTK_CELL_RENDERER_CLASS (empathy_cell_renderer_text_parent_class)->get_size) (cell, widget,
										      cell_area,
										      x_offset, y_offset,
										      width, height);
}

static void
cell_renderer_text_render (GtkCellRenderer      *cell,
			   GdkWindow            *window,
			   GtkWidget            *widget,
			   GdkRectangle         *background_area,
			   GdkRectangle         *cell_area,
			   GdkRectangle         *expose_area,
			   GtkCellRendererState  flags)
{
	EmpathyCellRendererText *celltext;

	celltext = EMPATHY_CELL_RENDERER_TEXT (cell);

	cell_renderer_text_update_text (celltext,
					widget,
					(flags & GTK_CELL_RENDERER_SELECTED));

	(GTK_CELL_RENDERER_CLASS (empathy_cell_renderer_text_parent_class)->render) (
		cell, window,
		widget,
		background_area,
		cell_area,
		expose_area, flags);
}

static void
cell_renderer_text_update_text (EmpathyCellRendererText *cell,
				GtkWidget              *widget,
				gboolean                selected)
{
	EmpathyCellRendererTextPriv *priv;
	PangoAttrList              *attr_list;
	PangoAttribute             *attr_color, *attr_size;
	GtkStyle                   *style;
	gchar                      *str;

	priv = GET_PRIV (cell);

	if (priv->is_valid && priv->is_selected == selected) {
		return;
	}

	if (priv->is_group) {
		g_object_set (cell,
			      "visible", TRUE,
			      "weight", PANGO_WEIGHT_BOLD,
			      "text", priv->name,
			      "attributes", NULL,
			      "xpad", 1,
			      "ypad", 1,
			      NULL);

		priv->is_selected = selected;
		priv->is_valid = TRUE;
		return;
	}

	style = gtk_widget_get_style (widget);

	attr_list = pango_attr_list_new ();

	attr_size = pango_attr_size_new (pango_font_description_get_size (style->font_desc) / 1.2);
	attr_size->start_index = strlen (priv->name) + 1;
	attr_size->end_index = -1;
	pango_attr_list_insert (attr_list, attr_size);

	if (!selected) {
		GdkColor color;

		color = style->text_aa[GTK_STATE_NORMAL];

		attr_color = pango_attr_foreground_new (color.red, color.green, color.blue);
		attr_color->start_index = attr_size->start_index;
		attr_color->end_index = -1;
		pango_attr_list_insert (attr_list, attr_color);
	}

	if (!priv->status || !priv->status[0] || !priv->show_status) {
		str = g_strdup (priv->name);
	} else {
		str = g_strdup_printf ("%s\n%s", priv->name, priv->status);
	}

	g_object_set (cell,
		      "visible", TRUE,
		      "weight", PANGO_WEIGHT_NORMAL,
		      "text", str,
		      "attributes", attr_list,
		      "xpad", 0,
		      "ypad", 1,
		      NULL);

	g_free (str);
	pango_attr_list_unref (attr_list);

	priv->is_selected = selected;
	priv->is_valid = TRUE;
}

GtkCellRenderer *
empathy_cell_renderer_text_new (void)
{
	return g_object_new (EMPATHY_TYPE_CELL_RENDERER_TEXT, NULL);
}
