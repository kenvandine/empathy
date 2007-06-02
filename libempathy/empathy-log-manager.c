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
#include <glib/gstdio.h>

#include "empathy-log-manager.h"
#include "gossip-contact.h"
#include "gossip-time.h"
#include "gossip-debug.h"
#include "gossip-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_LOG_MANAGER, EmpathyLogManagerPriv))

#define DEBUG_DOMAIN "LogManager"

#define LOG_DIR_CREATE_MODE       (S_IRUSR | S_IWUSR | S_IXUSR)
#define LOG_FILE_CREATE_MODE      (S_IRUSR | S_IWUSR)
#define LOG_FILENAME_SUFFIX       ".log"
#define LOG_TIME_FORMAT_FULL      "%Y%m%dT%H:%M:%S"
#define LOG_TIME_FORMAT           "%Y%m%d"
#define LOG_HEADER \
    "<?xml version='1.0' encoding='utf-8'?>\n" \
    "<?xml-stylesheet type=\"text/xsl\" href=\"gossip-log.xsl\"?>\n" \
    "<log>\n"

#define LOG_FOOTER \
    "</log>\n"

struct _EmpathyLogManagerPriv {
	gboolean dummy;
};

static void    empathy_log_manager_class_init         (EmpathyLogManagerClass *klass);
static void    empathy_log_manager_init               (EmpathyLogManager      *manager);
static void    log_manager_finalize                   (GObject                *object);
static gchar * log_manager_get_dir                    (McAccount              *account,
						       const gchar            *chat_id);
static gchar * log_manager_get_filename               (McAccount              *account,
						       const gchar            *chat_id);
static gchar * log_manager_get_filename_for_date      (McAccount              *account,
						       const gchar            *chat_id,
						       const gchar            *date);
static gchar * log_manager_get_timestamp_filename     (void);
static gchar * log_manager_get_timestamp_from_message (GossipMessage          *message);

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
				 GossipMessage     *message)
{
	FILE          *file;
	McAccount     *account;
	GossipContact *sender;
	const gchar   *body_str;
	const gchar   *str;
	gchar         *filename;
	gchar         *body;
	gchar         *timestamp;
	gchar         *contact_name;
	gchar         *contact_id;

	g_return_if_fail (EMPATHY_IS_LOG_MANAGER (manager));
	g_return_if_fail (chat_id != NULL);
	g_return_if_fail (GOSSIP_IS_MESSAGE (message));

	sender = gossip_message_get_sender (message);
	account = gossip_contact_get_account (sender);
	body_str = gossip_message_get_body (message);

	if (G_STR_EMPTY (body_str)) {
		return;
	}

	filename = log_manager_get_filename (account, chat_id);

	gossip_debug (DEBUG_DOMAIN, "Adding message: '%s' to file: '%s'",
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

	str = gossip_contact_get_name (sender);
	if (!str) {
		contact_name = g_strdup ("");
	} else {
		contact_name = g_markup_escape_text (str, -1);
	}

	str = gossip_contact_get_id (sender);
	if (!str) {
		contact_id = g_strdup ("");
	} else {
		contact_id = g_markup_escape_text (str, -1);
	}

	g_fprintf (file,
		   "<message time='%s' id='%s' name='%s'>%s</message>\n" LOG_FOOTER,
		   timestamp,
		   contact_id,
		   contact_name,
		   body);

	fclose (file);
	g_free (filename);
	g_free (contact_id);
	g_free (contact_name);
	g_free (timestamp);
	g_free (body);
}

GList *
empathy_log_manager_get_dates (EmpathyLogManager *manager,
			       McAccount         *account,
			       const gchar       *chat_id)
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

	directory = log_manager_get_dir (account, chat_id);
	if (!directory) {
		return NULL;
	}

	dir = g_dir_open (directory, 0, NULL);
	if (!dir) {
		gossip_debug (DEBUG_DOMAIN, "Could not open directory:'%s'", directory);
		g_free (directory);
		return NULL;
	}

	gossip_debug (DEBUG_DOMAIN, "Collating a list of dates in:'%s'", directory);

	while ((filename = g_dir_read_name (dir)) != NULL) {
		if (!g_str_has_suffix (filename, LOG_FILENAME_SUFFIX)) {
			continue;
		}

		p = strstr (filename, LOG_FILENAME_SUFFIX);
		date = g_strndup (filename, p - filename);
		if (!date) {
			continue;
		}

		dates = g_list_insert_sorted (dates, date, (GCompareFunc) strcmp);
	}

	g_free (directory);
	g_dir_close (dir);

	gossip_debug (DEBUG_DOMAIN, "Parsed %d dates", g_list_length (dates));

	return dates;
}

GList *
empathy_log_manager_get_messages_for_date (EmpathyLogManager *manager,
					   McAccount         *account,
					   const gchar       *chat_id,
					   const gchar       *date)
{
	gchar            *filename;
	GList            *messages = NULL;
	xmlParserCtxtPtr  ctxt;
	xmlDocPtr         doc;
	xmlNodePtr        log_node;
	xmlNodePtr        node;

	g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (chat_id != NULL, NULL);

	filename = log_manager_get_filename_for_date (account, chat_id, date);

	gossip_debug (DEBUG_DOMAIN, "Attempting to parse filename:'%s'...", filename);

	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		gossip_debug (DEBUG_DOMAIN, "Filename:'%s' does not exist", filename);
		g_free (filename);
		return NULL;
	}

	/* Create parser. */
	ctxt = xmlNewParserCtxt ();

	/* Parse and validate the file. */
	doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);
	if (!doc) {
		g_warning ("Failed to parse file:'%s'", filename);
		g_free (filename);
		xmlFreeParserCtxt (ctxt);
		return NULL;
	}

	/* The root node, presets. */
	log_node = xmlDocGetRootElement (doc);
	if (!log_node) {
		g_free (filename);
		xmlFreeDoc (doc);
		xmlFreeParserCtxt (ctxt);
		return NULL;
	}

	/* Now get the messages. */
	for (node = log_node->children; node; node = node->next) {
		GossipMessage *message;
		GossipContact *sender;
		gchar         *time;
		GossipTime     t;
		gchar         *sender_id;
		gchar         *sender_name;
		gchar         *body;

		if (strcmp (node->name, "message") != 0) {
			continue;
		}

		body = xmlNodeGetContent (node);
		time = xmlGetProp (node, "time");
		sender_id = xmlGetProp (node, "id");
		sender_name = xmlGetProp (node, "name");

		t = gossip_time_parse (time);

		sender = gossip_contact_new_full (account, sender_id, sender_name);
		message = gossip_message_new (body);
		gossip_message_set_sender (message, sender);
		gossip_message_set_timestamp (message, t);

		messages = g_list_append (messages, message);

		g_object_unref (sender);
		xmlFree (time);
		xmlFree (sender_id);
		xmlFree (sender_name);
		xmlFree (body);
	}

	gossip_debug (DEBUG_DOMAIN, "Parsed %d messages", g_list_length (messages));

	g_free (filename);
	xmlFreeDoc (doc);
	xmlFreeParserCtxt (ctxt);

	return messages;
}

GList *
empathy_log_manager_get_last_messages (EmpathyLogManager *manager,
				       McAccount         *account,
				       const gchar       *chat_id)
{
	GList *messages = NULL;
	GList *dates;
	GList *l;

	g_return_val_if_fail (EMPATHY_IS_LOG_MANAGER (manager), NULL);
	g_return_val_if_fail (MC_IS_ACCOUNT (account), NULL);
	g_return_val_if_fail (chat_id != NULL, NULL);

	dates = empathy_log_manager_get_dates (manager, account, chat_id);

	l = g_list_last (dates);
	if (l) {
		messages = empathy_log_manager_get_messages_for_date (manager,
								      account,
								      chat_id,
								      l->data);
	}

	g_list_foreach (dates, (GFunc) g_free, NULL);
	g_list_free (dates);

	return messages;
}

static gchar *
log_manager_get_dir (McAccount   *account,
		     const gchar *chat_id)
{
	const gchar *account_id;
	gchar       *basedir;

	account_id = mc_account_get_unique_name (account);
	basedir = g_build_path (G_DIR_SEPARATOR_S,
				g_get_home_dir (),
				".gnome2",
				PACKAGE_NAME,
				"logs",
				account_id,
				chat_id,
				NULL);

	if (!g_file_test (basedir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		gossip_debug (DEBUG_DOMAIN, "Creating directory:'%s'", basedir);

		g_mkdir_with_parents (basedir, LOG_DIR_CREATE_MODE);
	}

	return basedir;
}

static gchar *
log_manager_get_filename (McAccount   *account,
			  const gchar *chat_id)
{
	gchar *basedir;
	gchar *timestamp;
	gchar *filename;

	basedir = log_manager_get_dir (account, chat_id);
	timestamp = log_manager_get_timestamp_filename ();
	filename = g_build_filename (basedir, timestamp, NULL);

	g_free (basedir);
	g_free (timestamp);

	return filename;
}

static gchar *
log_manager_get_filename_for_date (McAccount   *account,
				   const gchar *chat_id,
				   const gchar *date)
{
	gchar *basedir;
	gchar *timestamp;
	gchar *filename;

	basedir = log_manager_get_dir (account, chat_id);
	timestamp = g_strconcat (date, LOG_FILENAME_SUFFIX, NULL);
	filename = g_build_filename (basedir, timestamp, NULL);

	g_free (basedir);
	g_free (timestamp);

	return filename;
}

static gchar *
log_manager_get_timestamp_filename (void)
{
	GossipTime  t;
	gchar      *time_str;
	gchar      *filename;

	t = gossip_time_get_current ();
	time_str = gossip_time_to_string_local (t, LOG_TIME_FORMAT);
	filename = g_strconcat (time_str, LOG_FILENAME_SUFFIX, NULL);

	g_free (time_str);

	return filename;
}

static gchar *
log_manager_get_timestamp_from_message (GossipMessage *message)
{
	GossipTime t;

	t = gossip_message_get_timestamp (message);

	/* We keep the timestamps in the messages as UTC. */
	return gossip_time_to_string_utc (t, LOG_TIME_FORMAT_FULL);
}

