SUBDIRS = po profiles pixmaps libempathy libempathy-gtk src

DISTCHECK_CONFIGURE_FLAGS =	\
	--disable-scrollkeeper

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
