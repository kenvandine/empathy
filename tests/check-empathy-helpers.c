/*
 * check-empathy-helpers.c - Source for some check helpers
 * Copyright (C) 2007-2008 Collabora Ltd.
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
 */

#include <stdio.h>
#include <stdlib.h>

#include <glib/gstdio.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include "check-helpers.h"
#include "check-empathy-helpers.h"

gchar *
get_xml_file (const gchar *filename)
{
  return g_build_filename (g_getenv ("EMPATHY_SRCDIR"), "tests", "xml",
      filename, NULL);
}

gchar *
get_user_xml_file (const gchar *filename)
{
  return g_build_filename (g_get_tmp_dir (), filename, NULL);
}

void
copy_xml_file (const gchar *orig,
               const gchar *dest)
{
  gboolean result;
  gchar *buffer;
  gsize length;
  gchar *sample;
  gchar *file;

  sample = get_xml_file (orig);
  result = g_file_get_contents (sample, &buffer, &length, NULL);
  fail_if (!result);

  file = get_user_xml_file (dest);
  result = g_file_set_contents (file, buffer, length, NULL);
  fail_if (!result);

  g_free (sample);
  g_free (file);
  g_free (buffer);
}

McAccount *
get_test_account (void)
{
  McProfile *profile;
  McAccount *account;
  GList *accounts;

  profile = mc_profile_lookup ("test");
  accounts = mc_accounts_list_by_profile (profile);
  if (g_list_length (accounts) == 0)
    {
      /* need to create a test account */
      account = mc_account_create (profile);
    }
  else
    {
      /* reuse an existing test account */
      account = accounts->data;
    }

  g_object_unref (profile);

  return account;
}

/* Not used for now as there is no API to remove completely gconf keys.
 * So we reuse existing accounts instead of creating new ones */
void
destroy_test_account (McAccount *account)
{
  GConfClient *client;
  gchar *path;
  GError *error = NULL;
  GSList *entries = NULL, *l;

  client = gconf_client_get_default ();
  path = g_strdup_printf ("/apps/telepathy/mc/accounts/%s",
      mc_account_get_unique_name (account));

  entries = gconf_client_all_entries (client, path, &error);
  if (error != NULL)
    {
      g_print ("failed to list entries in %s: %s\n", path, error->message);
      g_error_free (error);
      error = NULL;
    }

  for (l = entries; l != NULL; l = g_slist_next (l))
    {
      GConfEntry *entry = l->data;

      if (g_str_has_suffix (entry->key, "data_dir"))
        {
          gchar *dir;

          dir = gconf_client_get_string (client, entry->key, &error);
          if (error != NULL)
            {
              g_print ("get data_dir string failed: %s\n", entry->key);
              g_error_free (error);
              error = NULL;
            }
          else
            {
              if (g_rmdir (dir) != 0)
                g_print ("can't remove %s\n", dir);
            }

          g_free (dir);
        }

      /* FIXME: this doesn't remove the key */
      gconf_client_unset (client, entry->key, &error);
      if (error != NULL)
        {
          g_print ("unset of %s failed: %s\n", path, error->message);
          g_error_free (error);
          error = NULL;
        }

      gconf_entry_free (entry);
    }

  g_slist_free (entries);

  g_object_unref (client);
  g_free (path);

  mc_account_delete (account);
  g_object_unref (account);
}
