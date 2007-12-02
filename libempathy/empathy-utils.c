/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
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
#include <libtelepathy/tp-helpers.h>

#include "empathy-debug.h"
#include "empathy-utils.h"
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
#define USERCHARS "-A-Za-z0-9"
#define PASSCHARS "-A-Za-z0-9,?;.:/!%$^*&~\"#'"
#define HOSTCHARS "-A-Za-z0-9"
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

	path = g_build_filename (DATADIR, "empathy", dtd_filename, NULL);

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

gchar *
empathy_inspect_channel (McAccount *account,
		         TpChan    *tp_chan)
{
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (TELEPATHY_IS_CHAN (tp_chan), NULL);

	return empathy_inspect_handle (account,
				       tp_chan->handle,
				       tp_chan->handle_type);
}

gchar *
empathy_inspect_handle (McAccount *account,
			guint      handle,
			guint      handle_type)
{
	MissionControl  *mc;
	TpConn          *tp_conn;
	GArray          *handles;
	gchar          **names;
	gchar           *name;
	GError          *error = NULL;

	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (handle != 0, NULL);
	g_return_val_if_fail (handle_type != 0, NULL);

	mc = empathy_mission_control_new ();
	tp_conn = mission_control_get_connection (mc, account, NULL);
	g_object_unref (mc);

	if (!tp_conn) {
		return NULL;
	}

	/* Get the handle's name */
	handles = g_array_new (FALSE, FALSE, sizeof (guint));
	g_array_append_val (handles, handle);
	if (!tp_conn_inspect_handles (DBUS_G_PROXY (tp_conn),
				      handle_type,
				      handles,
				      &names,
				      &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "Couldn't get id: %s",
			      error ? error->message : "No error given");

		g_clear_error (&error);
		g_array_free (handles, TRUE);
		g_object_unref (tp_conn);
		
		return NULL;
	}

	g_array_free (handles, TRUE);
	name = *names;
	g_free (names);
	g_object_unref (tp_conn);

	return name;
}

void
empathy_call_contact (EmpathyContact *contact)
{
#ifdef HAVE_VOIP
	MissionControl *mc;
	McAccount      *account;
	TpConn         *tp_conn;
	gchar          *object_path;
	const gchar    *bus_name;
	TpChan         *new_chan;
	EmpathyTpGroup *group;
	GError         *error = NULL;

	g_return_if_fail (EMPATHY_IS_CONTACT (contact));

	/* StreamedMedia channels must have handle=0 and handle_type=none.
	 * To call a contact we have to add him in the group interface of the
	 * channel. MissionControl will detect the channel creation and 
	 * dispatch it to the VoIP chandler automatically. */

	mc = empathy_mission_control_new ();
	account = empathy_contact_get_account (contact);
	tp_conn = mission_control_get_connection (mc, account, NULL);
	/* FIXME: Should be async */
	if (!tp_conn_request_channel (DBUS_G_PROXY (tp_conn),
				      TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
				      TP_HANDLE_TYPE_NONE,
				      0,
				      FALSE,
				      &object_path,
				      &error)) {
		empathy_debug (DEBUG_DOMAIN, 
			      "Couldn't request channel: %s",
			      error ? error->message : "No error given");
		g_clear_error (&error);
		g_object_unref (mc);
		g_object_unref (tp_conn);
		return;
	}

	bus_name = dbus_g_proxy_get_bus_name (DBUS_G_PROXY (tp_conn));
	new_chan = tp_chan_new (tp_get_bus (),
				bus_name,
				object_path,
				TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
				TP_HANDLE_TYPE_NONE,
				0);

	group = empathy_tp_group_new (account, new_chan);
	empathy_tp_group_add_member (group, contact, "");

	g_object_unref (group);
	g_object_unref (mc);
	g_object_unref (tp_conn);
	g_object_unref (new_chan);
	g_free (object_path);
#endif
}

