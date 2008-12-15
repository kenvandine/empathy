#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib-object.h>

#include <check.h>

#include "check-helpers.h"
#include "check-libempathy.h"
#include <libempathy/empathy-utils.h>

#include "config.h"

static Suite *
make_libempathy_suite (void)
{
    Suite *s = suite_create ("libempathy");

    suite_add_tcase (s, make_empathy_utils_tcase ());
    suite_add_tcase (s, make_empathy_irc_server_tcase ());
    suite_add_tcase (s, make_empathy_irc_network_tcase ());
    suite_add_tcase (s, make_empathy_irc_network_manager_tcase ());
    suite_add_tcase (s, make_empathy_chatroom_tcase ());
    suite_add_tcase (s, make_empathy_chatroom_manager_tcase ());

    return s;
}

int
main (void)
{
    int number_failed = 0;
    Suite *s;
    SRunner *sr;

    check_helpers_init ();
    g_type_init ();
    empathy_init ();

    s = make_libempathy_suite ();
    sr = srunner_create (s);
    srunner_run_all (sr, CK_NORMAL);
    number_failed += srunner_ntests_failed (sr);
    srunner_free (sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
