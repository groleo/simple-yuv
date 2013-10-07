#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#include <hardware/gralloc.h>

#include "simple-yuv.h"

#include "wayland-android-client-protocol.h"

struct display_priv {
	gralloc_module_t *bufmod;
	alloc_device_t  *bufmgr;
	struct android_wlegl *android_wlegl;
};

struct buffer_priv {
	buffer_handle_t bo;
	void *vaddr;
	int usage;
	int width;
	int height;
};
/*
static void
buffer_release(void *data, struct wl_buffer *buffer)
{
	struct buffer *mybuf = data;

	mybuf->busy = 0;
}

static const struct wl_buffer_listener buffer_listener = {
	buffer_release
};
*/

struct buffer*
buffer_alloc(struct display* display, struct window *window, struct buffer *buffer, uint32_t format)
{
	printf("**** %s\n",__func__);
	int err;
	struct display_priv *dp = display->priv;


	if (format == fourcc_code('Y','U','Y','V'))
		buffer->stride0 = window->width * 4;
	else
		buffer->stride0 = window->width;

	buffer->offset0 = 0;

	if (format == fourcc_code('Y','U','1','2'))
		buffer->stride1 = buffer->stride0 / 2;
	else if (format == fourcc_code('N','V','1','2'))
		buffer->stride1 = window->width;
	else
		buffer->stride1 = 0;

	/////////
	buffer->offset1 = buffer->stride0 * window->height;

	if (format == fourcc_code('Y','U','1','2'))
		buffer->stride2 = window->height / 2;
	else
		buffer->stride2 = 0;

	buffer->priv = calloc(1,sizeof(struct buffer_priv));
	struct buffer_priv *bp = buffer->priv;

	bp->width = window->width;
	bp->height = window->height;
	//bp->usage= GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE;
	//bp->usage=  GRALLOC_USAGE_SW_READ_OFTEN|GRALLOC_USAGE_SW_WRITE_OFTEN;
	bp->usage=  GRALLOC_USAGE_SW_WRITE_OFTEN;
	bp->usage |= GRALLOC_USAGE_HW_COMPOSER  | GRALLOC_USAGE_HW_TEXTURE;

	err = dp->bufmgr->alloc(dp->bufmgr, window->width, window->height, format,
			bp->usage, &bp->bo, &buffer->stride0);
	if (err) {
		fprintf(stderr, "%s failed: %d\n",__func__, err);
		return NULL;
	}
	struct wl_array ints;
	int *ints_data;
	wl_array_init(&ints);
	ints_data = (int*) wl_array_add(&ints, bp->bo->numInts*sizeof(int));
	memcpy(ints_data, bp->bo->data + bp->bo->numFds, bp->bo->numInts*sizeof(int));
	struct android_wlegl_handle *wlegl_handle = android_wlegl_create_handle(dp->android_wlegl, bp->bo->numFds, &ints);
	wl_array_release(&ints);
	for (int i = 0; i < bp->bo->numFds; i++) {
		android_wlegl_handle_add_fd(wlegl_handle, bp->bo->data[i]);
	}
	buffer->buffer = android_wlegl_create_buffer(dp->android_wlegl,
				bp->width, bp->height,
				buffer->stride0,
				format,
				bp->usage,
				wlegl_handle);
	if (buffer->buffer == NULL) {
		fprintf(stderr,"android_wlegl_create_buffer failed\n");
		return NULL;
	}
	android_wlegl_handle_destroy(wlegl_handle);
	return buffer;
}


unsigned char *buffer_mmap(struct display *display, struct buffer* buffer)
{
	struct display_priv *dp=display->priv;
	struct buffer_priv *bp =buffer->priv;

	if (bp->vaddr != NULL)
		return bp->vaddr;

	int err = dp->bufmod->lock(dp->bufmod, bp->bo, bp->usage,
			0,0, bp->width, bp->height, (void**)(&bp->vaddr));
	if (err
	|| bp->vaddr == NULL) {
		fprintf(stderr, "%s failed: %d %p\n", __func__, err, bp->vaddr);
		return NULL;
	}
	printf("**** ? %s %p %dx%d\n", __func__, bp->vaddr, bp->width, bp->height);
	return bp->vaddr;
}

unsigned char *buffer_munmap(struct display *display, struct buffer* buffer)
{
	struct display_priv *dp=display->priv;
	struct buffer_priv *bp =buffer->priv;
	void *vaddr;
	printf("**** %s %p %dx%d\n",__func__, bp->vaddr, bp->width, bp->height);

	int err = dp->bufmod->unlock(dp->bufmod, bp->bo);
	if (err) {
		fprintf(stderr, "%s failed: %d\n",__func__, err);
		return NULL;
	}
	bp->vaddr=NULL;
}


int
buffer_manager_init(struct display* d)
{
	hw_module_t const *module;
	int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
	if (err)
		return err;

	struct display_priv *dp= d->priv;
	dp->bufmod = (gralloc_module_t *)module;
	err = gralloc_open(module, &dp->bufmgr);
	if (err) {
		fprintf(stderr, "%s failed\n",__func__);
		return err;
	}

	return 0;
}


struct display_priv*
display_priv_alloc(struct display* display)
{
	printf("**** %s\n",__func__);
	if (display->priv != NULL)
		return display->priv;

	display->priv = calloc(1,sizeof(struct display_priv));
	return display->priv;
}


static void
handle_format(void *data, struct wl_drm *drm, uint32_t format)
{
	struct display *d = data;
	printf("**** %s\n",__func__);

	d->formats[d->format_count++] = format;
}


const struct android_wlegl_listener wlegl_listener = {
	handle_format
};

void add_listeners(struct display* display,uint32_t id)
{
	printf("**** %s\n",__func__);
	display->authenticated = 1;
	struct display_priv *dp = display_priv_alloc(display);
	dp->android_wlegl = wl_registry_bind(display->registry, id,
			&android_wlegl_interface, 1);
	printf("**** %s -- done\n",__func__);
	android_wlegl_add_listener(dp->android_wlegl, &wlegl_listener, display);
	buffer_manager_init(display);
}

void
module_destroy_display(struct display *display)
{
	struct display_priv *dp = display->priv;
	if (dp->android_wlegl)
		android_wlegl_destroy(dp->android_wlegl);
}
