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

#include <glib/gi18n-lib.h>

#ifdef HAVE_ENCHANT
#include <enchant.h>
#endif

#include "empathy-spell.h"
#include "empathy-conf.h"

#define DEBUG_FLAG EMPATHY_DEBUG_OTHER
#include <libempathy/empathy-debug.h>

#ifdef HAVE_ENCHANT

typedef struct {
	EnchantBroker *config;
	EnchantDict   *speller;
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

	DEBUG ("Resetting languages due to config change");

	/* We just reset the languages list. */
	for (l = languages; l; l = l->next) {
		SpellLanguage *lang;

		lang = l->data;

		enchant_broker_free_dict (lang->config, lang->speller);
		enchant_broker_free (lang->config);
		
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

			DEBUG ("Setting up language:'%s'", strv[i]);

			lang = g_slice_new0 (SpellLanguage);

			lang->config = enchant_broker_init ();
			lang->speller = enchant_broker_request_dict (lang->config, strv[i]);

			languages = g_list_append (languages, lang);
			i++;
		}

		if (strv) {
			g_strfreev (strv);
		}

		g_free (str);
	}
}

const gchar *
empathy_spell_get_language_name (const gchar *code)
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

static void
enumerate_dicts (const gchar * const lang_tag,
                 const gchar * const provider_name,
                 const gchar * const provider_desc,
                 const gchar * const provider_file,
		 gpointer            user_data)
{
	GList **list = user_data;
	gchar  *lang = g_strdup (lang_tag);

	if (strchr (lang, '_')) {
		/* cut country part out of language */
		strchr (lang, '_')[0] = '\0';
	}

	if (g_list_find_custom (*list, lang, (GCompareFunc) strcmp)) {
		/* this language is already part of the list */
		g_free (lang);
		return;
	}

	*list = g_list_append (*list, g_strdup (lang));
}

GList *
empathy_spell_get_language_codes (void)
{
	EnchantBroker *broker;
	GList         *list_langs = NULL;

	broker = enchant_broker_init ();
	enchant_broker_list_dicts (broker, enumerate_dicts, &list_langs);
	enchant_broker_free (broker);

	return list_langs;
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
	gint         enchant_result = 1;
	const gchar *p;
	gboolean     digit;
	gunichar     c;
	gint         len;
	GList       *l;

	g_return_val_if_fail (word != NULL, FALSE);

	spell_setup_languages ();

	if (!languages) {
		DEBUG ("No languages to check against");
		return TRUE;
	}

	/* Ignore certain cases like numbers, etc. */
	for (p = word, digit = TRUE; *p && digit; p = g_utf8_next_char (p)) {
		c = g_utf8_get_char (p);
		digit = g_unichar_isdigit (c);
	}

	if (digit) {
		/* We don't spell check digits. */
		DEBUG ("Not spell checking word:'%s', it is all digits", word);
		return TRUE;
	}

	len = strlen (word);
	for (l = languages; l; l = l->next) {
		SpellLanguage  *lang;

		lang = l->data;

		enchant_result = enchant_dict_check (lang->speller, word, len);

		if (enchant_result == 0) {
			break;
		}
	}

	return (enchant_result == 0);
}

GList *
empathy_spell_get_suggestions (const gchar *word)
{
	gint   len;
	GList *l1;
	GList *suggestion_list = NULL;

	g_return_val_if_fail (word != NULL, NULL);

	spell_setup_languages ();

	len = strlen (word);

	for (l1 = languages; l1; l1 = l1->next) {
		SpellLanguage *lang;
		gchar **suggestions;
		gsize   i, number_of_suggestions;

		lang = l1->data;

		suggestions = enchant_dict_suggest (lang->speller, word, len,
						    &number_of_suggestions);
		
		for (i = 0; i < number_of_suggestions; i++) {
			suggestion_list = g_list_append (suggestion_list,
							 g_strdup (suggestions[i]));
		}

		if (suggestions) {
			enchant_dict_free_string_list (lang->speller, suggestions);
		}
	}

	return suggestion_list;
}

gboolean
empathy_spell_supported (void)
{
	if (g_getenv ("EMPATHY_SPELL_DISABLED")) {
		DEBUG ("EMPATHY_SPELL_DISABLE env variable defined");
		return FALSE;
	}

	return TRUE;
}

#else /* not HAVE_ENCHANT */

gboolean
empathy_spell_supported (void)
{
	return FALSE;
}

GList *
empathy_spell_get_suggestions (const gchar *word)
{
	DEBUG ("Support disabled, could not get suggestions");

	return NULL;
}

gboolean
empathy_spell_check (const gchar *word)
{
	DEBUG ("Support disabled, could not check spelling");

	return TRUE;
}

const gchar *
empathy_spell_get_language_name (const gchar *lang)
{
	DEBUG ("Support disabled, could not get language name");

	return NULL;
}

GList *
empathy_spell_get_language_codes (void)
{
	DEBUG ("Support disabled, could not get language codes");

	return NULL;
}

void
empathy_spell_free_language_codes (GList *codes)
{
}

#endif /* HAVE_ENCHANT */


void
empathy_spell_free_suggestions (GList *suggestions)
{
	g_list_foreach (suggestions, (GFunc) g_free, NULL);
	g_list_free (suggestions);
}

