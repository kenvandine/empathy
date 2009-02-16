SUBDIRS = tools extensions po data libempathy libempathy-gtk src docs help

if HAVE_MEGAPHONE
SUBDIRS += megaphone
endif

if HAVE_NOTHERE
SUBDIRS += nothere
endif

if HAVE_PYTHON
SUBDIRS += python
endif

if HAVE_TESTS
SUBDIRS += tests
endif

ACLOCAL_AMFLAGS = -I m4

DISTCHECK_CONFIGURE_FLAGS =		\
	--disable-scrollkeeper		\
	--disable-schemas-install	\
	--enable-gtk-doc

EXTRA_DIST = 			\
        ChangeLog               \
        ChangeLog.old           \
        README                  \
	CONTRIBUTORS		\
	autogen.sh

DISTCLEANFILES = 		\
        ChangeLog

# Workaround broken scrollkeeper that doesn't remove its files on
# uninstall.
distuninstallcheck_listfiles = find . -type f -print | grep -v '^\./var/scrollkeeper'

ChangeLog: Makefile.am
	git log --pretty=short > ChangeLog
	

dist: ChangeLog

.PHONY: ChangeLog

