SUBDIRS = xml

CLEANFILES=

include $(top_srcdir)/rules/check.mak

SUPPRESSIONS=valgrind.supp dlopen.supp

EXTRA_DIST = 		\
	test.manager	\
	test.profile

AM_CPPFLAGS =						\
	-I$(top_srcdir)                          	\
	-DPKGDATADIR=\""$(pkgdatadir)"\"		\
	$(EMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)

LDADD =								\
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la	\
	$(top_builddir)/libempathy/libempathy.la		\
	$(EMPATHY_LIBS)

noinst_PROGRAMS =			\
	contact-manager			\
	contact-run-until-ready		\
	contact-run-until-ready-2	\
	empetit

contact_manager_SOURCES = contact-manager.c
contact_run_until_ready_SOURCES = contact-run-until-ready.c
contact_run_until_ready_2_SOURCES = contact-run-until-ready-2.c
empetit_SOURCES = empetit.c

check_PROGRAMS = check-main
TESTS = check-main
check_main_SOURCES =                             \
    check-main.c                                 \
    check-helpers.c                              \
    check-helpers.h                              \
    check-libempathy.h                           \
    check-empathy-utils.c                        \
    check-empathy-helpers.h                      \
    check-empathy-helpers.c                      \
    check-irc-helper.h                           \
    check-irc-helper.c                           \
    check-empathy-irc-server.c                   \
    check-empathy-irc-network.c                  \
    check-empathy-irc-network-manager.c          \
    check-empathy-chatroom.c                     \
    check-empathy-chatroom-manager.c

check_main_LDADD = \
    @CHECK_LIBS@ \
    $(top_builddir)/libempathy-gtk/libempathy-gtk.la	\
    $(top_builddir)/libempathy/libempathy.la		\
    $(AM_LDFLAGS)

check_main_CFLAGS = \
    @CHECK_CFLAGS@ \
    $(AM_CFLAGS)

TESTS_ENVIRONMENT = EMPATHY_SRCDIR=@abs_top_srcdir@ \
		    MC_PROFILE_DIR=@abs_top_srcdir@/tests \
		    MC_MANAGER_DIR=@abs_top_srcdir@/tests
