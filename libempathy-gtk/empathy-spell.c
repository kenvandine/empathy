/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2007 Imendio AB
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
 *          Richard Hult <richard@imendio.com>
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>

#ifdef HAVE_ASPELL
#include <aspell.h>
#endif

#include <libempathy/empathy-debug.h>

#include "empathy-spell.h"
#include "empathy-conf.h"

#define DEBUG_DOMAIN "Spell"

#ifdef HAVE_ASPELL

/* Note: We could use aspell_reset_cache (NULL); periodically if we wanted
 * to...
 */

typedef struct {
	AspellConfig       *spell_config;
	AspellCanHaveError *spell_possible_err;
	AspellSpeller      *spell_checker;
} SpellLanguage;

#define ISO_CODES_DATADIR    ISO_CODES_PREFIX "/share/xml/iso-codes"
#define ISO_CODES_LOCALESDIR ISO_CODES_PREFIX "/share/locale"

static GHashTable  *iso_code_names = NULL;
static GList       *languages = NULL;
static gboolean     empathy_conf_notify_inited = FALSE;

static void
spell_iso_codes_parse_start_tag (GMarkupParseContext  *ctx,
				 const gchar          *element_name,
				 const gchar         **attr_names,
				 const gchar         **attr_values,
				 gpointer              data,
				 GError              **error)
{
	const gchar *ccode_longB, *ccode_longT, *ccode;
	const gchar *lang_name;

	if (!g_str_equal (element_name, "iso_639_entry") ||
	    attr_names == NULL || attr_values == NULL) {
		return;
	}

	ccode = NULL;
	ccode_longB = NULL;
	ccode_longT = NULL;
	lang_name = NULL;

	while (*attr_names && *attr_values) {
		if (g_str_equal (*attr_names, "iso_639_1_code")) {
			if (**attr_values) {
				ccode = *attr_values;
			}
		}
		else if (g_str_equal (*attr_names, "iso_639_2B_code")) {
			if (**attr_values) {
				ccode_longB = *attr_values;
			}
		}
		else if (g_str_equal (*attr_names, "iso_639_2T_code")) {
			if (**attr_values) {
				ccode_longT = *attr_values;
			}
		}
		else if (g_str_equal (*attr_names, "name")) {
			lang_name = *attr_values;
		}

		attr_names++;
		attr_values++;
	}

	if (!lang_name) {
		return;
	}

	if (ccode) {
		g_hash_table_insert (iso_code_names,
				     g_strdup (ccode),
				     g_strdup (lang_name));
	}

	if (ccode_longB) {
		g_hash_table_insert (iso_code_names,
				     g_strdup (ccode_longB),
				     g_strdup (lang_name));
	}

	if (ccode_longT) {
		g_hash_table_insert (iso_code_names,
				     g_strdup (ccode_longT),
				     g_strdup (lang_name));
	}
}

static void
spell_iso_code_names_init (void)
{
	GError *err = NULL;
	gchar  *buf;
	gsize   buf_len;

	iso_code_names = g_hash_table_new_full (g_str_hash, g_str_equal,
						g_free, g_free);

	bindtextdomain ("iso_639", ISO_CODES_LOCALESDIR);
	bind_textdomain_codeset ("iso_639", "UTF-8");

	/* FIXME: We should read this in chunks and pass to the parser. */
	if (g_file_get_contents (ISO_CODES_DATADIR "/iso_639.xml", &buf, &buf_len, &err)) {
		GMarkupParseContext *ctx;
		GMarkupParser        parser = {
			spell_iso_codes_parse_start_tag,
			NULL, NULL, NULL, NULL
		};

		ctx = g_markup_parse_context_new (&parser, 0, NULL, NULL);
		if (!g_markup_parse_context_parse (ctx, buf, buf_len, &err)) {
			g_warning ("Failed to parse '%s': %s",
				   ISO_CODES_DATADIR"/iso_639.xml",
				   err->message);
			g_error_free (err);
		}

		g_markup_parse_context_free (ctx);
		g_free (buf);
	} else {
		g_warning ("Failed to load '%s': %s",
				ISO_CODES_DATADIR"/iso_639.xml", err->message);
		g_error_free (err);
	}
}

static void
spell_notify_languages_cb (EmpathyConf  *conf,
			   const gchar *key,
			   gpointer     user_data)
{
	GList *l;

	empathy_debug (DEBUG_DOMAIN, "Resetting languages due to config change");

	/* We just reset the languages list. */
	for (l = languages; l; l = l->next) {
		SpellLanguage *lang;

		lang = l->data;

		delete_aspell_config (lang->spell_config);
		delete_aspell_speller (lang->spell_checker);

		g_slice_free (SpellLanguage, lang);
	}

	g_list_free (languages);
	languages = NULL;
}

static void
spell_setup_languages (void)
{
	gchar  *str;

	if (!empathy_conf_notify_inited) {
		empathy_conf_notify_add (empathy_conf_get (),
					 EMPATHY_PREFS_CHAT_SPELL_CHECKER_LANGUAGES,
					 spell_notify_languages_cb, NULL);

		empathy_conf_notify_inited = TRUE;
	}

	if (languages) {
		return;
	}

	if (empathy_conf_get_string (empathy_conf_get (),
				     EMPATHY_PREFS_CHAT_SPELL_CHECKER_LANGUAGES,
				     &str) && str) {
		gchar **strv;
		gint    i;

		strv = g_strsplit (str, ",", -1);

		i = 0;
		while (strv && strv[i]) {
			SpellLanguage *lang;

			empathy_debug (DEBUG_DOMAIN, "Setting up language:'%s'", strv[i]);

			lang = g_slice_new0 (SpellLanguage);

			lang->spell_config = new_aspell_config();

			aspell_config_replace (lang->spell_config, "encoding", "utf-8");
			aspell_config_replace (lang->spell_config, "lang", strv[i++]);

			lang->spell_possible_err = new_aspell_speller (lang->spell_config);

			if (aspell_error_number (lang->spell_possible_err) == 0) {
				lang->spell_checker = to_aspell_speller (lang->spell_possible_err);
				languages = g_list_append (languages, lang);
			} else {
				delete_aspell_config (lang->spell_config);
				g_slice_free (SpellLanguage, lang);
			}
		}

		if (strv) {
			g_strfreev (strv);
		}

		g_free (str);
	}
}

const char *
empathy_spell_get_language_name (const char *code)
{
	const gchar *name;

	g_return_val_if_fail (code != NULL, NULL);

	if (!iso_code_names) {
		spell_iso_code_names_init ();
	}

	name = g_hash_table_lookup (iso_code_names, code);
	if (!name) {
		return NULL;
	}

	return dgettext ("iso_639", name);
}

GList *
empathy_spell_get_language_codes (void)
{
	AspellConfig              *config;
	AspellDictInfoList        *dlist;
	AspellDictInfoEnumeration *dels;
	const AspellDictInfo      *entry;
	GList                     *codes = NULL;

	config = new_aspell_config ();
	dlist = get_aspell_dict_info_list (config);
	dels = aspell_dict_info_list_elements (dlist);

	while ((entry = aspell_dict_info_enumeration_next (dels)) != 0) {
		if (g_list_find_custom (codes, entry->code, (GCompareFunc) strcmp)) {
			continue;
		}

		codes = g_list_append (codes, g_strdup (entry->code));
	}

	delete_aspell_dict_info_enumeration (dels);
	delete_aspell_config (config);

	return codes;
}

void
empathy_spell_free_language_codes (GList *codes)
{
	g_list_foreach (codes, (GFunc) g_free, NULL);
	g_list_free (codes);
}

gboolean
empathy_spell_check (const gchar *word)
{
	GList       *l;
	gint         n_langs;
	gboolean     correct = FALSE;
	gint         len;
	const gchar *p;
	gunichar     c;
	gboolean     digit;

	g_return_val_if_fail (word != NULL, FALSE);

	spell_setup_languages ();

	if (!languages) {
		empathy_debug (DEBUG_DOMAIN, "No languages to check against");
		return TRUE;
	}

	/* Ignore certain cases like numbers, etc. */
	for (p = word, digit = TRUE; *p && digit; p = g_utf8_next_char (p)) {
		c = g_utf8_get_char (p);
		digit = g_unichar_isdigit (c);
	}

	if (digit) {
		/* We don't spell check digits. */
		empathy_debug (DEBUG_DOMAIN, "Not spell checking word:'%s', it is all digits", word);
		return TRUE;
	}

	len = strlen (word);
	n_langs = g_list_length (languages);
	for (l = languages; l; l = l->next) {
		SpellLanguage *lang;

		lang = l->data;

		correct = aspell_speller_check (lang->spell_checker, word, len);
		if (n_langs > 1 && correct) {
			break;
		}
	}

	return correct;
}

GList *
empathy_spell_get_suggestions (const gchar *word)
{
	GList                   *l1;
	GList                   *l2 = NULL;
	const AspellWordList    *suggestions;
	AspellStringEnumeration *elements;
	const char              *next;
	gint                     len;

	g_return_val_if_fail (word != NULL, NULL);

	spell_setup_languages ();

	len = strlen (word);

	for (l1 = languages; l1; l1 = l1->next) {
		SpellLanguage *lang;

		lang = l1->data;

		suggestions = aspell_speller_suggest (lang->spell_checker,
						      word, len);

		elements = aspell_word_list_elements (suggestions);

		while ((next = aspell_string_enumeration_next (elements))) {
			l2 = g_list_append (l2, g_strdup (next));
		}

		delete_aspell_string_enumeration (elements);
	}

	return l2;
}

gboolean
empathy_spell_supported (void)
{
	if (g_getenv ("EMPATHY_SPELL_DISABLED")) {
		empathy_debug (DEBUG_DOMAIN, "EMPATHY_SPELL_DISABLE env variable defined");
		return FALSE;
	}

	return TRUE;
}

#else /* not HAVE_ASPELL */

gboolean
empathy_spell_supported (void)
{
	return FALSE;
}

GList *
empathy_spell_get_suggestions (const gchar *word)
{
	empathy_debug (DEBUG_DOMAIN, "Support disabled, could not get suggestions");

	return NULL;
}

gboolean
empathy_spell_check (const gchar *word)
{
	empathy_debug (DEBUG_DOMAIN, "Support disabled, could not check spelling");

	return TRUE;
}

const char *
empathy_spell_get_language_name (const char *lang)
{
	empathy_debug (DEBUG_DOMAIN, "Support disabled, could not get language name");

	return NULL;
}

GList *
empathy_spell_get_language_codes (void)
{
	empathy_debug (DEBUG_DOMAIN, "Support disabled, could not get language codes");

	return NULL;
}

void
empathy_spell_free_language_codes (GList *codes)
{
}

#endif /* HAVE_ASPELL */


void
empathy_spell_free_suggestions (GList *suggestions)
{
	g_list_foreach (suggestions, (GFunc) g_free, NULL);
	g_list_free (suggestions);
}

