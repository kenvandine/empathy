/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005-2007 Imendio AB
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
 * Authors: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "empathy-utils.h"
#include "empathy-contact-groups.h"

#define DEBUG_FLAG EMPATHY_DEBUG_CONTACT
#include "empathy-debug.h"

#define CONTACT_GROUPS_XML_FILENAME "contact-groups.xml"
#define CONTACT_GROUPS_DTD_FILENAME "empathy-contact-groups.dtd"

typedef struct {
	gchar    *name;
	gboolean  expanded;
} ContactGroup;

static void          contact_groups_file_parse (const gchar  *filename);
static gboolean      contact_groups_file_save  (void);
static ContactGroup *contact_group_new         (const gchar  *name,
						gboolean      expanded);
static void          contact_group_free        (ContactGroup *group);

static GList *groups = NULL;

void
empathy_contact_groups_get_all (void)
{
	gchar *dir;
	gchar *file_with_path;

	/* If already set up clean up first */
	if (groups) {
		g_list_foreach (groups, (GFunc)contact_group_free, NULL);
		g_list_free (groups);
		groups = NULL;
	}

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
	file_with_path = g_build_filename (dir, CONTACT_GROUPS_XML_FILENAME, NULL);
	g_free (dir);

	if (g_file_test (file_with_path, G_FILE_TEST_EXISTS)) {
		contact_groups_file_parse (file_with_path);
	}

	g_free (file_with_path);
}

static void
contact_groups_file_parse (const gchar *filename)
{
	xmlParserCtxtPtr ctxt;
	xmlDocPtr        doc;
	xmlNodePtr       contacts;
	xmlNodePtr       account;
	xmlNodePtr       node;

	DEBUG ("Attempting to parse file:'%s'...", filename);

	ctxt = xmlNewParserCtxt ();

	/* Parse and validate the file. */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);
	if (!doc) {
		g_warning ("Failed to parse file:'%s'", filename);
		xmlFreeParserCtxt (ctxt);
		return;
	}

	if (!empathy_xml_validate (doc, CONTACT_GROUPS_DTD_FILENAME)) {
		g_warning ("Failed to validate file:'%s'", filename);
		xmlFreeDoc(doc);
		xmlFreeParserCtxt (ctxt);
		return;
	}

	/* The root node, contacts. */
	contacts = xmlDocGetRootElement (doc);

	account = NULL;
	node = contacts->children;
	while (node) {
		if (strcmp ((gchar *) node->name, "account") == 0) {
			account = node;
			break;
		}
		node = node->next;
	}

	node = NULL;
	if (account) {
		node = account->children;
	}

	while (node) {
		if (strcmp ((gchar *) node->name, "group") == 0) {
			gchar        *name;
			gchar        *expanded_str;
			gboolean      expanded;
			ContactGroup *contact_group;

			name = (gchar *) xmlGetProp (node, "name");
			expanded_str = (gchar *) xmlGetProp (node, "expanded");

			if (expanded_str && strcmp (expanded_str, "yes") == 0) {
				expanded = TRUE;
			} else {
				expanded = FALSE;
			}

			contact_group = contact_group_new (name, expanded);
			groups = g_list_append (groups, contact_group);

			xmlFree (name);
			xmlFree (expanded_str);
		}

		node = node->next;
	}

	DEBUG ("Parsed %d contact groups", g_list_length (groups));

	xmlFreeDoc(doc);
	xmlFreeParserCtxt (ctxt);
}

static ContactGroup *
contact_group_new (const gchar *name,
		   gboolean     expanded)
{
	ContactGroup *group;

	group = g_new0 (ContactGroup, 1);

	group->name = g_strdup (name);
	group->expanded = expanded;

	return group;
}

static void
contact_group_free (ContactGroup *group)
{
	g_return_if_fail (group != NULL);

	g_free (group->name);

	g_free (group);
}

static gboolean
contact_groups_file_save (void)
{
	xmlDocPtr   doc;
	xmlNodePtr  root;
	xmlNodePtr  node;
	GList      *l;
	gchar      *dir;
	gchar      *file;

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
	g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	file = g_build_filename (dir, CONTACT_GROUPS_XML_FILENAME, NULL);
	g_free (dir);

	doc = xmlNewDoc ("1.0");
	root = xmlNewNode (NULL, "contacts");
	xmlDocSetRootElement (doc, root);

	node = xmlNewChild (root, NULL, "account", NULL);
	xmlNewProp (node, "name", "Default");

	for (l = groups; l; l = l->next) {
		ContactGroup *cg;
		xmlNodePtr    subnode;

		cg = l->data;

		subnode = xmlNewChild (node, NULL, "group", NULL);
		xmlNewProp (subnode, "expanded", cg->expanded ? "yes" : "no");
		xmlNewProp (subnode, "name", cg->name);
	}

	/* Make sure the XML is indented properly */
	xmlIndentTreeOutput = 1;

	DEBUG ("Saving file:'%s'", file);
	xmlSaveFormatFileEnc (file, doc, "utf-8", 1);
	xmlFreeDoc (doc);

	xmlCleanupParser ();
	xmlMemoryDump ();

	g_free (file);

	return TRUE;
}

gboolean
empathy_contact_group_get_expanded (const gchar *group)
{
	GList    *l;
	gboolean  default_val = TRUE;

	g_return_val_if_fail (group != NULL, default_val);

	for (l = groups; l; l = l->next) {
		ContactGroup *cg = l->data;

		if (!cg || !cg->name) {
			continue;
		}

		if (strcmp (cg->name, group) == 0) {
			return cg->expanded;
		}
	}

	return default_val;
}

void
empathy_contact_group_set_expanded (const gchar *group,
				   gboolean     expanded)
{
	GList        *l;
	ContactGroup *cg;
	gboolean      changed = FALSE;

	g_return_if_fail (group != NULL);

	for (l = groups; l; l = l->next) {
		ContactGroup *cg = l->data;

		if (!cg || !cg->name) {
			continue;
		}

		if (strcmp (cg->name, group) == 0) {
			cg->expanded = expanded;
			changed = TRUE;
			break;
		}
	}

	/* if here... we don't have a ContactGroup for the group. */
	if (!changed) {
		cg = contact_group_new (group, expanded);
		groups = g_list_append (groups, cg);
	}

	contact_groups_file_save ();
}
