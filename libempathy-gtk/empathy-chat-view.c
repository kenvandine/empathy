/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Collabora Ltd.
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

#include "empathy-chat-view.h"
#include "empathy-smiley-manager.h"

static void chat_view_base_init (gpointer klass);

GType
empathy_chat_view_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo type_info = {
			sizeof (EmpathyChatViewIface),
			chat_view_base_init,
			NULL,
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EmpathyChatView",
					       &type_info, 0);
		
		g_type_interface_add_prerequisite (type, GTK_TYPE_WIDGET);
	}
	
	return type;
}

static void
chat_view_base_init (gpointer klass)
{
	static gboolean initialized = FALSE;
	
	if (!initialized) {
		initialized = TRUE;
	}
}

void
empathy_chat_view_append_message (EmpathyChatView *view,
				  EmpathyMessage  *msg)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->append_message) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->append_message (view, 
									 msg);
	}
}

void
empathy_chat_view_append_event (EmpathyChatView *view,
				const gchar    *str)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->append_event) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->append_event (view,
								       str);
	}
}

void
empathy_chat_view_scroll (EmpathyChatView *view,
			  gboolean        allow_scrolling)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->scroll) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->scroll (view, 
								 allow_scrolling);
	}
}

void
empathy_chat_view_scroll_down (EmpathyChatView *view)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->scroll_down) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->scroll_down (view);
	}
}

gboolean
empathy_chat_view_get_has_selection (EmpathyChatView *view)
{
	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), FALSE);
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->get_has_selection) {
		return EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->get_has_selection (view);
	}
	return FALSE;
}

void
empathy_chat_view_clear (EmpathyChatView *view)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->clear) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->clear (view);
	}
}

gboolean
empathy_chat_view_find_previous (EmpathyChatView *view,
				 const gchar    *search_criteria,
				 gboolean        new_search)
{
	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), FALSE);
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->find_previous) {
		return EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->find_previous (view, 
									       search_criteria, 
									       new_search);
	}
	return FALSE;
}

gboolean
empathy_chat_view_find_next (EmpathyChatView *view,
			     const gchar    *search_criteria,
			     gboolean        new_search)
{
	g_return_val_if_fail (EMPATHY_IS_CHAT_VIEW (view), FALSE);
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->find_next) {
		return EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->find_next (view, 
									   search_criteria, 
									   new_search);
	}
	return FALSE;
}


void
empathy_chat_view_find_abilities (EmpathyChatView *view,
				  const gchar    *search_criteria,
				  gboolean       *can_do_previous,
				  gboolean       *can_do_next)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->find_abilities) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->find_abilities (view, 
									 search_criteria, 
									 can_do_previous, 
									 can_do_next);
	}
}

void
empathy_chat_view_highlight (EmpathyChatView *view,
			     const gchar     *text)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->highlight) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->highlight (view, text);
	}
}

void
empathy_chat_view_copy_clipboard (EmpathyChatView *view)
{
	g_return_if_fail (EMPATHY_IS_CHAT_VIEW (view));
	
	if (EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->copy_clipboard) {
		EMPATHY_TYPE_CHAT_VIEW_GET_IFACE (view)->copy_clipboard (view);
	}
}

