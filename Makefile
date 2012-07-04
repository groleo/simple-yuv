CFLAGS = $(shell pkg-config --cflags wayland-client libdrm_intel) -Wall -g
LDFLAGS = $(shell pkg-config --libs wayland-client libdrm_intel)

all : simple-yuv webcam

simple-yuv : simple-yuv.o wayland-drm-protocol.o

webcam : webcam.o

clean :
	rm *.o simple-yuv webcam
