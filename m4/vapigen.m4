dnl vapigen.m4
dnl
dnl Copyright 2012 Evan Nemerson
dnl
dnl This library is free software; you can redistribute it and/or
dnl modify it under the terms of the GNU Lesser General Public
dnl License as published by the Free Software Foundation; either
dnl version 2.1 of the License, or (at your option) any later version.
dnl
dnl This library is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl Lesser General Public License for more details.
dnl
dnl You should have received a copy of the GNU Lesser General Public
dnl License along with this library; if not, write to the Free Software
dnl Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

# VAPIGEN_CHECK([VERSION], [API_VERSION], [FOUND-INTROSPECTION], [DEFAULT])
# --------------------------------------
# Check vapigen existence and version
#
# See http://live.gnome.org/Vala/UpstreamGuide for detailed documentation
AC_DEFUN([VAPIGEN_CHECK],
[
  AC_REQUIRE([GOBJECT_INTROSPECTION_CHECK])
  AC_REQUIRE([PKG_PROG_PKG_CONFIG])
  AC_REQUIRE([AM_PROG_VALAC])

  AC_ARG_ENABLE([vala],
    [AS_HELP_STRING([--enable-vala[=@<:@no/auto/yes@:>@]],[build Vala bindings @<:@default=]ifelse($4,,auto,$4)[@:>@])],
    [enable_vala=$withval],[
      AS_IF([test "x$4" = "x"], [
          enable_vala=auto
        ], [
          enable_vala=$4
        ])
    ])
  AS_CASE([$enable_vala], [no], [enable_vala=no],
      [yes], [
        AS_IF([test "x$3" != "xyes" -a "x$found_introspection" != "xyes"], [
            AC_MSG_ERROR([Vala bindings require GObject Introspection])
          ])
      ], [auto], [
        AS_IF([test "x$3" != "xyes" -a "x$found_introspection" != "xyes"], [
            enable_vala=no
          ])
      ], [
        AC_MSG_ERROR([Invalid argument passed to --enable-vala, should be one of @<:@no/auto/yes@:>@])
      ])

  AS_IF([ test "x$2" = "x"], [
      vapigen_pkg_name=vapigen
    ], [
      vapigen_pkg_name=vapigen-$2
    ])
  AS_IF([ test "x$1" = "x"], [
      vapigen_pkg="$vapigen_pkg_name"
    ], [
      vapigen_pkg="$vapigen_pkg_name >= $1"
    ])
  
  PKG_CHECK_EXISTS([$vapigen_pkg], [
      AS_IF([test "$enable_vala" = "auto"], [
          vapigen_pkg_found=yes
        ])
    ], [
          vapigen_pkg_found=no
          AC_PATH_PROG(VAPIGEN, [$vapigen_pkg_name], [no])
          if test "x$VAPIGEN" = "xno"
          then
            check_prog=no
          else
            check_prog=yes
          fi
    ])
  
  AC_MSG_CHECKING([for vapigen])
  
  AS_CASE([$vapigen_pkg_found],
    [yes], [
        VAPIGEN=`$PKG_CONFIG --variable=vapigen vapigen`
        VAPIGEN_MAKEFILE=`$PKG_CONFIG --variable=datadir vapigen`/vala/Makefile.vapigen
        AS_IF([test "x$2" = "x"], [
            VAPIGEN_VAPIDIR=`$PKG_CONFIG --variable=vapidir vapigen`
          ], [
            VAPIGEN_VAPIDIR=`$PKG_CONFIG --variable=vapidir_versioned vapigen`
          ])
      ],
      [no], [
      AS_CASE([$check_prog],[no],[
          AC_MSG_ERROR([Can't locate $vapigen_pkg])
        ], [yes], [
          AS_IF([test "x$2" = "x"], [
              vala_pkg_name=libvala
            ], [
              vala_pkg_name=libvala-$2
            ])
          AS_IF([test "x$1" = "x"], [
              vala_pkg="$vala_pkg_name"
            ], [
              vala_pkg="$vala_pkg_name >= $1"
            ])
          PKG_CHECK_EXISTS([$vala_pkg], [
              AS_IF([test "$enable_vala" = "auto"], [
                  VALA_DATADIR=`pkg-config $vala_pkg --variable=datadir`
                  VAPIGEN_VAPIDIR="$VALA_DATADIR/vala/vapi"
                  VAPIGEN_MAKEFILE="$VALA_DATADIR/vala/Makefile.vapigen"
                  vapigen_pkg_found=yes
                ])
            ], [
              AS_CASE([$enable_vala], [yes], [
                  AC_MSG_ERROR([$vala_pkg not found])
                ], [auto], [
                  vapigen_pkg_found=no
                ])
            ])
       ])
    ])

  AC_MSG_RESULT([$vapigen_pkg_found])

  AC_SUBST([VAPIGEN])
  AC_SUBST([VAPIGEN_VAPIDIR])
  AC_SUBST([VAPIGEN_MAKEFILE])

  AM_CONDITIONAL(ENABLE_VAPIGEN, test "x$vapigen_pkg_found" = "xyes")
])

