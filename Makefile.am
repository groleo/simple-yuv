bin_PROGRAMS = \
	webcam \
	simple-yuv

webcam_SOURCES = webcam.c

simple_yuv_SOURCES = simple-yuv.c
simple_yuv_CFLAGS = $(WAYLAND_DEPS_CFLAGS) -std=c99
simple_yuv_LDADD = $(WAYLAND_DEPS_LIBS)


if ENABLE_INTEL_DRM
simple_yuv_SOURCES += wayland-drm-protocol.c drm-module.c
simple_yuv_CFLAGS += $(INTEL_DRM_CFLAGS)
simple_yuv_LDADD += $(INTEL_DRM_LIBS)
endif

if ENABLE_GRALLOC
simple_yuv_SOURCES += wayland-android-protocol.c gralloc-module.c
simple_yuv_CFLAGS += $(HYBRIS_HARDWARE_CFLAGS)
simple_yuv_LDADD += $(HYBRIS_HARDWARE_LIBS)
endif

