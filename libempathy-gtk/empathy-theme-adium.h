/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008-2009 Collabora Ltd.
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

#ifndef __EMPATHY_THEME_ADIUM_H__
#define __EMPATHY_THEME_ADIUM_H__

#include <webkit/webkitwebview.h>

#include "empathy-chat-view.h"

G_BEGIN_DECLS

#define EMPATHY_TYPE_THEME_ADIUM         (empathy_theme_adium_get_type ())
#define EMPATHY_THEME_ADIUM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_THEME_ADIUM, EmpathyThemeAdium))
#define EMPATHY_THEME_ADIUM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EMPATHY_TYPE_THEME_ADIUM, EmpathyThemeAdiumClass))
#define EMPATHY_IS_THEME_ADIUM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_THEME_ADIUM))
#define EMPATHY_IS_THEME_ADIUM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_THEME_ADIUM))
#define EMPATHY_THEME_ADIUM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_THEME_ADIUM, EmpathyThemeAdiumClass))

typedef struct _EmpathyThemeAdium      EmpathyThemeAdium;
typedef struct _EmpathyThemeAdiumClass EmpathyThemeAdiumClass;
typedef struct _EmpathyAdiumData       EmpathyAdiumData;

struct _EmpathyThemeAdium {
	WebKitWebView parent;
	gpointer priv;
};

struct _EmpathyThemeAdiumClass {
	WebKitWebViewClass parent_class;
};

GType              empathy_theme_adium_get_type (void) G_GNUC_CONST;
EmpathyThemeAdium *empathy_theme_adium_new      (EmpathyAdiumData *data);

gboolean           empathy_adium_path_is_valid (const gchar *path);
GHashTable        *empathy_adium_info_new (const gchar *path);

#define EMPATHY_TYPE_ADIUM_DATA (empathy_adium_data_get_type ())
GType              empathy_adium_data_get_type (void) G_GNUC_CONST;
EmpathyAdiumData  *empathy_adium_data_new (const gchar *path);
EmpathyAdiumData  *empathy_adium_data_new_with_info (const gchar *path,
						     GHashTable *info);
EmpathyAdiumData  *empathy_adium_data_ref (EmpathyAdiumData *data);
void               empathy_adium_data_unref (EmpathyAdiumData *data);
GHashTable        *empathy_adium_data_get_info (EmpathyAdiumData *data);
const gchar       *empathy_adium_data_get_path (EmpathyAdiumData *data);


G_END_DECLS

#endif /* __EMPATHY_THEME_ADIUM_H__ */
