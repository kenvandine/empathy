PYDEFS=`pkg-config --variable=defsdir pygtk-2.0`

AM_CPPFLAGS =                                           \
	-I.                                             \
	-I$(top_srcdir)/libempathy			\
	-I$(top_builddir)/libempathy			\
	-I$(top_srcdir)                                 \
	-DDATADIR=\""$(datadir)"\"              	\
	$(PYTHON_BINDING_CFLAGS)			\
	$(PYTHON_INCLUDES)				\
	$(WARN_CFLAGS)

BUILT_SOURCES = 					\
	pyempathy.c					

pyempathydir = $(pyexecdir)
pyempathy_LTLIBRARIES = empathy.la

empathy_la_SOURCES =					\
	pyempathymodule.c

nodist_empathy_la_SOURCES =				\
	pyempathy.c

empathy_la_LIBADD =					\
	$(PYTHON_BINDING_LIBS)				\
	$(top_builddir)/libempathy/libempathy.la

empathy_la_LDFLAGS =					\
	-module -avoid-version

pyempathy.c: pyempathy.override pyempathy.defs
	$(PYGOBJECTCODEGEN)				\
		--prefix empathy 			\
		--register $(PYDEFS)/gdk-types.defs 	\
		--register $(PYDEFS)/gtk-types.defs 	\
		--override $(srcdir)/pyempathy.override \
		$(srcdir)/pyempathy.defs > $@ 

EXTRA_DIST =			\
	pyempathy.override	\
	pyempathy.defs

CLEANFILES = $(BUILT_SOURCES)

