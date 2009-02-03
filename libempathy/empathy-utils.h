/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
 * Copyright (C) 2007-2008 Collabora Ltd.
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
 *          Martyn Russell <martyn@imendio.com>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#ifndef __EMPATHY_UTILS_H__
#define __EMPATHY_UTILS_H__

#include <glib.h>
#include <glib-object.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <libmissioncontrol/mc-account.h>
#include <libmissioncontrol/mission-control.h>

#include "empathy-contact.h"

#define EMPATHY_GET_PRIV(obj,type) ((type##Priv*) ((type*)obj)->priv)
#define EMP_STR_EMPTY(x) ((x) == NULL || (x)[0] == '\0')

G_BEGIN_DECLS

void         empathy_init                           (void);
/* Strings */
gchar *      empathy_substring                      (const gchar     *str,
						    gint             start,
						    gint             end);
gint         empathy_strcasecmp                     (const gchar     *s1,
						    const gchar     *s2);
gint         empathy_strncasecmp                    (const gchar     *s1,
						    const gchar     *s2,
						    gsize            n);

/* XML */
gboolean     empathy_xml_validate                   (xmlDoc          *doc,
						    const gchar     *dtd_filename);
xmlNodePtr   empathy_xml_node_get_child             (xmlNodePtr       node,
						    const gchar     *child_name);
xmlChar *    empathy_xml_node_get_child_content     (xmlNodePtr       node,
						    const gchar     *child_name);
xmlNodePtr   empathy_xml_node_find_child_prop_value (xmlNodePtr       node,
						    const gchar     *prop_name,
						    const gchar     *prop_value);

/* Others */
guint        empathy_account_hash                   (gconstpointer    key);
gboolean     empathy_account_equal                  (gconstpointer    a,
						    gconstpointer    b);
MissionControl *empathy_mission_control_dup_singleton (void);
const gchar * empathy_presence_get_default_message  (McPresence       presence);
const gchar * empathy_presence_to_str               (McPresence       presence);
McPresence    empathy_presence_from_str             (const gchar     *str);
gchar *       empathy_file_lookup                   (const gchar     *filename,
						     const gchar     *subdir);

typedef gboolean (*EmpathyRunUntilReadyFunc)        (GObject         *object,
						     gpointer         user_data);
void          empathy_run_until_ready               (gpointer         object);
void          empathy_run_until_ready_full          (gpointer         object,
						     const gchar     *signal,
						     EmpathyRunUntilReadyFunc  func,
						     gpointer         user_data,
						     GMainLoop      **loop);
McAccount *  empathy_channel_get_account            (TpChannel       *channel);
gboolean     empathy_proxy_equal                    (gconstpointer    a,
						     gconstpointer    b);
guint        empathy_proxy_hash                     (gconstpointer    key);
gboolean     empathy_check_available_state          (void);

G_END_DECLS

#endif /*  __EMPATHY_UTILS_H__ */
