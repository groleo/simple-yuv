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
#include <wayland-client.h>

#include "simple-yuv.h"
int g_run=1;

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

static int
paint_source(struct window *window, uint32_t time)
{
	printf(" > %s %dx%d\n",__func__,window->width,window->height);
	int size = window->width * window->height;
	char buf[256];

	if (NULL==fgets(buf, sizeof buf, window->source)
	|| size!=fread(window->y, 1, size, window->source)
	|| size/4!=fread(window->u, 1, size / 4, window->source)
	|| size/4!=fread(window->v, 1, size / 4, window->source)) {
		fprintf(stderr, "error while reading yuv stream\n");
		g_run=0;
		return -1;
	}
	printf(" < %s\n",__func__);
	return 0;
}

static int
paint_pixels(struct window *window, uint32_t time)
{
	printf(" > %s\n",__func__);

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
	return 0;
}

static void
convert_to_yuyv(struct window *window, unsigned char *p)
{
	struct buffer *buffer = window->back;
	unsigned char *y, *u, *v, *yuyv_out;
	int i, j;

	for (i = 0; i < window->height; i++) {
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
convert_to_nv12(struct window *window, unsigned char *p)
{
	struct buffer *buffer = window->back;
	unsigned char *u, *v, *uv_out;
	int i;

	uv_out = p + buffer->offset1;
	u = window->u;
	v = window->v;
	for (i = 0; i < window->width * window->height / 4; i++) {
		*uv_out++ = *u++;
		*uv_out++ = *v++;
	}
}

static int
paint(struct window *window, uint32_t time)
{
	printf(" > %s\n",__func__);
	struct buffer *buffer = window->back;

	unsigned char *p = buffer_mmap(window->display,buffer);
	printf(" 0 %s\n",__func__);

	switch (window->format) {
	case fourcc_code('Y','U','1','2'):
		window->y = p + buffer->offset0;
		window->u = p + buffer->offset1;
		window->v = p + buffer->offset2;
		break;
	case fourcc_code('N','V','1','2'):
		printf(" 1.1 %s\n",__func__);
		window->y = p + buffer->offset0;
		printf(" 1.2 %s\n",__func__);
		break;
	}

	if (window->paint(window, time))
		return -1;
	printf(" 1 %s\n",__func__);

	switch (window->format) {
	case fourcc_code('Y','U','Y','V'):
		convert_to_yuyv(window, p);
		break;
	case fourcc_code('N','V','1','2'):
		convert_to_nv12(window, p);
		break;
	}
	buffer_munmap(window->display,buffer);
	printf(" < %s\n",__func__);
	return 0;
}

static const struct wl_callback_listener frame_listener;

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
	struct window *window = data;
	struct buffer *tmp;
	printf(" > %s time:%u\n",__func__, time);

	if (!window->has_timestamp
	||  window->next_frame < time
	||  window->frame_duration == 0) {
		printf(" 0 %s\n",__func__);
		if (paint(window, time))
			return;
		printf(" 1 %s\n",__func__);
		wl_surface_attach(window->surface, window->front->buffer, 0, 0);
		tmp = window->back;
		window->back = window->front;
		window->front = tmp;
		window->has_timestamp = 1;
		window->next_frame = time + window->frame_duration;
	}
	//sleep(1);
	printf(" 2 %s\n",__func__);

	wl_surface_damage(window->surface,
			  0, 0, window->width, window->height);

	if (callback)
		wl_callback_destroy(callback);
	printf(" 3 %s\n",__func__);

	window->callback = wl_surface_frame(window->surface);
	wl_callback_add_listener(window->callback, &frame_listener, window);

	wl_surface_commit(window->surface);
	printf(" 4 %s %p\n",__func__,window->display);
	wl_display_flush(window->display->display);
	printf(" < %s\n",__func__);
}

static const struct wl_callback_listener frame_listener = {
	redraw
};


struct buffer *
create_buffer(struct window *window, uint32_t format)
{
	struct display *display = window->display;
	struct buffer *buffer;

	buffer = malloc(sizeof *buffer);
	buffer_alloc(display, window, buffer, format);

	return buffer;
}

static struct window *
create_window(struct display *display, uint32_t format, int width, int height)
{
	struct window *window;
	int i;

	for (i = 0; i < display->format_count; i++) {
		if (display->formats[i] == fourcc_code('Y','U','1','2'))
			break;
	}
#if 0
	if (i == display->format_count)  {
		fprintf(stderr,
			"format WL_BUFFER_FORMAT_ARGB8888 not supported\n");
		return NULL;
	}		
#endif
	window = malloc(sizeof *window);

	window->has_timestamp = 0;
	window->format = format;
	window->callback = NULL;
	window->display = display;
	window->width = width;
	window->height = height;
	window->fullscreen = 0;
	window->surface = wl_compositor_create_surface(display->compositor);
	wl_surface_set_user_data(window->surface, window);
	window->shell_surface = wl_shell_get_shell_surface(display->shell,
							   window->surface);

	if (window->shell_surface)
		wl_shell_surface_add_listener(window->shell_surface,
					      &shell_surface_listener, window);

	wl_shell_surface_set_toplevel(window->shell_surface);

	window->front = create_buffer(window, window->format);
	window->back = create_buffer(window, window->format);


	switch (window->format) {
	case fourcc_code('Y','U','1','2'):
		break;
	case fourcc_code('Y','U','Y','V'):
		window->y = malloc(window->width * window->height);
		window->u = malloc(window->width * window->height / 4);
		window->v = malloc(window->width * window->height / 4);
		break;
	case fourcc_code('N','V','1','2'):
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
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
		       uint32_t format, int fd, uint32_t size)
{
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface,
		      struct wl_array *keys)
{
	struct display *d = data;

	d->keyboard_focus = surface;
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface)
{
	struct display *d = data;

	d->keyboard_focus = NULL;
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
		    uint32_t serial, uint32_t time, uint32_t key,
		    uint32_t state)
{
	struct display *d = data;
	struct window *window = wl_surface_get_user_data(d->keyboard_focus);

	if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
		return;

	switch (key) {
	case KEY_ESC:
		exit(1);
	case KEY_F11:
		if (window->fullscreen)
			wl_shell_surface_set_toplevel(window->shell_surface);
		else
			wl_shell_surface_set_fullscreen(window->shell_surface,
							WL_SHELL_SURFACE_FULLSCREEN_METHOD_SCALE,
							0, NULL);
		window->fullscreen = !window->fullscreen;
		break;
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
registry_handle_global(void *data, struct wl_registry *registry, uint32_t id,
		       const char *interface, uint32_t version)
{
	struct display *d = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			wl_registry_bind(d->registry, id,
					 &wl_compositor_interface, 1);
	} else if (strcmp(interface, "wl_shell") == 0) {
		d->shell = wl_registry_bind(d->registry, id, 
					   &wl_shell_interface, 1);
	} else if (strcmp(interface, "wl_drm") == 0) {
		add_listeners(d,id);
	} else if (strcmp(interface, "android_wlegl") == 0) {
		add_listeners(d,id);
	} else if (strcmp(interface, "wl_seat") == 0) {
		d->seat = wl_registry_bind(d->registry,
					   id, &wl_seat_interface, 1);
		wl_seat_add_listener(d->seat, &seat_listener, d);
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

static struct display *
create_display(void)
{
	struct display *display;

	display = malloc(sizeof *display);
	memset(display, 0, sizeof *display);
	display->display = wl_display_connect(NULL);
	assert(display->display);
	display->registry = wl_display_get_registry(display->display);
	wl_registry_add_listener(display->registry,
				 &registry_listener, display);

	return display;
}

static void
destroy_display(struct display *display)
{
	module_destroy_display(display);

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
	if (strncmp(buf, "YUV4MPEG2 ", 10) != 0) {
		fprintf(stderr, "yuv stream: missing YUV4MPEG2 header\n");
		return NULL;
	}
	frame_duration = 0;
	width = 0;
	height = 0;
	for (p = buf + 9; *p; p = strchrnul(p, ' ')) {
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

	if (width <= 0 || height <= 0) {
		fprintf(stderr, "yuv stream: no width(%d) or height(%d)\n",width,height);
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
	window->fullscreen = 1;
	wl_shell_surface_set_fullscreen(window->shell_surface,
					WL_SHELL_SURFACE_FULLSCREEN_METHOD_SCALE,
					0, NULL);
}

static void
signal_int(int signum)
{
	g_run = 0;
}

int
main(int argc, char **argv)
{
	struct display *display;
	struct window *window;
	uint32_t format = fourcc_code('Y','U','1','2');
	FILE *source = NULL;
	int i, fullscreen = 0;
	struct sigaction sigint;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--yuyv") == 0)
			format = fourcc_code('Y','U','Y','V');
		if (strcmp(argv[i], "--yuv") == 0)
			format = fourcc_code('Y','U','1','2');
		if (strcmp(argv[i], "--nv12") == 0)
			format = fourcc_code('N','V','1','2');
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
		window = create_window(display, format, atoi(getenv("WIDTH")), atoi(getenv("HEIGHT")));
		window->paint = paint_pixels;
	}
	if (window == NULL) {
		fprintf(stderr,"error, window is NULL\n");
		destroy_display(display);
		exit(1);
	}

	if (fullscreen)
		fullscreen_window(window);

	redraw(window, NULL, 0);

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	int ret=0;
	while (g_run && ret!=-1) {
		printf("loop\n");
		ret = wl_display_dispatch(display->display);
	}
	printf("all done.\n");
	destroy_window(window);
	destroy_display(display);

	return 0;
}
