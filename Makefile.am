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

distclean-local:
	if test $(srdcir) = .; then :; else \
		rm -f ChangeLog; \
	fi

ChangeLog:
	@echo Creating $@
	@if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run git log EMPATHY_2_24_0.. --stat) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi

.PHONY: ChangeLog

