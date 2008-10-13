#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib/gstdio.h>

#include <check.h>
#include "check-helpers.h"
#include "check-libempathy.h"

#include <libempathy/empathy-chatroom-manager.h>

#define CHATROOM_SAMPLE "chatrooms-sample.xml"
#define CHATROOM_FILE "chatrooms.xml"

static gchar *
get_xml_file (const gchar *filename)
{
  return g_build_filename (g_getenv ("EMPATHY_SRCDIR"), "tests", "xml",
      filename, NULL);
}

static gchar *
get_user_xml_file (const gchar *filename)
{
  return g_build_filename (g_get_tmp_dir (), filename, NULL);
}

static void
copy_chatroom_file (void)
{
  gboolean result;
  gchar *buffer;
  gsize length;
  gchar *sample;
  gchar *file;

  sample = get_xml_file (CHATROOM_SAMPLE);
  result = g_file_get_contents (sample, &buffer, &length, NULL);
  fail_if (!result);

  file = get_user_xml_file (CHATROOM_FILE);
  result = g_file_set_contents (file, buffer, length, NULL);
  fail_if (!result);

  g_free (sample);
  g_free (file);
  g_free (buffer);
}

START_TEST (test_empathy_chatroom_manager_new)
{
  EmpathyChatroomManager *mgr;
  gchar *file;

  copy_chatroom_file ();
  file = get_xml_file (CHATROOM_FILE);
  mgr = empathy_chatroom_manager_new (file);

  g_free (file);
  g_object_unref (mgr);
}
END_TEST

TCase *
make_empathy_chatroom_manager_tcase (void)
{
    TCase *tc = tcase_create ("empathy-chatroom-manager");
    tcase_add_test (tc, test_empathy_chatroom_manager_new);
    return tc;
}
