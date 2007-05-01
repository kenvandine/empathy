/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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

#include <config.h>

#include <glib.h>

#include <libtelepathy/tp-helpers.h>

#include <libmissioncontrol/mc-account-monitor.h>

#include "empathy-session.h"
#include "gossip-debug.h"

#define DEBUG_DOMAIN "Session"

static EmpathyContactManager *contact_manager = NULL;

void
empathy_session_finalize (void)
{
	if (contact_manager) {
		g_object_unref (contact_manager);
		contact_manager = NULL;
	}
}

EmpathyContactManager *
empathy_session_get_contact_manager (void)
{
	if (!contact_manager) {
		contact_manager = empathy_contact_manager_new ();
	}

	return contact_manager;
}

