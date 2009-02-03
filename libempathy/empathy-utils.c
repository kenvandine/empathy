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

#include "config.h"

#include <string.h>
#include <time.h>
#include <sys/types.h>

#include <glib/gi18n-lib.h>

#include <libxml/uri.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/dbus.h>

#include "empathy-utils.h"
#include "empathy-contact-factory.h"
#include "empathy-contact-manager.h"
#include "empathy-dispatcher.h"
#include "empathy-dispatch-operation.h"
#include "empathy-idle.h"
#include "empathy-tp-call.h"

#include <extensions/extensions.h>

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include "empathy-debug.h"


void
empathy_init (void)
{
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	g_type_init ();

	/* Setup gettext */
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* Setup debug output for empathy and telepathy-glib */
	if (g_getenv ("EMPATHY_TIMING") != NULL) {
		g_log_set_default_handler (tp_debug_timestamped_log_handler, NULL);
	}
	empathy_debug_set_flags (g_getenv ("EMPATHY_DEBUG"));
	tp_debug_divert_messages (g_getenv ("EMPATHY_LOGFILE"));

	emp_cli_init ();

	initialized = TRUE;
}

gchar *
empathy_substring (const gchar *str,
		  gint         start,
		  gint         end)
{
	return g_strndup (str + start, end - start);
}

gint
empathy_strcasecmp (const gchar *s1,
		   const gchar *s2)
{
	return empathy_strncasecmp (s1, s2, -1);
}

gint
empathy_strncasecmp (const gchar *s1,
		    const gchar *s2,
		    gsize        n)
{
	gchar *u1, *u2;
	gint   ret_val;

	u1 = g_utf8_casefold (s1, n);
	u2 = g_utf8_casefold (s2, n);

	ret_val = g_utf8_collate (u1, u2);
	g_free (u1);
	g_free (u2);

	return ret_val;
}

gboolean
empathy_xml_validate (xmlDoc      *doc,
		     const gchar *dtd_filename)
{
	gchar        *path, *escaped;
	xmlValidCtxt  cvp;
	xmlDtd       *dtd;
	gboolean      ret;

	path = g_build_filename (g_getenv ("EMPATHY_SRCDIR"), "libempathy",
				 dtd_filename, NULL);
	if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
		g_free (path);
		path = g_build_filename (DATADIR, "empathy", dtd_filename, NULL);
	}
	DEBUG ("Loading dtd file %s", path);

	/* The list of valid chars is taken from libxml. */
	escaped = xmlURIEscapeStr (path, ":@&=+$,/?;");
	g_free (path);

	memset (&cvp, 0, sizeof (cvp));
	dtd = xmlParseDTD (NULL, escaped);
	ret = xmlValidateDtd (&cvp, doc, dtd);

	xmlFree (escaped);
	xmlFreeDtd (dtd);

	return ret;
}

xmlNodePtr
empathy_xml_node_get_child (xmlNodePtr   node, 
			   const gchar *child_name)
{
	xmlNodePtr l;

        g_return_val_if_fail (node != NULL, NULL);
        g_return_val_if_fail (child_name != NULL, NULL);

	for (l = node->children; l; l = l->next) {
		if (l->name && strcmp (l->name, child_name) == 0) {
			return l;
		}
	}

	return NULL;
}

xmlChar *
empathy_xml_node_get_child_content (xmlNodePtr   node, 
				   const gchar *child_name)
{
	xmlNodePtr l;

        g_return_val_if_fail (node != NULL, NULL);
        g_return_val_if_fail (child_name != NULL, NULL);

	l = empathy_xml_node_get_child (node, child_name);
	if (l) {
		return xmlNodeGetContent (l);
	}
		
	return NULL;
}

xmlNodePtr
empathy_xml_node_find_child_prop_value (xmlNodePtr   node, 
				       const gchar *prop_name,
				       const gchar *prop_value)
{
	xmlNodePtr l;
	xmlNodePtr found = NULL;

        g_return_val_if_fail (node != NULL, NULL);
        g_return_val_if_fail (prop_name != NULL, NULL);
        g_return_val_if_fail (prop_value != NULL, NULL);

	for (l = node->children; l && !found; l = l->next) {
		xmlChar *prop;

		if (!xmlHasProp (l, prop_name)) {
			continue;
		}

		prop = xmlGetProp (l, prop_name);
		if (prop && strcmp (prop, prop_value) == 0) {
			found = l;
		}
		
		xmlFree (prop);
	}
		
	return found;
}

guint
empathy_account_hash (gconstpointer key)
{
	g_return_val_if_fail (MC_IS_ACCOUNT (key), 0);

	return g_str_hash (mc_account_get_unique_name (MC_ACCOUNT (key)));
}

gboolean
empathy_account_equal (gconstpointer a,
		       gconstpointer b)
{
	const gchar *name_a;
	const gchar *name_b;

	g_return_val_if_fail (MC_IS_ACCOUNT (a), FALSE);
	g_return_val_if_fail (MC_IS_ACCOUNT (b), FALSE);

	name_a = mc_account_get_unique_name (MC_ACCOUNT (a));
	name_b = mc_account_get_unique_name (MC_ACCOUNT (b));

	return g_str_equal (name_a, name_b);
}

MissionControl *
empathy_mission_control_dup_singleton (void)
{
	static MissionControl *mc = NULL;

	if (!mc) {
		mc = mission_control_new (tp_get_bus ());
		g_object_add_weak_pointer (G_OBJECT (mc), (gpointer) &mc);
	} else {
		g_object_ref (mc);
	}

	return mc;
}

const gchar *
empathy_presence_get_default_message (McPresence presence)
{
	switch (presence) {
	case MC_PRESENCE_AVAILABLE:
		return _("Available");
	case MC_PRESENCE_DO_NOT_DISTURB:
		return _("Busy");
	case MC_PRESENCE_AWAY:
	case MC_PRESENCE_EXTENDED_AWAY:
		return _("Away");
	case MC_PRESENCE_HIDDEN:
		return _("Hidden");
	case MC_PRESENCE_OFFLINE:
	case MC_PRESENCE_UNSET:
		return _("Offline");
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

const gchar *
empathy_presence_to_str (McPresence presence)
{
	switch (presence) {
	case MC_PRESENCE_AVAILABLE:
		return "available";
	case MC_PRESENCE_DO_NOT_DISTURB:
		return "busy";
	case MC_PRESENCE_AWAY:
		return "away";
	case MC_PRESENCE_EXTENDED_AWAY:
		return "ext_away";
	case MC_PRESENCE_HIDDEN:
		return "hidden";
	case MC_PRESENCE_OFFLINE:
		return "offline";
	case MC_PRESENCE_UNSET:
		return "unset";
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

McPresence
empathy_presence_from_str (const gchar *str)
{
	if (strcmp (str, "available") == 0) {
		return MC_PRESENCE_AVAILABLE;
	} else if ((strcmp (str, "dnd") == 0) || (strcmp (str, "busy") == 0)) {
		return MC_PRESENCE_DO_NOT_DISTURB;
	} else if ((strcmp (str, "away") == 0) || (strcmp (str, "brb") == 0)) {
		return MC_PRESENCE_AWAY;
	} else if ((strcmp (str, "xa") == 0) || (strcmp (str, "ext_away") == 0)) {
		return MC_PRESENCE_EXTENDED_AWAY;
	} else if (strcmp (str, "hidden") == 0) {
		return MC_PRESENCE_HIDDEN;
	} else if (strcmp (str, "offline") == 0) {
		return MC_PRESENCE_OFFLINE;
	} else if (strcmp (str, "unset") == 0) {
		return MC_PRESENCE_UNSET;
	}

	return MC_PRESENCE_UNSET;
}

gchar *
empathy_file_lookup (const gchar *filename, const gchar *subdir)
{
	gchar *path;

	if (!subdir) {
		subdir = ".";
	}

	path = g_build_filename (g_getenv ("EMPATHY_SRCDIR"), subdir, filename, NULL);
	if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
		g_free (path);
		path = g_build_filename (DATADIR, "empathy", filename, NULL);
	}

	return path;
}

typedef struct {
	EmpathyRunUntilReadyFunc  func;
	gpointer                  user_data;
	GObject                  *object;
	GMainLoop                *loop;
} RunUntilReadyData;

static void
run_until_ready_cb (RunUntilReadyData *data)
{
	if (!data->func || data->func (data->object, data->user_data)) {
		DEBUG ("Object %p is ready", data->object);
		g_main_loop_quit (data->loop);
	}
}

static gboolean
object_is_ready (GObject *object,
		 gpointer user_data)
{
	gboolean ready;

	g_object_get (object, "ready", &ready, NULL);

	return ready;
}

void
empathy_run_until_ready_full (gpointer                  object,
			      const gchar              *signal,
			      EmpathyRunUntilReadyFunc  func,
			      gpointer                  user_data,
			      GMainLoop               **loop)
{
	RunUntilReadyData  data;
	gulong             signal_id;

	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (signal != NULL);

	if (func && func (object, user_data)) {
		return;
	}

	DEBUG ("Starting run until ready for object %p", object);

	data.func = func;
	data.user_data = user_data;
	data.object = object;
	data.loop = g_main_loop_new (NULL, FALSE);

	signal_id = g_signal_connect_swapped (object, signal,
					      G_CALLBACK (run_until_ready_cb),
					      &data);
	if (loop != NULL) {
		*loop = data.loop;
	}

	g_main_loop_run (data.loop);

	if (loop != NULL) {
		*loop = NULL;
	}

	g_signal_handler_disconnect (object, signal_id);
	g_main_loop_unref (data.loop);
}

void
empathy_run_until_ready (gpointer object)
{
	empathy_run_until_ready_full (object, "notify::ready", object_is_ready,
				      NULL, NULL);
}

McAccount *
empathy_channel_get_account (TpChannel *channel)
{
	TpConnection   *connection;
	McAccount      *account;
	MissionControl *mc;

	g_object_get (channel, "connection", &connection, NULL);
	mc = empathy_mission_control_dup_singleton ();
	account = mission_control_get_account_for_tpconnection (mc, connection, NULL);
	g_object_unref (connection);
	g_object_unref (mc);

	return account;
}

guint
empathy_proxy_hash (gconstpointer key)
{
	TpProxy      *proxy = TP_PROXY (key);
	TpProxyClass *proxy_class = TP_PROXY_GET_CLASS (key);

	g_return_val_if_fail (TP_IS_PROXY (proxy), 0);
	g_return_val_if_fail (proxy_class->must_have_unique_name, 0);

	return g_str_hash (proxy->object_path) ^ g_str_hash (proxy->bus_name);
}

gboolean
empathy_proxy_equal (gconstpointer a,
		     gconstpointer b)
{
	TpProxy *proxy_a = TP_PROXY (a);
	TpProxy *proxy_b = TP_PROXY (b);
	TpProxyClass *proxy_a_class = TP_PROXY_GET_CLASS (a);
	TpProxyClass *proxy_b_class = TP_PROXY_GET_CLASS (b);

	g_return_val_if_fail (TP_IS_PROXY (proxy_a), FALSE);
	g_return_val_if_fail (TP_IS_PROXY (proxy_b), FALSE);
	g_return_val_if_fail (proxy_a_class->must_have_unique_name, 0);
	g_return_val_if_fail (proxy_b_class->must_have_unique_name, 0);

	return g_str_equal (proxy_a->object_path, proxy_b->object_path) &&
	       g_str_equal (proxy_a->bus_name, proxy_b->bus_name);
}

gboolean
empathy_check_available_state (void)
{
	McPresence presence;
	EmpathyIdle *idle;

	idle = empathy_idle_dup_singleton ();
	presence = empathy_idle_get_state (idle);
	g_object_unref (idle);

	if (presence != MC_PRESENCE_AVAILABLE &&
		presence != MC_PRESENCE_UNSET) {
		return FALSE;    
	}

	return TRUE;
}
