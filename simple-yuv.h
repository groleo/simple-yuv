#ifndef _SIMPLE_YUV_H_
#define _SIMPLE_YUV_H_

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
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
	struct wl_surface *keyboard_focus;
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
	int fullscreen;

	struct buffer *front, *back;
	uint32_t frame_duration, next_frame;
	uint32_t format;
	int has_timestamp;
	unsigned char *y, *u, *v;
	FILE *source;

	void (*paint)(struct window *window, uint32_t time);
};

#define fourcc_code(a,b,c,d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
			      ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

#endif /* _SIMPLE_YUV_H_ */
