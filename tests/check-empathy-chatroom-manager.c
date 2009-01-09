#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib/gstdio.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <telepathy-glib/util.h>
#include <check.h>

#include "check-helpers.h"
#include "check-libempathy.h"
#include "check-empathy-helpers.h"

#include <libempathy/empathy-chatroom-manager.h>

#define CHATROOM_SAMPLE "chatrooms-sample.xml"
#define CHATROOM_FILE "chatrooms.xml"

static void
check_chatroom (EmpathyChatroom *chatroom,
                const gchar *name,
                const gchar *room,
                gboolean auto_connect,
                gboolean favorite)
{
  gboolean _favorite;

  fail_if (tp_strdiff (empathy_chatroom_get_name (chatroom), name));
  fail_if (tp_strdiff (empathy_chatroom_get_room (chatroom), room));
  fail_if (empathy_chatroom_get_auto_connect (chatroom) != auto_connect);
  g_object_get (chatroom, "favorite", &_favorite, NULL);
  fail_if (favorite != _favorite);
}

struct chatroom_t
{
  gchar *name;
  gchar *room;
  gboolean auto_connect;
  gboolean favorite;
};

static void
check_chatrooms_list (EmpathyChatroomManager *mgr,
                      McAccount *account,
                      struct chatroom_t *_chatrooms,
                      guint nb_chatrooms)
{
  GList *chatrooms, *l;
  guint i;
  GHashTable *found;

  fail_if (empathy_chatroom_manager_get_count (mgr, account) != nb_chatrooms);

  found = g_hash_table_new (g_str_hash, g_str_equal);
  for (i = 0; i < nb_chatrooms; i++)
    {
      g_hash_table_insert (found, _chatrooms[i].room, &_chatrooms[i]);
    }

  chatrooms = empathy_chatroom_manager_get_chatrooms (mgr, account);
  fail_if (g_list_length (chatrooms) != nb_chatrooms);

  for (l = chatrooms; l != NULL; l = g_list_next (l))
    {
      EmpathyChatroom *chatroom = l->data;
      struct chatroom_t *tmp;

      tmp = g_hash_table_lookup (found, empathy_chatroom_get_room (chatroom));
      fail_if (tmp == NULL);

      check_chatroom (chatroom, tmp->name, tmp->room, tmp->auto_connect,
          tmp->favorite);

      g_hash_table_remove (found, empathy_chatroom_get_room (chatroom));
    }

  fail_if (g_hash_table_size (found) != 0);

  g_list_free (chatrooms);
  g_hash_table_destroy (found);
}

static gboolean
change_account_name_in_file (McAccount *account,
                             const gchar *file)
{
  gchar *cmd;

  cmd = g_strdup_printf ("sed -i 's/CHANGE_ME/%s/' %s",
      mc_account_get_unique_name (account), file);

  if (system (cmd) == -1)
    {
      g_print ("'%s' call failed\n", cmd);
      g_free (cmd);
      return FALSE;
    }

  g_free (cmd);
  return TRUE;
}

START_TEST (test_empathy_chatroom_manager_dup_singleton)
{
  EmpathyChatroomManager *mgr;
  gchar *file;
  McAccount *account;
  struct chatroom_t chatrooms[] = {
        { "name1", "room1", TRUE, TRUE },
        { "name2", "room2", FALSE, TRUE }};

  account = get_test_account ();

  copy_xml_file (CHATROOM_SAMPLE, CHATROOM_FILE);

  file = get_user_xml_file (CHATROOM_FILE);

  /* change the chatrooms XML file to use the account we just created */
  if (!change_account_name_in_file (account, file))
    return;

  mgr = empathy_chatroom_manager_dup_singleton (file);
  check_chatrooms_list (mgr, account, chatrooms, 2);

  g_free (file);
  g_object_unref (mgr);
  g_object_unref (account);
}
END_TEST

START_TEST (test_empathy_chatroom_manager_add)
{
  EmpathyChatroomManager *mgr;
  gchar *file;
  McAccount *account;
  struct chatroom_t chatrooms[] = {
        { "name1", "room1", TRUE, TRUE },
        { "name2", "room2", FALSE, TRUE },
        { "name3", "room3", FALSE, TRUE },
        { "name4", "room4", FALSE, FALSE }};
  EmpathyChatroom *chatroom;

  account = get_test_account ();

  copy_xml_file (CHATROOM_SAMPLE, CHATROOM_FILE);

  file = get_user_xml_file (CHATROOM_FILE);

  /* change the chatrooms XML file to use the account we just created */
  if (!change_account_name_in_file (account, file))
    return;

  mgr = empathy_chatroom_manager_dup_singleton (file);

  /* add a favorite chatroom */
  chatroom = empathy_chatroom_new_full (account, "room3", "name3", FALSE);
  g_object_set (chatroom, "favorite", TRUE, NULL);
  empathy_chatroom_manager_add (mgr, chatroom);
  g_object_unref (chatroom);

  check_chatrooms_list (mgr, account, chatrooms, 3);

  /* reload chatrooms file */
  g_object_unref (mgr);
  mgr = empathy_chatroom_manager_dup_singleton (file);

  /* chatroom has been added to the XML file as it's a favorite */
  check_chatrooms_list (mgr, account, chatrooms, 3);

  /* add a non favorite chatroom */
  chatroom = empathy_chatroom_new_full (account, "room4", "name4", FALSE);
  g_object_set (chatroom, "favorite", FALSE, NULL);
  empathy_chatroom_manager_add (mgr, chatroom);
  g_object_unref (chatroom);

  check_chatrooms_list (mgr, account, chatrooms, 4);

  /* reload chatrooms file */
  g_object_unref (mgr);
  mgr = empathy_chatroom_manager_dup_singleton (file);

  /* chatrooms has not been added to the XML file */
  check_chatrooms_list (mgr, account, chatrooms, 3);

  g_object_unref (mgr);
  g_free (file);
  g_object_unref (account);
}
END_TEST

START_TEST (test_empathy_chatroom_manager_remove)
{
  EmpathyChatroomManager *mgr;
  gchar *file;
  McAccount *account;
  struct chatroom_t chatrooms[] = {
        { "name2", "room2", FALSE, TRUE }};
  EmpathyChatroom *chatroom;

  account = get_test_account ();

  copy_xml_file (CHATROOM_SAMPLE, CHATROOM_FILE);

  file = get_user_xml_file (CHATROOM_FILE);

  /* change the chatrooms XML file to use the account we just created */
  if (!change_account_name_in_file (account, file))
    return;

  mgr = empathy_chatroom_manager_dup_singleton (file);

  /* remove room1 */
  chatroom = empathy_chatroom_manager_find (mgr, account, "room1");
  fail_if (chatroom == NULL);
  empathy_chatroom_manager_remove (mgr, chatroom);

  check_chatrooms_list (mgr, account, chatrooms, 1);

  /* reload chatrooms file */
  g_object_unref (mgr);
  mgr = empathy_chatroom_manager_dup_singleton (file);

  check_chatrooms_list (mgr, account, chatrooms, 1);

  /* remove room1 */
  chatroom = empathy_chatroom_manager_find (mgr, account, "room2");
  fail_if (chatroom == NULL);

  empathy_chatroom_manager_remove (mgr, chatroom);

  check_chatrooms_list (mgr, account, chatrooms, 0);

  /* reload chatrooms file */
  g_object_unref (mgr);
  mgr = empathy_chatroom_manager_dup_singleton (file);

  check_chatrooms_list (mgr, account, chatrooms, 0);

  g_object_unref (mgr);
  g_free (file);
  g_object_unref (account);
}
END_TEST

START_TEST (test_empathy_chatroom_manager_change_favorite)
{
  EmpathyChatroomManager *mgr;
  gchar *file;
  McAccount *account;
  struct chatroom_t chatrooms[] = {
        { "name1", "room1", TRUE, TRUE },
        { "name2", "room2", FALSE, FALSE }};
  EmpathyChatroom *chatroom;

  account = get_test_account ();

  copy_xml_file (CHATROOM_SAMPLE, CHATROOM_FILE);

  file = get_user_xml_file (CHATROOM_FILE);

  /* change the chatrooms XML file to use the account we just created */
  if (!change_account_name_in_file (account, file))
    return;

  mgr = empathy_chatroom_manager_dup_singleton (file);

  /* room2 is not favorite anymore */
  chatroom = empathy_chatroom_manager_find (mgr, account, "room2");
  fail_if (chatroom == NULL);
  g_object_set (chatroom, "favorite", FALSE, NULL);

  check_chatrooms_list (mgr, account, chatrooms, 2);

  /* reload chatrooms file */
  g_object_unref (mgr);
  mgr = empathy_chatroom_manager_dup_singleton (file);

  /* room2 is not present in the XML file anymore as it's not a favorite */
  check_chatrooms_list (mgr, account, chatrooms, 1);

  /* re-add room2 */
  chatroom = empathy_chatroom_new_full (account, "room2", "name2", FALSE);
  empathy_chatroom_manager_add (mgr, chatroom);

  check_chatrooms_list (mgr, account, chatrooms, 2);

  /* set room2 as favorite */
  g_object_set (chatroom, "favorite", TRUE, NULL);

  chatrooms[1].favorite = TRUE;
  check_chatrooms_list (mgr, account, chatrooms, 2);

  /* reload chatrooms file */
  g_object_unref (mgr);
  mgr = empathy_chatroom_manager_dup_singleton (file);

  /* room2 is back in the XML file now */
  check_chatrooms_list (mgr, account, chatrooms, 2);

  g_object_unref (mgr);
  g_object_unref (chatroom);
  g_free (file);
  g_object_unref (account);
}
END_TEST

START_TEST (test_empathy_chatroom_manager_change_chatroom)
{
  EmpathyChatroomManager *mgr;
  gchar *file;
  McAccount *account;
  struct chatroom_t chatrooms[] = {
        { "name1", "room1", TRUE, TRUE },
        { "name2", "room2", FALSE, TRUE }};
  EmpathyChatroom *chatroom;

  account = get_test_account ();

  copy_xml_file (CHATROOM_SAMPLE, "foo.xml");

  file = get_user_xml_file ("foo.xml");

  /* change the chatrooms XML file to use the account we just created */
  if (!change_account_name_in_file (account, file))
    return;

  mgr = empathy_chatroom_manager_dup_singleton (file);

  check_chatrooms_list (mgr, account, chatrooms, 2);

  /* change room2 name */
  chatroom = empathy_chatroom_manager_find (mgr, account, "room2");
  fail_if (chatroom == NULL);
  empathy_chatroom_set_name (chatroom, "new_name");

  /* reload chatrooms file */
  g_object_unref (mgr);
  mgr = empathy_chatroom_manager_dup_singleton (file);

  chatrooms[1].name = "new_name";
  check_chatrooms_list (mgr, account, chatrooms, 2);

  /* change room2 auto-connect status */
  chatroom = empathy_chatroom_manager_find (mgr, account, "room2");
  fail_if (chatroom == NULL);
  empathy_chatroom_set_auto_connect (chatroom, TRUE);

  /* reload chatrooms file */
  g_object_unref (mgr);
  mgr = empathy_chatroom_manager_dup_singleton (file);

  chatrooms[1].auto_connect = TRUE;
  check_chatrooms_list (mgr, account, chatrooms, 2);

  /* change room2 room */
  chatroom = empathy_chatroom_manager_find (mgr, account, "room2");
  fail_if (chatroom == NULL);
  empathy_chatroom_set_room (chatroom, "new_room");

  /* reload chatrooms file */
  g_object_unref (mgr);
  mgr = empathy_chatroom_manager_dup_singleton (file);

  chatrooms[1].room = "new_room";
  check_chatrooms_list (mgr, account, chatrooms, 2);

  g_object_unref (mgr);
  g_free (file);
  g_object_unref (account);
}
END_TEST

TCase *
make_empathy_chatroom_manager_tcase (void)
{
    TCase *tc = tcase_create ("empathy-chatroom-manager");
    tcase_add_test (tc, test_empathy_chatroom_manager_dup_singleton);
    tcase_add_test (tc, test_empathy_chatroom_manager_add);
    tcase_add_test (tc, test_empathy_chatroom_manager_remove);
    tcase_add_test (tc, test_empathy_chatroom_manager_change_favorite);
    tcase_add_test (tc, test_empathy_chatroom_manager_change_chatroom);
    return tc;
}
