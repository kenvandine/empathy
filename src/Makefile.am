AM_CPPFLAGS =	                                 	\
	-I$(top_srcdir)                          	\
	-DPREFIX="\"$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"              	\
	-DLIBDIR=\""$(libdir)"\"			\
	$(EMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)

bin_PROGRAMS = empathy

empathy_SOURCES = 				                \
	empathy-main.c

empathy_LDADD = \
	$(top_builddir)/libempathy/libempathy.la				\
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la			\
	$(EMPATHY_LIBS)


