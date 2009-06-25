/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Christophe Fergeau <teuf@gnome.org>
 * Based on itdb_plist parser from the gtkpod project.
 *
 * The code contained in this file is free software; you can redistribute
 * it and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either version
 * 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this code; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <telepathy-glib/util.h>
#include <telepathy-glib/dbus.h>

#include "empathy-plist.h"

static GValue *empathy_plist_parse_node (xmlNode *a_node);

static GValue *
empathy_plist_parse_integer (xmlNode *a_node)
{
	char *str_val;
	char *end_ptr;
	gint int_val;

	str_val = (char *) xmlNodeGetContent (a_node);
	int_val = strtol (str_val, &end_ptr, 0);
	if (*end_ptr != '\0') {
		xmlFree (str_val);
		return NULL;
	}
	xmlFree (str_val);

	return tp_g_value_slice_new_int (int_val);
}

static GValue *
empathy_plist_parse_string (xmlNode *a_node)
{
	char *str_val;
	GValue *value;

	str_val = (char *) xmlNodeGetContent (a_node);

	value = tp_g_value_slice_new_string (str_val);

	xmlFree (str_val);

	return value;
}

static GValue *
empathy_plist_parse_real (xmlNode *a_node)
{
	char *str_val;
	char *end_ptr;
	gdouble double_val;

	str_val = (char *) xmlNodeGetContent (a_node);
	double_val = g_ascii_strtod (str_val, &end_ptr);
	if (*end_ptr != '\0') {
		xmlFree (str_val);
		return NULL;
	}
	xmlFree (str_val);

	return tp_g_value_slice_new_double (double_val);
}

static GValue *
empathy_plist_parse_boolean (xmlNode *a_node)
{
	gboolean bool_val;

	if (strcmp ((char *) a_node->name, "true") == 0) {
		bool_val = TRUE;
	} else if (strcmp ((char *) a_node->name, "false") == 0) {
		bool_val = FALSE;
	} else {
		return NULL;
	}

	return tp_g_value_slice_new_boolean (bool_val);
}

static GValue *
empathy_plist_parse_data (xmlNode *a_node)
{
	char *str_val;
	guchar *raw_data;
	gsize len;
	GValue *value;

	str_val = (char *) xmlNodeGetContent (a_node);
	raw_data = g_base64_decode (str_val, &len);
	xmlFree (str_val);

	value = tp_g_value_slice_new_bytes (len, raw_data);

	g_free (raw_data);

	return value;
}

static xmlNode *
empathy_plist_parse_one_dict_entry (xmlNode *a_node, GHashTable *dict)
{
	xmlNode *cur_node = a_node;
	xmlChar *key_name;
	GValue *value;

	while (cur_node &&
	       (xmlStrcmp (cur_node->name, (xmlChar *) "key") != 0)) {
		cur_node = cur_node->next;
	}
	if (!cur_node) {
		return NULL;
	}
	key_name = xmlNodeGetContent (cur_node);
	cur_node = cur_node->next;
	while (cur_node && xmlIsBlankNode (cur_node)) {
		cur_node = cur_node->next;
	}
	if (!cur_node) {
		xmlFree (key_name);
		return NULL;
	}

	value = empathy_plist_parse_node (cur_node);
	if (value) {
		g_hash_table_insert (dict, g_strdup ((char *) key_name), value);
	}
	xmlFree (key_name);

	return cur_node->next;
}

static GValue *
empathy_plist_parse_dict (xmlNode *a_node)
{
	xmlNode *cur_node = a_node->children;
	GHashTable *dict;

	dict = g_hash_table_new_full (g_str_hash, g_str_equal,
				      g_free, (GDestroyNotify) tp_g_value_slice_free);

	while (cur_node) {
		if (xmlIsBlankNode (cur_node)) {
			cur_node = cur_node->next;
		} else {
			cur_node = empathy_plist_parse_one_dict_entry (cur_node, dict);
		}
	}

	return tp_g_value_slice_new_take_boxed (G_TYPE_HASH_TABLE, dict);
}

static GValue *
empathy_plist_parse_array (xmlNode *a_node)
{
	xmlNode *cur_node = a_node->children;
	GValueArray *array;

	array = g_value_array_new (4);

	while (cur_node) {
		GValue *cur_value;

		cur_value = empathy_plist_parse_node (cur_node);
		if (cur_value) {
			g_value_array_append (array, cur_value);
			tp_g_value_slice_free (cur_value);
		}

		/* When an array contains an element enclosed in "unknown" tags (ie
		 * non-type ones), we silently skip them since early
		 * SysInfoExtended files used to have <key> values enclosed within
		 * <array> tags.
		 */
		cur_node = cur_node->next;
	}

	return tp_g_value_slice_new_take_boxed (G_TYPE_VALUE_ARRAY, array);
}

typedef GValue *(*ParseCallback) (xmlNode *);

struct Parser {
	const char * const type_name;
	ParseCallback parser;
};

static const struct Parser parsers[] = { {"integer", empathy_plist_parse_integer},
					 {"real",    empathy_plist_parse_real},
					 {"string",  empathy_plist_parse_string},
					 {"true",    empathy_plist_parse_boolean},
					 {"false",   empathy_plist_parse_boolean},
					 {"data",    empathy_plist_parse_data},
					 {"dict",    empathy_plist_parse_dict},
					 {"array",   empathy_plist_parse_array},
					 {NULL,	  NULL} };

static ParseCallback
empathy_plist_get_parser_for_type (const xmlChar *type)
{
	guint i = 0;

	while (parsers[i].type_name) {
		if (xmlStrcmp (type, (xmlChar *) parsers[i].type_name) == 0) {
			if (parsers[i].parser) {
				return parsers[i].parser;
			}
		}
		i++;
	}
	return NULL;
}

static GValue *
empathy_plist_parse_node (xmlNode *a_node)
{
	ParseCallback parser;

	g_return_val_if_fail (a_node != NULL, NULL);
	parser = empathy_plist_get_parser_for_type (a_node->name);
	if (parser) {
		return parser (a_node);
	} else {
		return NULL;
	}
}

static GValue *
empathy_plist_parse (xmlNode * a_node)
{
	xmlNode *cur_node;

	if (!a_node) {
		return NULL;
	}
	if (xmlStrcmp (a_node->name, (xmlChar *) "plist") != 0) {
		return NULL;
	}
	cur_node = a_node->xmlChildrenNode;
	while (cur_node && (xmlIsBlankNode (cur_node))) {
		cur_node = cur_node->next;
	}
	if (cur_node) {
		return empathy_plist_parse_node (cur_node);
	}

	return NULL;
}

/**
 * empathy_plist_parse_from_file:
 * @filename: file containing XML plist data to parse
 *
 * Parses the XML plist file. If an error occurs during the parsing,
 * empathy_plist_parse_from_file() will return NULL.
 *
 * Returns: NULL on error, a newly allocated
 * #GValue otherwise. Free it using tp_g_value_slice_free()
 */
GValue *
empathy_plist_parse_from_file (const char *filename)
{
	xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;
	GValue *parsed_doc;

	doc = xmlReadFile (filename, NULL, 0);

	if (!doc) {
		return NULL;
	}

	root_element = xmlDocGetRootElement (doc);

	parsed_doc = empathy_plist_parse (root_element);

	xmlFreeDoc (doc);
	xmlCleanupParser ();

	return parsed_doc;
}

/**
 * empathy_plist_parse_from_memory:
 * @data:   memory location containing XML plist data to parse
 * @len:	length in bytes of the string to parse
 *
 * Parses the XML plist file stored in @data which length is @len
 * bytes. If an error occurs during the parsing,
 * empathy_plist_parse_from_memory() will return NULL.
 *
 * Returns: NULL on error, a newly allocated
 * #GValue otherwise. Free it using tp_g_value_slice_free()
 */
GValue *
empathy_plist_parse_from_memory (const char *data, gsize len)
{
	xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;
	GValue *parsed_doc;

	doc = xmlReadMemory (data, len, "noname.xml", NULL, 0);

	if (doc == NULL) {
		return NULL;
	}

	root_element = xmlDocGetRootElement (doc);

	parsed_doc = empathy_plist_parse (root_element);

	xmlFreeDoc (doc);
	xmlCleanupParser ();

	return parsed_doc;
}

