PHP_ARG_ENABLE([pdo-lake],
  [whether to enable PDO Lake support],
  [AS_HELP_STRING([--enable-pdo-lake],
    [Enable the PDO driver for TiDB Cloud Lake and Databend])],
  [yes])

PHP_ARG_WITH([lake-ffi],
  [for a prebuilt lake_ffi static library],
  [AS_HELP_STRING([[--with-lake-ffi[=DIR]]],
    [Directory containing a prebuilt liblake_ffi.a; by default make builds it with cargo])],
  [no],
  [no])

if test "$PHP_PDO_LAKE" != "no"; then
  PHP_CHECK_PDO_INCLUDES

  dnl PHP's bundled libtool picks linker flags that current ld64 reports as deprecated
  dnl (-flat_namespace -undefined suppress when no deployment target is set, and -single_module).
  case $host_os in
    darwin*)
      if test -z "$MACOSX_DEPLOYMENT_TARGET"; then
        MACOSX_DEPLOYMENT_TARGET=`sw_vers -productVersion | cut -d. -f1-2`
        export MACOSX_DEPLOYMENT_TARGET
      fi
      lt_cv_apple_cc_single_mod=no
      LAKE_FFI_SYSTEM_LIBS="-framework CoreFoundation -framework Security -framework SystemConfiguration -liconv -lresolv"
      ;;
    *)
      dnl -z nodelete keeps the module mapped after PHP's dlclose: thread-local destructors registered by
      dnl the Rust side (tokio, rand) run later, at thread or process exit, and would jump into unmapped code.
      LAKE_FFI_SYSTEM_LIBS="-lpthread -ldl -lm -lrt -Wl,-z,nodelete"
      ;;
  esac

  if test "$PHP_LAKE_FFI" = "no" || test "$PHP_LAKE_FFI" = "yes"; then
    AC_PATH_PROG([CARGO], [cargo], [no])
    if test "$CARGO" = "no"; then
      AC_MSG_ERROR([cargo is required to build the Rust part of pdo_lake; install Rust 1.88 or newer from https://rustup.rs, or pass --with-lake-ffi=DIR])
    fi
    LAKE_FFI_LIB="$abs_srcdir/ffi/target/release/liblake_ffi.a"
    LAKE_FFI_BUILD="cd '$abs_srcdir/ffi' && '$CARGO' build --release --locked"
  else
    LAKE_FFI_LIB="$PHP_LAKE_FFI/liblake_ffi.a"
    if test ! -f "$LAKE_FFI_LIB"; then
      AC_MSG_ERROR([$LAKE_FFI_LIB does not exist])
    fi
    LAKE_FFI_BUILD="true"
  fi

  dnl Passed through -Wl so libtool does not warn about linking a static archive into a shared module.
  PDO_LAKE_SHARED_LIBADD="-Wl,$LAKE_FFI_LIB $LAKE_FFI_SYSTEM_LIBS"
  PDO_LAKE_SHARED_DEPENDENCIES="$LAKE_FFI_LIB"
  PHP_SUBST([LAKE_FFI_LIB])
  PHP_SUBST([LAKE_FFI_BUILD])
  PHP_SUBST([PDO_LAKE_SHARED_LIBADD])
  PHP_SUBST([PDO_LAKE_SHARED_DEPENDENCIES])

  PHP_NEW_EXTENSION([pdo_lake],
    [src/pdo_lake.c src/lake_driver.c src/lake_statement.c],
    [$ext_shared],,
    [-I@ext_srcdir@/src])
  PHP_ADD_BUILD_DIR([$ext_builddir/src])
  PHP_ADD_EXTENSION_DEP(pdo_lake, pdo)
  PHP_ADD_EXTENSION_DEP(pdo_lake, json)
  PHP_ADD_MAKEFILE_FRAGMENT
fi
