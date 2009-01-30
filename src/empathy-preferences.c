/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2007 Imendio AB
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
 * Authors: Mikael Hallendal <micke@imendio.com>
 *          Richard Hult <richard@imendio.com>
 *          Martyn Russell <martyn@imendio.com>
 */

#include <config.h>

#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include <libempathy/empathy-utils.h>

#include <libempathy-gtk/empathy-conf.h>
#include <libempathy-gtk/empathy-ui-utils.h>
#include <libempathy-gtk/empathy-theme-manager.h>
#include <libempathy-gtk/empathy-spell.h>
#include <libempathy-gtk/empathy-contact-list-store.h>
#include <libempathy-gtk/empathy-gtk-enum-types.h>

#include "empathy-preferences.h"

typedef struct {
	GtkWidget *dialog;

	GtkWidget *notebook;

	GtkWidget *checkbutton_show_avatars;
	GtkWidget *checkbutton_compact_contact_list;
	GtkWidget *checkbutton_show_smileys;
	GtkWidget *combobox_chat_theme;
	GtkWidget *checkbutton_separate_chat_windows;
	GtkWidget *checkbutton_autoconnect;
	GtkWidget *radiobutton_contact_list_sort_by_name;
	GtkWidget *radiobutton_contact_list_sort_by_state;

	GtkWidget *checkbutton_sounds_enabled;
	GtkWidget *checkbutton_sounds_disabled_away;
	GtkWidget *treeview_sounds;

	GtkWidget *checkbutton_notifications_enabled;
	GtkWidget *checkbutton_notifications_disabled_away;
	GtkWidget *checkbutton_notifications_focus;

	GtkWidget *treeview_spell_checker;

	GList     *notify_ids;
} EmpathyPreferences;

static void     preferences_setup_widgets                (EmpathyPreferences      *preferences);
static void     preferences_languages_setup              (EmpathyPreferences      *preferences);
static void     preferences_languages_add                (EmpathyPreferences      *preferences);
static void     preferences_languages_save               (EmpathyPreferences      *preferences);
static gboolean preferences_languages_save_foreach       (GtkTreeModel           *model,
							  GtkTreePath            *path,
							  GtkTreeIter            *iter,
							  gchar                 **languages);
static void     preferences_languages_load               (EmpathyPreferences      *preferences);
static gboolean preferences_languages_load_foreach       (GtkTreeModel           *model,
							  GtkTreePath            *path,
							  GtkTreeIter            *iter,
							  gchar                 **languages);
static void     preferences_languages_cell_toggled_cb    (GtkCellRendererToggle  *cell,
							  gchar                  *path_string,
							  EmpathyPreferences      *preferences);
static void     preferences_themes_setup                 (EmpathyPreferences      *preferences);
static void     preferences_widget_sync_bool             (const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_widget_sync_string           (const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_widget_sync_string_combo     (const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_notify_string_cb             (EmpathyConf             *conf,
							  const gchar            *key,
							  gpointer                user_data);
static void     preferences_notify_string_combo_cb       (EmpathyConf             *conf,
							  const gchar            *key,
							  gpointer                user_data);
static void     preferences_notify_bool_cb               (EmpathyConf             *conf,
							  const gchar            *key,
							  gpointer                user_data);
static void     preferences_notify_sensitivity_cb        (EmpathyConf             *conf,
							  const gchar            *key,
							  gpointer                user_data);
static void     preferences_hookup_toggle_button         (EmpathyPreferences      *preferences,
							  const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_hookup_radio_button          (EmpathyPreferences      *preferences,
							  const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_hookup_string_combo          (EmpathyPreferences      *preferences,
							  const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_hookup_sensitivity           (EmpathyPreferences      *preferences,
							  const gchar            *key,
							  GtkWidget              *widget);
static void     preferences_toggle_button_toggled_cb     (GtkWidget              *button,
							  gpointer                user_data);
static void     preferences_radio_button_toggled_cb      (GtkWidget              *button,
							  gpointer                user_data);
static void     preferences_string_combo_changed_cb      (GtkWidget *button,
							  gpointer                user_data);
static void     preferences_destroy_cb                   (GtkWidget              *widget,
							  EmpathyPreferences      *preferences);
static void     preferences_response_cb                  (GtkWidget              *widget,
							  gint                    response,
							  EmpathyPreferences      *preferences);

enum {
	COL_LANG_ENABLED,
	COL_LANG_CODE,
	COL_LANG_NAME,
	COL_LANG_COUNT
};

enum {
	COL_COMBO_VISIBLE_NAME,
	COL_COMBO_NAME,
	COL_COMBO_COUNT
};

enum {
	COL_SOUND_ENABLED,
	COL_SOUND_NAME,
	COL_SOUND_KEY,
	COL_SOUND_COUNT
};

typedef struct {
	const char *name;
	const char *key;
} SoundEventEntry;

/* TODO: add phone related sounds also? */
static SoundEventEntry sound_entries [] = {
	{ N_("Message received"), EMPATHY_PREFS_SOUNDS_INCOMING_MESSAGE },
	{ N_("Message sent"), EMPATHY_PREFS_SOUNDS_OUTGOING_MESSAGE },
	{ N_("New conversation"), EMPATHY_PREFS_SOUNDS_NEW_CONVERSATION },
	{ N_("Contact goes online"), EMPATHY_PREFS_SOUNDS_CONTACT_LOGIN },
	{ N_("Contact goes offline"), EMPATHY_PREFS_SOUNDS_CONTACT_LOGOUT },
	{ N_("Account connected"), EMPATHY_PREFS_SOUNDS_SERVICE_LOGIN },
	{ N_("Account disconnected"), EMPATHY_PREFS_SOUNDS_SERVICE_LOGOUT }
};

static void
preferences_add_id (EmpathyPreferences *preferences, guint id)
{
	preferences->notify_ids = g_list_prepend (preferences->notify_ids,
						  GUINT_TO_POINTER (id));
}

static void
preferences_compact_contact_list_changed_cb (EmpathyConf *conf,
					     const gchar *key,
					     gpointer     user_data)
{
	EmpathyPreferences *preferences = user_data;
	gboolean            value;

	if (empathy_conf_get_bool (empathy_conf_get (), key, &value)) {
		gtk_widget_set_sensitive (preferences->checkbutton_show_avatars,
					  !value);
	}
}

static void
preferences_setup_widgets (EmpathyPreferences *preferences)
{
	guint id;

	preferences_hookup_toggle_button (preferences,
					  EMPATHY_PREFS_NOTIFICATIONS_ENABLED,
					  preferences->checkbutton_notifications_enabled);
	preferences_hookup_toggle_button (preferences,
					  EMPATHY_PREFS_NOTIFICATIONS_DISABLED_AWAY,
					  preferences->checkbutton_notifications_disabled_away);
	preferences_hookup_toggle_button (preferences,
					  EMPATHY_PREFS_NOTIFICATIONS_FOCUS,
					  preferences->checkbutton_notifications_focus);

	preferences_hookup_sensitivity (preferences,
					EMPATHY_PREFS_NOTIFICATIONS_ENABLED,
					preferences->checkbutton_notifications_disabled_away);
	preferences_hookup_sensitivity (preferences,
					EMPATHY_PREFS_NOTIFICATIONS_ENABLED,
					preferences->checkbutton_notifications_focus);

	preferences_hookup_toggle_button (preferences,
					  EMPATHY_PREFS_SOUNDS_ENABLED,
					  preferences->checkbutton_sounds_enabled);
	preferences_hookup_toggle_button (preferences,
					  EMPATHY_PREFS_SOUNDS_DISABLED_AWAY,
					  preferences->checkbutton_sounds_disabled_away);

	preferences_hookup_sensitivity (preferences,
					EMPATHY_PREFS_SOUNDS_ENABLED,
					preferences->checkbutton_sounds_disabled_away);
	preferences_hookup_sensitivity (preferences,
					EMPATHY_PREFS_SOUNDS_ENABLED,
					preferences->treeview_sounds);

	preferences_hookup_toggle_button (preferences,
					  EMPATHY_PREFS_UI_SEPARATE_CHAT_WINDOWS,
					  preferences->checkbutton_separate_chat_windows);

	preferences_hookup_toggle_button (preferences,
					  EMPATHY_PREFS_UI_SHOW_AVATARS,
					  preferences->checkbutton_show_avatars);

	preferences_hookup_toggle_button (preferences,
					  EMPATHY_PREFS_UI_COMPACT_CONTACT_LIST,
					  preferences->checkbutton_compact_contact_list);

	preferences_hookup_toggle_button (preferences,
					  EMPATHY_PREFS_CHAT_SHOW_SMILEYS,
					  preferences->checkbutton_show_smileys);

	preferences_hookup_string_combo (preferences,
					 EMPATHY_PREFS_CHAT_THEME,
					 preferences->combobox_chat_theme);

	preferences_hookup_radio_button (preferences,
					 EMPATHY_PREFS_CONTACTS_SORT_CRITERIUM,
					 preferences->radiobutton_contact_list_sort_by_name);

	preferences_hookup_toggle_button (preferences,
					  EMPATHY_PREFS_AUTOCONNECT,
					  preferences->checkbutton_autoconnect);

	id = empathy_conf_notify_add (empathy_conf_get (),
				      EMPATHY_PREFS_UI_COMPACT_CONTACT_LIST,
				      preferences_compact_contact_list_changed_cb,
				      preferences);
	if (id) {
		preferences_add_id (preferences, id);
	}
	preferences_compact_contact_list_changed_cb (empathy_conf_get (),
						     EMPATHY_PREFS_UI_COMPACT_CONTACT_LIST,
						     preferences);
}

static void
preferences_sound_cell_toggled_cb (GtkCellRendererToggle *toggle,
				   char *path_string,
				   EmpathyPreferences *preferences)
{
	GtkTreePath *path;
	gboolean toggled, instore;
	GtkTreeIter iter;
	GtkTreeView *view;
	GtkTreeModel *model;
	char *key;

	view = GTK_TREE_VIEW (preferences->treeview_sounds);
	model = gtk_tree_view_get_model (view);

	path = gtk_tree_path_new_from_string (path_string);
	toggled = gtk_cell_renderer_toggle_get_active (toggle);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COL_SOUND_KEY, &key,
			    COL_SOUND_ENABLED, &instore, -1);

	instore ^= 1;

	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    COL_SOUND_ENABLED, instore, -1);

	empathy_conf_set_bool (empathy_conf_get (), key, instore);

	gtk_tree_path_free (path);
}

static void
preferences_sound_load (EmpathyPreferences *preferences)
{
	int i;
	GtkTreeView *view;
	GtkListStore *store;
	GtkTreeIter iter;
	gboolean set;
	EmpathyConf *conf;

	view = GTK_TREE_VIEW (preferences->treeview_sounds);
	store = GTK_LIST_STORE (gtk_tree_view_get_model (view));
	conf = empathy_conf_get ();

	for (i = 0; i < G_N_ELEMENTS (sound_entries); i++) {
		empathy_conf_get_bool (conf, sound_entries[i].key, &set);

		gtk_list_store_insert_with_values (store, &iter, i,
						   COL_SOUND_NAME, gettext (sound_entries[i].name),
						   COL_SOUND_KEY, sound_entries[i].key,
						   COL_SOUND_ENABLED, set, -1);
	}
}

static void
preferences_sound_setup (EmpathyPreferences *preferences)
{
	GtkTreeView *view;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	view = GTK_TREE_VIEW (preferences->treeview_sounds);

	store = gtk_list_store_new (COL_SOUND_COUNT,
				    G_TYPE_BOOLEAN, /* enabled */
				    G_TYPE_STRING,  /* name */
				    G_TYPE_STRING); /* key */

	gtk_tree_view_set_model (view, GTK_TREE_MODEL (store));

	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (preferences_sound_cell_toggled_cb),
			  preferences);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "active", COL_SOUND_ENABLED);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", COL_SOUND_NAME);

	gtk_tree_view_append_column (view, column);

	gtk_tree_view_column_set_resizable (column, FALSE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	g_object_unref (store);
}

static void
preferences_languages_setup (EmpathyPreferences *preferences)
{
	GtkTreeView       *view;
	GtkListStore      *store;
	GtkTreeSelection  *selection;
	GtkTreeModel      *model;
	GtkTreeViewColumn *column;
	GtkCellRenderer   *renderer;
	guint              col_offset;

	view = GTK_TREE_VIEW (preferences->treeview_spell_checker);

	store = gtk_list_store_new (COL_LANG_COUNT,
				    G_TYPE_BOOLEAN,  /* enabled */
				    G_TYPE_STRING,   /* code */
				    G_TYPE_STRING);  /* name */

	gtk_tree_view_set_model (view, GTK_TREE_MODEL (store));

	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	model = GTK_TREE_MODEL (store);

	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (preferences_languages_cell_toggled_cb),
			  preferences);

	column = gtk_tree_view_column_new_with_attributes (NULL, renderer,
							   "active", COL_LANG_ENABLED,
							   NULL);

	gtk_tree_view_append_column (view, column);

	renderer = gtk_cell_renderer_text_new ();
	col_offset = gtk_tree_view_insert_column_with_attributes (view,
								  -1, _("Language"),
								  renderer,
								  "text", COL_LANG_NAME,
								  NULL);

	g_object_set_data (G_OBJECT (renderer),
			   "column", GINT_TO_POINTER (COL_LANG_NAME));

	column = gtk_tree_view_get_column (view, col_offset - 1);
	gtk_tree_view_column_set_sort_column_id (column, COL_LANG_NAME);
	gtk_tree_view_column_set_resizable (column, FALSE);
	gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (column), TRUE);

	g_object_unref (store);
}

static void
preferences_languages_add (EmpathyPreferences *preferences)
{
	GtkTreeView  *view;
	GtkListStore *store;
	GList        *codes, *l;

	view = GTK_TREE_VIEW (preferences->treeview_spell_checker);
	store = GTK_LIST_STORE (gtk_tree_view_get_model (view));

	codes = empathy_spell_get_language_codes ();

	empathy_conf_set_bool (empathy_conf_get(),
			       EMPATHY_PREFS_CHAT_SPELL_CHECKER_ENABLED,
			       codes != NULL);
	if (!codes) {
		gtk_widget_set_sensitive (preferences->treeview_spell_checker, FALSE);
	}		

	for (l = codes; l; l = l->next) {
		GtkTreeIter  iter;
		const gchar *code;
		const gchar *name;

		code = l->data;
		name = empathy_spell_get_language_name (code);
		if (!name) {
			continue;
		}

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COL_LANG_CODE, code,
				    COL_LANG_NAME, name,
				    -1);
	}

	empathy_spell_free_language_codes (codes);
}

static void
preferences_languages_save (EmpathyPreferences *preferences)
{
	GtkTreeView       *view;
	GtkTreeModel      *model;

	gchar             *languages = NULL;

	view = GTK_TREE_VIEW (preferences->treeview_spell_checker);
	model = gtk_tree_view_get_model (view);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) preferences_languages_save_foreach,
				&languages);

	/* if user selects no languages, we don't want spell check */
	empathy_conf_set_bool (empathy_conf_get (),
			       EMPATHY_PREFS_CHAT_SPELL_CHECKER_ENABLED,
			       languages != NULL);

	empathy_conf_set_string (empathy_conf_get (),
				 EMPATHY_PREFS_CHAT_SPELL_CHECKER_LANGUAGES,
				 languages ? languages : "");

	g_free (languages);
}

static gboolean
preferences_languages_save_foreach (GtkTreeModel  *model,
				    GtkTreePath   *path,
				    GtkTreeIter   *iter,
				    gchar        **languages)
{
	gboolean  enabled;
	gchar    *code;

	if (!languages) {
		return TRUE;
	}

	gtk_tree_model_get (model, iter, COL_LANG_ENABLED, &enabled, -1);
	if (!enabled) {
		return FALSE;
	}

	gtk_tree_model_get (model, iter, COL_LANG_CODE, &code, -1);
	if (!code) {
		return FALSE;
	}

	if (!(*languages)) {
		*languages = g_strdup (code);
	} else {
		gchar *str = *languages;
		*languages = g_strdup_printf ("%s,%s", str, code);
		g_free (str);
	}

	g_free (code);

	return FALSE;
}

static void
preferences_languages_load (EmpathyPreferences *preferences)
{
	GtkTreeView   *view;
	GtkTreeModel  *model;
	gchar         *value;
	gchar        **vlanguages;

	if (!empathy_conf_get_string (empathy_conf_get (),
				      EMPATHY_PREFS_CHAT_SPELL_CHECKER_LANGUAGES,
				      &value) || !value) {
		return;
	}

	vlanguages = g_strsplit (value, ",", -1);
	g_free (value);

	view = GTK_TREE_VIEW (preferences->treeview_spell_checker);
	model = gtk_tree_view_get_model (view);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) preferences_languages_load_foreach,
				vlanguages);

	g_strfreev (vlanguages);
}

static gboolean
preferences_languages_load_foreach (GtkTreeModel  *model,
				    GtkTreePath   *path,
				    GtkTreeIter   *iter,
				    gchar        **languages)
{
	gchar    *code;
	gchar    *lang;
	gint      i;
	gboolean  found = FALSE;

	if (!languages) {
		return TRUE;
	}

	gtk_tree_model_get (model, iter, COL_LANG_CODE, &code, -1);
	if (!code) {
		return FALSE;
	}

	for (i = 0, lang = languages[i]; lang; lang = languages[++i]) {
		if (strcmp (lang, code) == 0) {
			found = TRUE;
		}
	}

	gtk_list_store_set (GTK_LIST_STORE (model), iter, COL_LANG_ENABLED, found, -1);
	return FALSE;
}

static void
preferences_languages_cell_toggled_cb (GtkCellRendererToggle *cell,
				       gchar                 *path_string,
				       EmpathyPreferences     *preferences)
{
	GtkTreeView  *view;
	GtkTreeModel *model;
	GtkListStore *store;
	GtkTreePath  *path;
	GtkTreeIter   iter;
	gboolean      enabled;

	view = GTK_TREE_VIEW (preferences->treeview_spell_checker);
	model = gtk_tree_view_get_model (view);
	store = GTK_LIST_STORE (model);

	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COL_LANG_ENABLED, &enabled, -1);

	enabled ^= 1;

	gtk_list_store_set (store, &iter, COL_LANG_ENABLED, enabled, -1);
	gtk_tree_path_free (path);

	preferences_languages_save (preferences);
}

static void
preferences_themes_setup (EmpathyPreferences *preferences)
{
	GtkComboBox   *combo;
	GtkListStore  *model;
	GtkTreeIter    iter;
	const gchar  **themes;
	gint           i;

	combo = GTK_COMBO_BOX (preferences->combobox_chat_theme);

	model = gtk_list_store_new (COL_COMBO_COUNT,
				    G_TYPE_STRING,
				    G_TYPE_STRING);

	themes = empathy_theme_manager_get_themes ();
	for (i = 0; themes[i]; i += 2) {
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
				    COL_COMBO_VISIBLE_NAME, _(themes[i + 1]),
				    COL_COMBO_NAME, themes[i],
				    -1);
	}

	gtk_combo_box_set_model (combo, GTK_TREE_MODEL (model));
	g_object_unref (model);
}

static void
preferences_widget_sync_bool (const gchar *key, GtkWidget *widget)
{
	gboolean value;

	if (empathy_conf_get_bool (empathy_conf_get (), key, &value)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), value);
	}
}

static void
preferences_widget_sync_string (const gchar *key, GtkWidget *widget)
{
	gchar *value;

	if (empathy_conf_get_string (empathy_conf_get (), key, &value) && value) {
		if (GTK_IS_ENTRY (widget)) {
			gtk_entry_set_text (GTK_ENTRY (widget), value);
		} else if (GTK_IS_RADIO_BUTTON (widget)) {
			if (strcmp (key, EMPATHY_PREFS_CONTACTS_SORT_CRITERIUM) == 0) {
				GType        type;
				GEnumClass  *enum_class;
				GEnumValue  *enum_value;
				GSList      *list;
				GtkWidget   *toggle_widget;
				
				/* Get index from new string */
				type = empathy_contact_list_store_sort_get_type ();
				enum_class = G_ENUM_CLASS (g_type_class_peek (type));
				enum_value = g_enum_get_value_by_nick (enum_class, value);
				
				if (enum_value) { 
					list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
					toggle_widget = g_slist_nth_data (list, enum_value->value);
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle_widget), TRUE);
				}
			} else {
				g_warning ("Unhandled key:'%s' just had string change", key);
			}
		}

		g_free (value);
	}
}

static void
preferences_widget_sync_string_combo (const gchar *key, GtkWidget *widget)
{
	gchar        *value;
	GtkTreeModel *model;
	GtkTreeIter   iter;
	gboolean      found;

	if (!empathy_conf_get_string (empathy_conf_get (), key, &value)) {
		return;
	}

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));

	found = FALSE;
	if (value && gtk_tree_model_get_iter_first (model, &iter)) {
		gchar *name;

		do {
			gtk_tree_model_get (model, &iter,
					    COL_COMBO_NAME, &name,
					    -1);

			if (strcmp (name, value) == 0) {
				found = TRUE;
				gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
				break;
			} else {
				found = FALSE;
			}

			g_free (name);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	/* Fallback to the first one. */
	if (!found) {
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
		}
	}

	g_free (value);
}

static void
preferences_notify_string_cb (EmpathyConf  *conf,
			      const gchar *key,
			      gpointer     user_data)
{
	preferences_widget_sync_string (key, user_data);
}

static void
preferences_notify_string_combo_cb (EmpathyConf  *conf,
				    const gchar *key,
				    gpointer     user_data)
{
	preferences_widget_sync_string_combo (key, user_data);
}

static void
preferences_notify_bool_cb (EmpathyConf  *conf,
			    const gchar *key,
			    gpointer     user_data)
{
	preferences_widget_sync_bool (key, user_data);
}

static void
preferences_notify_sensitivity_cb (EmpathyConf  *conf,
				   const gchar *key,
				   gpointer     user_data)
{
	gboolean value;

	if (empathy_conf_get_bool (conf, key, &value)) {
		gtk_widget_set_sensitive (GTK_WIDGET (user_data), value);
	}
}

#if 0
static void
preferences_widget_sync_int (const gchar *key, GtkWidget *widget)
{
	gint value;

	if (empathy_conf_get_int (empathy_conf_get (), key, &value)) {
		if (GTK_IS_SPIN_BUTTON (widget)) {
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
		}
	}
}

static void
preferences_notify_int_cb (EmpathyConf  *conf,
			   const gchar *key,
			   gpointer     user_data)
{
	preferences_widget_sync_int (key, user_data);	
}

static void
preferences_hookup_spin_button (EmpathyPreferences *preferences,
				const gchar       *key,
				GtkWidget         *widget)
{
	guint id;

	preferences_widget_sync_int (key, widget);

	g_object_set_data_full (G_OBJECT (widget), "key",
				g_strdup (key), g_free);

	g_signal_connect (widget,
			  "value_changed",
			  G_CALLBACK (preferences_spin_button_value_changed_cb),
			  NULL);

	id = empathy_conf_notify_add (empathy_conf_get (),
				      key,
				      preferences_notify_int_cb,
				      widget);
	if (id) {
		preferences_add_id (preferences, id);
	}
}

static void
preferences_hookup_entry (EmpathyPreferences *preferences,
			  const gchar       *key,
			  GtkWidget         *widget)
{
	guint id;

	preferences_widget_sync_string (key, widget);

	g_object_set_data_full (G_OBJECT (widget), "key",
				g_strdup (key), g_free);

	g_signal_connect (widget,
			  "changed",
			  G_CALLBACK (preferences_entry_value_changed_cb),
			  NULL);

	id = empathy_conf_notify_add (empathy_conf_get (),
				      key,
				      preferences_notify_string_cb,
				      widget);
	if (id) {
		preferences_add_id (preferences, id);
	}
}

static void
preferences_spin_button_value_changed_cb (GtkWidget *button,
					  gpointer   user_data)
{
	const gchar *key;

	key = g_object_get_data (G_OBJECT (button), "key");

	empathy_conf_set_int (empathy_conf_get (),
			      key,
			      gtk_spin_button_get_value (GTK_SPIN_BUTTON (button)));
}

static void
preferences_entry_value_changed_cb (GtkWidget *entry,
				    gpointer   user_data)
{
	const gchar *key;

	key = g_object_get_data (G_OBJECT (entry), "key");

	empathy_conf_set_string (empathy_conf_get (),
				 key,
				 gtk_entry_get_text (GTK_ENTRY (entry)));
}
#endif

static void
preferences_hookup_toggle_button (EmpathyPreferences *preferences,
				  const gchar       *key,
				  GtkWidget         *widget)
{
	guint id;

	preferences_widget_sync_bool (key, widget);

	g_object_set_data_full (G_OBJECT (widget), "key",
				g_strdup (key), g_free);

	g_signal_connect (widget,
			  "toggled",
			  G_CALLBACK (preferences_toggle_button_toggled_cb),
			  NULL);

	id = empathy_conf_notify_add (empathy_conf_get (),
				     key,
				     preferences_notify_bool_cb,
				     widget);
	if (id) {
		preferences_add_id (preferences, id);
	}
}

static void
preferences_hookup_radio_button (EmpathyPreferences *preferences,
				 const gchar       *key,
				 GtkWidget         *widget)
{
	GSList *group, *l;
	guint   id;

	preferences_widget_sync_string (key, widget);

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
	for (l = group; l; l = l->next) {
		g_signal_connect (l->data,
				  "toggled",
				  G_CALLBACK (preferences_radio_button_toggled_cb),
				  NULL);

		g_object_set_data_full (G_OBJECT (l->data), "key",
					g_strdup (key), g_free);
	}

	id = empathy_conf_notify_add (empathy_conf_get (),
				     key,
				     preferences_notify_string_cb,
				     widget);
	if (id) {
		preferences_add_id (preferences, id);
	}
}

static void
preferences_hookup_string_combo (EmpathyPreferences *preferences,
				 const gchar       *key,
				 GtkWidget         *widget)
{
	guint id;

	preferences_widget_sync_string_combo (key, widget);

	g_object_set_data_full (G_OBJECT (widget), "key",
				g_strdup (key), g_free);

	g_signal_connect (widget,
			  "changed",
			  G_CALLBACK (preferences_string_combo_changed_cb),
			  NULL);

	id = empathy_conf_notify_add (empathy_conf_get (),
				      key,
				      preferences_notify_string_combo_cb,
				      widget);
	if (id) {
		preferences_add_id (preferences, id);
	}
}

static void
preferences_hookup_sensitivity (EmpathyPreferences *preferences,
				const gchar       *key,
				GtkWidget         *widget)
{
	gboolean value;
	guint    id;

	if (empathy_conf_get_bool (empathy_conf_get (), key, &value)) {
		gtk_widget_set_sensitive (widget, value);
	}

	id = empathy_conf_notify_add (empathy_conf_get (),
				      key,
				      preferences_notify_sensitivity_cb,
				      widget);
	if (id) {
		preferences_add_id (preferences, id);
	}
}

static void
preferences_toggle_button_toggled_cb (GtkWidget *button,
				      gpointer   user_data)
{
	const gchar *key;

	key = g_object_get_data (G_OBJECT (button), "key");

	empathy_conf_set_bool (empathy_conf_get (),
			       key,
			       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
}

static void
preferences_radio_button_toggled_cb (GtkWidget *button,
				     gpointer   user_data)
{
	const gchar *key;
	const gchar *value = NULL;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
		return;
	}

	key = g_object_get_data (G_OBJECT (button), "key");

	if (key && strcmp (key, EMPATHY_PREFS_CONTACTS_SORT_CRITERIUM) == 0) {
		GSList      *group;
		GType        type;
		GEnumClass  *enum_class;
		GEnumValue  *enum_value;
		
		group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
		
		/* Get string from index */
		type = empathy_contact_list_store_sort_get_type ();
		enum_class = G_ENUM_CLASS (g_type_class_peek (type));
		enum_value = g_enum_get_value (enum_class, g_slist_index (group, button));
		
		if (!enum_value) {
			g_warning ("No GEnumValue for EmpathyContactListSort with GtkRadioButton index:%d", 
				   g_slist_index (group, button));
			return;
		}

		value = enum_value->value_nick;
	}

	empathy_conf_set_string (empathy_conf_get (), key, value);
}

static void
preferences_string_combo_changed_cb (GtkWidget *combo,
				     gpointer   user_data)
{
	const gchar  *key;
	GtkTreeModel *model;
	GtkTreeIter   iter;
	gchar        *name;

	key = g_object_get_data (G_OBJECT (combo), "key");

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter)) {
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

		gtk_tree_model_get (model, &iter,
				    COL_COMBO_NAME, &name,
				    -1);
		empathy_conf_set_string (empathy_conf_get (), key, name);
		g_free (name);
	}
}

static void
preferences_response_cb (GtkWidget         *widget,
			 gint               response,
			 EmpathyPreferences *preferences)
{
	gtk_widget_destroy (widget);
}

static void
preferences_destroy_cb (GtkWidget         *widget,
			EmpathyPreferences *preferences)
{
	GList *l;

	for (l = preferences->notify_ids; l; l = l->next) {
		guint id;

		id = GPOINTER_TO_UINT (l->data);
		empathy_conf_notify_remove (empathy_conf_get (), id);
	}

	g_list_free (preferences->notify_ids);
	g_free (preferences);
}

GtkWidget *
empathy_preferences_show (GtkWindow *parent)
{
	static EmpathyPreferences *preferences;
	GladeXML                 *glade;
	gchar                    *filename;

	if (preferences) {
		gtk_window_present (GTK_WINDOW (preferences->dialog));
		return preferences->dialog;
	}

	preferences = g_new0 (EmpathyPreferences, 1);

	filename = empathy_file_lookup ("empathy-preferences.glade", "src");
	glade = empathy_glade_get_file (filename,
		"preferences_dialog",
		NULL,
		"preferences_dialog", &preferences->dialog,
		"notebook", &preferences->notebook,
		"checkbutton_show_avatars", &preferences->checkbutton_show_avatars,
		"checkbutton_compact_contact_list", &preferences->checkbutton_compact_contact_list,
		"checkbutton_show_smileys", &preferences->checkbutton_show_smileys,
		"combobox_chat_theme", &preferences->combobox_chat_theme,
		"checkbutton_separate_chat_windows", &preferences->checkbutton_separate_chat_windows,
		"checkbutton_autoconnect", &preferences->checkbutton_autoconnect,
		"radiobutton_contact_list_sort_by_name", &preferences->radiobutton_contact_list_sort_by_name,
		"radiobutton_contact_list_sort_by_state", &preferences->radiobutton_contact_list_sort_by_state,
		"checkbutton_notifications_enabled", &preferences->checkbutton_notifications_enabled,
		"checkbutton_notifications_disabled_away", &preferences->checkbutton_notifications_disabled_away,
		"checkbutton_notifications_focus", &preferences->checkbutton_notifications_focus,
		"checkbutton_sounds_enabled", &preferences->checkbutton_sounds_enabled,
		"checkbutton_sounds_disabled_away", &preferences->checkbutton_sounds_disabled_away,
		"treeview_sounds", &preferences->treeview_sounds,
		"treeview_spell_checker", &preferences->treeview_spell_checker,
		NULL);
	g_free (filename);

	empathy_glade_connect (glade,
			      preferences,
			      "preferences_dialog", "destroy", preferences_destroy_cb,
			      "preferences_dialog", "response", preferences_response_cb,
			      NULL);

	g_object_unref (glade);

	g_object_add_weak_pointer (G_OBJECT (preferences->dialog), (gpointer) &preferences);

	preferences_themes_setup (preferences);

	preferences_setup_widgets (preferences);

	preferences_languages_setup (preferences);
	preferences_languages_add (preferences);
	preferences_languages_load (preferences);

	preferences_sound_setup (preferences);
	preferences_sound_load (preferences);

	if (empathy_spell_supported ()) {
		GtkWidget *page;

		page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (preferences->notebook), 2);
		gtk_widget_show (page);
	}

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (preferences->dialog),
					      GTK_WINDOW (parent));
	}

	gtk_widget_show (preferences->dialog);

	return preferences->dialog;
}

