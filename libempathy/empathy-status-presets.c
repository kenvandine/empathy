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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * Author: Martyn Russell <martyn@imendio.com>
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <telepathy-glib/util.h>

#include "empathy-utils.h"
#include "empathy-status-presets.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include "empathy-debug.h"

#define STATUS_PRESETS_XML_FILENAME "status-presets.xml"
#define STATUS_PRESETS_DTD_FILENAME "empathy-status-presets.dtd"
#define STATUS_PRESETS_MAX_EACH     15

typedef struct {
	gchar      *status;
	TpConnectionPresenceType  state;
} StatusPreset;

static StatusPreset *status_preset_new          (TpConnectionPresenceType    state,
						 const gchar  *status);
static void     status_preset_free              (StatusPreset *status);
static void     status_presets_file_parse       (const gchar  *filename);
const gchar *   status_presets_get_state_as_str (TpConnectionPresenceType    state);
static gboolean status_presets_file_save        (void);
static void     status_presets_set_default      (TpConnectionPresenceType    state,
						 const gchar  *status);

static GList        *presets = NULL;
static StatusPreset *default_preset = NULL;

static StatusPreset *
status_preset_new (TpConnectionPresenceType   state,
		   const gchar *status)
{
	StatusPreset *preset;

	preset = g_new0 (StatusPreset, 1);

	preset->status = g_strdup (status);
	preset->state = state;

	return preset;
}

static void
status_preset_free (StatusPreset *preset)
{
	g_free (preset->status);
	g_free (preset);
}

static void
status_presets_file_parse (const gchar *filename)
{
	xmlParserCtxtPtr ctxt;
	xmlDocPtr        doc;
	xmlNodePtr       presets_node;
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

	if (!empathy_xml_validate (doc, STATUS_PRESETS_DTD_FILENAME)) {
		g_warning ("Failed to validate file:'%s'", filename);
		xmlFreeDoc (doc);
		xmlFreeParserCtxt (ctxt);
		return;
	}

	/* The root node, presets. */
	presets_node = xmlDocGetRootElement (doc);

	node = presets_node->children;
	while (node) {
		if (strcmp ((gchar *) node->name, "status") == 0 ||
		    strcmp ((gchar *) node->name, "default") == 0) {
			TpConnectionPresenceType    state;
			gchar        *status;
			gchar        *state_str;
			StatusPreset *preset;
			gboolean      is_default = FALSE;

			if (strcmp ((gchar *) node->name, "default") == 0) {
				is_default = TRUE;
			}

			status = (gchar *) xmlNodeGetContent (node);
			state_str = (gchar *) xmlGetProp (node, "presence");

			if (state_str) {
				state = empathy_presence_from_str (state_str);
				if (empathy_status_presets_is_valid (state)) {
					if (is_default) {
						DEBUG ("Default status preset state is:"
							" '%s', status:'%s'", state_str,
							status);

						status_presets_set_default (state, status);
					} else {
						preset = status_preset_new (state, status);
						presets = g_list_append (presets, preset);
					}
				}
			}

			xmlFree (status);
			xmlFree (state_str);
		}

		node = node->next;
	}

	/* Use the default if not set */
	if (!default_preset) {
		status_presets_set_default (TP_CONNECTION_PRESENCE_TYPE_OFFLINE, NULL);
	}

	DEBUG ("Parsed %d status presets", g_list_length (presets));

	xmlFreeDoc (doc);
	xmlFreeParserCtxt (ctxt);
}

void
empathy_status_presets_get_all (void)
{
	gchar *dir;
	gchar *file_with_path;

	/* If already set up clean up first. */
	if (presets) {
		g_list_foreach (presets, (GFunc) status_preset_free, NULL);
		g_list_free (presets);
		presets = NULL;
	}

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
	g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	file_with_path = g_build_filename (dir, STATUS_PRESETS_XML_FILENAME, NULL);
	g_free (dir);

	if (g_file_test (file_with_path, G_FILE_TEST_EXISTS)) {
		status_presets_file_parse (file_with_path);
	}

	g_free (file_with_path);
}

static gboolean
status_presets_file_save (void)
{
	xmlDocPtr   doc;
	xmlNodePtr  root;
	GList      *l;
	gchar      *dir;
	gchar      *file;
	gint        count[NUM_TP_CONNECTION_PRESENCE_TYPES];
	gint        i;

	for (i = 0; i < NUM_TP_CONNECTION_PRESENCE_TYPES; i++) {
		count[i] = 0;
	}

	dir = g_build_filename (g_get_home_dir (), ".gnome2", PACKAGE_NAME, NULL);
	g_mkdir_with_parents (dir, S_IRUSR | S_IWUSR | S_IXUSR);
	file = g_build_filename (dir, STATUS_PRESETS_XML_FILENAME, NULL);
	g_free (dir);

	doc = xmlNewDoc ("1.0");
	root = xmlNewNode (NULL, "presets");
	xmlDocSetRootElement (doc, root);

	if (default_preset) {
		xmlNodePtr  subnode;
		xmlChar    *state;

		state = (gchar *) empathy_presence_to_str (default_preset->state);

		subnode = xmlNewTextChild (root, NULL, "default",
					   default_preset->status);
		xmlNewProp (subnode, "presence", state);
	}

	for (l = presets; l; l = l->next) {
		StatusPreset *sp;
		xmlNodePtr    subnode;
		xmlChar      *state;

		sp = l->data;
		state = (gchar *) empathy_presence_to_str (sp->state);

		count[sp->state]++;
		if (count[sp->state] > STATUS_PRESETS_MAX_EACH) {
			continue;
		}

		subnode = xmlNewTextChild (root, NULL,
					   "status", sp->status);
		xmlNewProp (subnode, "presence", state);
	}

	/* Make sure the XML is indented properly */
	xmlIndentTreeOutput = 1;

	DEBUG ("Saving file:'%s'", file);
	xmlSaveFormatFileEnc (file, doc, "utf-8", 1);
	xmlFreeDoc (doc);

	g_free (file);

	return TRUE;
}

GList *
empathy_status_presets_get (TpConnectionPresenceType state,
			   gint       max_number)
{
	GList *list = NULL;
	GList *l;
	gint   i;

	i = 0;
	for (l = presets; l; l = l->next) {
		StatusPreset *sp;

		sp = l->data;

		if (sp->state != state) {
			continue;
		}

		list = g_list_append (list, sp->status);
		i++;

		if (max_number != -1 && i >= max_number) {
			break;
		}
	}

	return list;
}

void
empathy_status_presets_set_last (TpConnectionPresenceType   state,
				const gchar *status)
{
	GList        *l;
	StatusPreset *preset;
	gint          num;

	/* Check if duplicate */
	for (l = presets; l; l = l->next) {
		preset = l->data;

		if (state == preset->state &&
		    !tp_strdiff (status, preset->status)) {
			return;
		}
	}

	preset = status_preset_new (state, status);
	presets = g_list_prepend (presets, preset);

	num = 0;
	for (l = presets; l; l = l->next) {
		preset = l->data;

		if (state != preset->state) {
			continue;
		}

		num++;

		if (num > STATUS_PRESETS_MAX_EACH) {
			status_preset_free (preset);
			presets = g_list_delete_link (presets, l);
			break;
		}
	}

	status_presets_file_save ();
}

void
empathy_status_presets_remove (TpConnectionPresenceType   state,
			       const gchar *status)
{
	StatusPreset *preset;
	GList        *l;

	for (l = presets; l; l = l->next) {
		preset = l->data;

		if (state == preset->state &&
		    !tp_strdiff (status, preset->status)) {
			status_preset_free (preset);
			presets = g_list_delete_link (presets, l);
			status_presets_file_save ();
			break;
		}
	}
}

void
empathy_status_presets_reset (void)
{
	g_list_foreach (presets, (GFunc) status_preset_free, NULL);
	g_list_free (presets);

	presets = NULL;

	status_presets_set_default (TP_CONNECTION_PRESENCE_TYPE_AVAILABLE, NULL);

	status_presets_file_save ();
}

TpConnectionPresenceType
empathy_status_presets_get_default_state (void)
{
	if (!default_preset) {
		return TP_CONNECTION_PRESENCE_TYPE_OFFLINE;
	}

	return default_preset->state;
}

const gchar *
empathy_status_presets_get_default_status (void)
{
	if (!default_preset ||
	    !default_preset->status) {
		return NULL;
	}

	return default_preset->status;
}

static void
status_presets_set_default (TpConnectionPresenceType   state,
			    const gchar *status)
{
	if (default_preset) {
		status_preset_free (default_preset);
	}

	default_preset = status_preset_new (state, status);
}

void
empathy_status_presets_set_default (TpConnectionPresenceType   state,
				   const gchar *status)
{
	status_presets_set_default (state, status);
	status_presets_file_save ();
}

void
empathy_status_presets_clear_default (void)
{
	if (default_preset) {
		status_preset_free (default_preset);
		default_preset = NULL;
	}

	status_presets_file_save ();
}

/**
 * empathy_status_presets_is_valid:
 * @state: a #TpConnectionPresenceType
 *
 * Check if a presence type can be used as a preset.
 *
 * Returns: %TRUE if the presence type can be used as a preset.
 */
gboolean
empathy_status_presets_is_valid (TpConnectionPresenceType state)
{
	switch (state) {
		case TP_CONNECTION_PRESENCE_TYPE_UNSET:
		case TP_CONNECTION_PRESENCE_TYPE_OFFLINE:
		case TP_CONNECTION_PRESENCE_TYPE_UNKNOWN:
		case TP_CONNECTION_PRESENCE_TYPE_ERROR:
			return FALSE;

		case TP_CONNECTION_PRESENCE_TYPE_AVAILABLE:
		case TP_CONNECTION_PRESENCE_TYPE_AWAY:
		case TP_CONNECTION_PRESENCE_TYPE_EXTENDED_AWAY:
		case TP_CONNECTION_PRESENCE_TYPE_HIDDEN:
		case TP_CONNECTION_PRESENCE_TYPE_BUSY:
			return TRUE;
	}
	return FALSE;
}
