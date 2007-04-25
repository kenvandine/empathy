AM_CPPFLAGS =	                                 	\
	-I$(top_srcdir)                          	\
	-DPREFIX="\"$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"              	\
	-DLIBDIR=\""$(libdir)"\"			\
	$(EMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)

bin_PROGRAMS = empathy-accounts

empathy_accounts_SOURCES = 				                       	\
	empathy-accounts-main.c

empathy_accounts_LDADD = \
	$(top_builddir)/libempathy/libempathy.la				\
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la			\
	$(EMPATHY_LIBS)

