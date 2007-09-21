/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
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
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>

#include "empathy-log-manager.h"
#include "empathy-contact.h"
#include "empathy-time.h"
#include "empathy-debug.h"
#include "empathy-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_LOG_MANAGER, EmpathyLogManagerPriv))

#define DEBUG_DOMAIN "LogManager"

#define LOG_DIR_CREATE_MODE       (S_IRUSR | S_IWUSR | S_IXUSR)
#define LOG_FILE_CREATE_MODE      (S_IRUSR | S_IWUSR)
#define LOG_DIR_CHATROOMS         "chatrooms"
#define LOG_FILENAME_SUFFIX       ".log"
#define LOG_TIME_FORMAT_FULL      "%Y%m%dT%H:%M:%S"
#define LOG_TIME_FORMAT           "%Y%m%d"
#define LOG_HEADER \
    "<?xml version='1.0' encoding='utf-8'?>\n" \
    "<?xml-stylesheet type=\"text/xsl\" href=\"empathy-log.xsl\"?>\n" \
    "<log>\n"

#define LOG_FOOTER \
    "</log>\n"

struct _EmpathyLogManagerPriv {
	gchar *basedir;
};

static void                 empathy_log_manager_class_init         (EmpathyLogManagerClass *klass);
static void                 empathy_log_manager_init               (EmpathyLogManager      *manager);
static void                 log_manager_finalize                   (GObject                *object);
static const gchar *        log_manager_get_basedir                (EmpathyLogManager      *manager);
static GList *              log_manager_get_all_files              (EmpathyLogManager      *manager,
								    const gchar            *dir);
static GList *              log_manager_get_chats                  (EmpathyLogManager      *manager,
								    const gchar            *dir,
								    gboolean                is_chatroom);
static gchar *              log_manager_get_dir                    (EmpathyLogManager      *manager,
								    McAccount              *account,
								    const gchar            *chat_id,
								    gboolean                chatroom);
static gchar *              log_manager_get_filename               (EmpathyLogManager      *manager,
								    McAccount              *account,
								    const gchar            *chat_id,
								    gboolean                chatroom);
static gchar *              log_manager_get_filename_for_date      (EmpathyLogManager      *manager,
								    McAccount              *account,
								    const gchar            *chat_id,
								    gboolean                chatroom,
								    const gchar            *date);
static gchar *              log_manager_get_timestamp_filename     (void);
static gchar *              log_manager_get_timestamp_from_message (EmpathyMessage          *message);
static EmpathyLogSearchHit *log_manager_search_hit_new             (EmpathyLogManager      *manager,
								    const gchar            *filename);
static void                 log_manager_search_hit_free            (EmpathyLogSearchHit    *hit);

G_DEFINE_TYPE (EmpathyLogManager, empathy_log_manager, G_TYPE_OBJECT);

static void
empathy_log_manager_class_init (EmpathyLogManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = log_manager_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathyLogManagerPriv));
}

static void
empathy_log_manager_init (EmpathyLogManager *manager)
{
}

static void
log_manager_finalize (GObject *object)
{
	EmpathyLogManagerPriv *priv;

	priv = GET_PRIV (object);

	g_free (priv->basedir);
}

EmpathyLogManager *
empathy_log_manager_new (void)
{
	static EmpathyLogManager *manager = NULL;

	if (!manager) {
		manager = g_object_new (EMPATHY_TYPE_LOG_MANAGER, NULL);
		g_object_add_weak_pointer (G_OBJECT (manager), (gpointer) &manager);
	} else {
		g_object_ref (manager);
	}

	return manager;
}

void
empathy_log_manager_add_message (EmpathyLogManager *manager,
				 const gchar       *chat_id,
				 gboolean           chatroom,
				 EmpathyMessage     *message)
{
	FILE          *file;
	McAccount     *account;
	EmpathyContact *sender;
	const gchar   *body_str;
	const gchar   *str;
	gchar         *filename;
	gchar         *basedir;
	gchar         *body;
	gchar         *timestamp;
	gchar         *contact_name;
	gchar         *contact_id;
	EmpathyMessageType msg_type;

	g_return_if_fail (EMPATHY_IS_LOG_MANAGER (manager));
	g_return_if_fail (chat_id != NULL);
	g_return_if_fail (EMPATHY_IS_MESSAGE (message));

	sender = empathy_message_get_sender (message);
	account = empathy_contact_get_account (sender);
	body_str = empathy_message_get_body (message);
	msg_type = empathy_message_get_type (message);

	if (G_STR_EMPTY (body_str)) {
		return;
	}

	filename = log_manager_get_filename (manager, account, chat_id, chatroom);
	basedir = g_path_get_dirname (filename);
	if (!g_file_test (basedir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		empathy_debug (DEBUG_DOMAIN, "Creating directory:'%s'", basedir);

		g_mkdir_with_parents (basedir, LOG_DIR_CREATE_MODE);
	}
	g_free (basedir);

	empathy_debug (DEBUG_DOMAIN, "Adding message: '%s' to file: '%s'",
		      body_str, filename);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		file = g_fopen (filename, "w+");
		if (file) {
			g_fprintf (file, LOG_HEADER);
		}
		g_chmod (filename, LOG_FILE_CREATE_MODE);
	} else {
		file = g_fopen (filename, "r+");
		if (file) {
			fseek (file, - strlen (LOG_FOOTER), SEEK_END);
		}
	}

	body = g_markup_escape_text (body_str, -1);
	timestamp = log_manager_get_timestamp_from_message (message);

	str = empathy_contact_get_name (sender);
	contact_name = g_markup_escape_text (str, -1);

	str = empathy_contact_get_id (sender);
	contact_id = g_markup_escape_text (str, -1);

	g_fprintf (file,
		   "<message time='%s' id='%s' name='%s' isuser='%s' type='%s'>%s</message>\n" LOG_FOOTER,
		   timestamp,
		   contact_id,
		   contact_name,
		   empathy_contact_is_user (sender) ? "true" : "false",
		   empathy_message_type_to_str (msg_type),
		   body);

	fclose (file);
	g_free (filename);
	g_free (contact_id);
	g_free (contact_name);
	g_free (timestamp);
	g_free (body);
}

gboolean
empathy_log_manager_exists (EmpathyLogManager *manager,
			    McAccount         *account,
			    const gchar       *chat_id,
			    gboolean           chatroom)
{
	gchar    *dir;
	gboolean  exists;

	dir = log_manager_get_dir (manager, account, chat_id, chatroom);
	exists = g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR);
	g_free (dir);

	return exists;
}

GList *
empathy_log_manager_get_dates (EmpathyLogManager *manager,
			       McAccount         *account,
			       const gchar       *chat_id,
			       gboolean           chatroom)
{
	GList       *dates = NULL;
	gchar       *date;
	gchar       *directory;
	GDir        *dir;
	const gchar *filename;
	const gchar *p;

	g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (chat_id != NULL, NULL);

	directory = log_manager_get_dir (manager, account, chat_id, chatroom);
	dir = g_dir_open (directory, 0, NULL);
	if (!dir) {
		empathy_debug (DEBUG_DOMAIN, "Could not open directory:'%s'", directory);
		g_free (directory);
		return NULL;
	}

	empathy_debug (DEBUG_DOMAIN, "Collating a list of dates in:'%s'", directory);

	while ((filename = g_dir_read_name (dir)) != NULL) {
		if (!g_str_has_suffix (filename, LOG_FILENAME_SUFFIX)) {
			continue;
		}

		p = strstr (filename, LOG_FILENAME_SUFFIX);
		date = g_strndup (filename, p - filename);
		if (!date) {
			continue;
		}

		if (!g_regex_match_simple ("\\d{8}", date, 0, 0)) {
			continue;
		}

		dates = g_list_insert_sorted (dates, date, (GCompareFunc) strcmp);
	}

	g_free (directory);
	g_dir_close (dir);

	empathy_debug (DEBUG_DOMAIN, "Parsed %d dates", g_list_length (dates));

	return dates;
}

GList *
empathy_log_manager_get_messages_for_file (EmpathyLogManager *manager,
					   const gchar       *filename)
{
	GList               *messages = NULL;
	xmlParserCtxtPtr     ctxt;
	xmlDocPtr            doc;
	xmlNodePtr           log_node;
	xmlNodePtr           node;
	EmpathyLogSearchHit *hit;
	McAccount           *account;

	g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
	g_return_val_if_fail (filename != NULL, NULL);

	empathy_debug (DEBUG_DOMAIN, "Attempting to parse filename:'%s'...", filename);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		empathy_debug (DEBUG_DOMAIN, "Filename:'%s' does not exist", filename);
		return NULL;
	}

	/* Get the account from the filename */
	hit = log_manager_search_hit_new (manager, filename);
	account = g_object_ref (hit->account);
	log_manager_search_hit_free (hit);

	/* Create parser. */
	ctxt = xmlNewParserCtxt ();

	/* Parse and validate the file. */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);
	if (!doc) {
		g_warning ("Failed to parse file:'%s'", filename);
		xmlFreeParserCtxt (ctxt);
		return NULL;
	}

	/* The root node, presets. */
	log_node = xmlDocGetRootElement (doc);
	if (!log_node) {
		xmlFreeDoc (doc);
		xmlFreeParserCtxt (ctxt);
		return NULL;
	}

	/* Now get the messages. */
	for (node = log_node->children; node; node = node->next) {
		EmpathyMessage     *message;
		EmpathyContact     *sender;
		gchar              *time;
		EmpathyTime         t;
		gchar              *sender_id;
		gchar              *sender_name;
		gchar              *body;
		gchar              *is_user_str;
		gboolean            is_user = FALSE;
		gchar              *msg_type_str;
		EmpathyMessageType  msg_type = EMPATHY_MESSAGE_TYPE_NORMAL;

		if (strcmp (node->name, "message") != 0) {
			continue;
		}

		body = xmlNodeGetContent (node);
		time = xmlGetProp (node, "time");
		sender_id = xmlGetProp (node, "id");
		sender_name = xmlGetProp (node, "name");
		is_user_str = xmlGetProp (node, "isuser");
		msg_type_str = xmlGetProp (node, "type");

		if (is_user_str) {
			is_user = strcmp (is_user_str, "true") == 0;
		}
		if (msg_type_str) {
			msg_type = empathy_message_type_from_str (msg_type_str);
		}

		t = empathy_time_parse (time);

		sender = empathy_contact_new_full (account, sender_id, sender_name);
		empathy_contact_set_is_user (sender, is_user);
		message = empathy_message_new (body);
		empathy_message_set_sender (message, sender);
		empathy_message_set_timestamp (message, t);
		empathy_message_set_type (message, msg_type);

		messages = g_list_append (messages, message);

		g_object_unref (sender);
		xmlFree (time);
		xmlFree (sender_id);
		xmlFree (sender_name);
		xmlFree (body);
		xmlFree (is_user_str);
		xmlFree (msg_type_str);
	}

	empathy_debug (DEBUG_DOMAIN, "Parsed %d messages", g_list_length (messages));

	xmlFreeDoc (doc);
	xmlFreeParserCtxt (ctxt);

	return messages;
}

GList *
empathy_log_manager_get_messages_for_date (EmpathyLogManager *manager,
					   McAccount         *account,
					   const gchar       *chat_id,
					   gboolean           chatroom,
					   const gchar       *date)
{
	gchar *filename;
	GList *messages;

	g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (chat_id != NULL, NULL);

	filename = log_manager_get_filename_for_date (manager, account, chat_id, chatroom, date);
	messages = empathy_log_manager_get_messages_for_file (manager, filename);
	g_free (filename);

	return messages;
}

GList *
empathy_log_manager_get_last_messages (EmpathyLogManager *manager,
				       McAccount         *account,
				       const gchar       *chat_id,
				       gboolean           chatroom)
{
	GList *messages = NULL;
	GList *dates;
	GList *l;

	g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (chat_id != NULL, NULL);

	dates = empathy_log_manager_get_dates (manager, account, chat_id, chatroom);

	l = g_list_last (dates);
	if (l) {
		messages = empathy_log_manager_get_messages_for_date (manager,
								      account,
								      chat_id,
								      chatroom,
								      l->data);
	}

	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);

	return messages;
}

GList *
empathy_log_manager_get_chats (EmpathyLogManager *manager,
			       McAccount         *account)
{
	const gchar *basedir;
	gchar       *dir;

	basedir = log_manager_get_basedir (manager);
	dir = g_build_filename (basedir,
				mc_account_get_unique_name (account),
				NULL);

	return log_manager_get_chats (manager, dir, FALSE);
}

GList *
empathy_log_manager_search_new (EmpathyLogManager *manager,
				const gchar       *text)
{
	GList *files, *l;
	GList *hits = NULL;
	gchar *text_casefold;

	g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
	g_return_val_if_fail (!G_STR_EMPTY (text), NULL);

	text_casefold = g_utf8_casefold (text, -1);

	files = log_manager_get_all_files (manager, NULL);
	empathy_debug (DEBUG_DOMAIN, "Found %d log files in total",
		      g_list_length (files));

	for (l = files; l; l = l->next) {
		gchar       *filename;
		GMappedFile *file;
		gsize        length;
		gchar       *contents;
		gchar       *contents_casefold;

		filename = l->data;

		file = g_mapped_file_new (filename, FALSE, NULL);
		if (!file) {
			continue;
		}

		length = g_mapped_file_get_length (file);
		contents = g_mapped_file_get_contents (file);
		contents_casefold = g_utf8_casefold (contents, length);

		g_mapped_file_free (file);

		if (strstr (contents_casefold, text_casefold)) {
			EmpathyLogSearchHit *hit;

			hit = log_manager_search_hit_new (manager, filename);

			if (hit) {
				hits = g_list_prepend (hits, hit);
				empathy_debug (DEBUG_DOMAIN, 
					      "Found text:'%s' in file:'%s' on date:'%s'...",
					      text, hit->filename, hit->date);
			}
		}

		g_free (contents_casefold);
		g_free (filename);
	}
	g_list_free (files);

	g_free (text_casefold);

	return hits;
}

void
empathy_log_manager_search_free (GList *hits)
{
	GList *l;

	for (l = hits; l; l = l->next) {
		log_manager_search_hit_free (l->data);
	}
	
	g_list_free (hits);
}

/* Format is just date, 20061201. */
gchar *
empathy_log_manager_get_date_readable (const gchar *date)
{
	EmpathyTime t;

	t = empathy_time_parse (date);

	return empathy_time_to_string_local (t, "%a %d %b %Y");
}

static const gchar *
log_manager_get_basedir (EmpathyLogManager *manager)
{
	EmpathyLogManagerPriv *priv;	

	priv = GET_PRIV (manager);

	if (priv->basedir) {
		return priv->basedir;
	}

	priv->basedir = g_build_path (G_DIR_SEPARATOR_S,
				      g_get_home_dir (),
				      ".gnome2",
				      PACKAGE_NAME,
				      "logs",
				      NULL);

	return priv->basedir;
}

static GList *
log_manager_get_all_files (EmpathyLogManager *manager,
			   const gchar       *dir)
{
	GDir        *gdir;
	GList       *files = NULL;
	const gchar *name;
	
	if (!dir) {
		dir = log_manager_get_basedir (manager);
	}

	gdir = g_dir_open (dir, 0, NULL);
	if (!gdir) {
		return NULL;
	}

	while ((name = g_dir_read_name (gdir)) != NULL) {
		gchar *filename;

		filename = g_build_filename (dir, name, NULL);
		if (g_str_has_suffix (filename, LOG_FILENAME_SUFFIX)) {
			files = g_list_prepend (files, filename);
			continue;
		}

		if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
			/* Recursively get all log files */
			files = g_list_concat (files, log_manager_get_all_files (manager, filename));
		}
		g_free (filename);
	}

	g_dir_close (gdir);

	return files;
}

static GList *
log_manager_get_chats (EmpathyLogManager *manager,
		       const gchar       *dir,
		       gboolean           is_chatroom)
{
	GDir        *gdir;
	GList       *hits = NULL;
	const gchar *name;

	gdir = g_dir_open (dir, 0, NULL);
	if (!gdir) {
		return NULL;
	}

	while ((name = g_dir_read_name (gdir)) != NULL) {
		EmpathyLogSearchHit *hit;
		gchar *filename;

		filename = g_build_filename (dir, name, NULL);
		if (strcmp (name, LOG_DIR_CHATROOMS) == 0) {
			hits = g_list_concat (hits, log_manager_get_chats (manager, filename, TRUE));
			g_free (filename);
			continue;
		}

		hit = g_slice_new0 (EmpathyLogSearchHit);
		hit->chat_id = g_strdup (name);
		hit->is_chatroom = is_chatroom;

		hits = g_list_prepend (hits, hit);
	}

	g_dir_close (gdir);

	return hits;
}

static gchar *
log_manager_get_dir (EmpathyLogManager *manager,
		     McAccount         *account,
		     const gchar       *chat_id,
		     gboolean           chatroom)
{
	const gchar *account_id;
	gchar       *basedir;

	account_id = mc_account_get_unique_name (account);

	if (chatroom) {
		basedir = g_build_path (G_DIR_SEPARATOR_S,
					log_manager_get_basedir (manager),
					account_id,
					LOG_DIR_CHATROOMS,
					chat_id,
					NULL);
	} else {
		basedir = g_build_path (G_DIR_SEPARATOR_S,
					log_manager_get_basedir (manager),
					account_id,
					chat_id,
					NULL);
	}

	return basedir;
}

static gchar *
log_manager_get_filename (EmpathyLogManager *manager,
			  McAccount         *account,
			  const gchar       *chat_id,
			  gboolean           chatroom)
{
	gchar *basedir;
	gchar *timestamp;
	gchar *filename;

	basedir = log_manager_get_dir (manager, account, chat_id, chatroom);
	timestamp = log_manager_get_timestamp_filename ();
	filename = g_build_filename (basedir, timestamp, NULL);

	g_free (basedir);
	g_free (timestamp);

	return filename;
}

static gchar *
log_manager_get_filename_for_date (EmpathyLogManager *manager,
				   McAccount         *account,
				   const gchar       *chat_id,
				   gboolean           chatroom,
				   const gchar       *date)
{
	gchar *basedir;
	gchar *timestamp;
	gchar *filename;

	basedir = log_manager_get_dir (manager, account, chat_id, chatroom);
	timestamp = g_strconcat (date, LOG_FILENAME_SUFFIX, NULL);
	filename = g_build_filename (basedir, timestamp, NULL);

	g_free (basedir);
	g_free (timestamp);

	return filename;
}

static gchar *
log_manager_get_timestamp_filename (void)
{
	EmpathyTime  t;
	gchar      *time_str;
	gchar      *filename;

	t = empathy_time_get_current ();
	time_str = empathy_time_to_string_local (t, LOG_TIME_FORMAT);
	filename = g_strconcat (time_str, LOG_FILENAME_SUFFIX, NULL);

	g_free (time_str);

	return filename;
}

static gchar *
log_manager_get_timestamp_from_message (EmpathyMessage *message)
{
	EmpathyTime t;

	t = empathy_message_get_timestamp (message);

	/* We keep the timestamps in the messages as UTC. */
	return empathy_time_to_string_utc (t, LOG_TIME_FORMAT_FULL);
}

static EmpathyLogSearchHit *
log_manager_search_hit_new (EmpathyLogManager *manager,
			    const gchar       *filename)
{
	EmpathyLogSearchHit  *hit;
	const gchar          *account_name;
	const gchar          *end;
	gchar               **strv;
	guint                 len;

	if (!g_str_has_suffix (filename, LOG_FILENAME_SUFFIX)) {
		return NULL;
	}

	strv = g_strsplit (filename, G_DIR_SEPARATOR_S, -1);
	len = g_strv_length (strv);

	hit = g_slice_new0 (EmpathyLogSearchHit);

	end = strstr (strv[len-1], LOG_FILENAME_SUFFIX);
	hit->date = g_strndup (strv[len-1], end - strv[len-1]);
	hit->chat_id = g_strdup (strv[len-2]);
	hit->is_chatroom = (strcmp (strv[len-3], LOG_DIR_CHATROOMS) == 0);
	if (hit->is_chatroom) {
		account_name = strv[len-4];
	} else {
		account_name = strv[len-3];
	}
	hit->account = mc_account_lookup (account_name);
	hit->filename = g_strdup (filename);

	g_strfreev (strv);

	return hit;
}

static void
log_manager_search_hit_free (EmpathyLogSearchHit *hit)
{
	if (hit->account) {
		g_object_unref (hit->account);
	}

	g_free (hit->date);
	g_free (hit->filename);
	g_free (hit->chat_id);

	g_slice_free (EmpathyLogSearchHit, hit);
}
