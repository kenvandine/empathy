#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <check.h>
#include "check-helpers.h"
#include "check-libempathy.h"
#include "check-empathy-helpers.h"

#include <libempathy/empathy-chatroom.h>

static EmpathyChatroom *
create_chatroom (void)
{
  McAccount *account;
  EmpathyChatroom *chatroom;

  account = get_test_account ();
  chatroom = empathy_chatroom_new (account);
  fail_if (chatroom == NULL);

  return chatroom;
}

START_TEST (test_empathy_chatroom_new)
{
  EmpathyChatroom *chatroom;
  gboolean auto_connect, favorite;

  chatroom = create_chatroom ();
  fail_if (empathy_chatroom_get_auto_connect (chatroom));
  g_object_get (chatroom,
      "auto_connect", &auto_connect,
      "favorite", &favorite,
      NULL);
  fail_if (auto_connect);
  fail_if (favorite);

  g_object_unref (empathy_chatroom_get_account (chatroom));
  g_object_unref (chatroom);
}
END_TEST

START_TEST (test_favorite_and_auto_connect)
{
  /* auto connect implies favorite */
  EmpathyChatroom *chatroom;
  gboolean auto_connect, favorite;

  chatroom = create_chatroom ();

  /* set auto_connect so favorite as a side effect */
  empathy_chatroom_set_auto_connect (chatroom, TRUE);
  fail_if (!empathy_chatroom_get_auto_connect (chatroom));
  g_object_get (chatroom,
      "auto_connect", &auto_connect,
      "favorite", &favorite,
      NULL);
  fail_if (!auto_connect);
  fail_if (!favorite);

  /* Remove auto_connect. Chatroom is still favorite */
  empathy_chatroom_set_auto_connect (chatroom, FALSE);
  fail_if (empathy_chatroom_get_auto_connect (chatroom));
  g_object_get (chatroom,
      "auto_connect", &auto_connect,
      "favorite", &favorite,
      NULL);
  fail_if (auto_connect);
  fail_if (!favorite);

  /* Remove favorite too now */
  g_object_set (chatroom, "favorite", FALSE, NULL);
  fail_if (empathy_chatroom_get_auto_connect (chatroom));
  g_object_get (chatroom,
      "auto_connect", &auto_connect,
      "favorite", &favorite,
      NULL);
  fail_if (auto_connect);
  fail_if (favorite);

  /* Just add favorite but not auto-connect */
  g_object_set (chatroom, "favorite", TRUE, NULL);
  fail_if (empathy_chatroom_get_auto_connect (chatroom));
  g_object_get (chatroom,
      "auto_connect", &auto_connect,
      "favorite", &favorite,
      NULL);
  fail_if (auto_connect);
  fail_if (!favorite);

  /* ... and re-add auto_connect */
  g_object_set (chatroom, "auto_connect", TRUE, NULL);
  fail_if (!empathy_chatroom_get_auto_connect (chatroom));
  g_object_get (chatroom,
      "auto_connect", &auto_connect,
      "favorite", &favorite,
      NULL);
  fail_if (!auto_connect);
  fail_if (!favorite);

  /* Remove favorite remove auto_connect too */
  g_object_set (chatroom, "favorite", FALSE, NULL);
  fail_if (empathy_chatroom_get_auto_connect (chatroom));
  g_object_get (chatroom,
      "auto_connect", &auto_connect,
      "favorite", &favorite,
      NULL);
  fail_if (auto_connect);
  fail_if (favorite);

  g_object_unref (empathy_chatroom_get_account (chatroom));
  g_object_unref (chatroom);
}
END_TEST

static void
favorite_changed (EmpathyChatroom *chatroom,
                  GParamSpec *spec,
                  gboolean *changed)
{
  *changed = TRUE;
}

START_TEST (test_change_favorite)
{
  EmpathyChatroom *chatroom;
  gboolean changed = FALSE;

  chatroom = create_chatroom ();

  g_signal_connect (chatroom, "notify::favorite", G_CALLBACK (favorite_changed),
      &changed);

  /* change favorite to TRUE */
  g_object_set (chatroom, "favorite", TRUE, NULL);
  fail_if (!changed);

  changed = FALSE;

  /* change favorite to FALSE */
  g_object_set (chatroom, "favorite", FALSE, NULL);
  fail_if (!changed);
}
END_TEST

TCase *
make_empathy_chatroom_tcase (void)
{
    TCase *tc = tcase_create ("empathy-chatroom");
    tcase_add_test (tc, test_empathy_chatroom_new);
    tcase_add_test (tc, test_favorite_and_auto_connect);
    tcase_add_test (tc, test_change_favorite);
    return tc;
}
