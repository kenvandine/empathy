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
        ChangeLog.old           \
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

# Build ChangeLog from SVN history (r470 was the first ChangeLog-less checkin).
# Only build this when in an svn checkout.

ChangeLog:
	@if test -f $(top_srcdir)/.svn/entries; then \
		svn log -v --xml -r HEAD:470 $(SVN_ROOT)/$(SVN_MODULE) | \
			xsltproc --stringparam strip-prefix "$(SVN_MODULE)/$(SVN_BRANCH)" \
				 --stringparam include-rev "yes" $(top_srcdir)/svn2cl.xsl - > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

