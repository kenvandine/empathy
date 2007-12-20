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

#include <gtk/gtktexttag.h>

#include "empathy-theme-utils.h"

void
empathy_theme_utils_ensure_tag_by_name (GtkTextBuffer *buffer, const gchar *name)
{
	GtkTextTagTable *table;
	GtkTextTag      *tag;

	table = gtk_text_buffer_get_tag_table (buffer);
	tag = gtk_text_tag_table_lookup (table, name);

	if (!tag) {
		gtk_text_buffer_create_tag (buffer,
					    name,
					    NULL);
	}
}

GtkTextTag *
empathy_theme_utils_init_tag_by_name (GtkTextTagTable *table, const gchar *name)
{
	GtkTextTag *tag;

	tag = gtk_text_tag_table_lookup (table, name);

	if (!tag) {
		return gtk_text_tag_new (name);
	}

	/* Clear the old values so that we don't affect the new theme. */
	g_object_set (tag,
		      "background-set", FALSE,
		      "foreground-set", FALSE,
		      "invisible-set", FALSE,
		      "justification-set", FALSE,
		      "paragraph-background-set", FALSE,
		      "pixels-above-lines-set", FALSE,
		      "pixels-below-lines-set", FALSE,
		      "rise-set", FALSE,
		      "scale-set", FALSE,
		      "size-set", FALSE,
		      "style-set", FALSE,
		      "weight-set", FALSE,
		      NULL);

	return tag;
}

void
empathy_theme_utils_add_tag (GtkTextTagTable *table, GtkTextTag *tag)
{
	gchar      *name;
	GtkTextTag *check_tag;

	g_object_get (tag, "name", &name, NULL);
	check_tag = gtk_text_tag_table_lookup (table, name);
	g_free (name);
	if (check_tag) {
		return;
	}

	gtk_text_tag_table_add (table, tag);

	g_object_unref (tag);
}

