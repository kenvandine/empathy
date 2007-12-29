#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <check.h>
#include "check-helpers.h"
#include "check-libempathy.h"

#include <libempathy/empathy-utils.h>

START_TEST (test_empathy_substring)
{
  gchar *tmp;

  tmp = empathy_substring ("empathy", 2, 6);
  fail_if (tmp == NULL);
  fail_if (strcmp (tmp, "path") != 0);

  g_free (tmp);
}
END_TEST

TCase *
make_empathy_utils_tcase (void)
{
    TCase *tc = tcase_create ("empathy-utils");
    tcase_add_test (tc, test_empathy_substring);
    return tc;
}
