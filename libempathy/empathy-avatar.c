/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Xavier Claessens <xclaesse@gmail.com>
 * Copyright (C) 2007 Collabora Ltd.
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

#include "empathy-avatar.h"
#include "empathy-utils.h"
#include "empathy-debug.h"

#define DEBUG_DOMAIN "Avatar"

GType
empathy_avatar_get_type (void)
{
	static GType type_id = 0;

	if (!type_id) {
		type_id = g_boxed_type_register_static ("EmpathyAvatar",
							(GBoxedCopyFunc) empathy_avatar_ref,
							(GBoxedFreeFunc) empathy_avatar_unref);
	}

	return type_id;
}

static gchar *
avatar_get_filename (const gchar *token)
{
	gchar *avatar_path;
	gchar *avatar_file;
	gchar *token_escaped;

	avatar_path = g_build_filename (g_get_home_dir (),
					".gnome2",
					PACKAGE_NAME,
					"avatars",
					NULL);
	g_mkdir_with_parents (avatar_path, 0700);

	token_escaped = empathy_escape_as_identifier (token);
	avatar_file = g_build_filename (avatar_path, token_escaped, NULL);

	g_free (token_escaped);
	g_free (avatar_path);

	return avatar_file;
}

static EmpathyAvatar *
avatar_new (guchar *data,
	    gsize   len,
	    gchar  *format,
	    gchar  *token)
{
	EmpathyAvatar *avatar;

	avatar = g_slice_new0 (EmpathyAvatar);
	avatar->data = data;
	avatar->len = len;
	avatar->format = format;
	avatar->token = token;
	avatar->refcount = 1;

	return avatar;
}

EmpathyAvatar *
empathy_avatar_new (const guchar *data,
		    const gsize   len,
		    const gchar  *format,
		    const gchar  *token)
{
	EmpathyAvatar *avatar;
	gchar         *filename;
	GError        *error = NULL;

	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (len > 0, NULL);
	g_return_val_if_fail (format != NULL, NULL);
	g_return_val_if_fail (!G_STR_EMPTY (token), NULL);

	avatar = avatar_new (g_memdup (data, len),
			     len,
			     g_strdup (format),
			     g_strdup (token));

	/* Save to cache if not yet in it */
	filename = avatar_get_filename (token);
	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		if (!g_file_set_contents (filename, data, len, &error)) {
			empathy_debug (DEBUG_DOMAIN,
				       "Failed to save avatar in cache: %s",
				       error ? error->message : "No error given");
			g_clear_error (&error);
		} else {
			empathy_debug (DEBUG_DOMAIN, "Avatar saved to %s", filename);
		}
	}
	g_free (filename);

	return avatar;
}

EmpathyAvatar *
empathy_avatar_new_from_cache (const gchar *token)
{
	EmpathyAvatar *avatar = NULL;
	gchar         *filename;
	gchar         *data = NULL;
	gsize          len;
	GError        *error = NULL;

	g_return_val_if_fail (!G_STR_EMPTY (token), NULL);

	/* Load the avatar from file if it exists */
	filename = avatar_get_filename (token);
	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		if (!g_file_get_contents (filename, &data, &len, &error)) {
			empathy_debug (DEBUG_DOMAIN,
				       "Failed to load avatar from cache: %s",
				       error ? error->message : "No error given");
			g_clear_error (&error);
		}
	}

	if (data) {
		empathy_debug (DEBUG_DOMAIN, "Avatar loaded from %s", filename);
		avatar = avatar_new (data, len, NULL, g_strdup (token));
	}

	g_free (filename);

	return avatar;
}

void
empathy_avatar_unref (EmpathyAvatar *avatar)
{
	g_return_if_fail (avatar != NULL);

	avatar->refcount--;
	if (avatar->refcount == 0) {
		g_free (avatar->data);
		g_free (avatar->format);
		g_free (avatar->token);
		g_slice_free (EmpathyAvatar, avatar);
	}
}

EmpathyAvatar *
empathy_avatar_ref (EmpathyAvatar *avatar)
{
	g_return_val_if_fail (avatar != NULL, NULL);

	avatar->refcount++;

	return avatar;
}

