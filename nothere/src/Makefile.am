AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DPKGDATADIR=\""$(pkgdatadir)"\" \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DIMAGEDIR=\"$(datadir)/empathy\" \
	$(NOTHERE_CFLAGS) $(WARN_CFLAGS)

libexec_PROGRAMS = nothere-applet
nothere_applet_SOURCES = 					\
	nothere-applet.c 	nothere-applet.h

nothere_applet_LDFLAGS = 					\
	$(NOTHERE_LIBS)

nothere_applet_LDADD = 					\
	$(top_builddir)/libempathy/libempathy.la \
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la
