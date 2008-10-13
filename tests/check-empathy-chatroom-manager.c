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

  fail_if (empathy_chatroom_manager_get_count (mgr, account) != nb_chatrooms);

  chatrooms = empathy_chatroom_manager_get_chatrooms (mgr, account);
  fail_if (g_list_length (chatrooms) != nb_chatrooms);

  for (l = chatrooms, i = 0; l != NULL; l = g_list_next (l), i++)
    {
      EmpathyChatroom *chatroom = l->data;

      check_chatroom (chatroom, _chatrooms[i].name, _chatrooms[i].room,
          _chatrooms[i].auto_connect, _chatrooms[i].favorite);
    }
}

START_TEST (test_empathy_chatroom_manager_new)
{
  EmpathyChatroomManager *mgr;
  gchar *cmd;
  gchar *file;
  McAccount *account;
  struct chatroom_t chatrooms[] = {
        { "name2", "room2", FALSE, TRUE },
        { "name1", "room1", TRUE, TRUE }};

  account = create_test_account ();

  copy_xml_file (CHATROOM_SAMPLE, CHATROOM_FILE);

  file = get_user_xml_file (CHATROOM_FILE);
  /* change the chatrooms XML file to use the account we just created */
  cmd = g_strdup_printf ("sed -i 's/CHANGE_ME/%s/' %s",
      mc_account_get_unique_name (account), file);
  system (cmd);
  g_free (cmd);

  mgr = empathy_chatroom_manager_new (file);
  check_chatrooms_list (mgr, account, chatrooms, 2);

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
