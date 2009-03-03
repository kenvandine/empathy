/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 *          Jonny Lamb <jonny.lamb@collabora.co.uk>
 * 
 *          Part of this file is copied from GtkSourceView (gtksourceiter.c):
 *          Paolo Maggi
 *          Jeroen Zwartepoorte
 */

#include <config.h>

#include <string.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glade/glade.h>
#include <canberra-gtk.h>

#include <libmissioncontrol/mc-profile.h>

#include "empathy-ui-utils.h"
#include "empathy-images.h"
#include "empathy-conf.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>
#include <libempathy/empathy-utils.h>
#include <libempathy/empathy-dispatcher.h>
#include <libempathy/empathy-idle.h>
#include <libempathy/empathy-tp-file.h>

#define SCHEMES "(https?|s?ftps?|nntp|news|javascript|about|ghelp|apt|telnet|"\
		"file|webcal|mailto)"
#define BODY "([^\\ \\n]+)"
#define END_BODY "([^\\ \\n]*[^,;\?><()\\ \"\\.\\n])"
#define URI_REGEX "("SCHEMES"://"END_BODY")" \
		  "|((mailto:)?"BODY"@"BODY"\\."END_BODY")"\
		  "|((www|ftp)\\."END_BODY")"

void
empathy_gtk_init (void)
{
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	empathy_init ();
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   PKGDATADIR G_DIR_SEPARATOR_S "icons");

	initialized = TRUE;
}

GRegex *
empathy_uri_regex_dup_singleton (void)
{
	static GRegex *uri_regex = NULL;

	/* We intentionally leak the regex so it's not recomputed */
	if (!uri_regex) {
		uri_regex = g_regex_new (URI_REGEX, 0, 0, NULL);
	}

	return g_regex_ref (uri_regex);
}

struct SizeData {
	gint     width;
	gint     height;
	gboolean preserve_aspect_ratio;
};

static GladeXML *
get_glade_file (const gchar *filename,
		const gchar *root,
		const gchar *domain,
		const gchar *first_required_widget,
		va_list      args)
{
	GladeXML   *gui;
	const char *name;
	GtkWidget **widget_ptr;

	DEBUG ("Loading glade file %s", filename);

	gui = glade_xml_new (filename, root, domain);

	if (!gui) {
		g_warning ("Couldn't find necessary glade file '%s'", filename);
	}

	for (name = first_required_widget; name; name = va_arg (args, char *)) {
		widget_ptr = va_arg (args, void *);

		*widget_ptr = glade_xml_get_widget (gui, name);

		if (!*widget_ptr) {
			g_warning ("Glade file '%s' is missing widget '%s'.",
				   filename, name);
			continue;
		}
	}

	return gui;
}

void
empathy_glade_get_file_simple (const gchar *filename,
			      const gchar *root,
			      const gchar *domain,
			      const gchar *first_required_widget, ...)
{
	va_list   args;
	GladeXML *gui;

	va_start (args, first_required_widget);

	gui = get_glade_file (filename,
			      root,
			      domain,
			      first_required_widget,
			      args);

	va_end (args);

	if (gui) {
		g_object_unref (gui);
	}
}

GladeXML *
empathy_glade_get_file (const gchar *filename,
		       const gchar *root,
		       const gchar *domain,
		       const gchar *first_required_widget, ...)
{
	va_list   args;
	GladeXML *gui;

	va_start (args, first_required_widget);

	gui = get_glade_file (filename,
			      root,
			      domain,
			      first_required_widget,
			      args);

	va_end (args);

	if (!gui) {
		return NULL;
	}

	return gui;
}

void
empathy_glade_connect (GladeXML *gui,
		      gpointer  user_data,
		      gchar     *first_widget, ...)
{
	va_list      args;
	const gchar *name;
	const gchar *signal;
	GtkWidget   *widget;
	gpointer    *callback;

	va_start (args, first_widget);

	for (name = first_widget; name; name = va_arg (args, char *)) {
		signal = va_arg (args, void *);
		callback = va_arg (args, void *);

		widget = glade_xml_get_widget (gui, name);
		if (!widget) {
			g_warning ("Glade file is missing widget '%s', aborting",
				   name);
			continue;
		}

		g_signal_connect (widget,
				  signal,
				  G_CALLBACK (callback),
				  user_data);
	}

	va_end (args);
}

void
empathy_glade_setup_size_group (GladeXML         *gui,
			       GtkSizeGroupMode  mode,
			       gchar            *first_widget, ...)
{
	va_list       args;
	GtkWidget    *widget;
	GtkSizeGroup *size_group;
	const gchar  *name;

	va_start (args, first_widget);

	size_group = gtk_size_group_new (mode);

	for (name = first_widget; name; name = va_arg (args, char *)) {
		widget = glade_xml_get_widget (gui, name);
		if (!widget) {
			g_warning ("Glade file is missing widget '%s'", name);
			continue;
		}

		gtk_size_group_add_widget (size_group, widget);
	}

	g_object_unref (size_group);

	va_end (args);
}

const gchar *
empathy_icon_name_from_account (McAccount *account)
{
	McProfile *profile;

	profile = mc_account_get_profile (account);

	return mc_profile_get_icon_name (profile);
}

const gchar *
empathy_icon_name_for_presence (McPresence presence)
{
	switch (presence) {
	case MC_PRESENCE_AVAILABLE:
		return EMPATHY_IMAGE_AVAILABLE;
	case MC_PRESENCE_DO_NOT_DISTURB:
		return EMPATHY_IMAGE_BUSY;
	case MC_PRESENCE_AWAY:
		return EMPATHY_IMAGE_AWAY;
	case MC_PRESENCE_EXTENDED_AWAY:
		return EMPATHY_IMAGE_EXT_AWAY;
	case MC_PRESENCE_HIDDEN:
		return EMPATHY_IMAGE_HIDDEN;
	case MC_PRESENCE_OFFLINE:
	case MC_PRESENCE_UNSET:
		return EMPATHY_IMAGE_OFFLINE;
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

const gchar *
empathy_icon_name_for_contact (EmpathyContact *contact)
{
	McPresence presence;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact),
			      EMPATHY_IMAGE_OFFLINE);

	presence = empathy_contact_get_presence (contact);
	return empathy_icon_name_for_presence (presence);
}

GdkPixbuf *
empathy_pixbuf_from_data (gchar *data,
			  gsize  data_size)
{
	return empathy_pixbuf_from_data_and_mime (data, data_size, NULL);
}

GdkPixbuf *
empathy_pixbuf_from_data_and_mime (gchar  *data,
				   gsize   data_size,
				   gchar **mime_type)
{
	GdkPixbufLoader *loader;
	GdkPixbufFormat *format;
	GdkPixbuf       *pixbuf = NULL;
	gchar          **mime_types;
	GError          *error = NULL;

	if (!data) {
		return NULL;
	}

	loader = gdk_pixbuf_loader_new ();
	if (!gdk_pixbuf_loader_write (loader, data, data_size, &error)) {
		DEBUG ("Failed to write to pixbuf loader: %s",
			error ? error->message : "No error given");
		goto out;
	}
	if (!gdk_pixbuf_loader_close (loader, &error)) {
		DEBUG ("Failed to close pixbuf loader: %s",
			error ? error->message : "No error given");
		goto out;
	}

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	if (pixbuf) {
		g_object_ref (pixbuf);

		if (mime_type != NULL) {
			format = gdk_pixbuf_loader_get_format (loader);
			mime_types = gdk_pixbuf_format_get_mime_types (format);

			*mime_type = g_strdup (*mime_types);
			if (mime_types[1] != NULL) {
				DEBUG ("Loader supports more than one mime "
					"type! Picking the first one, %s",
					*mime_type);
			}
			g_strfreev (mime_types);
		}
	}

out:
	g_clear_error (&error);
	g_object_unref (loader);

	return pixbuf;
}

static void
pixbuf_from_avatar_size_prepared_cb (GdkPixbufLoader *loader,
				     int              width,
				     int              height,
				     struct SizeData *data)
{
	g_return_if_fail (width > 0 && height > 0);

	if (data->preserve_aspect_ratio && (data->width > 0 || data->height > 0)) {
		if (width < data->width && height < data->height) {
			width = width;
			height = height;
		}

		if (data->width < 0) {
			width = width * (double) data->height / (gdouble) height;
			height = data->height;
		} else if (data->height < 0) {
			height = height * (double) data->width / (double) width;
			width = data->width;
		} else if ((double) height * (double) data->width >
			   (double) width * (double) data->height) {
			width = 0.5 + (double) width * (double) data->height / (double) height;
			height = data->height;
		} else {
			height = 0.5 + (double) height * (double) data->width / (double) width;
			width = data->width;
		}
	} else {
		if (data->width > 0) {
			width = data->width;
		}

		if (data->height > 0) {
			height = data->height;
		}
	}

	gdk_pixbuf_loader_set_size (loader, width, height);
}

static void
empathy_avatar_pixbuf_roundify (GdkPixbuf *pixbuf)
{
	gint width, height, rowstride;
	guchar *pixels;

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	if (width < 6 || height < 6) {
		return;
	}

	/* Top left */
	pixels[3] = 0;
	pixels[7] = 0x80;
	pixels[11] = 0xC0;
	pixels[rowstride + 3] = 0x80;
	pixels[rowstride * 2 + 3] = 0xC0;

	/* Top right */
	pixels[width * 4 - 1] = 0;
	pixels[width * 4 - 5] = 0x80;
	pixels[width * 4 - 9] = 0xC0;
	pixels[rowstride + (width * 4) - 1] = 0x80;
	pixels[(2 * rowstride) + (width * 4) - 1] = 0xC0;

	/* Bottom left */
	pixels[(height - 1) * rowstride + 3] = 0;
	pixels[(height - 1) * rowstride + 7] = 0x80;
	pixels[(height - 1) * rowstride + 11] = 0xC0;
	pixels[(height - 2) * rowstride + 3] = 0x80;
	pixels[(height - 3) * rowstride + 3] = 0xC0;

	/* Bottom right */
	pixels[height * rowstride - 1] = 0;
	pixels[(height - 1) * rowstride - 1] = 0x80;
	pixels[(height - 2) * rowstride - 1] = 0xC0;
	pixels[height * rowstride - 5] = 0x80;
	pixels[height * rowstride - 9] = 0xC0;
}

static gboolean
empathy_gdk_pixbuf_is_opaque (GdkPixbuf *pixbuf)
{
	gint width, height, rowstride, i;
	guchar *pixels;
	guchar *row;

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	row = pixels;
	for (i = 3; i < rowstride; i+=4) {
		if (row[i] < 0xfe) {
			return FALSE;
		}
	}

	for (i = 1; i < height - 1; i++) {
		row = pixels + (i*rowstride);
		if (row[3] < 0xfe || row[rowstride-1] < 0xfe) {
			return FALSE;
		}
	}

	row = pixels + ((height-1) * rowstride);
	for (i = 3; i < rowstride; i+=4) {
		if (row[i] < 0xfe) {
			return FALSE;
		}
	}

	return TRUE;
}

GdkPixbuf *
empathy_pixbuf_from_avatar_scaled (EmpathyAvatar *avatar,
				  gint          width,
				  gint          height)
{
	GdkPixbuf        *pixbuf;
	GdkPixbufLoader	 *loader;
	struct SizeData   data;
	GError           *error = NULL;

	if (!avatar) {
		return NULL;
	}

	data.width = width;
	data.height = height;
	data.preserve_aspect_ratio = TRUE;

	loader = gdk_pixbuf_loader_new ();

	g_signal_connect (loader, "size-prepared",
			  G_CALLBACK (pixbuf_from_avatar_size_prepared_cb),
			  &data);

	if (!gdk_pixbuf_loader_write (loader, avatar->data, avatar->len, &error)) {
		g_warning ("Couldn't write avatar image:%p with "
			   "length:%" G_GSIZE_FORMAT " to pixbuf loader: %s",
			   avatar->data, avatar->len, error->message);
		g_error_free (error);
		return NULL;
	}

	gdk_pixbuf_loader_close (loader, NULL);

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	if (!gdk_pixbuf_get_has_alpha (pixbuf)) {
		GdkPixbuf *rounded_pixbuf;

		rounded_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
						 gdk_pixbuf_get_width (pixbuf),
						 gdk_pixbuf_get_height (pixbuf));
		gdk_pixbuf_copy_area (pixbuf, 0, 0,
				      gdk_pixbuf_get_width (pixbuf),
				      gdk_pixbuf_get_height (pixbuf),
				      rounded_pixbuf,
				      0, 0);
		pixbuf = rounded_pixbuf;
	} else {
		g_object_ref (pixbuf);
	}

	if (empathy_gdk_pixbuf_is_opaque (pixbuf)) {
		empathy_avatar_pixbuf_roundify (pixbuf);
	}

	g_object_unref (loader);

	return pixbuf;
}

GdkPixbuf *
empathy_pixbuf_avatar_from_contact_scaled (EmpathyContact *contact,
					  gint           width,
					  gint           height)
{
	EmpathyAvatar *avatar;

	g_return_val_if_fail (EMPATHY_IS_CONTACT (contact), NULL);

	avatar = empathy_contact_get_avatar (contact);

	return empathy_pixbuf_from_avatar_scaled (avatar, width, height);
}

GdkPixbuf *
empathy_pixbuf_scale_down_if_necessary (GdkPixbuf *pixbuf, gint max_size)
{
	gint      width, height;
	gdouble   factor;

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	if (width > 0 && (width > max_size || height > max_size)) {
		factor = (gdouble) max_size / MAX (width, height);

		width = width * factor;
		height = height * factor;

		return gdk_pixbuf_scale_simple (pixbuf,
						width, height,
						GDK_INTERP_HYPER);
	}

	return g_object_ref (pixbuf);
}

GdkPixbuf *
empathy_pixbuf_from_icon_name_sized (const gchar *icon_name,
				     gint size)
{
	GtkIconTheme *theme;
	GdkPixbuf *pixbuf;
	GError *error = NULL;

	if (!icon_name) {
		return NULL;
	}

	theme = gtk_icon_theme_get_default ();

	pixbuf = gtk_icon_theme_load_icon (theme,
					   icon_name,
					   size,
					   0,
					   &error);
	if (error) {
		DEBUG ("Error loading icon: %s", error->message);
		g_clear_error (&error);
	}

	return pixbuf;
}

GdkPixbuf *
empathy_pixbuf_from_icon_name (const gchar *icon_name,
			       GtkIconSize  icon_size)
{
	gint  w, h;
	gint  size = 48;

	if (!icon_name) {
		return NULL;
	}

	if (gtk_icon_size_lookup (icon_size, &w, &h)) {
		size = (w + h) / 2;
	}

	return empathy_pixbuf_from_icon_name_sized (icon_name, size);
}

/* Stolen from GtkSourceView, hence the weird intendation. Please keep it like
 * that to make it easier to apply changes from the original code.
 */
#define GTK_TEXT_UNKNOWN_CHAR 0xFFFC

/* this function acts like g_utf8_offset_to_pointer() except that if it finds a
 * decomposable character it consumes the decomposition length from the given
 * offset.  So it's useful when the offset was calculated for the normalized
 * version of str, but we need a pointer to str itself. */
static const gchar *
pointer_from_offset_skipping_decomp (const gchar *str, gint offset)
{
	gchar *casefold, *normal;
	const gchar *p, *q;

	p = str;
	while (offset > 0)
	{
		q = g_utf8_next_char (p);
		casefold = g_utf8_casefold (p, q - p);
		normal = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
		offset -= g_utf8_strlen (normal, -1);
		g_free (casefold);
		g_free (normal);
		p = q;
	}
	return p;
}

static const gchar *
g_utf8_strcasestr (const gchar *haystack, const gchar *needle)
{
	gsize needle_len;
	gsize haystack_len;
	const gchar *ret = NULL;
	gchar *p;
	gchar *casefold;
	gchar *caseless_haystack;
	gint i;

	g_return_val_if_fail (haystack != NULL, NULL);
	g_return_val_if_fail (needle != NULL, NULL);

	casefold = g_utf8_casefold (haystack, -1);
	caseless_haystack = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
	g_free (casefold);

	needle_len = g_utf8_strlen (needle, -1);
	haystack_len = g_utf8_strlen (caseless_haystack, -1);

	if (needle_len == 0)
	{
		ret = (gchar *)haystack;
		goto finally_1;
	}

	if (haystack_len < needle_len)
	{
		ret = NULL;
		goto finally_1;
	}

	p = (gchar*)caseless_haystack;
	needle_len = strlen (needle);
	i = 0;

	while (*p)
	{
		if ((strncmp (p, needle, needle_len) == 0))
		{
			ret = pointer_from_offset_skipping_decomp (haystack, i);
			goto finally_1;
		}

		p = g_utf8_next_char (p);
		i++;
	}

finally_1:
	g_free (caseless_haystack);

	return ret;
}

static gboolean
g_utf8_caselessnmatch (const char *s1, const char *s2,
		       gssize n1, gssize n2)
{
	gchar *casefold;
	gchar *normalized_s1;
	gchar *normalized_s2;
	gint len_s1;
	gint len_s2;
	gboolean ret = FALSE;

	g_return_val_if_fail (s1 != NULL, FALSE);
	g_return_val_if_fail (s2 != NULL, FALSE);
	g_return_val_if_fail (n1 > 0, FALSE);
	g_return_val_if_fail (n2 > 0, FALSE);

	casefold = g_utf8_casefold (s1, n1);
	normalized_s1 = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
	g_free (casefold);

	casefold = g_utf8_casefold (s2, n2);
	normalized_s2 = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
	g_free (casefold);

	len_s1 = strlen (normalized_s1);
	len_s2 = strlen (normalized_s2);

	if (len_s1 < len_s2)
		goto finally_2;

	ret = (strncmp (normalized_s1, normalized_s2, len_s2) == 0);

finally_2:
	g_free (normalized_s1);
	g_free (normalized_s2);

	return ret;
}

static void
forward_chars_with_skipping (GtkTextIter *iter,
			     gint         count,
			     gboolean     skip_invisible,
			     gboolean     skip_nontext,
			     gboolean     skip_decomp)
{
	gint i;

	g_return_if_fail (count >= 0);

	i = count;

	while (i > 0)
	{
		gboolean ignored = FALSE;

		/* minimal workaround to avoid the infinite loop of bug #168247.
		 * It doesn't fix the problemjust the symptom...
		 */
		if (gtk_text_iter_is_end (iter))
			return;

		if (skip_nontext && gtk_text_iter_get_char (iter) == GTK_TEXT_UNKNOWN_CHAR)
			ignored = TRUE;

		if (!ignored && skip_invisible &&
		    /* _gtk_text_btree_char_is_invisible (iter)*/ FALSE)
			ignored = TRUE;

		if (!ignored && skip_decomp)
		{
			/* being UTF8 correct sucks; this accounts for extra
			   offsets coming from canonical decompositions of
			   UTF8 characters (e.g. accented characters) which
			   g_utf8_normalize() performs */
			gchar *normal;
			gchar buffer[6];
			gint buffer_len;

			buffer_len = g_unichar_to_utf8 (gtk_text_iter_get_char (iter), buffer);
			normal = g_utf8_normalize (buffer, buffer_len, G_NORMALIZE_NFD);
			i -= (g_utf8_strlen (normal, -1) - 1);
			g_free (normal);
		}

		gtk_text_iter_forward_char (iter);

		if (!ignored)
			--i;
	}
}

static gboolean
lines_match (const GtkTextIter *start,
	     const gchar      **lines,
	     gboolean           visible_only,
	     gboolean           slice,
	     GtkTextIter       *match_start,
	     GtkTextIter       *match_end)
{
	GtkTextIter next;
	gchar *line_text;
	const gchar *found;
	gint offset;

	if (*lines == NULL || **lines == '\0')
	{
		if (match_start)
			*match_start = *start;
		if (match_end)
			*match_end = *start;
		return TRUE;
	}

	next = *start;
	gtk_text_iter_forward_line (&next);

	/* No more text in buffer, but *lines is nonempty */
	if (gtk_text_iter_equal (start, &next))
		return FALSE;

	if (slice)
	{
		if (visible_only)
			line_text = gtk_text_iter_get_visible_slice (start, &next);
		else
			line_text = gtk_text_iter_get_slice (start, &next);
	}
	else
	{
		if (visible_only)
			line_text = gtk_text_iter_get_visible_text (start, &next);
		else
			line_text = gtk_text_iter_get_text (start, &next);
	}

	if (match_start) /* if this is the first line we're matching */
	{
		found = g_utf8_strcasestr (line_text, *lines);
	}
	else
	{
		/* If it's not the first line, we have to match from the
		 * start of the line.
		 */
		if (g_utf8_caselessnmatch (line_text, *lines, strlen (line_text),
					   strlen (*lines)))
			found = line_text;
		else
			found = NULL;
	}

	if (found == NULL)
	{
		g_free (line_text);
		return FALSE;
	}

	/* Get offset to start of search string */
	offset = g_utf8_strlen (line_text, found - line_text);

	next = *start;

	/* If match start needs to be returned, set it to the
	 * start of the search string.
	 */
	forward_chars_with_skipping (&next, offset, visible_only, !slice, FALSE);
	if (match_start)
	{
		*match_start = next;
	}

	/* Go to end of search string */
	forward_chars_with_skipping (&next, g_utf8_strlen (*lines, -1), visible_only, !slice, TRUE);

	g_free (line_text);

	++lines;

	if (match_end)
		*match_end = next;

	/* pass NULL for match_start, since we don't need to find the
	 * start again.
	 */
	return lines_match (&next, lines, visible_only, slice, NULL, match_end);
}

/* strsplit () that retains the delimiter as part of the string. */
static gchar **
strbreakup (const char *string,
	    const char *delimiter,
	    gint        max_tokens)
{
	GSList *string_list = NULL, *slist;
	gchar **str_array, *s, *casefold, *new_string;
	guint i, n = 1;

	g_return_val_if_fail (string != NULL, NULL);
	g_return_val_if_fail (delimiter != NULL, NULL);

	if (max_tokens < 1)
		max_tokens = G_MAXINT;

	s = strstr (string, delimiter);
	if (s)
	{
		guint delimiter_len = strlen (delimiter);

		do
		{
			guint len;

			len = s - string + delimiter_len;
			new_string = g_new (gchar, len + 1);
			strncpy (new_string, string, len);
			new_string[len] = 0;
			casefold = g_utf8_casefold (new_string, -1);
			g_free (new_string);
			new_string = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
			g_free (casefold);
			string_list = g_slist_prepend (string_list, new_string);
			n++;
			string = s + delimiter_len;
			s = strstr (string, delimiter);
		} while (--max_tokens && s);
	}

	if (*string)
	{
		n++;
		casefold = g_utf8_casefold (string, -1);
		new_string = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
		g_free (casefold);
		string_list = g_slist_prepend (string_list, new_string);
	}

	str_array = g_new (gchar*, n);

	i = n - 1;

	str_array[i--] = NULL;
	for (slist = string_list; slist; slist = slist->next)
		str_array[i--] = slist->data;

	g_slist_free (string_list);

	return str_array;
}

gboolean
empathy_text_iter_forward_search (const GtkTextIter   *iter,
				 const gchar         *str,
				 GtkTextIter         *match_start,
				 GtkTextIter         *match_end,
				 const GtkTextIter   *limit)
{
	gchar **lines = NULL;
	GtkTextIter match;
	gboolean retval = FALSE;
	GtkTextIter search;
	gboolean visible_only;
	gboolean slice;

	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (str != NULL, FALSE);

	if (limit && gtk_text_iter_compare (iter, limit) >= 0)
		return FALSE;

	if (*str == '\0') {
		/* If we can move one char, return the empty string there */
		match = *iter;

		if (gtk_text_iter_forward_char (&match)) {
			if (limit && gtk_text_iter_equal (&match, limit)) {
				return FALSE;
			}

			if (match_start) {
				*match_start = match;
			}
			if (match_end) {
				*match_end = match;
			}
			return TRUE;
		} else {
			return FALSE;
		}
	}

	visible_only = TRUE;
	slice = FALSE;

	/* locate all lines */
	lines = strbreakup (str, "\n", -1);

	search = *iter;

	do {
		/* This loop has an inefficient worst-case, where
		 * gtk_text_iter_get_text () is called repeatedly on
		 * a single line.
		 */
		GtkTextIter end;

		if (limit && gtk_text_iter_compare (&search, limit) >= 0) {
			break;
		}

		if (lines_match (&search, (const gchar**)lines,
				 visible_only, slice, &match, &end)) {
			if (limit == NULL ||
			    (limit && gtk_text_iter_compare (&end, limit) <= 0)) {
				retval = TRUE;

				if (match_start) {
					*match_start = match;
				}
				if (match_end) {
					*match_end = end;
				}
			}
			break;
		}
	} while (gtk_text_iter_forward_line (&search));

	g_strfreev ((gchar**)lines);

	return retval;
}

static const gchar *
g_utf8_strrcasestr (const gchar *haystack, const gchar *needle)
{
	gsize needle_len;
	gsize haystack_len;
	const gchar *ret = NULL;
	gchar *p;
	gchar *casefold;
	gchar *caseless_haystack;
	gint i;

	g_return_val_if_fail (haystack != NULL, NULL);
	g_return_val_if_fail (needle != NULL, NULL);

	casefold = g_utf8_casefold (haystack, -1);
	caseless_haystack = g_utf8_normalize (casefold, -1, G_NORMALIZE_NFD);
	g_free (casefold);

	needle_len = g_utf8_strlen (needle, -1);
	haystack_len = g_utf8_strlen (caseless_haystack, -1);

	if (needle_len == 0)
	{
		ret = (gchar *)haystack;
		goto finally_1;
	}

	if (haystack_len < needle_len)
	{
		ret = NULL;
		goto finally_1;
	}

	i = haystack_len - needle_len;
	p = g_utf8_offset_to_pointer (caseless_haystack, i);
	needle_len = strlen (needle);

	while (p >= caseless_haystack)
	{
		if (strncmp (p, needle, needle_len) == 0)
		{
			ret = pointer_from_offset_skipping_decomp (haystack, i);
			goto finally_1;
		}

		p = g_utf8_prev_char (p);
		i--;
	}

finally_1:
	g_free (caseless_haystack);

	return ret;
}

static gboolean
backward_lines_match (const GtkTextIter *start,
		      const gchar      **lines,
		      gboolean           visible_only,
		      gboolean           slice,
		      GtkTextIter       *match_start,
		      GtkTextIter       *match_end)
{
	GtkTextIter line, next;
	gchar *line_text;
	const gchar *found;
	gint offset;

	if (*lines == NULL || **lines == '\0')
	{
		if (match_start)
			*match_start = *start;
		if (match_end)
			*match_end = *start;
		return TRUE;
	}

	line = next = *start;
	if (gtk_text_iter_get_line_offset (&next) == 0)
	{
		if (!gtk_text_iter_backward_line (&next))
			return FALSE;
	}
	else
		gtk_text_iter_set_line_offset (&next, 0);

	if (slice)
	{
		if (visible_only)
			line_text = gtk_text_iter_get_visible_slice (&next, &line);
		else
			line_text = gtk_text_iter_get_slice (&next, &line);
	}
	else
	{
		if (visible_only)
			line_text = gtk_text_iter_get_visible_text (&next, &line);
		else
			line_text = gtk_text_iter_get_text (&next, &line);
	}

	if (match_start) /* if this is the first line we're matching */
	{
		found = g_utf8_strrcasestr (line_text, *lines);
	}
	else
	{
		/* If it's not the first line, we have to match from the
		 * start of the line.
		 */
		if (g_utf8_caselessnmatch (line_text, *lines, strlen (line_text),
					   strlen (*lines)))
			found = line_text;
		else
			found = NULL;
	}

	if (found == NULL)
	{
		g_free (line_text);
		return FALSE;
	}

	/* Get offset to start of search string */
	offset = g_utf8_strlen (line_text, found - line_text);

	forward_chars_with_skipping (&next, offset, visible_only, !slice, FALSE);

	/* If match start needs to be returned, set it to the
	 * start of the search string.
	 */
	if (match_start)
	{
		*match_start = next;
	}

	/* Go to end of search string */
	forward_chars_with_skipping (&next, g_utf8_strlen (*lines, -1), visible_only, !slice, TRUE);

	g_free (line_text);

	++lines;

	if (match_end)
		*match_end = next;

	/* try to match the rest of the lines forward, passing NULL
	 * for match_start so lines_match will try to match the entire
	 * line */
	return lines_match (&next, lines, visible_only,
			    slice, NULL, match_end);
}

gboolean
empathy_text_iter_backward_search (const GtkTextIter   *iter,
				  const gchar         *str,
				  GtkTextIter         *match_start,
				  GtkTextIter         *match_end,
				  const GtkTextIter   *limit)
{
	gchar **lines = NULL;
	GtkTextIter match;
	gboolean retval = FALSE;
	GtkTextIter search;
	gboolean visible_only;
	gboolean slice;

	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (str != NULL, FALSE);

	if (limit && gtk_text_iter_compare (iter, limit) <= 0)
		return FALSE;

	if (*str == '\0')
	{
		/* If we can move one char, return the empty string there */
		match = *iter;

		if (gtk_text_iter_backward_char (&match))
		{
			if (limit && gtk_text_iter_equal (&match, limit))
				return FALSE;

			if (match_start)
				*match_start = match;
			if (match_end)
				*match_end = match;
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}

	visible_only = TRUE;
	slice = TRUE;

	/* locate all lines */
	lines = strbreakup (str, "\n", -1);

	search = *iter;

	while (TRUE)
	{
		/* This loop has an inefficient worst-case, where
		 * gtk_text_iter_get_text () is called repeatedly on
		 * a single line.
		 */
		GtkTextIter end;

		if (limit && gtk_text_iter_compare (&search, limit) <= 0)
			break;

		if (backward_lines_match (&search, (const gchar**)lines,
					  visible_only, slice, &match, &end))
		{
			if (limit == NULL || (limit &&
					      gtk_text_iter_compare (&end, limit) > 0))
			{
				retval = TRUE;

				if (match_start)
					*match_start = match;
				if (match_end)
					*match_end = end;
			}
			break;
		}

		if (gtk_text_iter_get_line_offset (&search) == 0)
		{
			if (!gtk_text_iter_backward_line (&search))
				break;
		}
		else
		{
			gtk_text_iter_set_line_offset (&search, 0);
		}
	}

	g_strfreev ((gchar**)lines);

	return retval;
}

gboolean
empathy_window_get_is_visible (GtkWindow *window)
{
	GdkWindowState  state;
	GdkWindow      *gdk_window;

	g_return_val_if_fail (GTK_IS_WINDOW (window), FALSE);

	gdk_window = GTK_WIDGET (window)->window;
	if (!gdk_window) {
		return FALSE;
	}

	state = gdk_window_get_state (gdk_window);
	if (state & (GDK_WINDOW_STATE_WITHDRAWN | GDK_WINDOW_STATE_ICONIFIED)) {
		return FALSE;
	}

	return TRUE;
}

void 
empathy_window_iconify (GtkWindow *window, GtkStatusIcon *status_icon)
{
	GdkRectangle  icon_location;
	gulong        data[4];
	Display      *dpy;
	GdkWindow    *gdk_window;

	gtk_status_icon_get_geometry (status_icon, NULL, &icon_location, NULL);
	gdk_window = GTK_WIDGET (window)->window;
	dpy = gdk_x11_drawable_get_xdisplay (gdk_window);

	data[0] = icon_location.x;
	data[1] = icon_location.y;
	data[2] = icon_location.width;
	data[3] = icon_location.height;

	XChangeProperty (dpy,
			 GDK_WINDOW_XID (gdk_window),
			 gdk_x11_get_xatom_by_name_for_display (gdk_drawable_get_display (gdk_window),
			 "_NET_WM_ICON_GEOMETRY"),
			 XA_CARDINAL, 32, PropModeReplace,
			 (guchar *)&data, 4);

	gtk_window_set_skip_taskbar_hint (window, TRUE);
	gtk_window_iconify (window);
}

/* Takes care of moving the window to the current workspace. */
void
empathy_window_present (GtkWindow *window,
			gboolean   steal_focus)
{
	guint32 timestamp;

	g_return_if_fail (GTK_IS_WINDOW (window));

	/* There are three cases: hidden, visible, visible on another
	 * workspace.
	 */

	if (!empathy_window_get_is_visible (window)) {
		/* Hide it so present brings it to the current workspace. */
		gtk_widget_hide (GTK_WIDGET (window));
	}

	timestamp = gtk_get_current_event_time ();
	gtk_window_set_skip_taskbar_hint (window, FALSE);
	gtk_window_present_with_time (window, timestamp);
	/* FIXME: This shouldn't be required as gtk_window_present's doc says
	 *        it deiconify automatically. */
	gtk_window_deiconify (window);
}

GtkWindow *
empathy_get_toplevel_window (GtkWidget *widget)
{
	GtkWidget *toplevel;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	toplevel = gtk_widget_get_toplevel (widget);
	if (GTK_IS_WINDOW (toplevel) &&
	    GTK_WIDGET_TOPLEVEL (toplevel)) {
		return GTK_WINDOW (toplevel);
	}

	return NULL;
}

/* The URL opening code can't handle schemeless strings, so we try to be
 * smart and add http if there is no scheme or doesn't look like a mail
 * address. This should work in most cases, and let us click on strings
 * like "www.gnome.org".
 */
static gchar *
fixup_url (const gchar *url)
{
	if (g_str_has_prefix (url, "ghelp:") ||
	    g_str_has_prefix (url, "mailto:") ||
	    strstr (url, ":/")) {
		return NULL;
	}

	if (strstr (url, "@")) {
		return g_strdup_printf ("mailto:%s", url);
	}

	return g_strdup_printf ("http://%s", url);
}

void
empathy_url_show (GtkWidget *parent,
		  const char *url)
{
	gchar  *real_url;
	GError *error = NULL;

	real_url = fixup_url (url);
	if (real_url) {
		url = real_url;
	}

	gtk_show_uri (gtk_widget_get_screen (parent), url,
		      gtk_get_current_event_time (), &error);

	if (error) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL, 0,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, 
						 _("Unable to open URI"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  "%s", error->message);

		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);
		gtk_window_present (GTK_WINDOW (dialog));

		g_clear_error (&error);
	}

	g_free (real_url);
}

static void
link_button_hook (GtkLinkButton *button,
		  const gchar *link,
		  gpointer user_data)
{
	empathy_url_show (GTK_WIDGET (button), link);
}

GtkWidget *
empathy_link_button_new (const gchar *url,
			const gchar *title)
{
	static gboolean hook = FALSE;

	if (!hook) {
		hook = TRUE;
		gtk_link_button_set_uri_hook (link_button_hook, NULL, NULL);
	}

	return gtk_link_button_new_with_label (url, title);
}

void
empathy_toggle_button_set_state_quietly (GtkWidget *widget,
					GCallback  callback,
					gpointer   user_data,
					gboolean   active)
{
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));

	g_signal_handlers_block_by_func (widget, callback, user_data);
	g_object_set (widget, "active", active, NULL);
	g_signal_handlers_unblock_by_func (widget, callback, user_data);
}

/* Sending files with the file chooser */

static void
file_manager_send_file_request_cb (EmpathyDispatchOperation *operation,
				   const GError *error, gpointer user_data)
{
	GFile *file = (GFile *)user_data;
	EmpathyTpFile *tp_file;

	if (error != NULL) {
		DEBUG ("Couldn't request channel: %s", error->message);
		g_object_unref (file);
		return;
	}

	DEBUG ("Starting to send file");

	tp_file = EMPATHY_TP_FILE (
		empathy_dispatch_operation_get_channel_wrapper (operation));

	empathy_tp_file_offer (tp_file, file, NULL);

	g_object_unref (file);
}

static void
file_manager_send_file_response_cb (GtkDialog      *widget,
				    gint            response_id,
				    EmpathyContact *contact)
{
	if (response_id == GTK_RESPONSE_OK) {
		GSList *list;
		GSList *l;

		list = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (widget));

		DEBUG ("File chooser selected files:");

		for (l = list; l; l = l->next) {
			gchar            *uri;
			GFile            *gfile;
			GFileInfo        *info;
			GtkRecentManager *manager;
			gchar *filename;
			GTimeVal mtime;
			GError *error = NULL;

			uri = l->data;
			gfile = g_file_new_for_uri (uri);
			info = g_file_query_info (gfile,
				G_FILE_ATTRIBUTE_STANDARD_SIZE ","
				G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				G_FILE_ATTRIBUTE_TIME_MODIFIED,
				0, NULL, &error);

			if (error) {
				DEBUG ("Can't get info about the file: %s", error->message);
				g_clear_error (&error);
				g_object_unref (gfile);
				continue;
			}

			DEBUG ("\t%s", uri);
			filename = g_file_get_basename (gfile);
			g_file_info_get_modification_time (info, &mtime);

			empathy_dispatcher_send_file_to_contact (contact,
				filename, g_file_info_get_size (info), mtime.tv_sec,
				g_file_info_get_content_type (info),
				file_manager_send_file_request_cb, gfile);

			g_free (filename);
			g_object_unref (info);

			manager = gtk_recent_manager_get_default ();
			gtk_recent_manager_add_item (manager, uri);

			g_free (uri);
		}
		g_slist_free (list);
	}

	gtk_widget_destroy (GTK_WIDGET (widget));
}

void
empathy_send_file_with_file_chooser (EmpathyContact *contact)
{
	GtkWidget               *widget;
	GtkWidget               *button;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	DEBUG ("Creating selection file chooser");

	widget = gtk_file_chooser_dialog_new (_("Select a file"),
					      NULL,
					      GTK_FILE_CHOOSER_ACTION_OPEN,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      NULL);

	/* send button */
	button = gtk_button_new_with_mnemonic (_("_Send"));
	gtk_button_set_image (GTK_BUTTON (button),
		gtk_image_new_from_icon_name (EMPATHY_IMAGE_DOCUMENT_SEND,
					      GTK_ICON_SIZE_BUTTON));
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (widget), button,
				      GTK_RESPONSE_OK);
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_dialog_set_default_response (GTK_DIALOG (widget),
					 GTK_RESPONSE_OK);

	g_signal_connect (widget, "response",
			  G_CALLBACK (file_manager_send_file_response_cb),
			  contact);

	gtk_widget_show (widget);
}

typedef struct {
	EmpathySound sound_id;
	const char * event_ca_id;
	const char * event_ca_description;
	const char * gconf_key;
} EmpathySoundEntry;

/* NOTE: these entries MUST be in the same order than EmpathySound enum */
static EmpathySoundEntry sound_entries[LAST_EMPATHY_SOUND] = {
	{ EMPATHY_SOUND_MESSAGE_INCOMING, "message-new-instant",
	  N_("Received an instant message"), EMPATHY_PREFS_SOUNDS_INCOMING_MESSAGE } ,
	{ EMPATHY_SOUND_MESSAGE_OUTGOING, "message-sent-instant",
	  N_("Sent an instant message"), EMPATHY_PREFS_SOUNDS_OUTGOING_MESSAGE } ,
	{ EMPATHY_SOUND_CONVERSATION_NEW, "message-new-instant",
	  N_("Incoming chat request"), EMPATHY_PREFS_SOUNDS_NEW_CONVERSATION },
	{ EMPATHY_SOUND_CONTACT_CONNECTED, "service-login",
	  N_("Contact connected"), EMPATHY_PREFS_SOUNDS_CONTACT_LOGIN },
	{ EMPATHY_SOUND_CONTACT_DISCONNECTED, "service-logout",
	  N_("Contact disconnected"), EMPATHY_PREFS_SOUNDS_CONTACT_LOGOUT },
	{ EMPATHY_SOUND_ACCOUNT_CONNECTED, "service-login",
	  N_("Connected to server"), EMPATHY_PREFS_SOUNDS_SERVICE_LOGIN },
	{ EMPATHY_SOUND_ACCOUNT_DISCONNECTED, "service-logout",
	  N_("Disconnected from server"), EMPATHY_PREFS_SOUNDS_SERVICE_LOGOUT },
	{ EMPATHY_SOUND_PHONE_INCOMING, "phone-incoming-call",
	  N_("Incoming voice call"), NULL },
	{ EMPATHY_SOUND_PHONE_OUTGOING, "phone-outgoing-calling",
	  N_("Outgoing voice call"), NULL },
	{ EMPATHY_SOUND_PHONE_HANGUP, "phone-hangup",
	  N_("Voice call ended"), NULL },
};


static gboolean
empathy_sound_pref_is_enabled (const char *key)
{
	EmpathyConf *conf;
	gboolean res;

	conf = empathy_conf_get ();
	res = FALSE;

	empathy_conf_get_bool (conf, EMPATHY_PREFS_SOUNDS_ENABLED, &res);

	if (!res) {
		return FALSE;
	}

	if (!empathy_check_available_state ()) {
		empathy_conf_get_bool (conf, EMPATHY_PREFS_SOUNDS_DISABLED_AWAY,
				       &res);
		if (res) {
			return FALSE;
		}
	}

	empathy_conf_get_bool (conf, key, &res);

	return res;
}

void
empathy_sound_stop (EmpathySound sound_id)
{
	EmpathySoundEntry *entry;

	g_return_if_fail (sound_id < LAST_EMPATHY_SOUND);

	entry = &(sound_entries[sound_id]);
	g_return_if_fail (entry->sound_id == sound_id);

	ca_context_cancel (ca_gtk_context_get (), entry->sound_id);
}


gboolean
empathy_sound_play_full (GtkWidget *widget, EmpathySound sound_id,
	ca_finish_callback_t callback, gpointer user_data)
{
	EmpathySoundEntry *entry;
	gboolean should_play = TRUE;
	ca_proplist *p = NULL;
	ca_context *c;

	g_return_val_if_fail (sound_id < LAST_EMPATHY_SOUND, FALSE);

	entry = &(sound_entries[sound_id]);
	g_return_val_if_fail (entry->sound_id == sound_id, FALSE);

	if (entry->gconf_key != NULL) {
		should_play = empathy_sound_pref_is_enabled (entry->gconf_key);
	}

	if (!should_play)
		return FALSE;

	c = ca_gtk_context_get ();
	ca_context_cancel (c, entry->sound_id);

	DEBUG ("Play sound \"%s\" (%s)",
	       entry->event_ca_id,
	       entry->event_ca_description);

	if (ca_proplist_create(&p) < 0)
		goto failed;

	if (ca_proplist_sets (p, CA_PROP_EVENT_ID, entry->event_ca_id) < 0)
		goto failed;

	if (ca_proplist_sets (p, CA_PROP_EVENT_DESCRIPTION,
			gettext (entry->event_ca_id)) < 0)
		goto failed;

	if (ca_gtk_proplist_set_for_widget (p, widget) < 0)
		goto failed;

	ca_context_play_full (ca_gtk_context_get (), entry->sound_id,
		p, callback, user_data);

	ca_proplist_destroy (p);

	return TRUE;

failed:
	if (p != NULL)
		ca_proplist_destroy (p);

	return FALSE;
}

void
empathy_sound_play (GtkWidget *widget, EmpathySound sound_id)
{
	empathy_sound_play_full (widget, sound_id, NULL, NULL);
}

