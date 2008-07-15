/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Collabora Ltd.
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include "config.h"

#include "empathy-theme-adium.h"

#define DEBUG_FLAG EMPATHY_DEBUG_CHAT
#include <libempathy/empathy-debug.h>

#define GET_PRIV(obj) EMPATHY_GET_PRIV (obj, EmpathyThemeAdium)

typedef struct {
	guint unused;
} EmpathyThemeAdiumPriv;

static void theme_adium_iface_init (EmpathyChatViewIface *iface);

G_DEFINE_TYPE_WITH_CODE (EmpathyThemeAdium, empathy_theme_adium,
			 GTK_TYPE_TEXT_VIEW,
			 G_IMPLEMENT_INTERFACE (EMPATHY_TYPE_CHAT_VIEW,
						theme_adium_iface_init));

static void
theme_adium_finalize (GObject *object)
{
	G_OBJECT_CLASS (empathy_theme_adium_parent_class)->finalize (object);
}

static void
empathy_theme_adium_class_init (EmpathyThemeAdiumClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = theme_adium_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyThemeAdiumPriv));
}

static void
empathy_theme_adium_init (EmpathyThemeAdium *view)
{
	EmpathyThemeAdiumPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (view,
		EMPATHY_TYPE_THEME_ADIUM, EmpathyThemeAdiumPriv);

	view->priv = priv;	
}

static void
theme_adium_scroll_down (EmpathyChatView *view)
{
}

static void
theme_adium_append_message (EmpathyChatView *view,
			    EmpathyMessage  *msg)
{
}

static void
theme_adium_append_event (EmpathyChatView *view,
			  const gchar     *str)
{
}

static void
theme_adium_scroll (EmpathyChatView *view,
		    gboolean         allow_scrolling)
{
}

static gboolean
theme_adium_get_has_selection (EmpathyChatView *view)
{
	return FALSE;
}

static void
theme_adium_clear (EmpathyChatView *view)
{
}

static gboolean
theme_adium_find_previous (EmpathyChatView *view,
			   const gchar     *search_criteria,
			   gboolean         new_search)
{
	return FALSE;
}

static gboolean
theme_adium_find_next (EmpathyChatView *view,
		       const gchar     *search_criteria,
		       gboolean         new_search)
{
	return FALSE;
}

static void
theme_adium_find_abilities (EmpathyChatView *view,
			    const gchar    *search_criteria,
			    gboolean       *can_do_previous,
			    gboolean       *can_do_next)
{
}

static void
theme_adium_highlight (EmpathyChatView *view,
		       const gchar     *text)
{
}

static void
theme_adium_copy_clipboard (EmpathyChatView *view)
{
}

static void
theme_adium_iface_init (EmpathyChatViewIface *iface)
{
	iface->append_message = theme_adium_append_message;
	iface->append_event = theme_adium_append_event;
	iface->scroll = theme_adium_scroll;
	iface->scroll_down = theme_adium_scroll_down;
	iface->get_has_selection = theme_adium_get_has_selection;
	iface->clear = theme_adium_clear;
	iface->find_previous = theme_adium_find_previous;
	iface->find_next = theme_adium_find_next;
	iface->find_abilities = theme_adium_find_abilities;
	iface->highlight = theme_adium_highlight;
	iface->copy_clipboard = theme_adium_copy_clipboard;
}

EmpathyThemeAdium *
empathy_theme_adium_new (void)
{
	return g_object_new (EMPATHY_TYPE_THEME_ADIUM, NULL);
}

