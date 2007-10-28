/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Authors: Dafydd Harrie <dafydd.harries@collabora.co.uk>
 *          Xavier Claessens <xclaesse@gmail.com>
 */

#include <config.h>

#include <string.h>

#include <libempathy/empathy-debug.h>

#include "empathy-smiley-manager.h"
#include "empathy-ui-utils.h"

#define GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
		       EMPATHY_TYPE_SMILEY_MANAGER, EmpathySmileyManagerPriv))

#define DEBUG_DOMAIN "SmileyManager"

typedef struct {
	gunichar   c;
	GdkPixbuf *pixbuf;
	GSList    *childrens;
} SmileyManagerTree;

struct _EmpathySmileyManagerPriv {
	SmileyManagerTree *tree;
	GSList            *smileys;
};

static void empathy_smiley_manager_class_init (EmpathySmileyManagerClass *klass);
static void empathy_smiley_manager_init       (EmpathySmileyManager      *manager);

G_DEFINE_TYPE (EmpathySmileyManager, empathy_smiley_manager, G_TYPE_OBJECT);

static SmileyManagerTree *
smiley_manager_tree_new (gunichar c)
{
	SmileyManagerTree *tree;

	tree = g_slice_new0 (SmileyManagerTree);
	tree->c = c;
	tree->pixbuf = NULL;
	tree->childrens = NULL;

	return tree;
}

static void
smiley_manager_tree_free (SmileyManagerTree *tree)
{
	GSList *l;

	if (!tree) {
		return;
	}

	for (l = tree->childrens; l; l = l->next) {
		smiley_manager_tree_free (l->data);
	}

	if (tree->pixbuf) {
		g_object_unref (tree->pixbuf);
	}
	g_slist_free (tree->childrens);
	g_slice_free (SmileyManagerTree, tree);
}

static EmpathySmiley *
smiley_new (GdkPixbuf *pixbuf, const gchar *str)
{
	EmpathySmiley *smiley;

	smiley = g_slice_new0 (EmpathySmiley);
	if (pixbuf) {
		smiley->pixbuf = g_object_ref (pixbuf);
	}
	smiley->str = g_strdup (str);

	return smiley;
}

void
empathy_smiley_free (EmpathySmiley *smiley)
{
	if (!smiley) {
		return;
	}

	if (smiley->pixbuf) {
		g_object_unref (smiley->pixbuf);
	}
	g_free (smiley->str);
	g_slice_free (EmpathySmiley, smiley);
}

static void
smiley_manager_finalize (GObject *object)
{
	EmpathySmileyManagerPriv *priv = GET_PRIV (object);

	smiley_manager_tree_free (priv->tree);
	g_slist_foreach (priv->smileys, (GFunc) empathy_smiley_free, NULL);
	g_slist_free (priv->smileys);
}

static void
empathy_smiley_manager_class_init (EmpathySmileyManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = smiley_manager_finalize;

	g_type_class_add_private (object_class, sizeof (EmpathySmileyManagerPriv));
}

static void
empathy_smiley_manager_init (EmpathySmileyManager *manager)
{
	EmpathySmileyManagerPriv *priv = GET_PRIV (manager);

	priv->tree = smiley_manager_tree_new ('\0');
	priv->smileys = NULL;
}

EmpathySmileyManager *
empathy_smiley_manager_new (void)
{
	static EmpathySmileyManager *manager = NULL;

	if (!manager) {
		manager = g_object_new (EMPATHY_TYPE_SMILEY_MANAGER, NULL);
		g_object_add_weak_pointer (G_OBJECT (manager), (gpointer) &manager);
		empathy_smiley_manager_load (manager);
	} else {
		g_object_ref (manager);
	}

	return manager;
}

static SmileyManagerTree *
smiley_manager_tree_find_child (SmileyManagerTree *tree, gunichar c)
{
	GSList *l;

	for (l = tree->childrens; l; l = l->next) {
		SmileyManagerTree *child = l->data;

		if (child->c == c) {
			return child;
		}
	}

	return NULL;
}

static SmileyManagerTree *
smiley_manager_tree_find_or_insert_child (SmileyManagerTree *tree, gunichar c)
{
	SmileyManagerTree *child;

	child = smiley_manager_tree_find_child (tree, c);

	if (!child) {
		child = smiley_manager_tree_new (c);
		tree->childrens = g_slist_prepend (tree->childrens, child);
	}

	return child;
}

static void
smiley_manager_tree_insert (SmileyManagerTree *tree,
			    GdkPixbuf         *smiley,
			    const gchar       *str)
{
	SmileyManagerTree *child;

	child = smiley_manager_tree_find_or_insert_child (tree, g_utf8_get_char (str));

	str = g_utf8_next_char (str);
	if (*str) {
		smiley_manager_tree_insert (child, smiley, str);
		return;
	}

	child->pixbuf = g_object_ref (smiley);
}

static void
smiley_manager_add_valist (EmpathySmileyManager *manager,
			   GdkPixbuf            *smiley,
			   const gchar          *first_str,
			   va_list               var_args)
{
	EmpathySmileyManagerPriv *priv = GET_PRIV (manager);
	const gchar              *str;

	for (str = first_str; str; str = va_arg (var_args, gchar*)) {
		smiley_manager_tree_insert (priv->tree, smiley, str);
	}

	priv->smileys = g_slist_prepend (priv->smileys, smiley_new (smiley, first_str));
}

void
empathy_smiley_manager_add (EmpathySmileyManager *manager,
			    const gchar          *icon_name,
			    const gchar          *first_str,
			    ...)
{
	GdkPixbuf *smiley;
	va_list    var_args;

	g_return_if_fail (EMPATHY_IS_SMILEY_MANAGER (manager));
	g_return_if_fail (!G_STR_EMPTY (icon_name));
	g_return_if_fail (!G_STR_EMPTY (first_str));

	smiley = empathy_pixbuf_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
	if (smiley) {
		va_start (var_args, first_str);
		smiley_manager_add_valist (manager, smiley, first_str, var_args);
		va_end (var_args);
		g_object_unref (smiley);
	}
}

void
empathy_smiley_manager_add_from_pixbuf (EmpathySmileyManager *manager,
					GdkPixbuf            *smiley,
					const gchar          *first_str,
					...)
{
	va_list var_args;

	g_return_if_fail (EMPATHY_IS_SMILEY_MANAGER (manager));
	g_return_if_fail (GDK_IS_PIXBUF (smiley));
	g_return_if_fail (!G_STR_EMPTY (first_str));

	va_start (var_args, first_str);
	smiley_manager_add_valist (manager, smiley, first_str, var_args);
	va_end (var_args);
}

void
empathy_smiley_manager_load (EmpathySmileyManager *manager)
{
	g_return_if_fail (EMPATHY_IS_SMILEY_MANAGER (manager));

	/* From fd.o icon-naming spec */
	empathy_smiley_manager_add (manager, "face-angel",      "0:-)",  "0:)",  NULL);
        empathy_smiley_manager_add (manager, "face-cool",       "B-)",   "B)",   NULL);
	empathy_smiley_manager_add (manager, "face-crying",     ":'(", NULL);
	empathy_smiley_manager_add (manager, "face-devilish",   ">:-)",  ">:)",  NULL);
        empathy_smiley_manager_add (manager, "face-embarrassed",":-[",   ":[",   ":-$", ":$", NULL);
	empathy_smiley_manager_add (manager, "face-kiss",       ":-*",   ":*",   NULL);
	empathy_smiley_manager_add (manager, "face-monkey",     ":-(|)", ":(|)", NULL);
	empathy_smiley_manager_add (manager, "face-plain",      ":-|",   ":|",   NULL);
        empathy_smiley_manager_add (manager, "face-raspberry",  ":-P",   ":P",	 ":-p", ":p", NULL);
	empathy_smiley_manager_add (manager, "face-sad",        ":-(",   ":(",   NULL);
	empathy_smiley_manager_add (manager, "face-smile",      ":-)",   ":)",   NULL);
	empathy_smiley_manager_add (manager, "face-smile-big",  ":-D",   ":D",   ":-d", ":d", NULL);
	empathy_smiley_manager_add (manager, "face-smirk",      ":-!",   ":!",   NULL);
	empathy_smiley_manager_add (manager, "face-surprise",   ":-0",   ":0",   NULL);
	empathy_smiley_manager_add (manager, "face-wink",       ";-)",   ";)",   NULL);
}

GSList *
empathy_smiley_manager_parse (EmpathySmileyManager *manager,
			      const gchar          *text)
{
	EmpathySmileyManagerPriv *priv = GET_PRIV (manager);
	EmpathySmiley            *smiley;
	SmileyManagerTree        *cur_tree = priv->tree;
	const gchar              *t;
	const gchar              *cur_str = text;
	GSList                   *smileys = NULL;

	g_return_val_if_fail (EMPATHY_IS_SMILEY_MANAGER (manager), NULL);
	g_return_val_if_fail (text != NULL, NULL);

	for (t = text; *t; t = g_utf8_next_char (t)) {
		SmileyManagerTree *child;
		gunichar           c;
		
		c = g_utf8_get_char (t);
		child = smiley_manager_tree_find_child (cur_tree, c);

		if (cur_tree == priv->tree) {
			if (child) {
				if (t > cur_str) {
					smiley = smiley_new (NULL, g_strndup (cur_str, t - cur_str));
					smileys = g_slist_prepend (smileys, smiley);
				}
				cur_str = t;
				cur_tree = child;
			}

			continue;
		}

		if (child) {
			cur_tree = child;
			continue;
		}

		smiley = smiley_new (cur_tree->pixbuf, g_strndup (cur_str, t - cur_str));
		smileys = g_slist_prepend (smileys, smiley);
		if (cur_tree->pixbuf) {
			cur_str = t;
			cur_tree = smiley_manager_tree_find_child (priv->tree, c);

			if (!cur_tree) {
				cur_tree = priv->tree;
			}
		} else {
			cur_str = t;
			cur_tree = priv->tree;
		}
	}

	smiley = smiley_new (cur_tree->pixbuf, g_strndup (cur_str, t - cur_str));
	smileys = g_slist_prepend (smileys, smiley);

	return g_slist_reverse (smileys);
}

GSList *
empathy_smiley_manager_get_all (EmpathySmileyManager *manager)
{
	EmpathySmileyManagerPriv *priv = GET_PRIV (manager);

	return priv->smileys;
}

