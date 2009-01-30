/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Collabora Ltd.
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
 * Author: Cosimo Cecchi <cosimo.cecchi@collabora.co.uk>
 *
 */

#include "empathy-misc.h"

#include <libempathy/empathy-utils.h>
#include <libempathy-gtk/empathy-ui-utils.h>
#include <libempathy-gtk/empathy-conf.h>

/* public methods */

GdkPixbuf *
empathy_misc_get_pixbuf_for_notification (EmpathyContact *contact,
					  const char *icon_name)
{
	GdkPixbuf *pixbuf = NULL;

	if (contact != NULL) {
		pixbuf = empathy_pixbuf_avatar_from_contact_scaled (contact,
								    48, 48);
	}

	if (!pixbuf) {
		pixbuf = empathy_pixbuf_from_icon_name_sized
					(icon_name, 48);
	}

	return pixbuf;
}

gboolean
empathy_notification_is_enabled (void)
{
	EmpathyConf *conf;
	gboolean res;

	conf = empathy_conf_get ();
	res = FALSE;

	empathy_conf_get_bool (conf, EMPATHY_PREFS_NOTIFICATIONS_ENABLED, &res);

	if (!res) {
		return FALSE;
	}

	if (!empathy_check_available_state ()) {
		empathy_conf_get_bool (conf,
				       EMPATHY_PREFS_NOTIFICATIONS_DISABLED_AWAY,
				       &res);
		if (res) {
			return FALSE;
		}
	}

	return TRUE;
}

