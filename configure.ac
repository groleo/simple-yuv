AC_PREREQ([2.68])
AC_INIT([simple-yuv], [1.0], [https://bugs.freedesktop.org/enter_bug.cgi?product=Wayland&component=weston&version=weston_version])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.11])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h malloc.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([getpagesize memset munmap select strerror])

PKG_CHECK_MODULES(WAYLAND_DEPS, [wayland-client wayland-egl])

AC_ARG_ENABLE(intel-drm,
    AS_HELP_STRING([ --enable-intel-drm],[Use Intel DRM]),,
    enable_intel_drm=yes)
AM_CONDITIONAL(ENABLE_INTEL_DRM, test "x$enable_intel_drm" = "xyes")
if test "x$enable_intel_drm" = "xyes"; then
    AC_DEFINE([BUILD_INTEL_DRM], [1], [Use Intel DRM])
    PKG_CHECK_MODULES(INTEL_DRM, [libdrm_intel])
fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
