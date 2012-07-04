/*
 * Copyright © 2011 Benjamin Franzke
 * Copyright © 2010 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>

#include <libdrm/intel_bufmgr.h>
#include <xf86drm.h>
#include <wayland-client.h>

#include "wayland-drm-client-protocol.h"

#define CAP 1

struct display {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_shell *shell;
	struct wl_drm *drm;
	drm_intel_bufmgr *bufmgr;
	int fd;
	int authenticated;
	uint32_t formats[10];
	int format_count;
	uint32_t mask;
};

struct buffer {
	drm_intel_bo *bo;
	struct wl_buffer *buffer;
	uint8_t *y;
	uint8_t *u;
	uint8_t *v;
};
	

struct window {
	struct display *display;
	int width, height;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	struct wl_callback *callback;

	struct buffer *front, *back;
};

static void
handle_ping(void *data, struct wl_shell_surface *shell_surface,
							uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
		 uint32_t edges, int32_t width, int32_t height)
{
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
	handle_ping,
	handle_configure,
	handle_popup_done
};

static void
read_frame(struct window *window)
{
	struct buffer *tmp;
	int size = window->width * window->height;

	fread(window->back->y, 1, size, stdin);
	fread(window->back->u, 1, size / 4, stdin);
	fread(window->back->v, 1, size / 4, stdin);

	wl_surface_attach(window->surface, window->back->buffer, 0, 0);
	wl_surface_damage(window->surface,
			  0, 0, window->width, window->height);

	tmp = window->back;
	window->back = window->front;
	window->front = tmp;
}

static void
paint_pixels(struct window *window, int width, int height, uint32_t time)
{
	const int halfh = height / 2;
	const int halfw = width / 2;
	int ir, or;
	uint8_t *y = window->back->y;
	uint8_t *u = window->back->u;
	uint8_t *v = window->back->v;
	int i, x;

	/* squared radii thresholds */
	or = (halfw < halfh ? halfw : halfh) - 8;
	ir = or - 32;
	or *= or;
	ir *= ir;

	for (i = 0; i < height; i++) {
		int x;
		int y2 = (i - halfh) * (i - halfh);

		for (x = 0; x < width; x++) {
			uint32_t v;

			/* squared distance from center */
			int r2 = (x - halfw) * (x - halfw) + y2;

			if (r2 < ir)
				v = (r2 / 32 + time / 64) * 0x0080401;
			else if (r2 < or)
				v = (i + time / 32) * 0x0080401;
			else
				v = (x + time / 16) * 0x0080401;
			v &= 0x00ffffff;

			*y++ = v;
		}
	}

	for (i = 0; i < halfh; i++) {
		for (x = 0; x < halfw; x++) {
			*u++ = i > x + 30 ? 200 : 10;
			*v++ = i > x - 30 ? 150 : 50;
		}
	}
}

static const struct wl_callback_listener frame_listener;

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
	struct window *window = data;
	struct buffer *tmp;

	paint_pixels(window, window->width, window->height, time);
	wl_surface_attach(window->surface, window->back->buffer, 0, 0);
	wl_surface_damage(window->surface,
			  0, 0, window->width, window->height);

	tmp = window->back;
	window->back = window->front;
	window->front = tmp;

	if (callback)
		wl_callback_destroy(callback);

	window->callback = wl_surface_frame(window->surface);
	wl_callback_add_listener(window->callback, &frame_listener, window);
}

static const struct wl_callback_listener frame_listener = {
	redraw
};

static inline int
align(int value, int size)
{
	return (value + size - 1) & ~(size - 1);
}

struct buffer *
create_buffer(struct window *window)
{
	struct display *display = window->display;
	struct buffer *buffer;
	int size, offset1, offset2;
	uint32_t name;

	buffer = malloc(sizeof *buffer);

	offset1 = align(window->width * window->height, 4096);
	offset2 = align(offset1 + window->width * window->height, 4096);

	size = align(offset2 + window->width * window->height, 4096);
	buffer->bo =
		drm_intel_bo_alloc(display->bufmgr, "simple-yuv", size, 0);
	drm_intel_gem_bo_map_gtt(buffer->bo);

	drm_intel_bo_flink(buffer->bo, &name);

	buffer->buffer =
		wl_drm_create_planar_buffer(display->drm, name,
					    window->width, window->height,
					    WL_BUFFER_FORMAT_YUV420,
					    0, window->width,
					    offset1, window->width / 2,
					    offset2, window->width / 2);

	buffer->y = buffer->bo->virtual;
	buffer->u = buffer->y + offset1;
	buffer->v = buffer->y + offset2;

	return buffer;
}

static struct window *
create_window(struct display *display, int width, int height)
{
	struct window *window;
	int i;

	for (i = 0; i < display->format_count; i++) {
		if (display->formats[i] == WL_BUFFER_FORMAT_YUV420)
			break;
	}

	if (i == display->format_count)  {
		fprintf(stderr,
			"format WL_BUFFER_FORMAT_ARGB8888 not supported\n");
		return NULL;
	}		

	window = malloc(sizeof *window);

	window->callback = NULL;
	window->display = display;
	window->width = width;
	window->height = height;
	window->surface = wl_compositor_create_surface(display->compositor);
	window->shell_surface = wl_shell_get_shell_surface(display->shell,
							   window->surface);

	if (window->shell_surface)
		wl_shell_surface_add_listener(window->shell_surface,
					      &shell_surface_listener, window);

	wl_shell_surface_set_toplevel(window->shell_surface);

	window->front = create_buffer(window);
	window->back = create_buffer(window);

	return window;
}

static void
destroy_window(struct window *window)
{
	if (window->callback)
		wl_callback_destroy(window->callback);

	wl_shell_surface_destroy(window->shell_surface);
	wl_surface_destroy(window->surface);
	free(window);
}

static void
drm_handle_device(void *data, struct wl_drm *drm, const char *device)
{
	struct display *d = data;
	drm_magic_t magic;

	d->fd = open(device, O_RDWR | O_CLOEXEC);
	if (d->fd == -1) {
		fprintf(stderr, "could not open %s: %m\n", device);
		exit(-1);
	}

	drmGetMagic(d->fd, &magic);
	wl_drm_authenticate(d->drm, magic);
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

   d->authenticated = 1;

   d->bufmgr = drm_intel_bufmgr_gem_init(d->fd, 4096);
}

static const struct wl_drm_listener drm_listener = {
	drm_handle_device,
	drm_handle_format,
	drm_handle_authenticated
};

static void
display_handle_global(struct wl_display *display, uint32_t id,
		      const char *interface, uint32_t version, void *data)
{
	struct display *d = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			wl_display_bind(display, id, &wl_compositor_interface);
	} else if (strcmp(interface, "wl_shell") == 0) {
		d->shell = wl_display_bind(display, id, &wl_shell_interface);
	} else if (strcmp(interface, "wl_drm") == 0) {
		d->drm = wl_display_bind(display, id, &wl_drm_interface);
		wl_drm_add_listener(d->drm, &drm_listener, d);
	}
}

static struct display *
create_display(void)
{
	struct display *display;

	display = malloc(sizeof *display);
	memset(display, 0, sizeof *display);
	display->display = wl_display_connect(NULL);
	assert(display->display);

	wl_display_add_global_listener(display->display,
				       display_handle_global, display);

	return display;
}

static void
destroy_display(struct display *display)
{
	if (display->drm)
		wl_drm_destroy(display->drm);

	if (display->shell)
		wl_shell_destroy(display->shell);

	if (display->compositor)
		wl_compositor_destroy(display->compositor);

	wl_display_flush(display->display);
	wl_display_disconnect(display->display);
	free(display);
}

int
main(int argc, char **argv)
{
	struct display *display;
	struct window *window;
	int test_pattern = 1;

	if (argc == 2 && strcmp(argv[1], "-") == 0)
		test_pattern = 0;

	display = create_display();
	while (!display->authenticated)
		wl_display_roundtrip(display->display);

	if (test_pattern) {
		window = create_window(display, 512, 512);
		redraw(window, NULL, 0);
	} else {
		window = create_window(display, 640, 480);
	}

	while (1) {
		if (test_pattern) {
			wl_display_flush(display->display);
			wl_display_iterate(display->display,
					   WL_DISPLAY_READABLE);
		} else {
			read_frame(window);
			wl_display_flush(display->display);
			wl_display_roundtrip(display->display);
		}

	}

	destroy_window(window);
	destroy_display(display);

	return 0;
}
