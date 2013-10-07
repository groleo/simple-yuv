/* 
 * Copyright © 2012 Collabora, Ltd.
 * 
 * Permission to use, copy, modify, distribute, and sell this
 * software and its documentation for any purpose is hereby granted
 * without fee, provided that the above copyright notice appear in
 * all copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of
 * the copyright holders not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */

#ifndef ANDROID_CLIENT_PROTOCOL_H
#define ANDROID_CLIENT_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

struct wl_client;
struct wl_resource;

struct android_wlegl;
struct android_wlegl_handle;

extern const struct wl_interface android_wlegl_interface;
extern const struct wl_interface android_wlegl_handle_interface;

#ifndef ANDROID_WLEGL_ERROR_ENUM
#define ANDROID_WLEGL_ERROR_ENUM
enum android_wlegl_error {
	ANDROID_WLEGL_ERROR_BAD_HANDLE = 0,
	ANDROID_WLEGL_ERROR_BAD_VALUE = 1,
};
#endif /* ANDROID_WLEGL_ERROR_ENUM */

#ifndef ANDROID_WLEGL_FORMAT_ENUM
#define ANDROID_WLEGL_FORMAT_ENUM
enum android_wlegl_format {
	ANDROID_WLEGL_FORMAT_C8 = 0x20203843,
	ANDROID_WLEGL_FORMAT_RGB332 = 0x38424752,
	ANDROID_WLEGL_FORMAT_BGR233 = 0x38524742,
	ANDROID_WLEGL_FORMAT_XRGB4444 = 0x32315258,
	ANDROID_WLEGL_FORMAT_XBGR4444 = 0x32314258,
	ANDROID_WLEGL_FORMAT_RGBX4444 = 0x32315852,
	ANDROID_WLEGL_FORMAT_BGRX4444 = 0x32315842,
	ANDROID_WLEGL_FORMAT_ARGB4444 = 0x32315241,
	ANDROID_WLEGL_FORMAT_ABGR4444 = 0x32314241,
	ANDROID_WLEGL_FORMAT_RGBA4444 = 0x32314152,
	ANDROID_WLEGL_FORMAT_BGRA4444 = 0x32314142,
	ANDROID_WLEGL_FORMAT_XRGB1555 = 0x35315258,
	ANDROID_WLEGL_FORMAT_XBGR1555 = 0x35314258,
	ANDROID_WLEGL_FORMAT_RGBX5551 = 0x35315852,
	ANDROID_WLEGL_FORMAT_BGRX5551 = 0x35315842,
	ANDROID_WLEGL_FORMAT_ARGB1555 = 0x35315241,
	ANDROID_WLEGL_FORMAT_ABGR1555 = 0x35314241,
	ANDROID_WLEGL_FORMAT_RGBA5551 = 0x35314152,
	ANDROID_WLEGL_FORMAT_BGRA5551 = 0x35314142,
	ANDROID_WLEGL_FORMAT_RGB565 = 0x36314752,
	ANDROID_WLEGL_FORMAT_BGR565 = 0x36314742,
	ANDROID_WLEGL_FORMAT_RGB888 = 0x34324752,
	ANDROID_WLEGL_FORMAT_BGR888 = 0x34324742,
	ANDROID_WLEGL_FORMAT_XRGB8888 = 0x34325258,
	ANDROID_WLEGL_FORMAT_XBGR8888 = 0x34324258,
	ANDROID_WLEGL_FORMAT_RGBX8888 = 0x34325852,
	ANDROID_WLEGL_FORMAT_BGRX8888 = 0x34325842,
	ANDROID_WLEGL_FORMAT_ARGB8888 = 0x34325241,
	ANDROID_WLEGL_FORMAT_ABGR8888 = 0x34324241,
	ANDROID_WLEGL_FORMAT_RGBA8888 = 0x34324152,
	ANDROID_WLEGL_FORMAT_BGRA8888 = 0x34324142,
	ANDROID_WLEGL_FORMAT_XRGB2101010 = 0x30335258,
	ANDROID_WLEGL_FORMAT_XBGR2101010 = 0x30334258,
	ANDROID_WLEGL_FORMAT_RGBX1010102 = 0x30335852,
	ANDROID_WLEGL_FORMAT_BGRX1010102 = 0x30335842,
	ANDROID_WLEGL_FORMAT_ARGB2101010 = 0x30335241,
	ANDROID_WLEGL_FORMAT_ABGR2101010 = 0x30334241,
	ANDROID_WLEGL_FORMAT_RGBA1010102 = 0x30334152,
	ANDROID_WLEGL_FORMAT_BGRA1010102 = 0x30334142,
	ANDROID_WLEGL_FORMAT_YUYV = 0x56595559,
	ANDROID_WLEGL_FORMAT_YVYU = 0x55595659,
	ANDROID_WLEGL_FORMAT_UYVY = 0x59565955,
	ANDROID_WLEGL_FORMAT_VYUY = 0x59555956,
	ANDROID_WLEGL_FORMAT_AYUV = 0x56555941,
	ANDROID_WLEGL_FORMAT_NV12 = 0x3231564e,
	ANDROID_WLEGL_FORMAT_NV21 = 0x3132564e,
	ANDROID_WLEGL_FORMAT_NV16 = 0x3631564e,
	ANDROID_WLEGL_FORMAT_NV61 = 0x3136564e,
	ANDROID_WLEGL_FORMAT_YUV410 = 0x39565559,
	ANDROID_WLEGL_FORMAT_YVU410 = 0x39555659,
	ANDROID_WLEGL_FORMAT_YUV411 = 0x31315559,
	ANDROID_WLEGL_FORMAT_YVU411 = 0x31315659,
	ANDROID_WLEGL_FORMAT_YUV420 = 0x32315559,
	ANDROID_WLEGL_FORMAT_YVU420 = 0x32315659,
	ANDROID_WLEGL_FORMAT_YUV422 = 0x36315559,
	ANDROID_WLEGL_FORMAT_YVU422 = 0x36315659,
	ANDROID_WLEGL_FORMAT_YUV444 = 0x34325559,
	ANDROID_WLEGL_FORMAT_YVU444 = 0x34325659,
};
#endif /* ANDROID_WLEGL_FORMAT_ENUM */

/**
 * android_wlegl - Android EGL graphics buffer support
 * @format: (none)
 *
 * Interface used in the Android wrapper libEGL to share graphics buffers
 * between the server and the client.
 */
struct android_wlegl_listener {
	/**
	 * format - (none)
	 * @format: (none)
	 */
	void (*format)(void *data,
		       struct android_wlegl *android_wlegl,
		       uint32_t format);
};

static inline int
android_wlegl_add_listener(struct android_wlegl *android_wlegl,
			   const struct android_wlegl_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) android_wlegl,
				     (void (**)(void)) listener, data);
}

#define ANDROID_WLEGL_CREATE_HANDLE	0
#define ANDROID_WLEGL_CREATE_BUFFER	1

static inline void
android_wlegl_set_user_data(struct android_wlegl *android_wlegl, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) android_wlegl, user_data);
}

static inline void *
android_wlegl_get_user_data(struct android_wlegl *android_wlegl)
{
	return wl_proxy_get_user_data((struct wl_proxy *) android_wlegl);
}

static inline void
android_wlegl_destroy(struct android_wlegl *android_wlegl)
{
	wl_proxy_destroy((struct wl_proxy *) android_wlegl);
}

static inline struct android_wlegl_handle *
android_wlegl_create_handle(struct android_wlegl *android_wlegl, int32_t num_fds, struct wl_array *ints)
{
	struct wl_proxy *id;

	id = wl_proxy_create((struct wl_proxy *) android_wlegl,
			     &android_wlegl_handle_interface);
	if (!id)
		return NULL;

	wl_proxy_marshal((struct wl_proxy *) android_wlegl,
			 ANDROID_WLEGL_CREATE_HANDLE, id, num_fds, ints);

	return (struct android_wlegl_handle *) id;
}

static inline struct wl_buffer *
android_wlegl_create_buffer(struct android_wlegl *android_wlegl, int32_t width, int32_t height, int32_t stride, int32_t format, int32_t usage, struct android_wlegl_handle *native_handle)
{
	struct wl_proxy *id;

	id = wl_proxy_create((struct wl_proxy *) android_wlegl,
			     &wl_buffer_interface);
	if (!id)
		return NULL;

	wl_proxy_marshal((struct wl_proxy *) android_wlegl,
			 ANDROID_WLEGL_CREATE_BUFFER, id, width, height, stride, format, usage, native_handle);

	return (struct wl_buffer *) id;
}

#ifndef ANDROID_WLEGL_HANDLE_ERROR_ENUM
#define ANDROID_WLEGL_HANDLE_ERROR_ENUM
enum android_wlegl_handle_error {
	ANDROID_WLEGL_HANDLE_ERROR_TOO_MANY_FDS = 0,
};
#endif /* ANDROID_WLEGL_HANDLE_ERROR_ENUM */

#define ANDROID_WLEGL_HANDLE_ADD_FD	0
#define ANDROID_WLEGL_HANDLE_DESTROY	1

static inline void
android_wlegl_handle_set_user_data(struct android_wlegl_handle *android_wlegl_handle, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) android_wlegl_handle, user_data);
}

static inline void *
android_wlegl_handle_get_user_data(struct android_wlegl_handle *android_wlegl_handle)
{
	return wl_proxy_get_user_data((struct wl_proxy *) android_wlegl_handle);
}

static inline void
android_wlegl_handle_add_fd(struct android_wlegl_handle *android_wlegl_handle, int32_t fd)
{
	wl_proxy_marshal((struct wl_proxy *) android_wlegl_handle,
			 ANDROID_WLEGL_HANDLE_ADD_FD, fd);
}

static inline void
android_wlegl_handle_destroy(struct android_wlegl_handle *android_wlegl_handle)
{
	wl_proxy_marshal((struct wl_proxy *) android_wlegl_handle,
			 ANDROID_WLEGL_HANDLE_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) android_wlegl_handle);
}

#ifdef  __cplusplus
}
#endif

#endif
