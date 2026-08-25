#                                               -*- Autoconf -*-
# configure.ac - autoconf/automake build for net.
#
# NOTE: net must be configured OUT OF TREE so the hand-written ./Makefile
# (the GNU make build) is never overwritten:
#
#     autoreconf --install
#     mkdir build-auto && cd build-auto && ../configure && make

AC_PREREQ([2.69])
AC_INIT([net],
        [m4_esyscmd_s([cat VERSION])],
        [https://github.com/bhaskarbhaumik/cli-net/issues],
        [net],
        [https://github.com/bhaskarbhaumik/cli-net])
AC_CONFIG_SRCDIR([src/net.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])
AC_CANONICAL_HOST

# Refuse in-tree builds to protect the hand-written ./Makefile.
if test "`cd $srcdir && pwd`" = "`pwd`"; then
  AC_MSG_ERROR([in-tree builds are disabled to protect the hand-written ./Makefile.
    Please build out of tree, e.g.:
        mkdir build-auto && cd build-auto && ../configure])
fi

AM_INIT_AUTOMAKE([1.16 foreign subdir-objects -Wall -Werror])
AM_SILENT_RULES([yes])

AC_PROG_CC
AC_PROG_INSTALL

# Prefer C11 if the compiler supports it (no autoconf-archive dependency).
AC_MSG_CHECKING([whether $CC accepts -std=c11])
net_save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS -std=c11"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
  [AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no]); CFLAGS=$net_save_CFLAGS])

# net reads the interface list through SystemConfiguration and IOKit, so it is
# macOS-only. Fail early and clearly rather than deep in the link step.
case "$host_os" in
  darwin*) ;;
  *) AC_MSG_ERROR([net is macOS-only: it requires the SystemConfiguration,
    CoreFoundation and IOKit frameworks. Detected host: $host_os]) ;;
esac

NET_FRAMEWORKS="-framework SystemConfiguration -framework CoreFoundation -framework IOKit"
AC_SUBST([NET_FRAMEWORKS])

# Confirm the framework headers are actually reachable with this SDK.
AC_CHECK_HEADERS([SystemConfiguration/SCNetworkConfiguration.h IOKit/IOKitLib.h],,
  [AC_MSG_ERROR([macOS framework headers not found. Install the Xcode command
    line tools: xcode-select --install])])

AC_CHECK_HEADERS([net/if_media.h],,
  [AC_MSG_ERROR([net/if_media.h not found; it is required for link speed.])],
  [[#include <sys/types.h>
    #include <sys/socket.h>
    #include <net/if.h>]])

# ncurses drives the -t/--table view.
AC_CHECK_HEADERS([curses.h],, [AC_MSG_ERROR([curses.h not found. Install ncurses.])])
AC_SEARCH_LIBS([initscr], [ncurses curses],,
  [AC_MSG_ERROR([ncurses not found (no library provides initscr). Install ncurses.])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

AC_MSG_NOTICE([

  net $PACKAGE_VERSION configured.
    CC .......... $CC
    CFLAGS ...... $CFLAGS
    frameworks .. $NET_FRAMEWORKS
    prefix ...... $prefix

  Now run: make && make install
])
