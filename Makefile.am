SUBDIRS = po data libempathy libempathy-gtk src docs

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

DISTCHECK_CONFIGURE_FLAGS =	\
	--disable-scrollkeeper  \
	--enable-gtk-doc

INTLTOOL = 			\
	intltool-extract.in 	\
	intltool-merge.in 	\
	intltool-update.in

EXTRA_DIST = 			\
        ChangeLog               \
        README                  \
	CONTRIBUTORS		\
	$(INTLTOOL)

DISTCLEANFILES = 		\
	intltool-extract	\
	intltool-merge		\
	intltool-update

# Workaround broken scrollkeeper that doesn't remove its files on
# uninstall.
distuninstallcheck_listfiles = find . -type f -print | grep -v '^\./var/scrollkeeper'

dist-hook:
	chmod a+w ${distdir}/ChangeLog
	if test -d _darcs; then darcs changes >${distdir}/ChangeLog; fi
