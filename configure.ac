#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([ifdnfc],[0.1.4],[http://sourceforge.net/projects/ifdnfc/support])
AC_CONFIG_SRCDIR([src/ifd-nfc.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE
LT_INIT

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O
PKG_PROG_PKG_CONFIG


# Checks for libraries.
PKG_CHECK_EXISTS([libnfc >= 1.6.0],
                 [PKG_CHECK_MODULES([LIBNFC], [libnfc >= 1.3.9])],
                 [AC_MSG_WARN([libnfc >= 1.3.9 not found by pkg-config])])
PKG_CHECK_EXISTS([libpcsclite > 1.4.102],
                 [PKG_CHECK_MODULES([PCSC], [libpcsclite > 1.4.102])],
                 [AC_MSG_WARN([libpcsclite > 1.4.102 not found by pkg-config])])

saved_CPPFLAGS="$CPPFLAGS"
saved_LIBS="$LIBS"
CPPFLAGS="$CPPFLAGS $PCSC_CFLAGS $LIBNFC_CFLAGS"
LIBS="$LDFLAGS $PCSC_LIBS $LIBNFC_LIBS"
AC_CHECK_HEADERS(winscard.h,,
        [ AC_MSG_ERROR([winscard.h not found, install libpcsclite > 1.4.102 or use ./configure PCSC_CFLAGS=...]) ])
AC_MSG_CHECKING([for SCardEstablishContext])
AC_TRY_LINK_FUNC(SCardEstablishContext, [ AC_MSG_RESULT([yes]) ],
        [ AC_MSG_ERROR([libpcsclite > 1.4.102 not found, use ./configure PCSC_LIBS=...]) ])
AC_CHECK_HEADERS(nfc/nfc.h,,
        [ AC_MSG_ERROR([nfc/nfc.h not found, install libnfc >= 1.3.9 or use ./configure LIBNFC_CFLAGS=...]) ])
AC_MSG_CHECKING([for nfc_initiator_select_passive_target])
AC_TRY_LINK_FUNC(nfc_initiator_select_passive_target, [ AC_MSG_RESULT([yes]) ],
        [ AC_MSG_ERROR([libnfc >= 1.3.9 not found, use ./configure LIBNFC_LIBS=...]) ])
CPPFLAGS="$saved_CPPFLAGS"
LIBS="$saved_LIBS"


# --enable-usbdropdir=DIR
AC_ARG_ENABLE(usbdropdir,
	AS_HELP_STRING([--enable-usbdropdir=DIR],[directory containing USB
	drivers (default to pcscd config or $(prefix)/pcsc/drivers)]),
	[usbdropdir="${enableval}"], [usbdropdir=false])
if test "${usbdropdir}" = false ; then
	usbdropdir=`$PKG_CONFIG libpcsclite --variable=usbdropdir`
fi
AC_DEFINE_UNQUOTED(PCSCLITE_HP_DROPDIR, "$usbdropdir", [directory containing USB drivers])
if test "${usbdropdir}" = "" ; then
	AC_MSG_ERROR([use --enable-usbdropdir=DIR])
fi

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([memset])

# Select OS specific versions of source files.
AC_SUBST(BUNDLE_HOST)
AC_SUBST(DYN_LIB_EXT)
AC_SUBST(usbdropdir)

case "$host" in
*-*-darwin*)
	BUNDLE_HOST="MacOS"
	DYN_LIB_EXT="dylib"
	;;
*-*-freebsd*)
	BUNDLE_HOST="FreeBSD"
	DYN_LIB_EXT="so"
	;;
*-*-openbsd*)
	BUNDLE_HOST="OpenBSD"
	DYN_LIB_EXT="so.0.0"
	;;
*-*-solaris*)
	BUNDLE_HOST="Solaris"
	DYN_LIB_EXT="so"
	;;
*-*-dragonfly*)
	BUNDLE_HOST="DragonFly"
	DYN_LIB_EXT="so"
	;;
*)
	BUNDLE_HOST="Linux"
	DYN_LIB_EXT="so"
esac

CFLAGS="$CFLAGS -std=c99"

cat << EOF

${PACKAGE} has been configured with following options:

Version:              ${PACKAGE_VERSION}

Host:                 ${host}
Compiler:             ${CC}
Preprocessor flags:   ${CPPFLAGS}
Compiler flags:       ${CFLAGS}
Preprocessor flags:   ${CPPFLAGS}
Linker flags:         ${LDFLAGS}
Libraries:            ${LIBS}
LIBNFC_CFLAGS:        ${LIBNFC_CFLAGS}
LIBNFC_LIBS:          ${LIBNFC_LIBS}
PCSC_CFLAGS:          ${PCSC_CFLAGS}
PCSC_LIBS:            ${PCSC_LIBS}
BUNDLE_HOST:          ${BUNDLE_HOST}
DYN_LIB_EXT:          ${DYN_LIB_EXT}

EOF

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
