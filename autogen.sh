#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="Empathy"
REQUIRED_AUTOMAKE_VERSION=1.9

(test -f $srcdir/configure.ac) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level gnome directory"
    exit 1
}

# Fix to make shave + libtool 1.x + gtk-doc work.
# See http://git.lespiau.name/cgit/shave/tree/README#n83
sed -e 's#) --mode=compile#) --tag=CC --mode=compile#' gtk-doc.make \
    > gtk-doc.temp \
        && mv gtk-doc.temp gtk-doc.make
sed -e 's#) --mode=link#) --tag=CC --mode=link#' gtk-doc.make \
                > gtk-doc.temp \
        && mv gtk-doc.temp gtk-doc.make

which gnome-autogen.sh || {
    echo "You need to install gnome-common from the GNOME CVS"
    exit 1
}
USE_GNOME2_MACROS=1 USE_COMMON_DOC_BUILD=yes . gnome-autogen.sh


