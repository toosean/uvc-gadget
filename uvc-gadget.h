/*
 *	uvc_gadget.h  --  USB Video Class Gadget driver
 *
 *	Copyright (C) 2009-2010
 *	    Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 */

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/usb/ch9.h>

#define WIDTH1  640
#define HEIGHT1 360

#define WIDTH2	1920
#define HEIGHT2 1080

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define BITSET(x, mask) (x |= mask)
#define BITCLEAR(x, mask) (x &= ~(mask))

#define STATE_OK 0
#define STATE_FAILED 1

#define clamp(val, min, max)                        \
    ({                                              \
        typeof(val) __val = (val);                  \
        typeof(min) __min = (min);                  \
        typeof(max) __max = (max);                  \
        (void)(&__val == &__min);                   \
        (void)(&__val == &__max);                   \
        __val = __val < __min ? __min : __val;      \
        __val > __max ? __max : __val;              \
    })

#define ARRAY_SIZE(a) ((sizeof(a) / sizeof(a[0])))
#define pixfmtstr(x) (x) & 0xff, ((x) >> 8) & 0xff, ((x) >> 16) & 0xff, ((x) >> 24) & 0xff

#define UVC_EVENT_FIRST        (V4L2_EVENT_PRIVATE_START + 0)
#define UVC_EVENT_CONNECT      (V4L2_EVENT_PRIVATE_START + 0)
#define UVC_EVENT_DISCONNECT   (V4L2_EVENT_PRIVATE_START + 1)
#define UVC_EVENT_STREAMON     (V4L2_EVENT_PRIVATE_START + 2)
#define UVC_EVENT_STREAMOFF    (V4L2_EVENT_PRIVATE_START + 3)
#define UVC_EVENT_SETUP	       (V4L2_EVENT_PRIVATE_START + 4)
#define UVC_EVENT_DATA         (V4L2_EVENT_PRIVATE_START + 5)
#define UVC_EVENT_LAST         (V4L2_EVENT_PRIVATE_START + 5)

struct uvc_request_data
{
	__s32 length;
	__u8 data[60];
};

struct uvc_event
{
	union {
		enum usb_device_speed speed;
		struct usb_ctrlrequest req;
		struct uvc_request_data data;
	};
};

#define UVCIOC_SEND_RESPONSE		_IOW('U', 1, struct uvc_request_data)

#define UVC_INTF_CONTROL		0
#define UVC_INTF_STREAMING		1

// UVC - Request Error Code Control
#define REQEC_NO_ERROR 0x00
#define REQEC_NOT_READY 0x01
#define REQEC_WRONG_STATE 0x02
#define REQEC_POWER 0x03
#define REQEC_OUT_OF_RANGE 0x04
#define REQEC_INVALID_UNIT 0x05
#define REQEC_INVALID_CONTROL 0x06
#define REQEC_INVALID_REQUEST 0x07
#define REQEC_INVALID_VALUE 0x08

/* IO methods supported */
enum io_method {
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
};

/* Buffer representing one video frame */
struct buffer {
    struct v4l2_buffer buf;
    void *start;
    size_t length;
};

/* ---------------------------------------------------------------------------
 * UVC specific stuff
 */

struct uvc_frame_info {
    unsigned int width;
    unsigned int height;
    unsigned int intervals[8];
};

struct uvc_format_info {
    unsigned int fcc;
    const struct uvc_frame_info *frames;
};

/*
 * USB Device Class Definition for Video Devices - FAQ
 * Revision 1.1 
 * Question: How is the video frame interval (used in various payload frame descriptors and VS
 * interface controls) derived from the frame rate?
 *
 * Answer: The video frame interval is specified in 100 ns units and is derived from the frame rate
 * as follows: For a frame rate x, the video frame interval is (10,000,000/x) truncated to an integer
 * value.
 * For example:
 * 15 fps: Frame interval = (10000000/15) = 666666
 * 30 fps: Frame interval = (10000000/30) = 333333
 * 25 fps (PAL): Frame interval = (10000000/25) = 400000
 * 29.97 fps (NTSC): Frame interval = (10000000/29.97) = 333667
 */

static const struct uvc_frame_info uvc_frames_yuyv[] = {
    { WIDTH1, HEIGHT1, {666666, 400000, 333333, 0}, },
    { WIDTH2, HEIGHT2, {666666, 400000, 333333, 0}, },
    { 0, 0, {0,}, },
};

static const struct uvc_frame_info uvc_frames_mjpeg[] = {
    { WIDTH1, HEIGHT1, {666666, 400000, 333333, 0}, },
    { WIDTH2, HEIGHT2, {666666, 400000, 333333, 0}, },
    { 0, 0, { 0, }, },
};

static const struct uvc_format_info uvc_formats[] = {
    {V4L2_PIX_FMT_YUYV, uvc_frames_yuyv},
    {V4L2_PIX_FMT_MJPEG, uvc_frames_mjpeg},
};

/* ---------------------------------------------------------------------------
 * V4L2 and UVC device instances
 */

/* device type */
enum device_type {
    DEVICE_TYPE_UVC,
    DEVICE_TYPE_V4L2,
};

/* Represents a V4L2 based video capture device */
struct v4l2_device {
    enum device_type device_type;
    const char * device_type_name;

    /* v4l2 device specific */
    int fd;
    int is_streaming;

    /* v4l2 buffer specific */
    enum io_method io;
    struct buffer *mem;
    unsigned int nbufs;

    /* v4l2 buffer queue and dequeue counters */
    unsigned long long int qbuf_count;
    unsigned long long int dqbuf_count;

    /* uvc specific */
    int run_standalone;
    int control;
    struct uvc_streaming_control probe;
    struct uvc_streaming_control commit;
    unsigned char request_error_code;
    unsigned int control_interface;
    unsigned int control_type;

    /* uvc buffer specific */
    struct buffer *dummy_buf;
    unsigned int fcc;
    unsigned int width;
    unsigned int height;

    uint8_t color;
    unsigned int imgsize;
    void *imgdata;

    /* uvc specific flags */
    int first_buffer_queued;
    int uvc_shutdown_requested;

    /* v4l2 device hook */
    struct v4l2_device *vdev;
    
    /* uvc device hook */
    struct v4l2_device *udev;
};

struct uvc_settings {
    char * uvc_devname;
    char * v4l2_devname;
    enum io_method uvc_io_method;
    char * mjpeg_image;
    int default_format;
    unsigned int nbufs;
    int default_resolution;

    /* USB speed specific */
    unsigned int usb_mult;
    unsigned int usb_burst;
    unsigned int usb_maxpkt;
    enum usb_device_speed usb_speed;
};

struct uvc_settings settings = {
    .uvc_devname = "/dev/video0",
    .v4l2_devname = "/dev/video1",
    .uvc_io_method = IO_METHOD_USERPTR,
    .mjpeg_image = NULL,
    .default_format = V4L2_PIX_FMT_YUYV,
    .nbufs = 2,
    .default_resolution = 0,

    .usb_mult = 0,
    .usb_burst = 0,
    .usb_speed = USB_SPEED_SUPER
};

struct control_mapping_pair {
    unsigned int type;
    unsigned int uvc;
    const char * uvc_name;
    unsigned int v4l2;
    const char * v4l2_name;
    bool enabled;
    unsigned int control_type;
    unsigned int value;
    unsigned int length;
    unsigned int minimum;
    unsigned int maximum;
    unsigned int step;
    unsigned int default_value;
    int v4l2_minimum;
    int v4l2_maximum;
};

struct control_mapping_pair control_mapping[] = {
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_BACKLIGHT_COMPENSATION_CONTROL,
		.uvc_name = "UVC_PU_BACKLIGHT_COMPENSATION_CONTROL",
        .v4l2 = V4L2_CID_BACKLIGHT_COMPENSATION,
        .v4l2_name = "V4L2_CID_BACKLIGHT_COMPENSATION",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_BRIGHTNESS_CONTROL,
		.uvc_name = "UVC_PU_BRIGHTNESS_CONTROL",
        .v4l2 = V4L2_CID_BRIGHTNESS,
        .v4l2_name = "V4L2_CID_BRIGHTNESS"
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_CONTRAST_CONTROL,
		.uvc_name = "UVC_PU_CONTRAST_CONTROL",
        .v4l2 = V4L2_CID_CONTRAST,
        .v4l2_name = "V4L2_CID_CONTRAST",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_GAIN_CONTROL,
		.uvc_name = "UVC_PU_GAIN_CONTROL",
        .v4l2 = V4L2_CID_GAIN,
        .v4l2_name = "V4L2_CID_GAIN",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_POWER_LINE_FREQUENCY_CONTROL,
		.uvc_name = "UVC_PU_POWER_LINE_FREQUENCY_CONTROL",
        .v4l2 = V4L2_CID_POWER_LINE_FREQUENCY,
        .v4l2_name = "V4L2_CID_POWER_LINE_FREQUENCY",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_HUE_CONTROL,
		.uvc_name = "UVC_PU_HUE_CONTROL",
        .v4l2 = V4L2_CID_HUE,
        .v4l2_name = "V4L2_CID_HUE",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_SATURATION_CONTROL,
		.uvc_name = "UVC_PU_SATURATION_CONTROL",
        .v4l2 = V4L2_CID_SATURATION,
        .v4l2_name = "V4L2_CID_SATURATION",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_SHARPNESS_CONTROL,
		.uvc_name = "UVC_PU_SHARPNESS_CONTROL",
        .v4l2 = V4L2_CID_SHARPNESS,
        .v4l2_name = "V4L2_CID_SHARPNESS",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_GAMMA_CONTROL,
		.uvc_name = "UVC_PU_GAMMA_CONTROL",
        .v4l2 = V4L2_CID_GAMMA,
        .v4l2_name = "V4L2_CID_GAMMA",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL,
		.uvc_name = "UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL",
        .v4l2 = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
        .v4l2_name = "V4L2_CID_WHITE_BALANCE_TEMPERATURE",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL,
		.uvc_name = "UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL,
		.uvc_name = "UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL",
        .v4l2 = V4L2_CID_RED_BALANCE,
        .v4l2_name = "V4L2_CID_RED_BALANCE + V4L2_CID_BLUE_BALANCE"
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL,
		.uvc_name = "UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL",
        .v4l2 = V4L2_CID_AUTO_WHITE_BALANCE,
        .v4l2_name = "V4L2_CID_AUTO_WHITE_BALANCE",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_DIGITAL_MULTIPLIER_CONTROL,
		.uvc_name = "UVC_PU_DIGITAL_MULTIPLIER_CONTROL",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL,
		.uvc_name = "UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_HUE_AUTO_CONTROL,
		.uvc_name = "UVC_PU_HUE_AUTO_CONTROL",
        .v4l2 = V4L2_CID_HUE_AUTO,
        .v4l2_name = "V4L2_CID_HUE_AUTO",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL,
		.uvc_name = "UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL",
	},
	{
        .type = UVC_VC_PROCESSING_UNIT,
		.uvc = UVC_PU_ANALOG_LOCK_STATUS_CONTROL,
		.uvc_name = "UVC_PU_ANALOG_LOCK_STATUS_CONTROL",
	},
	// {
    //     .type = UVC_VC_PROCESSING_UNIT,
	// 	.uvc = UVC_PU_CONTRAST_AUTO_CONTROL,
	// 	.uvc_name = "UVC_PU_CONTRAST_AUTO_CONTROL",
	// },
    {
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_CONTROL_UNDEFINED,
		.uvc_name = "UVC_CT_CONTROL_UNDEFINED",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_SCANNING_MODE_CONTROL,
		.uvc_name = "UVC_CT_SCANNING_MODE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_AE_MODE_CONTROL,
		.uvc_name = "UVC_CT_AE_MODE_CONTROL",
        .v4l2 = V4L2_CID_EXPOSURE_AUTO,
        .v4l2_name = "V4L2_CID_EXPOSURE_AUTO"
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_AE_PRIORITY_CONTROL,
		.uvc_name = "UVC_CT_AE_PRIORITY_CONTROL",
        .v4l2 = V4L2_CID_EXPOSURE_AUTO_PRIORITY,
        .v4l2_name = "V4L2_CID_EXPOSURE_AUTO_PRIORITY"
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL,
		.uvc_name = "UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL",
        .v4l2 = V4L2_CID_EXPOSURE_ABSOLUTE,
        .v4l2_name = "V4L2_CID_EXPOSURE_ABSOLUTE"
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL,
		.uvc_name = "UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_FOCUS_ABSOLUTE_CONTROL,
		.uvc_name = "UVC_CT_FOCUS_ABSOLUTE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_FOCUS_RELATIVE_CONTROL,
		.uvc_name = "UVC_CT_FOCUS_RELATIVE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_FOCUS_AUTO_CONTROL,
		.uvc_name = "UVC_CT_FOCUS_AUTO_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_IRIS_ABSOLUTE_CONTROL,
		.uvc_name = "UVC_CT_IRIS_ABSOLUTE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_IRIS_RELATIVE_CONTROL,
		.uvc_name = "UVC_CT_IRIS_RELATIVE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_ZOOM_ABSOLUTE_CONTROL,
		.uvc_name = "UVC_CT_ZOOM_ABSOLUTE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_ZOOM_RELATIVE_CONTROL,
		.uvc_name = "UVC_CT_ZOOM_RELATIVE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_PANTILT_ABSOLUTE_CONTROL,
		.uvc_name = "UVC_CT_PANTILT_ABSOLUTE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_PANTILT_RELATIVE_CONTROL,
		.uvc_name = "UVC_CT_PANTILT_RELATIVE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_ROLL_ABSOLUTE_CONTROL,
		.uvc_name = "UVC_CT_ROLL_ABSOLUTE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_ROLL_RELATIVE_CONTROL,
		.uvc_name = "UVC_CT_ROLL_RELATIVE_CONTROL",
	},
	{
        .type = UVC_VC_INPUT_TERMINAL,
		.uvc = UVC_CT_PRIVACY_CONTROL,
		.uvc_name = "UVC_CT_PRIVACY_CONTROL",
	}
};

int control_mapping_size = sizeof(control_mapping) / sizeof(*control_mapping);
