/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Imendio AB
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

#ifndef __EMPATHY_CONF_H__
#define __EMPATHY_CONF_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define EMPATHY_TYPE_CONF         (empathy_conf_get_type ())
#define EMPATHY_CONF(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EMPATHY_TYPE_CONF, EmpathyConf))
#define EMPATHY_CONF_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), EMPATHY_TYPE_CONF, EmpathyConfClass))
#define EMPATHY_IS_CONF(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EMPATHY_TYPE_CONF))
#define EMPATHY_IS_CONF_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EMPATHY_TYPE_CONF))
#define EMPATHY_CONF_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EMPATHY_TYPE_CONF, EmpathyConfClass))

typedef struct _EmpathyConf      EmpathyConf;
typedef struct _EmpathyConfClass EmpathyConfClass;

struct _EmpathyConf  {
	GObject parent;
};

struct _EmpathyConfClass {
	GObjectClass parent_class;
};

typedef void (*EmpathyConfNotifyFunc) (EmpathyConf  *conf, 
				      const gchar *key,
				      gpointer     user_data);

GType       empathy_conf_get_type        (void) G_GNUC_CONST;
EmpathyConf *empathy_conf_get             (void);
void        empathy_conf_shutdown        (void);
guint       empathy_conf_notify_add      (EmpathyConf            *conf,
					 const gchar           *key,
					 EmpathyConfNotifyFunc   func,
					 gpointer               data);
gboolean    empathy_conf_notify_remove   (EmpathyConf            *conf,
					 guint                  id);
gboolean    empathy_conf_set_int         (EmpathyConf            *conf,
					 const gchar           *key,
					 gint                   value);
gboolean    empathy_conf_get_int         (EmpathyConf            *conf,
					 const gchar           *key,
					 gint                  *value);
gboolean    empathy_conf_set_bool        (EmpathyConf            *conf,
					 const gchar           *key,
					 gboolean               value);
gboolean    empathy_conf_get_bool        (EmpathyConf            *conf,
					 const gchar           *key,
					 gboolean              *value);
gboolean    empathy_conf_set_string      (EmpathyConf            *conf,
					 const gchar           *key,
					 const gchar           *value);
gboolean    empathy_conf_get_string      (EmpathyConf            *conf,
					 const gchar           *key,
					 gchar                **value);
gboolean    empathy_conf_set_string_list (EmpathyConf            *conf,
					 const gchar           *key,
					 GSList                *value);
gboolean    empathy_conf_get_string_list (EmpathyConf            *conf,
					 const gchar           *key,
					 GSList              **value);

G_END_DECLS

#endif /* __EMPATHY_CONF_H__ */

