#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <libdrm/intel_bufmgr.h>
#include <xf86drm.h>
#include "wayland-drm-client-protocol.h"

#include "simple-yuv.h"

struct display_priv {
	struct wl_drm *drm;
	drm_intel_bufmgr *bufmgr;
	int fd;
};

struct buffer_priv {
	drm_intel_bo *bo;
};

unsigned char* buffer_mmap(struct display *d, struct buffer *buffer)
{
	struct buffer_priv *bp=buffer->priv;
	return bp->bo->virtual;
}

struct display_priv*
display_priv_alloc(struct display* display)
{
	if (display->priv != NULL)
		return display->priv;

	display->priv = calloc(1,sizeof(struct display_priv));
	return display->priv;
}


static void
drm_handle_device(void *data, struct wl_drm *drm, const char *device)
{
	struct display *d = data;
	drm_magic_t magic;
	struct display_priv *dp = d->priv;

	dp->fd = open(device, O_RDWR | O_CLOEXEC);
	if (dp->fd == -1) {
		fprintf(stderr, "could not open %s: %m\n", device);
		exit(-1);
	}

	drmGetMagic(dp->fd, &magic);
	wl_drm_authenticate(dp->drm, magic);
}

static void
drm_handle_format(void *data, struct wl_drm *drm, uint32_t format)
{
	struct display *d = data;

	d->formats[d->format_count++] = format;
}

static void
drm_handle_authenticated(void *data, struct wl_drm *drm)
{
	struct display *d = data;
	struct display_priv *dp = d->priv;

	d->authenticated = 1;
	dp->bufmgr = drm_intel_bufmgr_gem_init(dp->fd, 4096);
}

const struct wl_drm_listener drm_listener = {
	drm_handle_device,
	drm_handle_format,
	drm_handle_authenticated
};


struct buffer*
buffer_alloc(struct display* display, struct window *window, struct buffer *buffer, uint32_t format)
{
	uint32_t name;
	int total;
	struct display_priv *dp = display->priv;
	buffer->priv=malloc(sizeof(struct buffer_priv));

	if (format == fourcc_code('Y','U','Y','V'))
		buffer->stride0 = window->width * 4;
	else
		buffer->stride0 = window->width;

	buffer->offset0 = 0;
	total = buffer->stride0 * window->height;

	if (format == fourcc_code('Y','U','1','2'))
		buffer->stride1 = window->width / 2;
	else if (format == fourcc_code('N','V','1','2'))
		buffer->stride1 = window->width;
	else
		buffer->stride1 = 0;

	buffer->offset1 = align(total, 4096);
	total = buffer->offset1 + buffer->stride1 * window->height / 2;

	if (format == fourcc_code('Y','U','1','2'))
		buffer->stride2 = window->width / 2;
	else
		buffer->stride2 = 0;

	buffer->offset2 = align(total, 4096);
	total = buffer->offset2 + buffer->stride2 * window->height / 2;

	struct buffer_priv *bp = buffer->priv;

	bp->bo =
		drm_intel_bo_alloc(dp->bufmgr, "simple-yuv", total, 0);
	drm_intel_gem_bo_map_gtt(bp->bo);

	drm_intel_bo_flink(bp->bo, &name);

	switch (format) {
		case fourcc_code('Y','U','1','2'):
		case fourcc_code('N','V','1','2'):
			buffer->buffer =
				wl_drm_create_planar_buffer(dp->drm, name,
						window->width,
						window->height,
						format,
						buffer->offset0,
						buffer->stride0,
						buffer->offset1,
						buffer->stride1,
						buffer->offset2,
						buffer->stride2);
			break;
		case fourcc_code('Y','U','Y','V'):
			buffer->buffer =
				wl_drm_create_buffer(dp->drm, name,
						window->width, window->height,
						buffer->stride0,
						WL_DRM_FORMAT_YUYV);
			break;
	}
}

void
add_listeners(struct display* display,uint32_t id)
{
	struct display_priv *dp = display_priv_alloc(display);
	dp->drm = wl_registry_bind(display->registry, id,
			&wl_drm_interface, 1);
	wl_drm_add_listener(dp->drm, &drm_listener, display);
}

void
module_destroy_display(struct display *display)
{
	struct display_priv *dp = display->priv;
	if (dp->drm)
		wl_drm_destroy(dp->drm);
}
