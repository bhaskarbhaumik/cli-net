# Makefile.am - automake build for net (out-of-tree; see configure.ac).

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -DNET_VERSION='"$(PACKAGE_VERSION)"'
AM_CFLAGS   = -Wall -Wextra

bin_PROGRAMS = net
net_SOURCES  = src/net.c
net_LDADD    = $(NET_FRAMEWORKS)

dist_man1_MANS = man/net.1

EXTRA_DIST = \
    README.md \
    LICENSE \
    VERSION \
    CMakeLists.txt \
    Makefile \
    build.sh \
    prototypes/x01.c \
    prototypes/x02.c \
    prototypes/x03.c
