/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2007 Imendio AB
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
 */

#include "config.h"

#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

#include <telepathy-glib/debug.h>

/* Set EMPATHY_DEBUG to a colon/comma/space separated list of domains, or "all"
 * to get all debug output.
 */

#include "empathy-debug.h"

static gchar    **debug_strv;
static gboolean   all_domains = FALSE;

static void
debug_init (void)
{
	static gboolean inited = FALSE;

	if (!inited) {
		const gchar *env;
		gint         i;

		env = g_getenv ("EMPATHY_DEBUG");
		tp_debug_set_flags (env);

		if (env) {
			debug_strv = g_strsplit_set (env, ":, ", 0);
		} else {
			debug_strv = NULL;
		}

		for (i = 0; debug_strv && debug_strv[i]; i++) {
			if (strcmp ("all", debug_strv[i]) == 0) {
				all_domains = TRUE;
			}
		}

		inited = TRUE;
	}
}

void
empathy_debug_impl (const gchar *domain, const gchar *msg, ...)
{
	gint i;

	g_return_if_fail (domain != NULL);
	g_return_if_fail (msg != NULL);

	debug_init ();

	for (i = 0; debug_strv && debug_strv[i]; i++) {
		if (all_domains || strcmp (domain, debug_strv[i]) == 0) {
			va_list args;

			g_print ("%s: ", domain);

			va_start (args, msg);
			g_vprintf (msg, args);
			va_end (args);

			g_print ("\n");
			break;
		}
	}
}

void
empathy_debug_set_log_file_from_env (void)
{
	const gchar *output_file;
	gint         out;

	output_file = g_getenv ("EMPATHY_LOGFILE");
	if (output_file == NULL) {
		return;
	}

	out = g_open (output_file, O_WRONLY | O_CREAT, 0644);
	if (out == -1) {
		g_warning ("Can't open logfile '%s': %s", output_file,
			   g_strerror (errno));
		return;
	}

	if (dup2 (out, STDOUT_FILENO) == -1) {
		g_warning ("Error when duplicating stdout file descriptor: %s",
			   g_strerror (errno));
		return;
	}

	if (dup2 (out, STDERR_FILENO) == -1) {
		g_warning ("Error when duplicating stderr file descriptor: %s",
			   g_strerror (errno));
		return;
	}
}

