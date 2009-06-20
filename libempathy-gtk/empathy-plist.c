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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <glib.h>
#include <glib-object.h>

#include "empathy-plist.h"

static GValue *empathy_plist_parse_node (xmlNode *a_node);

static void
empathy_plist_value_free (GValue *val)
{
	g_value_unset (val);
	g_free (val);
}

static GValue *
empathy_plist_parse_integer (xmlNode *a_node)
{
	char *str_val;
	char *end_ptr;
	gint int_val;
	GValue *value;

	str_val = (char *) xmlNodeGetContent (a_node);
	int_val = strtol (str_val, &end_ptr, 0);
	if (*end_ptr != '\0') {
		xmlFree (str_val);
		return NULL;
	}
	xmlFree (str_val);

	value = g_new0(GValue, 1);
	g_value_init(value, G_TYPE_INT);
	g_value_set_int (value, int_val);

	return value;
}

static GValue *
empathy_plist_parse_string (xmlNode *a_node)
{
	char *str_val;
	GValue *value;

	str_val = (char *) xmlNodeGetContent (a_node);

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_STRING);
	g_value_set_string (value, str_val);

	xmlFree (str_val);

	return value;
}

static GValue *
empathy_plist_parse_real (xmlNode *a_node)
{
	char *str_val;
	char *end_ptr;
	gfloat double_val;
	GValue *value;

	str_val = (char *) xmlNodeGetContent (a_node);
	double_val = g_ascii_strtod (str_val, &end_ptr);
	if (*end_ptr != '\0') {
		xmlFree (str_val);
		return NULL;
	}
	xmlFree (str_val);

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_DOUBLE);
	g_value_set_double (value, double_val);

	return value;
}

static GValue *
empathy_plist_parse_boolean (xmlNode *a_node)
{
	gboolean bool_val;
	GValue *value;

	if (strcmp ((char *) a_node->name, "true") == 0) {
		bool_val = TRUE;
	} else if (strcmp ((char *) a_node->name, "false") == 0) {
		bool_val = FALSE;
	} else {
		return NULL;
	}

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_BOOLEAN);
	g_value_set_boolean (value, bool_val);

	return value;
}

static GValue *
empathy_plist_parse_data (xmlNode *a_node)
{
	char *str_val;
	guchar *raw_data;
	gsize len;
	GString *data_val;
	GValue *value;

	str_val = (char *) xmlNodeGetContent (a_node);
	raw_data = g_base64_decode (str_val, &len);
	xmlFree (str_val);
	data_val = g_string_new_len ((char *) raw_data, len);
	g_free (raw_data);

	value = g_new0 (GValue, 1);
	g_value_init(value, G_TYPE_GSTRING);
	g_value_take_boxed (value, data_val);

	return value;
}

static xmlNode *
empathy_plist_parse_one_dict_entry (xmlNode *a_node, GHashTable *dict)
{
	xmlNode *cur_node = a_node;
	xmlChar *key_name;
	GValue *value;

	while (cur_node &&
	       (xmlStrcmp(cur_node->name, (xmlChar *) "key") != 0)) {
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
	GValue *value;
	GHashTable *dict;

	dict = g_hash_table_new_full (g_str_hash, g_str_equal,
	                              g_free, (GDestroyNotify) empathy_plist_value_free);

	while (cur_node) {
		if (xmlIsBlankNode (cur_node)) {
			cur_node = cur_node->next;
		} else {
			cur_node = empathy_plist_parse_one_dict_entry (cur_node, dict);
		}
	}
	value = g_new0 (GValue, 1);
	value = g_value_init (value, G_TYPE_HASH_TABLE);
	g_value_take_boxed (value, dict);

	return value;
}

typedef GValue *(*ParseCallback) (xmlNode *);

static ParseCallback empathy_plist_get_parser_for_type (const xmlChar *type);

static GValue *
empathy_plist_parse_array (xmlNode *a_node)
{
	xmlNode *cur_node = a_node->children;
	GValue *value;
	GValueArray *array;

	array = g_value_array_new (4);

	while (cur_node) {
		if (empathy_plist_get_parser_for_type (cur_node->name)) {
			GValue *cur_value;
			cur_value = empathy_plist_parse_node (cur_node);
			if (cur_value) {
				array = g_value_array_append (array, cur_value);
				g_value_unset (cur_value);
				g_free (cur_value);
			}
		}
		/* When an array contains an element enclosed in "unknown" tags (ie
		 * non-type ones), we silently skip them since early
		 * SysInfoExtended files used to have <key> values enclosed within
		 * <array> tags.
		 */
		cur_node = cur_node->next;
	}

	value = g_new0 (GValue, 1);
	value = g_value_init (value, G_TYPE_VALUE_ARRAY);
	g_value_take_boxed (value, array);

	return value;
}

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

	xmlFreeDoc(doc);
	xmlCleanupParser();

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
 * #GValue containing a #GHashTable otherwise.
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

	xmlFreeDoc(doc);
	xmlCleanupParser();

	return parsed_doc;
}

gboolean
empathy_plist_get_int (GValue *data, const gchar *key, gint *value)
{
	GHashTable *hash;
	GValue     *entry;

	if (!data || !G_VALUE_HOLDS (data, G_TYPE_HASH_TABLE)) {
		return FALSE;
	}

	hash = g_value_get_boxed (data);
	entry = g_hash_table_lookup (hash, key);

	if (!entry || !G_VALUE_HOLDS (entry, G_TYPE_INT)) {
		return FALSE;
	}

	*value = g_value_get_int (entry);
	return TRUE;
}

gboolean
empathy_plist_get_string (GValue *data, const gchar *key, gchar **value)
{
	GHashTable *hash;
	GValue     *entry;

	if (!data || !G_VALUE_HOLDS (data, G_TYPE_HASH_TABLE)) {
		return FALSE;
	}

	hash = g_value_get_boxed (data);
	entry = g_hash_table_lookup (hash, key);

	if (!entry || !G_VALUE_HOLDS (entry, G_TYPE_STRING)) {
		return FALSE;
	}

	*value = g_value_dup_string (entry);
	return TRUE;
}
