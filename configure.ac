
m4_define([udisks_lvm_major], [2])
m4_define([udisks_lvm_minor], [0])
m4_define([udisks_lvm_micro], [0])

dnl ---------------------------------------------------------------------------

m4_define([udisks_lvm_version],
          [udisks_lvm_major.udisks_lvm_minor.udisks_lvm_micro])

AC_INIT([udisks-lvm],
        [udisks_lvm_version],
        [mvollmer@redhat.com],
        [udisks-lvm])

UDISKS_LVM_MAJOR=udisks_lvm_major
UDISKS_LVM_MINOR=udisks_lvm_minor
UDISKS_LVM_MICRO=udisks_lvm_micro
UDISKS_LVM_VERSION=udisks_lvm_version
AC_SUBST(UDISKS_LVM_MAJOR)
AC_SUBST(UDISKS_LVM_MINOR)
AC_SUBST(UDISKS_LVM_MICRO)
AC_SUBST(UDISKS_LVM_VERSION)

AM_INIT_AUTOMAKE([1.9 foreign dist-bzip2 no-dist-gzip serial-tests])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])

AM_MAINTAINER_MODE

AC_PROG_CC
AC_ISC_POSIX
AC_HEADER_STDC
AC_PROG_LIBTOOL

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Compilation
#

CC_CHECK_CFLAGS_APPEND([                          \
        -Waggregate-return                        \
        -Wall                                     \
        -W                                        \
        -Wextra                                   \
        -Wcast-align                              \
        -Wdeclaration-after-statement             \
        -Wendif-labels                            \
        -Werror=format-security                   \
        -Werror=implicit-function-declaration     \
        -Werror=overflow                          \
        -Wformat-nonliteral                       \
        -Wformat=2                                \
        -Wimplicit                                \
        -Winit-self                               \
        -Wlogical-op                              \
        -Wmissing-declarations                    \
        -Wmissing-format-attribute                \
        -Wmissing-include-dirs                    \
        -Wmissing-noreturn                        \
        -Wmissing-prototypes                      \
        -Wnested-externs                          \
        -Wno-inline                               \
        -Wno-long-long                            \
        -Wno-missing-field-initializers           \
        -Wno-overlength-strings                   \
        -Wno-switch-enum                          \
        -Wno-unused-parameter                     \
        -Wno-unused-result                        \
        -Wold-style-definition                    \
        -Wpacked                                  \
        -Wpointer-arith                           \
        -Wshadow                                  \
        -Wsign-compare                            \
        -Wstrict-aliasing=2                       \
        -Wstrict-prototypes                       \
        -Wundef                                   \
        -Wuninitialized                           \
        -Wmaybe-uninitialized                     \
        -Wunsafe-loop-optimizations               \
        -Wvla                                     \
        -Wwrite-strings                           \
        -fdata-sections                           \
        -fdiagnostics-show-option                 \
        -ffast-math                               \
        -ffunction-sections                       \
        -fno-common                               \
        -fno-strict-aliasing                      \
        -D_LARGEFILE64_SOURCE                     \
        -D_FILE_OFFSET_BITS=64                    \
])

AC_PATH_PROG([XSLTPROC], [xsltproc])
if test -z "$XSLTPROC"; then
  AC_MSG_ERROR([xsltproc is needed])
fi

AC_ARG_ENABLE(man,
              [AS_HELP_STRING([--enable-man],
                              [generate man pages [default=auto]])],,
              enable_man=maybe)
AS_IF([test "$enable_man" != no], [
  AC_PATH_PROG([XSLTPROC], [xsltproc])
  AS_IF([test -z "$XSLTPROC"], [
    AS_IF([test "$enable_man" = yes], [
      AC_MSG_ERROR([xsltproc is required for --enable-man])
    ])
    enable_man=no
  ])
  enable_man=yes
])
AM_CONDITIONAL(ENABLE_MAN, test "$enable_man" != no)

GOBJECT_INTROSPECTION_CHECK([0.6.2])

# Libraries
#

PKG_CHECK_MODULES(GUDEV, [gudev-1.0 >= 165])
AC_SUBST(GUDEV_CFLAGS)
AC_SUBST(GUDEV_LIBS)

PKG_CHECK_MODULES(LVM2, [lvm2app])
AC_SUBST(LVM2_CFLAGS)
AC_SUBST(LVM2_LIBS)

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.31.13])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(GIO, [gio-unix-2.0 >= 2.31.13])
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

PKG_CHECK_MODULES(POLKIT_GOBJECT_1, [polkit-gobject-1 >= 0.92])
AC_SUBST(POLKIT_GOBJECT_1_CFLAGS)
AC_SUBST(POLKIT_GOBJECT_1_LIBS)

PKG_CHECK_MODULES(POLKIT_AGENT_1, [polkit-agent-1 >= 0.92])
AC_SUBST(POLKIT_AGENT_1_CFLAGS)
AC_SUBST(POLKIT_AGENT_1_LIBS)

PKG_CHECK_MODULES(UDISKS, [udisks2 >= 2.1.0])
AC_SUBST(UDISKS_CFLAGS)
AC_SUBST(UDISKS_LIBS)

# udevdir
AC_ARG_WITH([udevdir],
            AS_HELP_STRING([--with-udevdir=DIR], [Directory for udev]),
            [],
            [with_udevdir=$($PKG_CONFIG --variable=udevdir udev)])
AC_SUBST([udevdir], [$with_udevdir])

# systemd
AC_ARG_WITH([systemdsystemunitdir],
            AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
            [],
            [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
if test "x$with_systemdsystemunitdir" != "xno"; then
  AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
fi
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$systemdsystemunitdir"])

# Internationalization
#

IT_PROG_INTLTOOL([$INTLTOOL_REQUIRED])
GETTEXT_PACKAGE=udisks-lvm
AC_SUBST([GETTEXT_PACKAGE])
AM_GLIB_GNU_GETTEXT
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[gettext domain])

# Coverage
#

AC_MSG_CHECKING([whether to build with gcov testing])
AC_ARG_ENABLE([coverage],
              AS_HELP_STRING([--enable-coverage],
                             [Whether to enable coverage testing ]),
              [], [enable_coverage=no])
AC_MSG_RESULT([$enable_coverage])

AS_IF([test "$enable_coverage" = "yes"], [
  AS_IF([test "$GCC" != "yes"], [
    AC_MSG_ERROR(Coverage testing requires GCC)
  ])
  AC_PATH_PROG(GCOV, gcov, no)
  AS_IF([test "$GCOV" = "no"], [
    AC_MSG_ERROR(gcov tool is not available)
  ])
  AC_PATH_PROG(LCOV, lcov, no)
  AS_IF([test "$LCOV" = "no"], [
    AC_MSG_ERROR(lcov tool is not installed)
  ])
  AC_PATH_PROG(GENHTML, genhtml, no)
  AS_IF([test "$GENHTML" = "no"], [
    AC_MSG_ERROR(lcov's genhtml tool is not installed)
  ])

  CFLAGS="$CFLAGS -O0 -g -fprofile-arcs -ftest-coverage"
  LDFLAGS="$LDFLAGS -lgcov"
])

AM_CONDITIONAL([WITH_COVERAGE], [test "$enable_coverage" = "yes"])
AC_SUBST(LCOV)
AC_SUBST(GCOV)
AC_SUBST(GENHTML)

# Generate
#

AC_OUTPUT([
Makefile
data/Makefile
src/Makefile
src/tests/Makefile
doc/Makefile
doc/version.xml
doc/man/Makefile
po/Makefile.in
])

dnl ==========================================================================
echo "
               udisks-lvm $VERSION
             =====================

        prefix:                     ${prefix}
        libdir:                     ${libdir}
        libexecdir:                 ${libexecdir}
        bindir:                     ${bindir}
        sbindir:                    ${sbindir}
        datadir:                    ${datadir}
        sysconfdir:                 ${sysconfdir}
        localstatedir:              ${localstatedir}
        docdir:                     ${docdir}
        introspection:              ${found_introspection}

        udevdir:                    ${udevdir}
        systemdsystemunitdir:       ${systemdsystemunitdir}
        using libsystemd-login:     ${have_libsystemd_login}

        compiler:                   ${CC}
        cflags:                     ${CFLAGS}
        cppflags:                   ${CPPFLAGS}

        Maintainer mode:            ${USE_MAINTAINER_MODE}
        Coverage:                   ${enable_coverage}
        Building man pages:         ${enable_man}
"
