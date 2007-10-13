/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "empathy-time.h"

/* Note: EmpathyTime is always in UTC. */

time_t
empathy_time_get_current (void)
{
	return time (NULL);
}

time_t
empathy_time_get_local_time (struct tm *tm)
{
	const gchar *timezone;
	time_t       t;
	
	timezone = g_getenv ("TZ");
	g_setenv ("TZ", "", TRUE);

	tzset ();

	t = mktime (tm);

	if (timezone) {
		g_setenv ("TZ", timezone, TRUE);
	} else {
		g_unsetenv ("TZ");
	}

	tzset ();

	return t;
}

/* The format is: "20021209T23:51:30" and is in UTC. 0 is returned on
 * failure. The alternative format "20021209" is also accepted.
 */
time_t
empathy_time_parse (const gchar *str)
{
	struct tm tm;
	gint      year, month;
	gint      n_parsed;

	memset (&tm, 0, sizeof (struct tm));

	n_parsed = sscanf (str, "%4d%2d%2dT%2d:%2d:%2d",
		    &year, &month, &tm.tm_mday, &tm.tm_hour,
			   &tm.tm_min, &tm.tm_sec);
	if (n_parsed != 3 && n_parsed != 6) {
		return 0;
	}

	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_isdst = -1;

	return empathy_time_get_local_time (&tm);
}

/* Converts the UTC timestamp to a string, also in UTC. Returns NULL on failure. */
gchar *
empathy_time_to_string_utc (time_t       t,
			    const gchar *format)
{
	gchar      stamp[128];
	struct tm *tm;

	g_return_val_if_fail (format != NULL, NULL);

	tm = gmtime (&t);
	if (strftime (stamp, sizeof (stamp), format, tm) == 0) {
		return NULL;
	}

	return g_strdup (stamp);
}

/* Converts the UTC timestamp to a string, in local time. Returns NULL on failure. */
gchar *
empathy_time_to_string_local (time_t       t,
			      const gchar *format)
{
	gchar      stamp[128];
	struct tm *tm;

	g_return_val_if_fail (format != NULL, NULL);

	tm = localtime (&t);
	if (strftime (stamp, sizeof (stamp), format, tm) == 0) {
		return NULL;
	}

	return g_strdup (stamp);
}

