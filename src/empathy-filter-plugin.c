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

#include <glib.h>

#include <mcd-dispatcher.h>
#include <mcd-dispatcher-context.h>

static void filter_plugin_text_channel (McdDispatcherContext *ctx);

static McdFilter text_in_filters[] = {
    {filter_plugin_text_channel, MCD_FILTER_PRIORITY_USER},
    {NULL, 0}
};

void
mcd_filters_init (McdDispatcher *dispatcher)
{
	mcd_dispatcher_register_filters (dispatcher,
					 text_in_filters,
					 TELEPATHY_CHAN_IFACE_TEXT_QUARK,
					 MCD_FILTER_IN);
}

static void
filter_plugin_text_channel (McdDispatcherContext *ctx)
{
	McdChannel  *channel;
	const gchar *channel_name;

	channel = mcd_dispatcher_context_get_channel (ctx);
	channel_name = mcd_channel_get_name (channel);

	if (strcmp (channel_name, "goerge.w.bush@whitehouse.com") == 0) {
		g_debug ("Blocking contact");
		mcd_dispatcher_context_process (ctx, FALSE);
		return;
	}

	mcd_dispatcher_context_process (ctx, TRUE);
}

