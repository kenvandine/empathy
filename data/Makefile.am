SUBDIRS = icons

profiledir = $(datadir)/mission-control/profiles
profile_DATA = 				\
	jabber.profile			\
	gtalk.profile			\
	salut.profile			\
	irc.profile			\
	msn.profile			\
	sofiasip.profile		\
	aim.profile

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = empathy.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

schemasdir	 = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = empathy.schemas.in
schemas_DATA 	 = $(schemas_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
	  for p in $(schemas_DATA) ; do \
	    GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$$p ; \
	  done \
	fi
else
install-data-local:
endif

EXTRA_DIST = 				\
	$(profile_DATA)			\
	$(autostart_in_files)		\
	$(autostart_DATA)		\
	$(schemas_in_files)		\
	$(schemas_DATA)

DISTCLEANFILES = 			\
	$(autostart_DATA)		\
	$(schemas_DATA)

