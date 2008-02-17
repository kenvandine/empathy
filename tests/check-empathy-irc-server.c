#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <check.h>
#include "check-helpers.h"
#include "check-libempathy.h"
#include "check-irc-helper.h"

#include <libempathy/empathy-irc-server.h>

START_TEST (test_empathy_irc_server_new)
{
  EmpathyIrcServer *server;

  server = empathy_irc_server_new ("test.localhost", 6667, TRUE);
  check_server (server, "test.localhost", 6667, TRUE);

  g_object_unref (server);
}
END_TEST

START_TEST (test_property_change)
{
  EmpathyIrcServer *server;

  server = empathy_irc_server_new ("test.localhost", 6667, TRUE);
  fail_if (server == NULL);

  g_object_set (server,
      "address", "test2.localhost",
      "port", 6668,
      "ssl", FALSE,
      NULL);

  check_server (server, "test2.localhost", 6668, FALSE);

  g_object_unref (server);
}
END_TEST

static gboolean modified = FALSE;

static void
modified_cb (EmpathyIrcServer *server,
             gpointer unused)
{
  modified = TRUE;
}

START_TEST (test_modified_signal)
{
  EmpathyIrcServer *server;

  server = empathy_irc_server_new ("test.localhost", 6667, TRUE);
  fail_if (server == NULL);

  g_signal_connect (server, "modified", G_CALLBACK (modified_cb), NULL);

  /* address */
  g_object_set (server, "address", "test2.localhost", NULL);
  fail_if (!modified);
  modified = FALSE;
  g_object_set (server, "address", "test2.localhost", NULL);
  fail_if (modified);

  /* port */
  g_object_set (server, "port", 6668, NULL);
  fail_if (!modified);
  modified = FALSE;
  g_object_set (server, "port", 6668, NULL);
  fail_if (modified);

  /* ssl */
  g_object_set (server, "ssl", FALSE, NULL);
  fail_if (!modified);
  modified = FALSE;
  g_object_set (server, "ssl", FALSE, NULL);
  fail_if (modified);

  g_object_unref (server);
}
END_TEST

TCase *
make_empathy_irc_server_tcase (void)
{
    TCase *tc = tcase_create ("empathy-irc-server");
    tcase_add_test (tc, test_empathy_irc_server_new);
    tcase_add_test (tc, test_property_change);
    tcase_add_test (tc, test_modified_signal);
    return tc;
}
