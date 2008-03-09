NULL = 

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=libempathy-gtk

# The top-level SGML file.
DOC_MAIN_SGML_FILE=libempathy-gtk-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="G_DISABLE_DEPRECATED"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)/libempathy-gtk

HFILE_GLOB=$(top_srcdir)/libempathy-gtk/*.h
CFILE_GLOB=$(top_srcdir)/libempathy-gtk/*.c

# Headers to ignore
IGNORE_HFILES=				\
	empathy-gtk-enum-types.h	\
	stamp-empathy-gtk-enum-types.h	\
	ephy-spinner.h			\
	totem-subtitle-encoding.h	\
	$(NULL)

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
GTKDOC_CFLAGS = \
	-I$(srcdir) 				\
	-I$(top_srcdir) 			\
	-I$(top_srcdir)/libempathy-gtk		\
	-I$(top_builddir) 			\
	-I$(top_builddir)/libempathy-gtk	\
	$(EMPATHY_CFLAGS)			\
	$(NULL)

GTKDOC_LIBS = \
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la	\
	$(top_builddir)/libempathy/libempathy.la		\
	$(EMPATHY_LIBS)						\
	$(NULL)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --output-format=xml --sgml-mode

# Images to copy into HTML directory
HTML_IMAGES =

content_files = 		\
	version.xml

extra_files = version.xml.in

include $(top_srcdir)/gtk-doc.make

EXTRA_DIST +=				\
	version.xml.in
