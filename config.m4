dnl $Id$
dnl config.m4 for extension runkit

PHP_ARG_ENABLE(runkit, whether to enable runkit support,
[  --enable-runkit           Enable runkit support], no, yes)

PHP_ARG_ENABLE(runkit-modify, whether to enable runtime manipulation of functions/classes/constants,
[  --enable-runkit-modify    Enable runtime manipulation], inherit, no)

PHP_ARG_ENABLE(runkit-super, whether to enable registration of user-defined autoglobals,
[  --enable-runkit-super     Enable registration of user-defined autoglobals], inherit, no)

PHP_ARG_ENABLE(runkit_spl_object_id, whether to enable spl_object_id in PHP <= 7.1,
[  --enable-runkit-spl_object_id    Enable spl_object_id support], no, no)

dnl PHP_ARG_ENABLE(runkit-sandbox, whether to enable Sandbox support,
dnl [  --enable-runkit-sandbox   Enable Runkit_Sandbox (Requires ZTS)], inherit, no)

if test "$PHP_RUNKIT" != "no"; then
  if test "$PHP_RUNKIT_MODIFY" = "inherit"; then
    PHP_RUNKIT_MODIFY=yes
  fi
  if test "$PHP_RUNKIT_SUPER" = "inherit"; then
    PHP_RUNKIT_SUPER=yes
  fi
dnl  if test "$PHP_RUNKIT_SANDBOX" = "inherit"; then
dnl    PHP_RUNKIT_SANDBOX=yes
dnl  fi
else
  if test "$PHP_RUNKIT_MODIFY" = "inherit"; then
    PHP_RUNKIT_MODIFY=no
  fi
  if test "$PHP_RUNKIT_SUPER" = "inherit"; then
    PHP_RUNKIT_SUPER=no
  fi
dnl  if test "$PHP_RUNKIT_SANDBOX" = "inherit"; then
dnl    PHP_RUNKIT_SANDBOX=no
dnl  fi
fi

dnl   test "$PHP_RUNKIT_SANDBOX" = "yes" ||
if test "$PHP_RUNKIT_MODIFY" = "yes" ||
   test "$PHP_RUNKIT_SUPER" = "yes"; then
  if test "$PHP_RUNKIT" != "classkit"; then
    PHP_RUNKIT=yes
  fi
fi

if test "$PHP_RUNKIT" != "no"; then
  if test "$PHP_RUNKIT" = "classkit"; then
    AC_DEFINE(PHP_RUNKIT_CLASSKIT_COMPAT, 1, [Whether to export classkit compatible function aliases])
  fi
  if test "$PHP_RUNKIT_MODIFY" != "no"; then
    AC_DEFINE(PHP_RUNKIT_FEATURE_MODIFY, 1, [Whether to export runtime modification features])
  fi
  if test "$PHP_RUNKIT_SUPER" != "no"; then
    AC_DEFINE(PHP_RUNKIT_FEATURE_SUPER, 1, [Whether to export custom autoglobal registration feature])
  fi
dnl  if test "$PHP_RUNKIT_SANDBOX" != "no"; then
dnl    AC_DEFINE(PHP_RUNKIT_FEATURE_SANDBOX, 1, [Whether to export Sandbox feature])
dnl  fi
  if test "$PHP_RUNKIT_SPL_OBJECT_ID" != "no"; then
    AC_DEFINE(PHP_RUNKIT_SPL_OBJECT_ID, 1, [Whether to define spl_object_id in php <= 7.1])
  fi
dnl runkit_sandbox.c runkit_sandbox_parent.c
  PHP_NEW_EXTENSION(runkit, runkit.c runkit_functions.c runkit_methods.c \
runkit_import.c \
runkit_constants.c \
runkit_object_id.c \
runkit_common.c \
runkit_zend_execute_API.c \
runkit_classes.c \
runkit_props.c \
, $ext_shared,, -Wdeclaration-after-statement -Werror -Wall -Wno-deprecated-declarations -Wno-pedantic)
fi
