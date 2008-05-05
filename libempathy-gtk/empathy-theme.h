/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Imendio AB
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
 */

#ifndef __EMPATHY_THEME_H__
#define __EMPATHY_THEME_H__

#include <glib-object.h>
#include <gtk/gtktextbuffer.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_THEME            (empathy_theme_get_type ())
#define EMPATHY_THEME(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_THEME, EmpathyTheme))
#define EMPATHY_THEME_CLASS(k)        (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_THEME, EmpathyThemeClass))
#define EMPATHY_IS_THEME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_THEME))
#define EMPATHY_IS_THEME_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_THEME))
#define EMPATHY_THEME_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_THEME, EmpathyThemeClass))

typedef struct _EmpathyTheme      EmpathyTheme;
typedef struct _EmpathyThemeClass EmpathyThemeClass;

#include "empathy-chat-view.h"

struct _EmpathyTheme {
	GObject parent;
	gpointer priv;
};

struct _EmpathyThemeClass {
	GObjectClass parent_class;

	/* <vtable> */
	void (*update_view)         (EmpathyTheme    *theme,
				     EmpathyChatView *view);
	void (*append_message)      (EmpathyTheme    *theme,
				     EmpathyChatView *view,
				     EmpathyMessage  *message);
	void (*append_event)        (EmpathyTheme    *theme,
				     EmpathyChatView *view,
				     const gchar     *str);
	void (*append_timestamp)    (EmpathyTheme    *theme,
				     EmpathyChatView *view,
				     EmpathyMessage  *message,
				     gboolean         show_date,
				     gboolean         show_time);
	void (*append_spacing)      (EmpathyTheme    *theme,
				     EmpathyChatView *view);
};

GType          empathy_theme_get_type                   (void) G_GNUC_CONST;
void           empathy_theme_update_view                (EmpathyTheme    *theme,
							 EmpathyChatView *view);
void           empathy_theme_append_message             (EmpathyTheme    *theme,
							 EmpathyChatView *view,
							 EmpathyMessage  *msg);
void           empathy_theme_append_text                (EmpathyTheme    *theme,
							 EmpathyChatView *view,
							 const gchar     *body,
							 const gchar     *tag, 
							 const gchar     *link_tag);
void           empathy_theme_append_spacing             (EmpathyTheme    *theme,
							 EmpathyChatView *view);
void           empathy_theme_append_event               (EmpathyTheme    *theme,
							 EmpathyChatView *view,
							 const gchar     *str);
void           empathy_theme_append_timestamp           (EmpathyTheme    *theme,
							 EmpathyChatView *view,
							 EmpathyMessage  *message,
							 gboolean         show_date,
							 gboolean         show_time);
void           empathy_theme_maybe_append_date_and_time (EmpathyTheme    *theme,
							 EmpathyChatView *view,
							 EmpathyMessage  *message);
gboolean       empathy_theme_get_show_avatars           (EmpathyTheme    *theme);
void           empathy_theme_set_show_avatars           (EmpathyTheme    *theme,
							 gboolean         show);

G_END_DECLS

#endif /* __EMPATHY_THEME_H__ */

