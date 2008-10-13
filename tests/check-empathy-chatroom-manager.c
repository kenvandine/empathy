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

START_TEST (test_empathy_chatroom_manager_new)
{
  EmpathyChatroomManager *mgr;
  gchar *cmd;
  gchar *file;
  McAccount *account;
  GList *chatrooms, *l;
  gboolean room1_found, room2_found;

  account = create_test_account ();

  copy_xml_file (CHATROOM_SAMPLE, CHATROOM_FILE);

  file = get_user_xml_file (CHATROOM_FILE);
  /* change the chatrooms XML file to use the account we just created */
  cmd = g_strdup_printf ("sed -i 's/CHANGE_ME/%s/' %s",
      mc_account_get_unique_name (account), file);
  system (cmd);
  g_free (cmd);

  mgr = empathy_chatroom_manager_new (file);

  fail_if (empathy_chatroom_manager_get_count (mgr, account) != 2);

  chatrooms = empathy_chatroom_manager_get_chatrooms (mgr, account);
  fail_if (g_list_length (chatrooms) != 2);

  room1_found = room2_found = FALSE;
  for (l = chatrooms; l != NULL; l = g_list_next (l))
    {
      EmpathyChatroom *chatroom = l->data;
      gboolean favorite;

      if (!tp_strdiff (empathy_chatroom_get_room (chatroom), "room1"))
        {
          room1_found = TRUE;
          fail_if (tp_strdiff (empathy_chatroom_get_name (chatroom), "name1"));
          fail_if (!empathy_chatroom_get_auto_connect (chatroom));
          g_object_get (chatroom, "favorite", &favorite, NULL);
          fail_if (!favorite);
        }
      else if (!tp_strdiff (empathy_chatroom_get_room (chatroom), "room2"))
        {
          room2_found = TRUE;
          fail_if (tp_strdiff (empathy_chatroom_get_name (chatroom), "name2"));
          fail_if (empathy_chatroom_get_auto_connect (chatroom));
          g_object_get (chatroom, "favorite", &favorite, NULL);
          fail_if (!favorite);
        }
      else
        {
          g_assert_not_reached ();
        }
    }

  fail_if (!room1_found || !room2_found);

  g_free (file);
  g_object_unref (mgr);
  destroy_test_account (account);
}
END_TEST

TCase *
make_empathy_chatroom_manager_tcase (void)
{
    TCase *tc = tcase_create ("empathy-chatroom-manager");
    tcase_add_test (tc, test_empathy_chatroom_manager_new);
    return tc;
}
