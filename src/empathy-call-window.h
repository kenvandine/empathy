/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Elliot Fairweather
 * Copyright (C) 2007-2008 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Elliot Fairweather <elliot.fairweather@collabora.co.uk>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_CALL_WINDOW_H__
#define __EMPATHY_CALL_WINDOW_H__

#include <gtk/gtk.h>

#include <telepathy-glib/channel.h>

G_BEGIN_DECLS

GtkWidget *empathy_call_window_new (TpChannel *channel);
GtkWidget *empathy_call_window_find (TpChannel *channel);
void empathy_call_window_set_channel (GtkWidget *window,
    TpChannel *channel);

G_END_DECLS

#endif /* __EMPATHY_CALL_WINDOW_H__ */
