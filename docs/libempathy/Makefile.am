NULL = 

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=libempathy

# The top-level SGML file.
DOC_MAIN_SGML_FILE=libempathy-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="G_DISABLE_DEPRECATED"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)/libempathy

HFILE_GLOB=$(top_srcdir)/libempathy/*.h
CFILE_GLOB=$(top_srcdir)/libempathy/*.c

# Headers to ignore
IGNORE_HFILES=				\
	empathy-filter-glue.h		\
	empathy-debug.h			\
	empathy-marshal.h		\
	stamp-empathy-enum-types.h	\
	empathy-chandler-glue.h		\
	$(NULL)

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
GTKDOC_CFLAGS = \
	-I$(srcdir) 			\
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/libempathy	\
	-I$(top_builddir) 		\
	-I$(top_builddir)/libempathy	\
	$(LIBEMPATHY_CFLAGS)		\
	$(NULL)

GTKDOC_LIBS = \
	$(top_builddir)/libempathy/libempathy.la	\
	$(LIBEMPATHY_LIBS)				\
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
