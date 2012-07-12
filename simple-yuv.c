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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>

#include <linux/input.h>
#include <libdrm/intel_bufmgr.h>
#include <xf86drm.h>
#include <wayland-client.h>

#include "wayland-drm-client-protocol.h"

struct display {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_shell *shell;
	struct wl_drm *drm;
	struct wl_seat *seat;
	struct wl_keyboard *keyboard;
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
	int offset0, stride0;
	int offset1, stride1;
	int offset2, stride2;
};

struct window {
	struct display *display;
	int width, height;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	struct wl_callback *callback;

	struct buffer *front, *back;
	uint32_t frame_duration, next_frame;
	uint32_t format;
	int has_timestamp;
	unsigned char *y, *u, *v;
	FILE *source;

	void (*paint)(struct window *window, uint32_t time);
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
paint_source(struct window *window, uint32_t time)
{
	int size = window->width * window->height;
	char buf[256];

	fgets(buf, sizeof buf, window->source);
	fread(window->y, 1, size, window->source);
	fread(window->u, 1, size / 4, window->source);
	fread(window->v, 1, size / 4, window->source);
}

static void
paint_pixels(struct window *window, uint32_t time)
{
	int width = window->width, height = window->height;
	const int halfh = height / 2;
	const int halfw = width / 2;
	int ir, or;
	uint8_t *y = window->y;
	uint8_t *u = window->u;
	uint8_t *v = window->v;
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

static void
convert_to_yuyv(struct window *window)
{
	struct buffer *buffer = window->back;
	unsigned char *y, *u, *v, *yuyv_out, *p;
	int i, j;

	for (i = 0; i < window->height; i++) {
		p = buffer->bo->virtual;
		yuyv_out = p + buffer->offset0 + i * window->back->stride0;

		y = window->y + i * window->width;
		u = window->u + (i / 2) * window->width / 2;
		v = window->v + (i / 2) * window->width / 2;

		for (j = 0; j < window->width / 2; j++) {
			*yuyv_out++ = *y++;
			*yuyv_out++ = *u++;
			*yuyv_out++ = *y++;
			*yuyv_out++ = *v++;
		}
	}
}

static void
convert_to_nv12(struct window *window)
{
	struct buffer *buffer = window->back;
	unsigned char *u, *v, *uv_out, *p;
	int i;

	p = buffer->bo->virtual;
	uv_out = p + buffer->offset1;
	u = window->u;
	v = window->v;
	for (i = 0; i < window->width * window->height / 4; i++) {
		*uv_out++ = *u++;
		*uv_out++ = *v++;
	}
}

static void
paint(struct window *window, uint32_t time)
{
	struct buffer *buffer = window->back;
	unsigned char *p = buffer->bo->virtual;

	switch (window->format) {
	case WL_DRM_FORMAT_YUV420:
		window->y = p + buffer->offset0;
		window->u = p + buffer->offset1;
		window->v = p + buffer->offset2;
		break;
	case WL_DRM_FORMAT_NV12:
		window->y = p + buffer->offset0;
		break;
	}

	window->paint(window, time);

	switch (window->format) {
	case WL_DRM_FORMAT_YUYV:
		convert_to_yuyv(window);
		break;
	case WL_DRM_FORMAT_NV12:
		convert_to_nv12(window);
		break;
	}
}

static const struct wl_callback_listener frame_listener;

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
	struct window *window = data;
	struct buffer *tmp;

	if (!window->has_timestamp || window->next_frame < time ||
	    window->frame_duration == 0) {
		paint(window, time);
		wl_surface_attach(window->surface, window->back->buffer, 0, 0);

		tmp = window->back;
		window->back = window->front;
		window->front = tmp;
		window->has_timestamp = 1;
		window->next_frame = time + window->frame_duration;
	}

	wl_surface_damage(window->surface,
			  0, 0, window->width, window->height);

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
create_buffer(struct window *window, uint32_t format)
{
	struct display *display = window->display;
	struct buffer *buffer;
	uint32_t name;
	int total;

	buffer = malloc(sizeof *buffer);
	if (format == WL_DRM_FORMAT_YUYV)
		buffer->stride0 = window->width * 4;
	else
		buffer->stride0 = window->width;

	buffer->offset0 = 0;
	total = buffer->stride0 * window->height;

	if (format == WL_DRM_FORMAT_YUV420)
		buffer->stride1 = window->width / 2;
	else if (format == WL_DRM_FORMAT_NV12)
		buffer->stride1 = window->width;
	else
		buffer->stride1 = 0;

	buffer->offset1 = align(total, 4096);
	total = buffer->offset1 + buffer->stride1 * window->height / 2;

	if (format == WL_DRM_FORMAT_YUV420)
		buffer->stride2 = window->width / 2;
	else
		buffer->stride2 = 0;

	buffer->offset2 = align(total, 4096);
	total = buffer->offset2 + buffer->stride2 * window->height / 2;

	buffer->bo =
		drm_intel_bo_alloc(display->bufmgr, "simple-yuv", total, 0);
	drm_intel_gem_bo_map_gtt(buffer->bo);

	drm_intel_bo_flink(buffer->bo, &name);

	switch (format) {
	case WL_DRM_FORMAT_YUV420:
	case WL_DRM_FORMAT_NV12:
		buffer->buffer =
			wl_drm_create_planar_buffer(display->drm, name,
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
	case WL_DRM_FORMAT_YUYV:
		buffer->buffer =
			wl_drm_create_buffer(display->drm, name,
					     window->width, window->height,
					     buffer->stride0,
					     WL_DRM_FORMAT_YUYV);
		break;
	}

	return buffer;
}

static struct window *
create_window(struct display *display, uint32_t format, int width, int height)
{
	struct window *window;
	int i;

	for (i = 0; i < display->format_count; i++) {
		if (display->formats[i] == WL_DRM_FORMAT_YUV420)
			break;
	}

	if (i == display->format_count)  {
		fprintf(stderr,
			"format WL_BUFFER_FORMAT_ARGB8888 not supported\n");
		return NULL;
	}		

	window = malloc(sizeof *window);

	window->format = format;
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

	window->front = create_buffer(window, window->format);
	window->back = create_buffer(window, window->format);

	switch (window->format) {
	case WL_DRM_FORMAT_YUV420:
		break;
	case WL_DRM_FORMAT_YUYV:
		window->y = malloc(window->width * window->height);
		window->u = malloc(window->width * window->height / 4);
		window->v = malloc(window->width * window->height / 4);
		break;
	case WL_DRM_FORMAT_NV12:
		window->u = malloc(window->width * window->height / 4);
		window->v = malloc(window->width * window->height / 4);
		break;
	}

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
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
		       uint32_t format, int fd, uint32_t size)
{
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface,
		      struct wl_array *keys)
{
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface)
{
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
		    uint32_t serial, uint32_t time, uint32_t key,
		    uint32_t state_w)
{
	switch (key) {
	case KEY_ESC:
		exit(1);
	}
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
			  uint32_t serial, uint32_t mods_depressed,
			  uint32_t mods_latched, uint32_t mods_locked,
			  uint32_t group)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
	keyboard_handle_keymap,
	keyboard_handle_enter,
	keyboard_handle_leave,
	keyboard_handle_key,
	keyboard_handle_modifiers,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
			 enum wl_seat_capability caps)
{
	struct display *d = data;

	if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
		d->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(d->keyboard, &keyboard_listener, d);
	}
}

static const struct wl_seat_listener seat_listener = {
	seat_handle_capabilities,
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
	} else if (strcmp(interface, "wl_seat") == 0) {
		d->seat = wl_display_bind(display, id, &wl_seat_interface);
		wl_seat_add_listener(d->seat, &seat_listener, d);
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

static struct window *
parse_header(struct display *display, uint32_t format, FILE *source)
{
	struct window *window;
	int width, height, n, d;
	char buf[256], *p;
	uint32_t frame_duration;

	fgets(buf, sizeof buf, source);
	if (strncmp(buf, "YUV4MPEG2 ", 10) != 0)
		return NULL;
	frame_duration = 0;
	width = 0;
	height = 0;
	for (p = buf + 10; *p; p = strchrnul(p, ' ')) {
		p++;
		switch (*p) {
		case 'C':
			break;
		case 'W':
			width = atoi(p + 1);
			break;
		case 'H':
			height = atoi(p + 1);
			break;
		case 'F':
			sscanf(p + 1, "%d:%d", &n, &d);
			frame_duration = d * 1000 / n;
			break;
		}
	}

	if (width == 0 || height == 0) {
		fprintf(stderr, "didn't width or height\n");
		return NULL;
	}

	window = create_window(display, format, width, height);
	window->source = source;
	window->frame_duration = frame_duration;
	window->paint = paint_source;

	printf("incoming stream is %dx%d, %d:%d fps (%dms per frame)\n",
	       width, height, n, d, frame_duration);

	return window;
}

static void
fullscreen_window(struct window *window)
{
	wl_shell_surface_set_fullscreen(window->shell_surface,
					WL_SHELL_SURFACE_FULLSCREEN_METHOD_SCALE,
					0, NULL);
}

int
main(int argc, char **argv)
{
	struct display *display;
	struct window *window;
	uint32_t format = WL_DRM_FORMAT_YUV420;
	FILE *source = NULL;
	int i, fullscreen = 0;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--yuyv") == 0)
			format = WL_DRM_FORMAT_YUYV;
		if (strcmp(argv[i], "--yuv") == 0)
			format = WL_DRM_FORMAT_YUV420;
		if (strcmp(argv[i], "--nv12") == 0)
			format = WL_DRM_FORMAT_NV12;
		if (strcmp(argv[i], "-") == 0)
			source = stdin;
		if (strcmp(argv[i], "-f") == 0)
			fullscreen = 1;
	}

	display = create_display();
	while (!display->authenticated)
		wl_display_roundtrip(display->display);

	if (source) {
		window = parse_header(display, format, source);
	} else {
		window = create_window(display, format, 512, 512);
		window->paint = paint_pixels;
	}

	if (fullscreen)
		fullscreen_window(window);

	redraw(window, NULL, 0);
	while (1) {
		wl_display_flush(display->display);
		wl_display_iterate(display->display, WL_DISPLAY_READABLE);
	}

	destroy_window(window);
	destroy_display(display);

	return 0;
}
