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
#include <regex.h>

#include <glib/gi18n.h>

#include <libxml/uri.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/dbus.h>

#include "empathy-debug.h"
#include "empathy-utils.h"
#include "empathy-contact-factory.h"
#include "empathy-contact-manager.h"
#include "empathy-tp-group.h"

#define DEBUG_DOMAIN "Utils"

static void regex_init (void);

gchar *
empathy_substring (const gchar *str,
		  gint         start,
		  gint         end)
{
	return g_strndup (str + start, end - start);
}

/*
 * Regular Expression code to match urls.
 */
#define APTCHARS  "-A-Za-z0-9,-."
#define USERCHARS "-A-Za-z0-9"
#define PASSCHARS "-A-Za-z0-9,?;.:/!%$^*&~\"#'"
#define HOSTCHARS "-A-Za-z0-9_"
#define PATHCHARS "-A-Za-z0-9_$.+!*(),;:@&=?/~#%"
#define SCHEME    "(news:|telnet:|nntp:|file:/|https?:|ftps?:|webcal:)"
#define USER      "[" USERCHARS "]+(:["PASSCHARS "]+)?"
#define URLPATH   "/[" PATHCHARS "]*[^]'.}>) \t\r\n,\\\"]"

static regex_t dingus[EMPATHY_REGEX_ALL];

static void
regex_init (void)
{
	static gboolean  inited = FALSE;
	const gchar     *expression;
	gint             i;

	if (inited) {
		return;
	}

	for (i = 0; i < EMPATHY_REGEX_ALL; i++) {
		switch (i) {
		case EMPATHY_REGEX_AS_IS:
			expression =
				SCHEME "//(" USER "@)?[" HOSTCHARS ".]+"
				"(:[0-9]+)?(" URLPATH ")?";
			break;
		case EMPATHY_REGEX_BROWSER:
			expression =
				"(www|ftp)[" HOSTCHARS "]*\\.[" HOSTCHARS ".]+"
				"(:[0-9]+)?(" URLPATH ")?";
			break;
		case EMPATHY_REGEX_APT:
			expression =
				"apt://[" APTCHARS "]*";
			break;
		case EMPATHY_REGEX_EMAIL:
			expression =
				"(mailto:)?[a-z0-9][a-z0-9.-]*@[a-z0-9]"
				"[a-z0-9-]*(\\.[a-z0-9][a-z0-9-]*)+";
			break;
		case EMPATHY_REGEX_OTHER:
			expression =
				"news:[-A-Z\\^_a-z{|}~!\"#$%&'()*+,./0-9;:=?`]+"
				"@[" HOSTCHARS ".]+(:[0-9]+)?";
			break;
		default:
			/* Silence the compiler. */
			expression = NULL;
			continue;
		}

		memset (&dingus[i], 0, sizeof (regex_t));
		regcomp (&dingus[i], expression, REG_EXTENDED | REG_ICASE);
	}

	inited = TRUE;
}

gint
empathy_regex_match (EmpathyRegExType  type,
		    const gchar     *msg,
		    GArray          *start,
		    GArray          *end)
{
	regmatch_t matches[1];
	gint       ret = 0;
	gint       num_matches = 0;
	gint       offset = 0;
	gint       i;

	g_return_val_if_fail (type >= 0 || type <= EMPATHY_REGEX_ALL, 0);

	regex_init ();

	while (!ret && type != EMPATHY_REGEX_ALL) {
		ret = regexec (&dingus[type], msg + offset, 1, matches, 0);
		if (ret == 0) {
			gint s;

			num_matches++;

			s = matches[0].rm_so + offset;
			offset = matches[0].rm_eo + offset;

			g_array_append_val (start, s);
			g_array_append_val (end, offset);
		}
	}

	if (type != EMPATHY_REGEX_ALL) {
		empathy_debug (DEBUG_DOMAIN,
			      "Found %d matches for regex type:%d",
			      num_matches, type);
		return num_matches;
	}

	/* If EMPATHY_REGEX_ALL then we run ALL regex's on the string. */
	for (i = 0; i < EMPATHY_REGEX_ALL; i++, ret = 0) {
		while (!ret) {
			ret = regexec (&dingus[i], msg + offset, 1, matches, 0);
			if (ret == 0) {
				gint s;

				num_matches++;

				s = matches[0].rm_so + offset;
				offset = matches[0].rm_eo + offset;

				g_array_append_val (start, s);
				g_array_append_val (end, offset);
			}
		}
	}

	empathy_debug (DEBUG_DOMAIN,
		      "Found %d matches for ALL regex types",
		      num_matches);

	return num_matches;
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
	empathy_debug (DEBUG_DOMAIN, "Loading dtd file %s", path);

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
empathy_mission_control_new (void)
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

void
empathy_call_with_contact (EmpathyContact *contact)
{
	MissionControl        *mc;
	McAccount             *account;
	TpConnection          *connection;
	gchar                 *object_path;
	TpChannel             *channel;
	EmpathyContactFactory *factory;
	EmpathyTpGroup        *group;
	EmpathyContact        *self_contact;
	GError                *error = NULL;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	mc = empathy_mission_control_new ();
	account = empathy_contact_get_account (contact);
	connection = mission_control_get_tpconnection (mc, account, NULL);
	tp_connection_run_until_ready (connection, FALSE, NULL, NULL);
	g_object_unref (mc);

	/* We abuse of suppress_handler, TRUE means OUTGOING. The channel
	 * will be catched in EmpathyFilter */
	if (!tp_cli_connection_run_request_channel (connection, -1,
						    TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
						    TP_HANDLE_TYPE_NONE,
						    0,
						    TRUE,
						    &object_path,
						    &error,
						    NULL)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "Couldn't request channel: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		g_object_unref (connection);
		return;
	}

	channel = tp_channel_new (connection,
				  object_path, TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
				  TP_HANDLE_TYPE_NONE, 0, NULL);

	group = empathy_tp_group_new (channel);
	empathy_run_until_ready (group);

	factory = empathy_contact_factory_new ();
	self_contact = empathy_contact_factory_get_user (factory, account);
	empathy_contact_run_until_ready (self_contact,
					 EMPATHY_CONTACT_READY_HANDLE,
					 NULL);

	empathy_tp_group_add_member (group, contact, "");
	empathy_tp_group_add_member (group, self_contact, "");	

	g_object_unref (factory);
	g_object_unref (self_contact);
	g_object_unref (group);
	g_object_unref (connection);
	g_object_unref (channel);
	g_free (object_path);
}

void
empathy_call_with_contact_id (McAccount *account, const gchar *contact_id)
{
	EmpathyContactFactory *factory;
	EmpathyContact        *contact;

	factory = empathy_contact_factory_new ();
	contact = empathy_contact_factory_get_from_id (factory, account, contact_id);
	empathy_contact_run_until_ready (contact, EMPATHY_CONTACT_READY_HANDLE, NULL);

	empathy_call_with_contact (contact);

	g_object_unref (contact);
	g_object_unref (factory);
}

void
empathy_chat_with_contact (EmpathyContact  *contact)
{
	MissionControl        *mc;
	McAccount             *account;
	TpConnection          *connection;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	mc = empathy_mission_control_new ();
	account = empathy_contact_get_account (contact);
	connection = mission_control_get_tpconnection (mc, account, NULL);
	tp_connection_run_until_ready (connection, FALSE, NULL, NULL);
	g_object_unref (mc);

	/* We abuse of suppress_handler, TRUE means OUTGOING. The channel
	 * will be catched in EmpathyFilter */
	tp_cli_connection_call_request_channel (connection, -1,
						TP_IFACE_CHANNEL_TYPE_TEXT,
						TP_HANDLE_TYPE_CONTACT,
						empathy_contact_get_handle (contact),
						TRUE,
						NULL, NULL, NULL, NULL);
	g_object_unref (connection);
}

void
empathy_chat_with_contact_id (McAccount *account, const gchar *contact_id)
{
	EmpathyContactFactory *factory;
	EmpathyContact        *contact;

	factory = empathy_contact_factory_new ();
	contact = empathy_contact_factory_get_from_id (factory, account, contact_id);
	empathy_contact_run_until_ready (contact, EMPATHY_CONTACT_READY_HANDLE, NULL);

	empathy_chat_with_contact (contact);

	g_object_unref (contact);
	g_object_unref (factory);
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

	return MC_PRESENCE_AVAILABLE;
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
		empathy_debug (DEBUG_DOMAIN, "Object %p is ready", data->object);
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

	empathy_debug (DEBUG_DOMAIN, "Starting run until ready for object %p",
		       object);

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
	mc = empathy_mission_control_new ();
	account = mission_control_get_account_for_tpconnection (mc, connection, NULL);
	g_object_unref (connection);
	g_object_unref (mc);

	return account;
}

typedef void (*AccountStatusChangedFunc) (MissionControl           *mc,
					  TpConnectionStatus        status,
					  McPresence                presence,
					  TpConnectionStatusReason  reason,
					  const gchar              *unique_name,
					  gpointer                  user_data);

typedef struct {
	AccountStatusChangedFunc handler;
	gpointer                 user_data;
	GClosureNotify           free_func;
	MissionControl          *mc;
} AccountStatusChangedData;

typedef struct {
	TpConnectionStatus        status;
	McPresence                presence;
	TpConnectionStatusReason  reason;
	gchar                    *unique_name;
	AccountStatusChangedData *data;
} InvocationData;

static void
account_status_changed_data_free (gpointer ptr,
				  GClosure *closure)
{
	AccountStatusChangedData *data = ptr;

	if (data->free_func) {
		data->free_func (data->user_data, closure);
	}
	g_object_unref (data->mc);
	g_slice_free (AccountStatusChangedData, data);
}

static gboolean
account_status_changed_invoke_callback (gpointer data)
{
	InvocationData *invocation_data = data;

	invocation_data->data->handler (invocation_data->data->mc,
					invocation_data->status,
					invocation_data->presence,
					invocation_data->reason,
					invocation_data->unique_name,
					invocation_data->data->user_data);

	g_free (invocation_data->unique_name);
	g_slice_free (InvocationData, invocation_data);

	return FALSE;
}

static void
account_status_changed_cb (MissionControl           *mc,
			   TpConnectionStatus        status,
			   McPresence                presence,
			   TpConnectionStatusReason  reason,
			   const gchar              *unique_name,
			   AccountStatusChangedData *data)
{
	InvocationData *invocation_data;

	invocation_data = g_slice_new (InvocationData);
	invocation_data->status = status;
	invocation_data->presence = presence;
	invocation_data->reason = reason;
	invocation_data->unique_name = g_strdup (unique_name);
	invocation_data->data = data;

	g_idle_add_full (G_PRIORITY_HIGH,
			 account_status_changed_invoke_callback,
			 invocation_data, NULL);
}

gpointer
empathy_connect_to_account_status_changed (MissionControl *mc,
					   GCallback       handler,
					   gpointer        user_data,
					   GClosureNotify  free_func)
{
	AccountStatusChangedData *data;

	g_return_val_if_fail (IS_MISSIONCONTROL (mc), NULL);
	g_return_val_if_fail (handler != NULL, NULL);
	
	data = g_slice_new (AccountStatusChangedData);
	data->handler = (AccountStatusChangedFunc) handler;
	data->user_data = user_data;
	data->free_func = free_func;
	data->mc = g_object_ref (mc);

	dbus_g_proxy_connect_signal (DBUS_G_PROXY (mc), "AccountStatusChanged",
				     G_CALLBACK (account_status_changed_cb),
				     data, account_status_changed_data_free);

	return data;
}

void
empathy_disconnect_account_status_changed (gpointer token)
{
	AccountStatusChangedData *data = token;

	dbus_g_proxy_disconnect_signal (DBUS_G_PROXY (data->mc),
					"AccountStatusChanged",
					G_CALLBACK (account_status_changed_cb),
					data);
}

