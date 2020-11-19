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

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define max(a, b) (((a) > (b)) ? (a) : (b))

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

enum gpio {
    GPIO_EXPORT = 0,
    GPIO_DIRECTION,
    GPIO_VALUE,
};

#define GPIO_DIRECTION_OUT "out"
#define GPIO_DIRECTION_IN "in"
#define GPIO_DIRECTION_LOW "low"
#define GPIO_DIRECTION_HIGH "high"

#define GPIO_VALUE_OFF "0"
#define GPIO_VALUE_ON "1"

enum leds {
    LED_TRIGGER = 1,
    LED_BRIGHTNESS,
};

#define LED_TRIGGER_NONE "none"
#define LED_BRIGHTNESS_LOW "0"
#define LED_BRIGHTNESS_HIGH "1"

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

enum video_stream_action {
    STREAM_OFF,
    STREAM_ON,
};

enum stream_control_action {
    STREAM_CONTROL_INIT,
    STREAM_CONTROL_MIN,
    STREAM_CONTROL_MAX,
    STREAM_CONTROL_SET,
};

/* Buffer representing one video frame */
struct buffer {
    struct v4l2_buffer buf;
    void * start;
    size_t length;
};

/* ---------------------------------------------------------------------------
 * UVC specific stuff
 */

struct uvc_frame_format {
    bool defined;

    enum usb_device_speed usb_speed;
    int video_format;
    const char * format_name;

    unsigned int bFormatIndex;
    unsigned int bFrameIndex;

    unsigned int dwDefaultFrameInterval;
    unsigned int dwMaxVideoFrameBufferSize;
    unsigned int dwMaxBitRate;
    unsigned int dwMinBitRate;
    unsigned int wHeight;
    unsigned int wWidth;
    unsigned int bmCapabilities;
    
    unsigned int dwFrameInterval;
};

int last_format_index = 0;

struct uvc_frame_format uvc_frame_format[30];

enum uvc_frame_format_getter {
    FORMAT_INDEX_MIN,
    FORMAT_INDEX_MAX,
    FRAME_INDEX_MIN,
    FRAME_INDEX_MAX,
};

unsigned int streaming_maxburst = 0;
unsigned int streaming_maxpacket = 1023;
unsigned int streaming_interval = 1;

/* ---------------------------------------------------------------------------
 * V4L2 and UVC device instances
 */

bool uvc_shutdown_requested = false;

/* device type */
enum device_type {
    DEVICE_TYPE_UVC,
    DEVICE_TYPE_V4L2,
    DEVICE_TYPE_FRAMEBUFFER,
};

/* Represents a V4L2 based video capture device */
struct v4l2_device {
    enum device_type device_type;
    const char * device_type_name;

    /* v4l2 device specific */
    int fd;
    int is_streaming;

    /* v4l2 buffer specific */
    struct buffer * mem;
    unsigned int nbufs;
    unsigned int buffer_type;
    unsigned int memory_type;

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

    /* uvc specific flags */
    int uvc_shutdown_requested;

    struct buffer * dummy_buf;

    /* fb specific */
    unsigned int fb_screen_size;
    unsigned int fb_mem_size;
    unsigned int fb_width;
    unsigned int fb_height;
    unsigned int fb_bpp;
    unsigned int fb_line_length;
    void * fb_memory;

    double last_time_video_process;
    int buffers_processed;
};

static struct v4l2_device v4l2_dev;
static struct v4l2_device uvc_dev;
static struct v4l2_device fb_dev;

struct uvc_settings {
    char * uvc_devname;
    char * v4l2_devname;
    char * fb_devname;
    enum device_type source_device;
    unsigned int nbufs;
    bool show_fps;
    bool fb_grayscale;
    unsigned int fb_framerate;
    bool streaming_status_onboard;
    bool streaming_status_onboard_enabled;
    char * streaming_status_pin;
    bool streaming_status_enabled;
};

struct uvc_settings settings = {
    .uvc_devname = "/dev/video1",
    .v4l2_devname = "/dev/video0",
    .source_device = DEVICE_TYPE_V4L2,
    .nbufs = 2,
    .fb_framerate = 25,
    .fb_grayscale = false,
    .show_fps = false,
    .streaming_status_onboard = false,
    .streaming_status_onboard_enabled = false,
    .streaming_status_enabled = false,
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

int control_mapping_size = sizeof(control_mapping) / sizeof(* control_mapping);

/*
 * RGB to YUYV conversion 
 */

unsigned int red_66[256] = { 0, 66, 132, 198, 264, 330, 396, 462, 528, 594, 660, 726, 792, 858, 924,
    990, 1056, 1122, 1188, 1254, 1320, 1386, 1452, 1518, 1584, 1650, 1716, 1782, 1848, 1914, 1980,
    2046, 2112, 2178, 2244, 2310, 2376, 2442, 2508, 2574, 2640, 2706, 2772, 2838, 2904, 2970, 3036,
    3102, 3168, 3234, 3300, 3366, 3432, 3498, 3564, 3630, 3696, 3762, 3828, 3894, 3960, 4026, 4092,
    4158, 4224, 4290, 4356, 4422, 4488, 4554, 4620, 4686, 4752, 4818, 4884, 4950, 5016, 5082, 5148,
    5214, 5280, 5346, 5412, 5478, 5544, 5610, 5676, 5742, 5808, 5874, 5940, 6006, 6072, 6138, 6204,
    6270, 6336, 6402, 6468, 6534, 6600, 6666, 6732, 6798, 6864, 6930, 6996, 7062, 7128, 7194, 7260,
    7326, 7392, 7458, 7524, 7590, 7656, 7722, 7788, 7854, 7920, 7986, 8052, 8118, 8184, 8250, 8316,
    8382, 8448, 8514, 8580, 8646, 8712, 8778, 8844, 8910, 8976, 9042, 9108, 9174, 9240, 9306, 9372,
    9438, 9504, 9570, 9636, 9702, 9768, 9834, 9900, 9966, 10032, 10098, 10164, 10230, 10296, 10362,
    10428, 10494, 10560, 10626, 10692, 10758, 10824, 10890, 10956, 11022, 11088, 11154, 11220, 11286,
    11352, 11418, 11484, 11550, 11616, 11682, 11748, 11814, 11880, 11946, 12012, 12078, 12144, 12210,
    12276, 12342, 12408, 12474, 12540, 12606, 12672, 12738, 12804, 12870, 12936, 13002, 13068, 13134,
    13200, 13266, 13332, 13398, 13464, 13530, 13596, 13662, 13728, 13794, 13860, 13926, 13992, 14058,
    14124, 14190, 14256, 14322, 14388, 14454, 14520, 14586, 14652, 14718, 14784, 14850, 14916, 14982,
    15048, 15114, 15180, 15246, 15312, 15378, 15444, 15510, 15576, 15642, 15708, 15774, 15840, 15906,
    15972, 16038, 16104, 16170, 16236, 16302, 16368, 16434, 16500, 16566, 16632, 16698, 16764, 16830
};

unsigned int green_129[256] = {0, 129, 258, 387, 516, 645, 774, 903, 1032, 1161, 1290, 1419, 1548,
    1677, 1806, 1935, 2064, 2193, 2322, 2451, 2580, 2709, 2838, 2967, 3096, 3225, 3354, 3483, 3612,
    3741, 3870, 3999, 4128, 4257, 4386, 4515, 4644, 4773, 4902, 5031, 5160, 5289, 5418, 5547, 5676,
    5805, 5934, 6063, 6192, 6321, 6450, 6579, 6708, 6837, 6966, 7095, 7224, 7353, 7482, 7611, 7740,
    7869, 7998, 8127, 8256, 8385, 8514, 8643, 8772, 8901, 9030, 9159, 9288, 9417, 9546, 9675, 9804,
    9933, 10062, 10191, 10320, 10449, 10578, 10707, 10836, 10965, 11094, 11223, 11352, 11481, 11610,
    11739, 11868, 11997, 12126, 12255, 12384, 12513, 12642, 12771, 12900, 13029, 13158, 13287, 13416,
    13545, 13674, 13803, 13932, 14061, 14190, 14319, 14448, 14577, 14706, 14835, 14964, 15093, 15222,
    15351, 15480, 15609, 15738, 15867, 15996, 16125, 16254, 16383, 16512, 16641, 16770, 16899, 17028,
    17157, 17286, 17415, 17544, 17673, 17802, 17931, 18060, 18189, 18318, 18447, 18576, 18705, 18834,
    18963, 19092, 19221, 19350, 19479, 19608, 19737, 19866, 19995, 20124, 20253, 20382, 20511, 20640,
    20769, 20898, 21027, 21156, 21285, 21414, 21543, 21672, 21801, 21930, 22059, 22188, 22317, 22446,
    22575, 22704, 22833, 22962, 23091, 23220, 23349, 23478, 23607, 23736, 23865, 23994, 24123, 24252,
    24381, 24510, 24639, 24768, 24897, 25026, 25155, 25284, 25413, 25542, 25671, 25800, 25929, 26058,
    26187, 26316, 26445, 26574, 26703, 26832, 26961, 27090, 27219, 27348, 27477, 27606, 27735, 27864,
    27993, 28122, 28251, 28380, 28509, 28638, 28767, 28896, 29025, 29154, 29283, 29412, 29541, 29670,
    29799, 29928, 30057, 30186, 30315, 30444, 30573, 30702, 30831, 30960, 31089, 31218, 31347, 31476,
    31605, 31734, 31863, 31992, 32121, 32250, 32379, 32508, 32637, 32766, 32895
};

unsigned int blue_25[256] = {128, 153, 178, 203, 228, 253, 278, 303, 328, 353, 378, 403, 428, 453,
    478, 503, 528, 553, 578, 603, 628, 653, 678, 703, 728, 753, 778, 803, 828, 853, 878, 903, 928,
    953, 978, 1003, 1028, 1053, 1078, 1103, 1128, 1153, 1178, 1203, 1228, 1253, 1278, 1303, 1328,
    1353, 1378, 1403, 1428, 1453, 1478, 1503, 1528, 1553, 1578, 1603, 1628, 1653, 1678, 1703, 1728,
    1753, 1778, 1803, 1828, 1853, 1878, 1903, 1928, 1953, 1978, 2003, 2028, 2053, 2078, 2103, 2128,
    2153, 2178, 2203, 2228, 2253, 2278, 2303, 2328, 2353, 2378, 2403, 2428, 2453, 2478, 2503, 2528,
    2553, 2578, 2603, 2628, 2653, 2678, 2703, 2728, 2753, 2778, 2803, 2828, 2853, 2878, 2903, 2928,
    2953, 2978, 3003, 3028, 3053, 3078, 3103, 3128, 3153, 3178, 3203, 3228, 3253, 3278, 3303, 3328,
    3353, 3378, 3403, 3428, 3453, 3478, 3503, 3528, 3553, 3578, 3603, 3628, 3653, 3678, 3703, 3728,
    3753, 3778, 3803, 3828, 3853, 3878, 3903, 3928, 3953, 3978, 4003, 4028, 4053, 4078, 4103, 4128,
    4153, 4178, 4203, 4228, 4253, 4278, 4303, 4328, 4353, 4378, 4403, 4428, 4453, 4478, 4503, 4528,
    4553, 4578, 4603, 4628, 4653, 4678, 4703, 4728, 4753, 4778, 4803, 4828, 4853, 4878, 4903, 4928,
    4953, 4978, 5003, 5028, 5053, 5078, 5103, 5128, 5153, 5178, 5203, 5228, 5253, 5278, 5303, 5328,
    5353, 5378, 5403, 5428, 5453, 5478, 5503, 5528, 5553, 5578, 5603, 5628, 5653, 5678, 5703, 5728,
    5753, 5778, 5803, 5828, 5853, 5878, 5903, 5928, 5953, 5978, 6003, 6028, 6053, 6078, 6103, 6128,
    6153, 6178, 6203, 6228, 6253, 6278, 6303, 6328, 6353, 6378, 6403, 6428, 6453, 6478, 6503
};

unsigned int red_38[256] = {0, 38, 76, 114, 152, 190, 228, 266, 304, 342, 380, 418, 456, 494, 532,
    570, 608, 646, 684, 722, 760, 798, 836, 874, 912, 950, 988, 1026, 1064, 1102, 1140, 1178, 1216,
    1254, 1292, 1330, 1368, 1406, 1444, 1482, 1520, 1558, 1596, 1634, 1672, 1710, 1748, 1786, 1824,
    1862, 1900, 1938, 1976, 2014, 2052, 2090, 2128, 2166, 2204, 2242, 2280, 2318, 2356, 2394, 2432,
    2470, 2508, 2546, 2584, 2622, 2660, 2698, 2736, 2774, 2812, 2850, 2888, 2926, 2964, 3002, 3040,
    3078, 3116, 3154, 3192, 3230, 3268, 3306, 3344, 3382, 3420, 3458, 3496, 3534, 3572, 3610, 3648,
    3686, 3724, 3762, 3800, 3838, 3876, 3914, 3952, 3990, 4028, 4066, 4104, 4142, 4180, 4218, 4256,
    4294, 4332, 4370, 4408, 4446, 4484, 4522, 4560, 4598, 4636, 4674, 4712, 4750, 4788, 4826, 4864,
    4902, 4940, 4978, 5016, 5054, 5092, 5130, 5168, 5206, 5244, 5282, 5320, 5358, 5396, 5434, 5472,
    5510, 5548, 5586, 5624, 5662, 5700, 5738, 5776, 5814, 5852, 5890, 5928, 5966, 6004, 6042, 6080,
    6118, 6156, 6194, 6232, 6270, 6308, 6346, 6384, 6422, 6460, 6498, 6536, 6574, 6612, 6650, 6688,
    6726, 6764, 6802, 6840, 6878, 6916, 6954, 6992, 7030, 7068, 7106, 7144, 7182, 7220, 7258, 7296,
    7334, 7372, 7410, 7448, 7486, 7524, 7562, 7600, 7638, 7676, 7714, 7752, 7790, 7828, 7866, 7904,
    7942, 7980, 8018, 8056, 8094, 8132, 8170, 8208, 8246, 8284, 8322, 8360, 8398, 8436, 8474, 8512,
    8550, 8588, 8626, 8664, 8702, 8740, 8778, 8816, 8854, 8892, 8930, 8968, 9006, 9044, 9082, 9120,
    9158, 9196, 9234, 9272, 9310, 9348, 9386, 9424, 9462, 9500, 9538, 9576, 9614, 9652, 9690
};

unsigned int green_74[256] = {0, 74, 148, 222, 296, 370, 444, 518, 592, 666, 740, 814, 888, 962,
    1036, 1110, 1184, 1258, 1332, 1406, 1480, 1554, 1628, 1702, 1776, 1850, 1924, 1998, 2072, 2146,
    2220, 2294, 2368, 2442, 2516, 2590, 2664, 2738, 2812, 2886, 2960, 3034, 3108, 3182, 3256, 3330,
    3404, 3478, 3552, 3626, 3700, 3774, 3848, 3922, 3996, 4070, 4144, 4218, 4292, 4366, 4440, 4514,
    4588, 4662, 4736, 4810, 4884, 4958, 5032, 5106, 5180, 5254, 5328, 5402, 5476, 5550, 5624, 5698,
    5772, 5846, 5920, 5994, 6068, 6142, 6216, 6290, 6364, 6438, 6512, 6586, 6660, 6734, 6808, 6882,
    6956, 7030, 7104, 7178, 7252, 7326, 7400, 7474, 7548, 7622, 7696, 7770, 7844, 7918, 7992, 8066,
    8140, 8214, 8288, 8362, 8436, 8510, 8584, 8658, 8732, 8806, 8880, 8954, 9028, 9102, 9176, 9250,
    9324, 9398, 9472, 9546, 9620, 9694, 9768, 9842, 9916, 9990, 10064, 10138, 10212, 10286, 10360,
    10434, 10508, 10582, 10656, 10730, 10804, 10878, 10952, 11026, 11100, 11174, 11248, 11322, 11396,
    11470, 11544, 11618, 11692, 11766, 11840, 11914, 11988, 12062, 12136, 12210, 12284, 12358, 12432,
    12506, 12580, 12654, 12728, 12802, 12876, 12950, 13024, 13098, 13172, 13246, 13320, 13394, 13468,
    13542, 13616, 13690, 13764, 13838, 13912, 13986, 14060, 14134, 14208, 14282, 14356, 14430, 14504,
    14578, 14652, 14726, 14800, 14874, 14948, 15022, 15096, 15170, 15244, 15318, 15392, 15466, 15540,
    15614, 15688, 15762, 15836, 15910, 15984, 16058, 16132, 16206, 16280, 16354, 16428, 16502, 16576,
    16650, 16724, 16798, 16872, 16946, 17020, 17094, 17168, 17242, 17316, 17390, 17464, 17538, 17612,
    17686, 17760, 17834, 17908, 17982, 18056, 18130, 18204, 18278, 18352, 18426, 18500, 18574, 18648,
    18722, 18796, 18870
};

unsigned int blue_112[256] = {128, 240, 352, 464, 576, 688, 800, 912, 1024, 1136, 1248, 1360, 1472,
    1584, 1696, 1808, 1920, 2032, 2144, 2256, 2368, 2480, 2592, 2704, 2816, 2928, 3040, 3152, 3264,
    3376, 3488, 3600, 3712, 3824, 3936, 4048, 4160, 4272, 4384, 4496, 4608, 4720, 4832, 4944, 5056,
    5168, 5280, 5392, 5504, 5616, 5728, 5840, 5952, 6064, 6176, 6288, 6400, 6512, 6624, 6736, 6848,
    6960, 7072, 7184, 7296, 7408, 7520, 7632, 7744, 7856, 7968, 8080, 8192, 8304, 8416, 8528, 8640,
    8752, 8864, 8976, 9088, 9200, 9312, 9424, 9536, 9648, 9760, 9872, 9984, 10096, 10208, 10320,
    10432, 10544, 10656, 10768, 10880, 10992, 11104, 11216, 11328, 11440, 11552, 11664, 11776, 11888,
    12000, 12112, 12224, 12336, 12448, 12560, 12672, 12784, 12896, 13008, 13120, 13232, 13344, 13456,
    13568, 13680, 13792, 13904, 14016, 14128, 14240, 14352, 14464, 14576, 14688, 14800, 14912, 15024,
    15136, 15248, 15360, 15472, 15584, 15696, 15808, 15920, 16032, 16144, 16256, 16368, 16480, 16592,
    16704, 16816, 16928, 17040, 17152, 17264, 17376, 17488, 17600, 17712, 17824, 17936, 18048, 18160,
    18272, 18384, 18496, 18608, 18720, 18832, 18944, 19056, 19168, 19280, 19392, 19504, 19616, 19728,
    19840, 19952, 20064, 20176, 20288, 20400, 20512, 20624, 20736, 20848, 20960, 21072, 21184, 21296,
    21408, 21520, 21632, 21744, 21856, 21968, 22080, 22192, 22304, 22416, 22528, 22640, 22752, 22864,
    22976, 23088, 23200, 23312, 23424, 23536, 23648, 23760, 23872, 23984, 24096, 24208, 24320, 24432,
    24544, 24656, 24768, 24880, 24992, 25104, 25216, 25328, 25440, 25552, 25664, 25776, 25888, 26000,
    26112, 26224, 26336, 26448, 26560, 26672, 26784, 26896, 27008, 27120, 27232, 27344, 27456, 27568,
    27680, 27792, 27904, 28016, 28128, 28240, 28352, 28464, 28576, 28688
};

unsigned int red_112[256] = {0, 112, 224, 336, 448, 560, 672, 784, 896, 1008, 1120, 1232, 1344, 1456,
    1568, 1680, 1792, 1904, 2016, 2128, 2240, 2352, 2464, 2576, 2688, 2800, 2912, 3024, 3136, 3248,
    3360, 3472, 3584, 3696, 3808, 3920, 4032, 4144, 4256, 4368, 4480, 4592, 4704, 4816, 4928, 5040,
    5152, 5264, 5376, 5488, 5600, 5712, 5824, 5936, 6048, 6160, 6272, 6384, 6496, 6608, 6720, 6832,
    6944, 7056, 7168, 7280, 7392, 7504, 7616, 7728, 7840, 7952, 8064, 8176, 8288, 8400, 8512, 8624,
    8736, 8848, 8960, 9072, 9184, 9296, 9408, 9520, 9632, 9744, 9856, 9968, 10080, 10192, 10304,
    10416, 10528, 10640, 10752, 10864, 10976, 11088, 11200, 11312, 11424, 11536, 11648, 11760, 11872,
    11984, 12096, 12208, 12320, 12432, 12544, 12656, 12768, 12880, 12992, 13104, 13216, 13328, 13440,
    13552, 13664, 13776, 13888, 14000, 14112, 14224, 14336, 14448, 14560, 14672, 14784, 14896, 15008,
    15120, 15232, 15344, 15456, 15568, 15680, 15792, 15904, 16016, 16128, 16240, 16352, 16464, 16576,
    16688, 16800, 16912, 17024, 17136, 17248, 17360, 17472, 17584, 17696, 17808, 17920, 18032, 18144,
    18256, 18368, 18480, 18592, 18704, 18816, 18928, 19040, 19152, 19264, 19376, 19488, 19600, 19712,
    19824, 19936, 20048, 20160, 20272, 20384, 20496, 20608, 20720, 20832, 20944, 21056, 21168, 21280,
    21392, 21504, 21616, 21728, 21840, 21952, 22064, 22176, 22288, 22400, 22512, 22624, 22736, 22848,
    22960, 23072, 23184, 23296, 23408, 23520, 23632, 23744, 23856, 23968, 24080, 24192, 24304, 24416,
    24528, 24640, 24752, 24864, 24976, 25088, 25200, 25312, 25424, 25536, 25648, 25760, 25872, 25984,
    26096, 26208, 26320, 26432, 26544, 26656, 26768, 26880, 26992, 27104, 27216, 27328, 27440, 27552,
    27664, 27776, 27888, 28000, 28112, 28224, 28336, 28448, 28560
};

unsigned int green_94[256] = {0, 94, 188, 282, 376, 470, 564, 658, 752, 846, 940, 1034, 1128, 1222,
    1316, 1410, 1504, 1598, 1692, 1786, 1880, 1974, 2068, 2162, 2256, 2350, 2444, 2538, 2632, 2726,
    2820, 2914, 3008, 3102, 3196, 3290, 3384, 3478, 3572, 3666, 3760, 3854, 3948, 4042, 4136, 4230,
    4324, 4418, 4512, 4606, 4700, 4794, 4888, 4982, 5076, 5170, 5264, 5358, 5452, 5546, 5640, 5734,
    5828, 5922, 6016, 6110, 6204, 6298, 6392, 6486, 6580, 6674, 6768, 6862, 6956, 7050, 7144, 7238,
    7332, 7426, 7520, 7614, 7708, 7802, 7896, 7990, 8084, 8178, 8272, 8366, 8460, 8554, 8648, 8742,
    8836, 8930, 9024, 9118, 9212, 9306, 9400, 9494, 9588, 9682, 9776, 9870, 9964, 10058, 10152, 10246,
    10340, 10434, 10528, 10622, 10716, 10810, 10904, 10998, 11092, 11186, 11280, 11374, 11468, 11562,
    11656, 11750, 11844, 11938, 12032, 12126, 12220, 12314, 12408, 12502, 12596, 12690, 12784, 12878,
    12972, 13066, 13160, 13254, 13348, 13442, 13536, 13630, 13724, 13818, 13912, 14006, 14100, 14194,
    14288, 14382, 14476, 14570, 14664, 14758, 14852, 14946, 15040, 15134, 15228, 15322, 15416, 15510,
    15604, 15698, 15792, 15886, 15980, 16074, 16168, 16262, 16356, 16450, 16544, 16638, 16732, 16826,
    16920, 17014, 17108, 17202, 17296, 17390, 17484, 17578, 17672, 17766, 17860, 17954, 18048, 18142,
    18236, 18330, 18424, 18518, 18612, 18706, 18800, 18894, 18988, 19082, 19176, 19270, 19364, 19458,
    19552, 19646, 19740, 19834, 19928, 20022, 20116, 20210, 20304, 20398, 20492, 20586, 20680, 20774,
    20868, 20962, 21056, 21150, 21244, 21338, 21432, 21526, 21620, 21714, 21808, 21902, 21996, 22090,
    22184, 22278, 22372, 22466, 22560, 22654, 22748, 22842, 22936, 23030, 23124, 23218, 23312, 23406,
    23500, 23594, 23688, 23782, 23876, 23970
};

unsigned int blue_18[256] = {128, 146, 164, 182, 200, 218, 236, 254, 272, 290, 308, 326, 344, 362,
    380, 398, 416, 434, 452, 470, 488, 506, 524, 542, 560, 578, 596, 614, 632, 650, 668, 686, 704,
    722, 740, 758, 776, 794, 812, 830, 848, 866, 884, 902, 920, 938, 956, 974, 992, 1010, 1028, 1046,
    1064, 1082, 1100, 1118, 1136, 1154, 1172, 1190, 1208, 1226, 1244, 1262, 1280, 1298, 1316, 1334,
    1352, 1370, 1388, 1406, 1424, 1442, 1460, 1478, 1496, 1514, 1532, 1550, 1568, 1586, 1604, 1622,
    1640, 1658, 1676, 1694, 1712, 1730, 1748, 1766, 1784, 1802, 1820, 1838, 1856, 1874, 1892, 1910,
    1928, 1946, 1964, 1982, 2000, 2018, 2036, 2054, 2072, 2090, 2108, 2126, 2144, 2162, 2180, 2198,
    2216, 2234, 2252, 2270, 2288, 2306, 2324, 2342, 2360, 2378, 2396, 2414, 2432, 2450, 2468, 2486,
    2504, 2522, 2540, 2558, 2576, 2594, 2612, 2630, 2648, 2666, 2684, 2702, 2720, 2738, 2756, 2774,
    2792, 2810, 2828, 2846, 2864, 2882, 2900, 2918, 2936, 2954, 2972, 2990, 3008, 3026, 3044, 3062,
    3080, 3098, 3116, 3134, 3152, 3170, 3188, 3206, 3224, 3242, 3260, 3278, 3296, 3314, 3332, 3350,
    3368, 3386, 3404, 3422, 3440, 3458, 3476, 3494, 3512, 3530, 3548, 3566, 3584, 3602, 3620, 3638,
    3656, 3674, 3692, 3710, 3728, 3746, 3764, 3782, 3800, 3818, 3836, 3854, 3872, 3890, 3908, 3926,
    3944, 3962, 3980, 3998, 4016, 4034, 4052, 4070, 4088, 4106, 4124, 4142, 4160, 4178, 4196, 4214,
    4232, 4250, 4268, 4286, 4304, 4322, 4340, 4358, 4376, 4394, 4412, 4430, 4448, 4466, 4484, 4502,
    4520, 4538, 4556, 4574, 4592, 4610, 4628, 4646, 4664, 4682, 4700, 4718
};

#define RGB2Y(r, g, b) (uint8_t)(((red_66[r] + green_129[g] +  blue_25[b]) >> 8) +  16)
#define RGB2U(r, g, b) (uint8_t)(((-red_38[r] - green_74[g] + blue_112[b]) >> 8) + 128)
#define RGB2V(r, g, b) (uint8_t)(((red_112[r] - green_94[g] -  blue_18[b]) >> 8) + 128)

#define RGB2UX(r1, g1, b1, r2, g2, b2) (uint8_t)(( ((-red_38[r1] - green_74[g1] + blue_112[b1]) + (-red_38[r2] - green_74[g2] + blue_112[b2])) >> 9) + 128)
#define RGB2VX(r1, g1, b1, r2, g2, b3) (uint8_t)(( ((red_112[r1] - green_94[g1] -  blue_18[b1]) + (red_112[r2] - green_94[g2] -  blue_18[b2])) >> 9) + 128)
