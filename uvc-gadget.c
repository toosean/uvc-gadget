/*
 * UVC gadget test application
 *
 * Copyright (C) 2010 Ideas on board SPRL <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 */

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <linux/usb/ch9.h>
#include <linux/usb/video.h>
#include <linux/videodev2.h>

#include "uvc.h"

#define WIDTH1  640
#define HEIGHT1 360

#define WIDTH2	1920
#define HEIGHT2 1080

/* Enable debug prints. */
#undef ENABLE_BUFFER_DEBUG
#undef ENABLE_USB_REQUEST_DEBUG

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

struct camera_ctrl {
    bool enabled;
    unsigned int type;
    const char * name;
    unsigned int value;
    unsigned int minimum;
    unsigned int maximum;
    unsigned int step;
    unsigned int default_value;
    unsigned int v4l2_ctrl;
    int v4l2_minimum;
    int v4l2_maximum;
};

static struct camera_ctrl camera_ctrls[] = {
    [UVC_PU_BRIGHTNESS_CONTROL] = {
        .name = "BRIGHTNESS",
        .enabled = false,
        .minimum = 0,
        .maximum = 100,
        .step = 1,
        .default_value = 50,
        .v4l2_ctrl = V4L2_CID_BRIGHTNESS,
        .v4l2_minimum = 0,
        .v4l2_maximum = 100
    },
    [UVC_PU_CONTRAST_CONTROL] = {
        .name = "CONTRAST",
        .enabled = false,
        .minimum = 0,
        .maximum = 100,
        .step = 1,
        .default_value = 50,
        .v4l2_ctrl = V4L2_CID_CONTRAST,
        .v4l2_minimum = 0,
        .v4l2_maximum = 100
    },
    [UVC_PU_SATURATION_CONTROL] = {
        .name = "SATURATION",
        .enabled = false,
        .minimum = 0,
        .maximum = 100,
        .step = 1,
        .default_value = 50,
        .v4l2_ctrl = V4L2_CID_SATURATION,
        .v4l2_minimum = 0,
        .v4l2_maximum = 100
    },
    [UVC_PU_SHARPNESS_CONTROL] = {
        .name = "SHARPNESS",
        .enabled = false,
        .minimum = 0,
        .maximum = 100,
        .step = 1,
        .default_value = 50,
        .v4l2_ctrl = V4L2_CID_SHARPNESS,
        .v4l2_minimum = 0,
        .v4l2_maximum = 100
    },
    [UVC_PU_GAIN_CONTROL] = {
        .name = "GAIN",
        .enabled = false,
        .minimum = 0,
        .maximum = 16,
        .step = 1,
        .default_value = 16,
        .v4l2_ctrl = V4L2_CID_GAIN,
        .v4l2_minimum = 0,
        .v4l2_maximum = 16
    },
    [UVC_PU_HUE_CONTROL] = {
        .name = "HUE",
        .enabled = false,
        .minimum = 0,
        .maximum = 100,
        .step = 1,
        .default_value = 50,
        .v4l2_ctrl = V4L2_CID_HUE,
        .v4l2_minimum = 0,
        .v4l2_maximum = 100
    },
    [UVC_PU_GAMMA_CONTROL] = {
        .name = "GAMMA",
        .enabled = false,
        .minimum = 0,
        .maximum = 100,
        .step = 1,
        .default_value = 50,
        .v4l2_ctrl = V4L2_CID_GAMMA,
        .v4l2_minimum = 0,
        .v4l2_maximum = 100
    },
    [UVC_PU_BACKLIGHT_COMPENSATION_CONTROL] = {
        .name = "BACKLIGHT_COMPENSATION",
        .enabled = false,
        .minimum = 0,
        .maximum = 100,
        .step = 1,
        .default_value = 50,
        .v4l2_ctrl = V4L2_CID_BACKLIGHT_COMPENSATION,
        .v4l2_minimum = 0,
        .v4l2_maximum = 100
    },
    [UVC_PU_POWER_LINE_FREQUENCY_CONTROL] = {
        .name = "POWER_LINE_FREQUENCY",
        .enabled = false,
        .minimum = 0,
        .maximum = 3,
        .step = 1,
        .default_value = 1,
        .v4l2_ctrl = V4L2_CID_POWER_LINE_FREQUENCY,
        .v4l2_minimum = 0,
        .v4l2_maximum = 3
    },
    [UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL] = {
        .name = "WHITE_BALANCE_COMPONENT",
        .enabled = false,
        .minimum = 0,
        .maximum = 7999,
        .step = 1,
        .default_value = 1000,
        .v4l2_ctrl = V4L2_CID_RED_BALANCE,
        .v4l2_minimum = 0,
        .v4l2_maximum = 7999
    }

    // case UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL: return V4L2_CID_AUTO_WHITE_BALANCE;
};

/* ---------------------------------------------------------------------------
 * Generic stuff
 */

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

static const struct uvc_frame_info uvc_frames_yuyv[] = {
    { WIDTH1, HEIGHT1, {50000000, 0}, },
    { WIDTH2, HEIGHT2, {50000000, 0}, },
    { 0, 0, {0,}, },
};

static const struct uvc_frame_info uvc_frames_mjpeg[] = {
    { WIDTH1, HEIGHT1, {50000000, 0}, },
    { WIDTH2, HEIGHT2, {50000000, 0}, },
    { 0, 0, { 0, }, },
};

static const struct uvc_format_info uvc_formats[] = {
    {V4L2_PIX_FMT_YUYV, uvc_frames_yuyv},
    {V4L2_PIX_FMT_MJPEG, uvc_frames_mjpeg},
};

/* ---------------------------------------------------------------------------
 * V4L2 and UVC device instances
 */

/* Represents a V4L2 based video capture device */
struct v4l2_device {
    /* v4l2 device specific */
    int v4l2_fd;
    int is_streaming;
    char *v4l2_devname;

    /* v4l2 buffer specific */
    enum io_method io;
    struct buffer *mem;
    unsigned int nbufs;

    /* v4l2 buffer queue and dequeue counters */
    unsigned long long int qbuf_count;
    unsigned long long int dqbuf_count;

    /* uvc device hook */
    struct uvc_device *udev;
};

/* Represents a UVC based video output device */
struct uvc_device {
    /* uvc device specific */
    int uvc_fd;
    int is_streaming;
    int run_standalone;
    char *uvc_devname;

    /* uvc control request specific */

    struct uvc_streaming_control probe;
    struct uvc_streaming_control commit;
    int control;
    struct uvc_request_data request_error_code;
    unsigned int control_type;

    /* uvc buffer specific */
    enum io_method io;
    struct buffer *mem;
    struct buffer *dummy_buf;
    unsigned int nbufs;
    unsigned int fcc;
    unsigned int width;
    unsigned int height;

    unsigned int bulk;
    uint8_t color;
    unsigned int imgsize;
    void *imgdata;

    /* USB speed specific */
    int mult;
    int burst;
    int maxpkt;
    enum usb_device_speed speed;

    /* uvc specific flags */
    int first_buffer_queued;
    int uvc_shutdown_requested;

    /* uvc buffer queue and dequeue counters */
    unsigned long long int qbuf_count;
    unsigned long long int dqbuf_count;

    /* v4l2 device hook */
    struct v4l2_device *vdev;
};

/* forward declarations */
static int uvc_video_stream(struct uvc_device *dev, int enable);
static bool uvc_supported_controls(unsigned int uvc_control);

/* ---------------------------------------------------------------------------
 * V4L2 streaming related
 */

static int v4l2_uninit_device(struct v4l2_device *dev)
{
    unsigned int i;
    int ret;

    switch (dev->io) {
    case IO_METHOD_MMAP:
        for (i = 0; i < dev->nbufs; ++i) {
            ret = munmap(dev->mem[i].start, dev->mem[i].length);
            if (ret < 0) {
                printf("V4L2: munmap failed\n");
                return ret;
            }
        }

        free(dev->mem);
        break;

    case IO_METHOD_USERPTR:
    default:
        break;
    }

    return 0;
}

static int v4l2_reqbufs_mmap(struct v4l2_device *dev, int nbufs)
{
    struct v4l2_requestbuffers req;
    unsigned int i = 0;
    int ret;

    CLEAR(req);

    req.count = nbufs;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(dev->v4l2_fd, VIDIOC_REQBUFS, &req);
    if (ret < 0) {
        if (ret == -EINVAL)
            printf("V4L2: does not support memory mapping\n");
        else
            printf("V4L2: VIDIOC_REQBUFS error %s (%d).\n", strerror(errno), errno);
        goto err;
    }

    if (!req.count)
        return 0;

    if (req.count < 2) {
        printf("V4L2: Insufficient buffer memory.\n");
        ret = -EINVAL;
        goto err;
    }

    /* Map the buffers. */
    dev->mem = calloc(req.count, sizeof dev->mem[0]);
    if (!dev->mem) {
        printf("V4L2: Out of memory\n");
        ret = -ENOMEM;
        goto err;
    }

    for (i = 0; i < req.count; ++i) {
        memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));

        dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
        dev->mem[i].buf.index = i;

        ret = ioctl(dev->v4l2_fd, VIDIOC_QUERYBUF, &(dev->mem[i].buf));
        if (ret < 0) {
            printf("V4L2: VIDIOC_QUERYBUF failed for buf %d: %s (%d).\n", i, strerror(errno), errno);
            ret = -EINVAL;
            goto err_free;
        }

        dev->mem[i].start =
            mmap(NULL /* start anywhere */, dev->mem[i].buf.length, PROT_READ | PROT_WRITE /* required */,
                 MAP_SHARED /* recommended */, dev->v4l2_fd, dev->mem[i].buf.m.offset);

        if (MAP_FAILED == dev->mem[i].start) {
            printf("V4L2: Unable to map buffer %u: %s (%d).\n", i, strerror(errno), errno);
            dev->mem[i].length = 0;
            ret = -EINVAL;
            goto err_free;
        }

        dev->mem[i].length = dev->mem[i].buf.length;
        printf("V4L2: Buffer %u mapped at address %p, length %d.\n", i, dev->mem[i].start, dev->mem[i].length);
    }

    dev->nbufs = req.count;
    printf("V4L2: %u buffers allocated.\n", req.count);

    return 0;

err_free:
    free(dev->mem);
err:
    return ret;
}

static int v4l2_reqbufs_userptr(struct v4l2_device *dev, int nbufs)
{
    struct v4l2_requestbuffers req;
    int ret;

    CLEAR(req);

    req.count = nbufs;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;

    ret = ioctl(dev->v4l2_fd, VIDIOC_REQBUFS, &req);
    if (ret < 0) {
        if (ret == -EINVAL)
            printf("V4L2: does not support user pointer i/o\n");
        else
            printf("V4L2: VIDIOC_REQBUFS error %s (%d).\n", strerror(errno), errno);
        return ret;
    }

    dev->nbufs = req.count;
    printf("V4L2: %u buffers allocated.\n", req.count);

    return 0;
}

static int v4l2_reqbufs(struct v4l2_device *dev, int nbufs)
{
    int ret = 0;

    switch (dev->io) {
    case IO_METHOD_MMAP:
        ret = v4l2_reqbufs_mmap(dev, nbufs);
        break;

    case IO_METHOD_USERPTR:
        ret = v4l2_reqbufs_userptr(dev, nbufs);
        break;

    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

static int v4l2_qbuf_mmap(struct v4l2_device *dev)
{
    unsigned int i;
    int ret;

    for (i = 0; i < dev->nbufs; ++i) {
        memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));

        dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
        dev->mem[i].buf.index = i;

        ret = ioctl(dev->v4l2_fd, VIDIOC_QBUF, &(dev->mem[i].buf));
        if (ret < 0) {
            printf("V4L2: VIDIOC_QBUF failed : %s (%d).\n", strerror(errno), errno);
            return ret;
        }

        dev->qbuf_count++;
    }

    return 0;
}

static int v4l2_qbuf(struct v4l2_device *dev)
{
    int ret = 0;

    switch (dev->io) {
    case IO_METHOD_MMAP:
        ret = v4l2_qbuf_mmap(dev);
        break;

    case IO_METHOD_USERPTR:
        /* Empty. */
        ret = 0;
        break;

    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

static int v4l2_process_data(struct v4l2_device *dev)
{
    int ret;
    struct v4l2_buffer vbuf;
    struct v4l2_buffer ubuf;

    /* Return immediately if V4l2 streaming has not yet started. */
    if (!dev->is_streaming)
        return 0;

    if (dev->udev->first_buffer_queued)
        if (dev->dqbuf_count >= dev->qbuf_count)
            return 0;

    /* Dequeue spent buffer rom V4L2 domain. */
    CLEAR(vbuf);

    vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    switch (dev->io) {
    case IO_METHOD_USERPTR:
        vbuf.memory = V4L2_MEMORY_USERPTR;
        break;

    case IO_METHOD_MMAP:
    default:
        vbuf.memory = V4L2_MEMORY_MMAP;
        break;
    }

    ret = ioctl(dev->v4l2_fd, VIDIOC_DQBUF, &vbuf);
    if (ret < 0) {
        return ret;
    }

    dev->dqbuf_count++;

#ifdef ENABLE_BUFFER_DEBUG
    printf("Dequeueing buffer at V4L2 side = %d\n", vbuf.index);
#endif

    /* Queue video buffer to UVC domain. */
    CLEAR(ubuf);

    ubuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    switch (dev->udev->io) {
    case IO_METHOD_MMAP:
        ubuf.memory = V4L2_MEMORY_MMAP;
        ubuf.length = vbuf.length;
        ubuf.index = vbuf.index;
        ubuf.bytesused = vbuf.bytesused;
        break;

    case IO_METHOD_USERPTR:
    default:
        ubuf.memory = V4L2_MEMORY_USERPTR;
        ubuf.m.userptr = (unsigned long)dev->mem[vbuf.index].start;
        ubuf.length = dev->mem[vbuf.index].length;
        ubuf.index = vbuf.index;
        ubuf.bytesused = vbuf.bytesused;
        break;
    }

    ret = ioctl(dev->udev->uvc_fd, VIDIOC_QBUF, &ubuf);
    if (ret < 0) {
        /* Check for a USB disconnect/shutdown event. */
        if (errno == ENODEV) {
            dev->udev->uvc_shutdown_requested = 1;
            printf("UVC: Possible USB shutdown requested from Host, seen during VIDIOC_QBUF\n");
            return 0;
        } else {
            return ret;
        }
    }

    dev->udev->qbuf_count++;

#ifdef ENABLE_BUFFER_DEBUG
    printf("Queueing buffer at UVC side = %d\n", ubuf.index);
#endif

    if (!dev->udev->first_buffer_queued && !dev->udev->run_standalone) {
        uvc_video_stream(dev->udev, 1);
        dev->udev->first_buffer_queued = 1;
        dev->udev->is_streaming = 1;
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * V4L2 generic stuff
 */

static int v4l2_get_format(struct v4l2_device *dev)
{
    struct v4l2_format fmt;
    int ret;

    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(dev->v4l2_fd, VIDIOC_G_FMT, &fmt);
    if (ret < 0) {
        return ret;
    }

    printf("V4L2: Getting current format: %c%c%c%c %ux%u\n", pixfmtstr(fmt.fmt.pix.pixelformat), fmt.fmt.pix.width,
           fmt.fmt.pix.height);

    return 0;
}

static int v4l2_set_format(struct v4l2_device *dev, struct v4l2_format *fmt)
{
    int ret;

    ret = ioctl(dev->v4l2_fd, VIDIOC_S_FMT, fmt);
    if (ret < 0) {
        printf("V4L2: Unable to set format %s (%d).\n", strerror(errno), errno);
        return ret;
    }

    printf("V4L2: Setting format to: %c%c%c%c %ux%u\n", pixfmtstr(fmt->fmt.pix.pixelformat), fmt->fmt.pix.width,
           fmt->fmt.pix.height);

    return 0;
}

static int v4l2_set_ctrl(struct v4l2_device *dev, struct camera_ctrl ctrl)
{
    struct v4l2_queryctrl queryctrl;
    struct v4l2_control control;
    int ret;
    int v4l2_ctrl_value = 0;

    CLEAR(queryctrl);

    if (ctrl.value < ctrl.minimum) {
        ctrl.value = ctrl.minimum;
    }

    if (ctrl.value > ctrl.maximum) {
        ctrl.value = ctrl.maximum;
    }

    v4l2_ctrl_value = (ctrl.value - ctrl.minimum) * (ctrl.v4l2_maximum - ctrl.v4l2_minimum) / (ctrl.maximum - ctrl.minimum) + ctrl.v4l2_minimum;

    unsigned int steps = 1;
    unsigned int step = 1;
    unsigned int v4l2_ctrl = ctrl.v4l2_ctrl;

    if (v4l2_ctrl == V4L2_CID_RED_BALANCE) {
        steps = 2;
    }

    for (step = 1; step <= steps; step++) {
        queryctrl.id = v4l2_ctrl;
        ret = ioctl(dev->v4l2_fd, VIDIOC_QUERYCTRL, &queryctrl);
        if (-1 == ret) {
            if (errno != EINVAL)
                printf("V4L2: VIDIOC_QUERYCTRL failed: %s (%d).\n", strerror(errno), errno);
            else
                printf("v4l2: %s is not supported: %s (%d).\n", ctrl.name, strerror(errno), errno);

            return ret;

        } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
            printf("V4L2: %s is disabled.\n", ctrl.name);
            ret = -EINVAL;
            return ret;

        } else {
            CLEAR(control);
            control.id = v4l2_ctrl;
            control.value = v4l2_ctrl_value;

            ret = ioctl(dev->v4l2_fd, VIDIOC_S_CTRL, &control);
            if (-1 == ret) {
                printf("V4L2: VIDIOC_S_CTRL failed: %s (%d).\n", strerror(errno), errno);
                return ret;
            }
        }

        if (v4l2_ctrl == V4L2_CID_RED_BALANCE) {
            v4l2_ctrl = V4L2_CID_BLUE_BALANCE;
        }
    }
    printf("V4L2: %s changed to value = %d\n", ctrl.name, v4l2_ctrl_value);
    return 0;
}

static int v4l2_start_capturing(struct v4l2_device *dev)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret;

    ret = ioctl(dev->v4l2_fd, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        printf("V4L2: Unable to start streaming: %s (%d).\n", strerror(errno), errno);
        return ret;
    }

    printf("V4L2: Starting video stream.\n");

    return 0;
}

static int v4l2_stop_capturing(struct v4l2_device *dev)
{
    enum v4l2_buf_type type;
    int ret;

    switch (dev->io) {
    case IO_METHOD_MMAP:
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        ret = ioctl(dev->v4l2_fd, VIDIOC_STREAMOFF, &type);
        if (ret < 0) {
            printf("V4L2: VIDIOC_STREAMOFF failed: %s (%d).\n", strerror(errno), errno);
            return ret;
        }

        break;
    default:
        /* Nothing to do. */
        break;
    }

    return 0;
}

static unsigned int v4l2_to_uvc_control_code(unsigned int v4l2_code) {
    switch(v4l2_code) {
        case V4L2_CID_BRIGHTNESS:
            return UVC_PU_BRIGHTNESS_CONTROL;

        case V4L2_CID_CONTRAST:
            return UVC_PU_CONTRAST_CONTROL;

        case V4L2_CID_SATURATION:
            return UVC_PU_SATURATION_CONTROL;

        case V4L2_CID_HUE:
            return UVC_PU_HUE_CONTROL;

        case V4L2_CID_GAMMA:
            return UVC_PU_GAMMA_CONTROL;

        case V4L2_CID_GAIN:
            return UVC_PU_GAIN_CONTROL;

        case V4L2_CID_SHARPNESS:
            return UVC_PU_SHARPNESS_CONTROL;

        case V4L2_CID_BACKLIGHT_COMPENSATION:
            return UVC_PU_BACKLIGHT_COMPENSATION_CONTROL;

        case V4L2_CID_POWER_LINE_FREQUENCY:
            return UVC_PU_POWER_LINE_FREQUENCY_CONTROL;

        case V4L2_CID_RED_BALANCE:
            return UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL;

        default:
            return 0xffffffff;
    }
}

static void v4l2_get_controls(struct v4l2_device *dev)
{
    printf("V4L2: Checking controls\n");
    struct v4l2_queryctrl queryctrl;
    // struct v4l2_querymenu querymenu;
    struct v4l2_control control;
    unsigned int id;
    unsigned int uvc_control_code;
    const unsigned next_fl = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;

    memset (&queryctrl, 0, sizeof (queryctrl));

    queryctrl.id = next_fl;
    while (0 == ioctl (dev->v4l2_fd, VIDIOC_QUERYCTRL, &queryctrl)) {
        
        id = queryctrl.id;
        queryctrl.id |= next_fl;

        if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
            continue;
        }

        if (id && V4L2_CTRL_CLASS_USER) {
            uvc_control_code = v4l2_to_uvc_control_code(id);

            if (uvc_supported_controls(uvc_control_code)) {
                printf ("V4L2: Supported control %s\n", queryctrl.name);

                control.id = queryctrl.id;
                if (0 == ioctl (dev->v4l2_fd, VIDIOC_G_CTRL, &control)) {
                    camera_ctrls[uvc_control_code].enabled = true;
                    camera_ctrls[uvc_control_code].type = queryctrl.type;
                    camera_ctrls[uvc_control_code].v4l2_minimum = queryctrl.minimum;
                    camera_ctrls[uvc_control_code].v4l2_maximum = queryctrl.maximum;
                    camera_ctrls[uvc_control_code].minimum = 0;
                    camera_ctrls[uvc_control_code].maximum = (0 - queryctrl.minimum) + queryctrl.maximum;
                    camera_ctrls[uvc_control_code].step = queryctrl.step;
                    camera_ctrls[uvc_control_code].default_value = (0 - queryctrl.minimum) + queryctrl.default_value;
                    camera_ctrls[uvc_control_code].value = (0 - queryctrl.minimum) + control.value;

                    printf ("V4L2:   V4L2: min: %d, max: %d, step: %d, default_value: %d, value: %d\n",
                        queryctrl.minimum,
                        queryctrl.maximum,
                        queryctrl.step,
                        queryctrl.default_value,
                        control.value
                    );

                    printf ("V4L2:   UVC: min: %d, max: %d, step: %d, default_value: %d, value: %d\n",
                        camera_ctrls[uvc_control_code].minimum,
                        camera_ctrls[uvc_control_code].maximum,
                        queryctrl.step,
                        camera_ctrls[uvc_control_code].default_value,
                        camera_ctrls[uvc_control_code].value
                    );
                }
            } else {
                // printf ("V4L2: Unsupported control %s\n", queryctrl.name);
                // printf ("V4L2:   V4L2: min: %d, max: %d, step: %d, default_value: %d, value: %d\n",
                // 	queryctrl.minimum,
                // 	queryctrl.maximum,
                // 	queryctrl.step,
                // 	queryctrl.default_value,
                // 	control.value
                // );
            }
        }
    }
}

static int v4l2_open(struct v4l2_device **v4l2, char *devname, struct v4l2_format *s_fmt)
{
    struct v4l2_device *dev;
    struct v4l2_capability cap;
    int fd;
    int ret = -EINVAL;

    fd = open(devname, O_RDWR | O_NONBLOCK, 0);
    if (fd == -1) {
        printf("V4L2: Device open failed: %s (%d).\n", strerror(errno), errno);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (ret < 0) {
        printf("V4L2: VIDIOC_QUERYCAP failed: %s (%d).\n", strerror(errno), errno);
        goto err;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        printf("V4L2: %s is no video capture device\n", devname);
        goto err;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        printf("V4L2: %s does not support streaming i/o\n", devname);
        goto err;
    }

    dev = calloc(1, sizeof *dev);
    if (dev == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    printf("V4L2: device is %s on bus %s\n", cap.card, cap.bus_info);

    dev->v4l2_fd = fd;

    /* Get the default image format supported. */
    ret = v4l2_get_format(dev);
    if (ret < 0)
        goto err_free;

    /*
     * Set the desired image format.
     * Note: VIDIOC_S_FMT may change width and height.
     */
    ret = v4l2_set_format(dev, s_fmt);
    if (ret < 0)
        goto err_free;

    /* Get the changed image format. */
    ret = v4l2_get_format(dev);
    if (ret < 0)
        goto err_free;

    v4l2_get_controls(dev);

    printf("V4L2: Open succeeded, file descriptor = %d\n", fd);

    *v4l2 = dev;

    return 0;

err_free:
    free(dev);
err:
    close(fd);

    return ret;
}

static void v4l2_close(struct v4l2_device *dev)
{
    close(dev->v4l2_fd);
    free(dev);
}

/* ---------------------------------------------------------------------------
 * UVC generic stuff
 */

__attribute__((unused))
static int uvc_video_set_format(struct uvc_device *dev)
{
    struct v4l2_format fmt;
    int ret;

    CLEAR(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width = dev->width;
    fmt.fmt.pix.height = dev->height;
    fmt.fmt.pix.pixelformat = dev->fcc;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (dev->fcc == V4L2_PIX_FMT_MJPEG)
        fmt.fmt.pix.sizeimage = dev->imgsize * 1.5;

    ret = ioctl(dev->uvc_fd, VIDIOC_S_FMT, &fmt);
    if (ret < 0) {
        printf("UVC: Unable to set format %s (%d).\n", strerror(errno), errno);
        return ret;
    }

    printf("UVC: Setting format to: %c%c%c%c %ux%u\n", pixfmtstr(dev->fcc), dev->width, dev->height);

    return 0;
}

static int uvc_video_stream(struct uvc_device *dev, int enable)
{
    int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    int ret;

    if (!enable) {
        ret = ioctl(dev->uvc_fd, VIDIOC_STREAMOFF, &type);
        if (ret < 0) {
            printf("UVC: VIDIOC_STREAMOFF failed: %s (%d).\n", strerror(errno), errno);
            return ret;
        }

        printf("UVC: Stopping video stream.\n");

        return 0;
    }

    ret = ioctl(dev->uvc_fd, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        printf("UVC: Unable to start streaming %s (%d).\n", strerror(errno), errno);
        return ret;
    }

    printf("UVC: Starting video stream.\n");

    dev->uvc_shutdown_requested = 0;

    return 0;
}

static int uvc_uninit_device(struct uvc_device *dev)
{
    unsigned int i;
    int ret;

    switch (dev->io) {
    case IO_METHOD_MMAP:
        for (i = 0; i < dev->nbufs; ++i) {
            ret = munmap(dev->mem[i].start, dev->mem[i].length);
            if (ret < 0) {
                printf("UVC: munmap failed\n");
                return ret;
            }
        }

        free(dev->mem);
        break;

    case IO_METHOD_USERPTR:
    default:
        if (dev->run_standalone) {
            for (i = 0; i < dev->nbufs; ++i)
                free(dev->dummy_buf[i].start);

            free(dev->dummy_buf);
        }
        break;
    }

    return 0;
}

static int uvc_open(struct uvc_device **uvc, char *devname)
{
    struct uvc_device *dev;
    struct v4l2_capability cap;
    int fd;
    int ret = -EINVAL;

    fd = open(devname, O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        printf("UVC: Device open failed: %s (%d).\n", strerror(errno), errno);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (ret < 0) {
        printf("UVC: Unable to query uvc device: %s (%d)\n", strerror(errno), errno);
        goto err;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
        printf("UVC: %s is no video output device\n", devname);
        goto err;
    }

    dev = calloc(1, sizeof *dev);
    if (dev == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    printf("UVC: Device is %s on bus %s\n", cap.card, cap.bus_info);
    printf("UVC: Open succeeded, file descriptor = %d\n", fd);

    dev->uvc_fd = fd;
    *uvc = dev;

    return 0;

err:
    close(fd);
    return ret;
}

static void uvc_close(struct uvc_device *dev)
{
    close(dev->uvc_fd);
    free(dev->imgdata);
    free(dev);
}

/* ---------------------------------------------------------------------------
 * UVC streaming related
 */

static void uvc_video_fill_buffer(struct uvc_device *dev, struct v4l2_buffer *buf)
{
    unsigned int bpl;
    unsigned int i;

    switch (dev->fcc) {
    case V4L2_PIX_FMT_YUYV:
        /* Fill the buffer with video data. */
        bpl = dev->width * 2;
        for (i = 0; i < dev->height; ++i)
            memset(dev->mem[buf->index].start + i * bpl, dev->color++, bpl);

        buf->bytesused = bpl * dev->height;
        break;

    case V4L2_PIX_FMT_MJPEG:
        memcpy(dev->mem[buf->index].start, dev->imgdata, dev->imgsize);
        buf->bytesused = dev->imgsize;
        break;
    }
}

static int uvc_video_process(struct uvc_device *dev)
{
    struct v4l2_buffer ubuf;
    struct v4l2_buffer vbuf;
    unsigned int i;
    int ret;
    /*
     * Return immediately if UVC video output device has not started
     * streaming yet.
     */
    if (!dev->is_streaming)
        return 0;
    /* Prepare a v4l2 buffer to be dequeued from UVC domain. */
    CLEAR(ubuf);

    ubuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    switch (dev->io) {
    case IO_METHOD_MMAP:
        ubuf.memory = V4L2_MEMORY_MMAP;
        break;

    case IO_METHOD_USERPTR:
    default:
        ubuf.memory = V4L2_MEMORY_USERPTR;
        break;
    }
    if (dev->run_standalone) {
        /* UVC stanalone setup. */
        ret = ioctl(dev->uvc_fd, VIDIOC_DQBUF, &ubuf);
        if (ret < 0)
            return ret;

        dev->dqbuf_count++;
#ifdef ENABLE_BUFFER_DEBUG
        printf("DeQueued buffer at UVC side = %d\n", ubuf.index);
#endif
        uvc_video_fill_buffer(dev, &ubuf);

        ret = ioctl(dev->uvc_fd, VIDIOC_QBUF, &ubuf);
        if (ret < 0)
            return ret;

        dev->qbuf_count++;

#ifdef ENABLE_BUFFER_DEBUG
        printf("ReQueueing buffer at UVC side = %d\n", ubuf.index);
#endif
    } else {
        /* UVC - V4L2 integrated path. */

        /*
         * Return immediately if V4L2 video capture device has not
         * started streaming yet or if QBUF was not called even once on
         * the UVC side.
         */
        if (!dev->vdev->is_streaming || !dev->first_buffer_queued)
            return 0;

        /*
         * Do not dequeue buffers from UVC side until there are atleast
         * 2 buffers available at UVC domain.
         */
        if (!dev->uvc_shutdown_requested)
            if ((dev->dqbuf_count + 1) >= dev->qbuf_count)
                return 0;

        /* Dequeue the spent buffer from UVC domain */
        ret = ioctl(dev->uvc_fd, VIDIOC_DQBUF, &ubuf);
        if (ret < 0) {
            printf("UVC: Unable to dequeue buffer: %s (%d).\n", strerror(errno), errno);
            return ret;
        }

        if (dev->io == IO_METHOD_USERPTR)
            for (i = 0; i < dev->nbufs; ++i)
                if (ubuf.m.userptr == (unsigned long)dev->vdev->mem[i].start && ubuf.length == dev->vdev->mem[i].length)
                    break;

        dev->dqbuf_count++;

#ifdef ENABLE_BUFFER_DEBUG
        printf("DeQueued buffer at UVC side=%d\n", ubuf.index);
#endif

        /*
         * If the dequeued buffer was marked with state ERROR by the
         * underlying UVC driver gadget, do not queue the same to V4l2
         * and wait for a STREAMOFF event on UVC side corresponding to
         * set_alt(0). So, now all buffers pending at UVC end will be
         * dequeued one-by-one and we will enter a state where we once
         * again wait for a set_alt(1) command from the USB host side.
         */
        if (ubuf.flags & V4L2_BUF_FLAG_ERROR) {
            dev->uvc_shutdown_requested = 1;
            printf("UVC: Possible USB shutdown requested from Host, seen during VIDIOC_DQBUF\n");
            return 0;
        }

        /* Queue the buffer to V4L2 domain */
        CLEAR(vbuf);

        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_MMAP;
        vbuf.index = ubuf.index;

        ret = ioctl(dev->vdev->v4l2_fd, VIDIOC_QBUF, &vbuf);
        if (ret < 0)
            return ret;

        dev->vdev->qbuf_count++;

#ifdef ENABLE_BUFFER_DEBUG
        printf("ReQueueing buffer at V4L2 side = %d\n", vbuf.index);
#endif
    }

    return 0;
}

static int uvc_video_qbuf_mmap(struct uvc_device *dev)
{
    unsigned int i;
    int ret;

    for (i = 0; i < dev->nbufs; ++i) {
        memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));

        dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
        dev->mem[i].buf.index = i;

        /* UVC standalone setup. */
        if (dev->run_standalone)
            uvc_video_fill_buffer(dev, &(dev->mem[i].buf));

        ret = ioctl(dev->uvc_fd, VIDIOC_QBUF, &(dev->mem[i].buf));
        if (ret < 0) {
            printf("UVC: VIDIOC_QBUF failed : %s (%d).\n", strerror(errno), errno);
            return ret;
        }

        dev->qbuf_count++;
    }

    return 0;
}

static int uvc_video_qbuf_userptr(struct uvc_device *dev)
{
    unsigned int i;
    int ret;

    /* UVC standalone setup. */
    if (dev->run_standalone) {
        for (i = 0; i < dev->nbufs; ++i) {
            struct v4l2_buffer buf;

            CLEAR(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            buf.memory = V4L2_MEMORY_USERPTR;
            buf.m.userptr = (unsigned long)dev->dummy_buf[i].start;
            buf.length = dev->dummy_buf[i].length;
            buf.index = i;

            ret = ioctl(dev->uvc_fd, VIDIOC_QBUF, &buf);
            if (ret < 0) {
                printf("UVC: VIDIOC_QBUF failed : %s (%d).\n", strerror(errno), errno);
                return ret;
            }

            dev->qbuf_count++;
        }
    }

    return 0;
}

static int uvc_video_qbuf(struct uvc_device *dev)
{
    int ret = 0;

    switch (dev->io) {
    case IO_METHOD_MMAP:
        ret = uvc_video_qbuf_mmap(dev);
        break;

    case IO_METHOD_USERPTR:
        ret = uvc_video_qbuf_userptr(dev);
        break;

    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

static int uvc_video_reqbufs_mmap(struct uvc_device *dev, int nbufs)
{
    struct v4l2_requestbuffers rb;
    unsigned int i;
    int ret;

    CLEAR(rb);

    rb.count = nbufs;
    rb.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    rb.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(dev->uvc_fd, VIDIOC_REQBUFS, &rb);
    if (ret < 0) {
        if (ret == -EINVAL)
            printf("UVC: does not support memory mapping\n");
        else
            printf("UVC: Unable to allocate buffers: %s (%d).\n", strerror(errno), errno);
        goto err;
    }

    if (!rb.count)
        return 0;

    if (rb.count < 2) {
        printf("UVC: Insufficient buffer memory.\n");
        ret = -EINVAL;
        goto err;
    }

    /* Map the buffers. */
    dev->mem = calloc(rb.count, sizeof dev->mem[0]);
    if (!dev->mem) {
        printf("UVC: Out of memory\n");
        ret = -ENOMEM;
        goto err;
    }

    for (i = 0; i < rb.count; ++i) {
        memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));

        dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
        dev->mem[i].buf.index = i;

        ret = ioctl(dev->uvc_fd, VIDIOC_QUERYBUF, &(dev->mem[i].buf));
        if (ret < 0) {
            printf("UVC: VIDIOC_QUERYBUF failed for buf %d: %s (%d).\n", i, strerror(errno), errno);
            ret = -EINVAL;
            goto err_free;
        }
        dev->mem[i].start =
            mmap(NULL /* start anywhere */, dev->mem[i].buf.length, PROT_READ | PROT_WRITE /* required */,
                 MAP_SHARED /* recommended */, dev->uvc_fd, dev->mem[i].buf.m.offset);

        if (MAP_FAILED == dev->mem[i].start) {
            printf("UVC: Unable to map buffer %u: %s (%d).\n", i, strerror(errno), errno);
            dev->mem[i].length = 0;
            ret = -EINVAL;
            goto err_free;
        }

        dev->mem[i].length = dev->mem[i].buf.length;
        printf("UVC: Buffer %u mapped at address %p.\n", i, dev->mem[i].start);
    }

    dev->nbufs = rb.count;
    printf("UVC: %u buffers allocated.\n", rb.count);

    return 0;

err_free:
    free(dev->mem);
err:
    return ret;
}

static int uvc_video_reqbufs_userptr(struct uvc_device *dev, int nbufs)
{
    struct v4l2_requestbuffers rb;
    unsigned int i, j, bpl, payload_size;
    int ret;

    CLEAR(rb);

    rb.count = nbufs;
    rb.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    rb.memory = V4L2_MEMORY_USERPTR;

    ret = ioctl(dev->uvc_fd, VIDIOC_REQBUFS, &rb);
    if (ret < 0) {
        if (ret == -EINVAL)
            printf("UVC: Does not support user pointer i/o\n");
        else
            printf("UVC: VIDIOC_REQBUFS error %s (%d).\n", strerror(errno), errno);
        goto err;
    }

    if (!rb.count)
        return 0;

    dev->nbufs = rb.count;
    printf("UVC: %u buffers allocated.\n", rb.count);

    if (dev->run_standalone) {
        /* Allocate buffers to hold dummy data pattern. */
        dev->dummy_buf = calloc(rb.count, sizeof dev->dummy_buf[0]);
        if (!dev->dummy_buf) {
            printf("UVC: Out of memory\n");
            ret = -ENOMEM;
            goto err;
        }

        switch (dev->fcc) {
        case V4L2_PIX_FMT_YUYV:
            bpl = dev->width * 2;
            payload_size = dev->width * dev->height * 2;
            break;
        case V4L2_PIX_FMT_MJPEG:
            payload_size = dev->imgsize;
            break;
        }

        for (i = 0; i < rb.count; ++i) {
            dev->dummy_buf[i].length = payload_size;
            dev->dummy_buf[i].start = malloc(payload_size);
            if (!dev->dummy_buf[i].start) {
                printf("UVC: Out of memory\n");
                ret = -ENOMEM;
                goto err;
            }

            if (V4L2_PIX_FMT_YUYV == dev->fcc)
                for (j = 0; j < dev->height; ++j)
                    memset(dev->dummy_buf[i].start + j * bpl, dev->color++, bpl);

            if (V4L2_PIX_FMT_MJPEG == dev->fcc)
                memcpy(dev->dummy_buf[i].start, dev->imgdata, dev->imgsize);
        }

        dev->mem = dev->dummy_buf;
    }

    return 0;

err:
    return ret;
}

static int uvc_video_reqbufs(struct uvc_device *dev, int nbufs)
{
    int ret = 0;

    switch (dev->io) {
    case IO_METHOD_MMAP:
        ret = uvc_video_reqbufs_mmap(dev, nbufs);
        break;

    case IO_METHOD_USERPTR:
        ret = uvc_video_reqbufs_userptr(dev, nbufs);
        break;

    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

/*
 * This function is called in response to either:
 * 	- A SET_ALT(interface 1, alt setting 1) command from USB host,
 * 	  if the UVC gadget supports an ISOCHRONOUS video streaming endpoint
 * 	  or,
 *
 *	- A UVC_VS_COMMIT_CONTROL command from USB host, if the UVC gadget
 *	  supports a BULK type video streaming endpoint.
 */
static int uvc_handle_streamon_event(struct uvc_device *dev)
{
    int ret;

    ret = uvc_video_reqbufs(dev, dev->nbufs);
    if (ret < 0)
        goto err;

    if (!dev->run_standalone) {
        /* UVC - V4L2 integrated path. */
        if (IO_METHOD_USERPTR == dev->vdev->io) {
            /*
             * Ensure that the V4L2 video capture device has already
             * some buffers queued.
             */
            ret = v4l2_reqbufs(dev->vdev, dev->vdev->nbufs);
            if (ret < 0)
                goto err;
        }

        ret = v4l2_qbuf(dev->vdev);
        if (ret < 0)
            goto err;

        /* Start V4L2 capturing now. */
        ret = v4l2_start_capturing(dev->vdev);
        if (ret < 0)
            goto err;

        dev->vdev->is_streaming = 1;
    }

    /* Common setup. */

    /* Queue buffers to UVC domain and start streaming. */
    ret = uvc_video_qbuf(dev);
    if (ret < 0)
        goto err;

    if (dev->run_standalone) {
        uvc_video_stream(dev, 1);
        dev->first_buffer_queued = 1;
        dev->is_streaming = 1;
    }

    return 0;

err:
    return ret;
}

/* ---------------------------------------------------------------------------
 * UVC Request processing
 */

static void uvc_fill_streaming_control(struct uvc_device *dev, struct uvc_streaming_control *ctrl, int iframe, int iformat)
{
    const struct uvc_format_info *format;
    const struct uvc_frame_info *frame;
    unsigned int nframes;

    if (iformat < 0)
        iformat = ARRAY_SIZE(uvc_formats) + iformat;
    if (iformat < 0 || iformat >= (int)ARRAY_SIZE(uvc_formats))
        return;
    format = &uvc_formats[iformat];

    nframes = 0;
    while (format->frames[nframes].width != 0)
        ++nframes;

    if (iframe < 0)
        iframe = nframes + iframe;
    if (iframe < 0 || iframe >= (int)nframes)
        return;
    frame = &format->frames[iframe];

    memset(ctrl, 0, sizeof *ctrl);

    ctrl->bmHint = 1;
    ctrl->bFormatIndex = iformat + 1;
    ctrl->bFrameIndex = iframe + 1;
    ctrl->dwFrameInterval = frame->intervals[0];

    switch (format->fcc) {
    case V4L2_PIX_FMT_YUYV:
        ctrl->dwMaxVideoFrameSize = frame->width * frame->height * 2;
        break;
    case V4L2_PIX_FMT_MJPEG:
        ctrl->dwMaxVideoFrameSize = dev->imgsize;
        break;
    }

    /* TODO: the UVC maxpayload transfer size should be filled
     * by the driver.
     */
    if (!dev->bulk)
        ctrl->dwMaxPayloadTransferSize = (dev->maxpkt) * (dev->mult + 1) * (dev->burst + 1);
    else
        ctrl->dwMaxPayloadTransferSize = ctrl->dwMaxVideoFrameSize;

    ctrl->bmFramingInfo = 3;
    ctrl->bPreferedVersion = 1;
    ctrl->bMaxVersion = 1;
}

static void uvc_events_process_standard(struct uvc_device *dev, struct usb_ctrlrequest *ctrl, struct uvc_request_data *resp)
{
    printf("standard request\n");
    (void)dev;
    (void)ctrl;
    (void)resp;
}

static bool uvc_supported_controls(unsigned int uvc_control)
{
    switch (uvc_control) {
    case UVC_PU_BRIGHTNESS_CONTROL:
    case UVC_PU_CONTRAST_CONTROL:
    case UVC_PU_SHARPNESS_CONTROL:
    case UVC_PU_SATURATION_CONTROL:
    case UVC_PU_GAIN_CONTROL:
    case UVC_PU_HUE_CONTROL:
    case UVC_PU_GAMMA_CONTROL:
    case UVC_PU_BACKLIGHT_COMPENSATION_CONTROL:
    case UVC_PU_POWER_LINE_FREQUENCY_CONTROL:
    case UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL:
        return true;

    default:
        return false;
    }
}

static void uvc_events_process_control(struct uvc_device *dev, uint8_t req, uint8_t cs, uint8_t entity_id, uint8_t len, struct uvc_request_data *resp)
{
    printf("UVC: Process control request (entity_id %02x req %02x cs %02x)\n", entity_id, req, cs);

    switch (entity_id) {
    case UVC_VC_DESCRIPTOR_UNDEFINED:
        printf("UVC:  entity: UVC_VC_DESCRIPTOR_UNDEFINED\n");

        switch (cs) {
        case UVC_VC_REQUEST_ERROR_CODE_CONTROL:
            printf("UVC:  cs: UVC_VC_REQUEST_ERROR_CODE_CONTROL\n");
            /* Send the request error code last prepared. */
            resp->data[0] = dev->request_error_code.data[0];
            resp->length = dev->request_error_code.length;
            break;

        default:
            printf("UVC:  cs: UNKNOWN\n");
            dev->request_error_code.data[0] = REQEC_INVALID_CONTROL;
            dev->request_error_code.length = 1;
            break;
        }
        break;

    case UVC_VC_HEADER:
        printf("UVC:  entity: UVC_VC_HEADER\n");
        break;

    case UVC_VC_INPUT_TERMINAL:
        printf("UVC:  entity: UVC_VC_INPUT_TERMINAL\n");

        if (uvc_supported_controls(cs)) {
            if (!camera_ctrls[cs].enabled) {
                goto unsupported_control;
            }

            switch (req) {
            case UVC_SET_CUR:
                printf("UVC:    %s: UVC_SET_CUR\n", camera_ctrls[cs].name);
                resp->data[0] = 0x0;
                resp->length = len;
                dev->control_type = cs;
                goto successfull_response;

            case UVC_GET_MIN:
                printf("UVC:    %s: UVC_GET_MIN\n", camera_ctrls[cs].name);
                resp->data[0] = camera_ctrls[cs].minimum;
                resp->length = 2;
                goto successfull_response;

            case UVC_GET_MAX:
                printf("UVC:    %s: UVC_GET_MAX\n", camera_ctrls[cs].name);
                resp->data[0] = camera_ctrls[cs].maximum;
                resp->length = 2;
                goto successfull_response;

            case UVC_GET_CUR:
                printf("UVC:    %s: UVC_GET_CUR\n", camera_ctrls[cs].name);
                resp->length = 2;
                memcpy(&resp->data[0], &camera_ctrls[cs].value, resp->length);
                goto successfull_response;

            case UVC_GET_INFO:
                printf("UVC:    %s: UVC_GET_INFO\n", camera_ctrls[cs].name);
                resp->data[0] = (uint8_t)(UVC_CONTROL_CAP_GET | UVC_CONTROL_CAP_SET);
                resp->length = 1;
                goto successfull_response;

            case UVC_GET_DEF:
                printf("UVC:    %s: UVC_GET_DEF\n", camera_ctrls[cs].name);
                resp->data[0] = camera_ctrls[cs].default_value;
                resp->length = 2;
                goto successfull_response;

            case UVC_GET_RES:
                printf("UVC:    %s: UVC_GET_RES\n", camera_ctrls[cs].name);
                resp->data[0] = camera_ctrls[cs].step;
                resp->length = 2;
                goto successfull_response;

            default:
                goto unsupported_request_code;

            }
            break;
        } else {
            goto unsupported_control;
        }

        break;

    default:
        printf("UVC:  entity: UNKNOWN\n");
        break;
    }


    switch (entity_id) {
    /* Camera terminal unit 'UVC_VC_INPUT_TERMINAL'. */
    case 1:
        switch (cs) {
        /*
         * We support only 'UVC_CT_AE_MODE_CONTROL' for CAMERA
         * terminal, as our bmControls[0] = 2 for CT. Also we
         * support only auto exposure.
         */
        case UVC_CT_AE_MODE_CONTROL:
            switch (req) {
            case UVC_SET_CUR:
                /* Incase of auto exposure, attempts to
                 * programmatically set the auto-adjusted
                 * controls are ignored.
                 */
                resp->data[0] = 0x01;
                resp->length = 1;
                goto successfull_response;
                break;

            case UVC_GET_INFO:
                resp->data[0] = (uint8_t)(UVC_CONTROL_CAP_GET | UVC_CONTROL_CAP_SET);
                resp->length = 1;
                goto successfull_response;
                break;

            case UVC_GET_CUR:
            case UVC_GET_DEF:
            case UVC_GET_RES:
                /* Auto Mode â€“ auto Exposure Time, auto Iris. */
                resp->data[0] = 0x02;
                resp->length = 1;
                goto successfull_response;
                break;
            default:
                goto unsupported_request_code;
                break;
            }
            break;

        default:
            goto unsupported_control;
            break;
        }
        break;

    /* processing unit 'UVC_VC_PROCESSING_UNIT' */
    case 2:
        break;

    default:
        goto unsupported_entity;
        break;
    }

    return;

unsupported_entity:
    resp->length = -EL2HLT;
    dev->request_error_code.data[0] = REQEC_INVALID_CONTROL;
    dev->request_error_code.length = 1;
    return;

unsupported_control:
    resp->length = -EL2HLT;
    dev->request_error_code.data[0] = REQEC_INVALID_CONTROL;
    dev->request_error_code.length = 1;
    return;

unsupported_request_code:
    resp->length = -EL2HLT;
    dev->request_error_code.data[0] = REQEC_INVALID_REQUEST;
    dev->request_error_code.length = 1;
    return;

successfull_response:
    dev->request_error_code.data[0] = REQEC_NO_ERROR;
    dev->request_error_code.length = 1;
    return;

}

static void uvc_events_process_streaming(struct uvc_device *dev, uint8_t req, uint8_t cs, struct uvc_request_data *resp)
{
    struct uvc_streaming_control *ctrl;

    printf("UVC: streaming request (req %02x cs %02x)\n", req, cs);

    if (cs != UVC_VS_PROBE_CONTROL && cs != UVC_VS_COMMIT_CONTROL)
        return;

    ctrl = (struct uvc_streaming_control *)&resp->data;
    resp->length = sizeof *ctrl;

    switch (req) {
    case UVC_SET_CUR:
        dev->control = cs;
        resp->length = 34;
        break;

    case UVC_GET_CUR:
        if (cs == UVC_VS_PROBE_CONTROL)
            memcpy(ctrl, &dev->probe, sizeof *ctrl);
        else
            memcpy(ctrl, &dev->commit, sizeof *ctrl);
        break;

    case UVC_GET_MIN:
    case UVC_GET_MAX:
    case UVC_GET_DEF:
        if (req == UVC_GET_MAX) {
            uvc_fill_streaming_control(dev, ctrl, -1, -1);
        } else {
            uvc_fill_streaming_control(dev, ctrl, 0, 0);
        }
        break;

    case UVC_GET_RES:
        CLEAR(ctrl);
        break;

    case UVC_GET_LEN:
        resp->data[0] = 0x00;
        resp->data[1] = 0x22;
        resp->length = 2;
        break;

    case UVC_GET_INFO:
        resp->data[0] = 0x03;
        resp->length = 1;
        break;
    }
}

static void uvc_events_process_class(struct uvc_device *dev, struct usb_ctrlrequest *ctrl, struct uvc_request_data *resp)
{
    if ((ctrl->bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE)
        return;

    switch (ctrl->wIndex & 0xff) {
    case UVC_INTF_CONTROL:
        uvc_events_process_control(dev, ctrl->bRequest, ctrl->wValue >> 8, ctrl->wIndex >> 8, ctrl->wLength, resp);
        break;

    case UVC_INTF_STREAMING:
        uvc_events_process_streaming(dev, ctrl->bRequest, ctrl->wValue >> 8, resp);
        break;

    default:
        break;
    }
}

static void uvc_events_process_setup(struct uvc_device *dev, struct usb_ctrlrequest *ctrl, struct uvc_request_data *resp)
{
    dev->control = 0;

#ifdef ENABLE_USB_REQUEST_DEBUG
    printf("\nbRequestType %02x bRequest %02x wValue %04x wIndex %04x wLength %04x\n",
        ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, ctrl->wIndex, ctrl->wLength);
#endif

    switch (ctrl->bRequestType & USB_TYPE_MASK) {
    case USB_TYPE_STANDARD:
        uvc_events_process_standard(dev, ctrl, resp);
        break;

    case USB_TYPE_CLASS:
        uvc_events_process_class(dev, ctrl, resp);
        break;

    default:
        break;
    }
}

static int uvc_events_process_data(struct uvc_device *dev, struct uvc_request_data *data)
{
    struct uvc_streaming_control *target;
    struct uvc_streaming_control *ctrl;
    //struct v4l2_format fmt;
    const struct uvc_format_info *format;
    const struct uvc_frame_info *frame;
    const unsigned int *interval;
    unsigned int iformat, iframe;
    unsigned int nframes;
    unsigned int cs;
    
    // printf("uvc_events_process_data %02x\n", dev->control);
    // printf("uvc_events_process_data DATA %02x, LENGTH %02x\n", data->data, data->length);

    switch (dev->control) {
    case UVC_VS_PROBE_CONTROL:
        printf("UVC: Setting probe control, length = %d\n", data->length);
        target = &dev->probe;
        break;

    case UVC_VS_COMMIT_CONTROL:
        printf("UVC: Setting commit control, length = %d\n", data->length);
        target = &dev->commit;
        break;

    case UVC_VS_CONTROL_UNDEFINED:
        printf("UVC: Setting undefined control, length = %d\n", data->length);

        if (data->length > 0 && data->length <= 4) {
            cs = dev->control_type;
            if (uvc_supported_controls(cs)) {
                memcpy(&camera_ctrls[cs].value, data->data, data->length);
                v4l2_set_ctrl(dev->vdev, camera_ctrls[cs]);
            }
        }
        return 0;
    default:
        printf("UVC: Setting unknown control, length = %d\n", data->length);
        return 0;
    }

    ctrl = (struct uvc_streaming_control *)&data->data;
    iformat = clamp((unsigned int)ctrl->bFormatIndex, 1U, (unsigned int)ARRAY_SIZE(uvc_formats));
    format = &uvc_formats[iformat - 1];

    nframes = 0;
    while (format->frames[nframes].width != 0)
        ++nframes;

    iframe = clamp((unsigned int)ctrl->bFrameIndex, 1U, nframes);
    frame = &format->frames[iframe - 1];
    interval = frame->intervals;

    while (interval[0] < ctrl->dwFrameInterval && interval[1])
        ++interval;

    target->bFormatIndex = iformat;
    target->bFrameIndex = iframe;

    switch (format->fcc) {
    case V4L2_PIX_FMT_YUYV:
        target->dwMaxVideoFrameSize = frame->width * frame->height * 2;
        break;
    case V4L2_PIX_FMT_MJPEG:
        if (dev->imgsize == 0)
            printf("WARNING: MJPEG requested and no image loaded.\n");
        target->dwMaxVideoFrameSize = dev->imgsize;
        break;
    }

    target->dwFrameInterval = *interval;

    if (dev->control == UVC_VS_COMMIT_CONTROL) {
        dev->fcc = format->fcc;
        dev->width = frame->width;
        dev->height = frame->height;
    }

    return 0;
}

static void uvc_events_process(struct uvc_device *dev)
{
    struct v4l2_event v4l2_event;
    struct uvc_event *uvc_event = (void *)&v4l2_event.u.data;
    struct uvc_request_data resp;
    int ret;

    ret = ioctl(dev->uvc_fd, VIDIOC_DQEVENT, &v4l2_event);
    if (ret < 0) {
        printf("VIDIOC_DQEVENT failed: %s (%d)\n", strerror(errno), errno);
        return;
    }

    memset(&resp, 0, sizeof resp);
    resp.length = -EL2HLT;

    switch (v4l2_event.type) {
    case UVC_EVENT_CONNECT:
        return;

    case UVC_EVENT_DISCONNECT:
        dev->uvc_shutdown_requested = 1;
        printf("UVC: Possible USB shutdown requested from Host, seen via UVC_EVENT_DISCONNECT\n");
        return;

    case UVC_EVENT_SETUP:
        uvc_events_process_setup(dev, &uvc_event->req, &resp);
        break;

    case UVC_EVENT_DATA:
        ret = uvc_events_process_data(dev, &uvc_event->data);
        if (ret < 0)
            break;
        return;

    case UVC_EVENT_STREAMON:
        if (!dev->bulk)
            uvc_handle_streamon_event(dev);
        return;

    case UVC_EVENT_STREAMOFF:
        /* Stop V4L2 streaming... */
        if (!dev->run_standalone && dev->vdev->is_streaming) {
            /* UVC - V4L2 integrated path. */
            v4l2_stop_capturing(dev->vdev);
            dev->vdev->is_streaming = 0;
        }

        /* ... and now UVC streaming.. */
        if (dev->is_streaming) {
            uvc_video_stream(dev, 0);
            uvc_uninit_device(dev);
            uvc_video_reqbufs(dev, 0);
            dev->is_streaming = 0;
            dev->first_buffer_queued = 0;
        }

        return;
    }

    ret = ioctl(dev->uvc_fd, UVCIOC_SEND_RESPONSE, &resp);
    if (ret < 0) {
        printf("UVCIOC_S_EVENT failed: %s (%d)\n", strerror(errno), errno);
        return;
    }
}

static void uvc_events_init(struct uvc_device *dev)
{
    struct v4l2_event_subscription sub;
    unsigned int payload_size;

    switch (dev->fcc) {
    case V4L2_PIX_FMT_YUYV:
        payload_size = dev->width * dev->height * 2;
        break;
    case V4L2_PIX_FMT_MJPEG:
        payload_size = dev->imgsize;
        break;
    }

    uvc_fill_streaming_control(dev, &dev->probe, 0, 0);
    uvc_fill_streaming_control(dev, &dev->commit, 0, 0);

    if (dev->bulk) {
        /* FIXME Crude hack, must be negotiated with the driver. */
        dev->probe.dwMaxPayloadTransferSize = dev->commit.dwMaxPayloadTransferSize = payload_size;
    }

    memset(&sub, 0, sizeof sub);
    sub.type = UVC_EVENT_SETUP;
    ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_DATA;
    ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_STREAMON;
    ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_STREAMOFF;
    ioctl(dev->uvc_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
}

/* ---------------------------------------------------------------------------
 * main
 */

static void image_load(struct uvc_device *dev, const char *img)
{
    int fd = -1;

    if (img == NULL)
        return;

    fd = open(img, O_RDONLY);
    if (fd == -1) {
        printf("Unable to open MJPEG image '%s'\n", img);
        return;
    }

    dev->imgsize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    dev->imgdata = malloc(dev->imgsize);
    if (dev->imgdata == NULL) {
        printf("Unable to allocate memory for MJPEG image\n");
        dev->imgsize = 0;
        return;
    }

    read(fd, dev->imgdata, dev->imgsize);
    close(fd);
}

static void usage(const char *argv0)
{
    fprintf(stderr, "Usage: %s [options]\n", argv0);
    fprintf(stderr, "Available options are\n");
    fprintf(stderr, " -b		Use bulk mode\n");
    fprintf(stderr, " -d		Do not use any real V4L2 capture device\n");
    fprintf(stderr,
            " -f <format>    Select frame format\n\t"
            "0 = V4L2_PIX_FMT_YUYV\n\t"
            "1 = V4L2_PIX_FMT_MJPEG\n");
    fprintf(stderr, " -h		Print this help screen and exit\n");
    fprintf(stderr, " -i image	MJPEG image\n");
    fprintf(stderr, " -m		Streaming mult for ISOC (b/w 0 and 2)\n");
    fprintf(stderr, " -n		Number of Video buffers (b/w 2 and 32)\n");
    fprintf(stderr,
            " -o <IO method> Select UVC IO method:\n\t"
            "0 = MMAP\n\t"
            "1 = USER_PTR\n");
    fprintf(stderr,
            " -r <resolution> Select frame resolution:\n\t"
            "0 = HEIGHT1p, VGA (WIDTH1xHEIGHT1)\n\t"
            "1 = 720p, (WIDTH2xHEIGHT2)\n");
    fprintf(stderr,
            " -s <speed>	Select USB bus speed (b/w 0 and 2)\n\t"
            "0 = Full Speed (FS)\n\t"
            "1 = High Speed (HS)\n\t"
            "2 = Super Speed (SS)\n");
    fprintf(stderr, " -t		Streaming burst (b/w 0 and 15)\n");
    fprintf(stderr, " -u device	UVC Video Output device\n");
    fprintf(stderr, " -v device	V4L2 Video Capture device\n");
}

int main(int argc, char *argv[])
{
    struct uvc_device *udev;
    struct v4l2_device *vdev;
    struct timeval tv;
    struct v4l2_format fmt;
    char *uvc_devname = "/dev/video0";
    char *v4l2_devname = "/dev/video1";
    char *mjpeg_image = NULL;

    fd_set fdsv, fdsu;
    int ret, opt, nfds;
    int bulk_mode = 0;
    int dummy_data_gen_mode = 0;
    /* Frame format/resolution related params. */
    int default_format = 0;     /* V4L2_PIX_FMT_YUYV */
    int default_resolution = 0; /* VGA HEIGHT1p */
    int nbufs = 2;              /* Ping-Pong buffers */
    /* USB speed related params */
    int mult = 0;
    int burst = 0;
    enum usb_device_speed speed = USB_SPEED_SUPER; /* High-Speed */
    enum io_method uvc_io_method = IO_METHOD_USERPTR;

    while ((opt = getopt(argc, argv, "bdf:hi:m:n:o:r:s:t:u:v:")) != -1) {
        switch (opt) {
        case 'b':
            bulk_mode = 1;
            break;

        case 'd':
            dummy_data_gen_mode = 1;
            break;

        case 'f':
            if (atoi(optarg) < 0 || atoi(optarg) > 1) {
                usage(argv[0]);
                return 1;
            }

            default_format = atoi(optarg);
            break;

        case 'h':
            usage(argv[0]);
            return 1;

        case 'i':
            mjpeg_image = optarg;
            break;

        case 'm':
            if (atoi(optarg) < 0 || atoi(optarg) > 2) {
                usage(argv[0]);
                return 1;
            }

            mult = atoi(optarg);
            printf("Requested Mult value = %d\n", mult);
            break;

        case 'n':
            if (atoi(optarg) < 2 || atoi(optarg) > 32) {
                usage(argv[0]);
                return 1;
            }

            nbufs = atoi(optarg);
            printf("Number of buffers requested = %d\n", nbufs);
            break;

        case 'o':
            if (atoi(optarg) < 0 || atoi(optarg) > 1) {
                usage(argv[0]);
                return 1;
            }

            uvc_io_method = atoi(optarg);
            printf("UVC: IO method requested is %s\n", (uvc_io_method == IO_METHOD_MMAP) ? "MMAP" : "USER_PTR");
            break;

        case 'r':
            if (atoi(optarg) < 0 || atoi(optarg) > 1) {
                usage(argv[0]);
                return 1;
            }

            default_resolution = atoi(optarg);
            break;

        case 's':
            if (atoi(optarg) < 0 || atoi(optarg) > 2) {
                usage(argv[0]);
                return 1;
            }

            speed = atoi(optarg);
            break;

        case 't':
            if (atoi(optarg) < 0 || atoi(optarg) > 15) {
                usage(argv[0]);
                return 1;
            }

            burst = atoi(optarg);
            printf("Requested Burst value = %d\n", burst);
            break;

        case 'u':
            uvc_devname = optarg;
            break;

        case 'v':
            v4l2_devname = optarg;
            break;

        default:
            printf("Invalid option '-%c'\n", opt);
            usage(argv[0]);
            return 1;
        }
    }

    if (!dummy_data_gen_mode && !mjpeg_image) {
        /*
         * Try to set the default format at the V4L2 video capture
         * device as requested by the user.
         */
        CLEAR(fmt);
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = (default_resolution == 0) ? WIDTH1 : WIDTH2;
        fmt.fmt.pix.height = (default_resolution == 0) ? HEIGHT1 : HEIGHT2;
        fmt.fmt.pix.sizeimage = (default_format == 0) ? (fmt.fmt.pix.width * fmt.fmt.pix.height * 2)
                                                      : (fmt.fmt.pix.width * fmt.fmt.pix.height * 1.5);
        fmt.fmt.pix.pixelformat = (default_format == 0) ? V4L2_PIX_FMT_YUYV : V4L2_PIX_FMT_MJPEG;
        fmt.fmt.pix.field = V4L2_FIELD_ANY;

        /* Open the V4L2 device. */
        ret = v4l2_open(&vdev, v4l2_devname, &fmt);
        if (vdev == NULL || ret < 0)
            return 1;
    }

    /* Open the UVC device. */
    ret = uvc_open(&udev, uvc_devname);
    if (udev == NULL || ret < 0)
        return 1;

    udev->uvc_devname = uvc_devname;

    if (!dummy_data_gen_mode && !mjpeg_image) {
        vdev->v4l2_devname = v4l2_devname;
        /* Bind UVC and V4L2 devices. */
        udev->vdev = vdev;
        vdev->udev = udev;
    }

    /* Set parameters as passed by user. */
    udev->width = (default_resolution == 0) ? WIDTH1 : WIDTH2;
    udev->height = (default_resolution == 0) ? HEIGHT1 : HEIGHT2;
    udev->imgsize = (default_format == 0) ? (udev->width * udev->height * 2) : (udev->width * udev->height * 1.5);
    udev->fcc = (default_format == 0) ? V4L2_PIX_FMT_YUYV : V4L2_PIX_FMT_MJPEG;
    udev->io = uvc_io_method;
    udev->bulk = bulk_mode;
    udev->nbufs = nbufs;
    udev->mult = mult;
    udev->burst = burst;
    udev->speed = speed;

    if (dummy_data_gen_mode || mjpeg_image)
        /* UVC standalone setup. */
        udev->run_standalone = 1;

    if (!dummy_data_gen_mode && !mjpeg_image) {
        /* UVC - V4L2 integrated path */
        vdev->nbufs = nbufs;

        /*
         * IO methods used at UVC and V4L2 domains must be
         * complementary to avoid any memcpy from the CPU.
         */
        switch (uvc_io_method) {
        case IO_METHOD_MMAP:
            vdev->io = IO_METHOD_USERPTR;
            break;

        case IO_METHOD_USERPTR:
        default:
            vdev->io = IO_METHOD_MMAP;
            break;
        }
    }

    switch (speed) {
    case USB_SPEED_FULL:
        /* Full Speed. */
        if (bulk_mode)
            udev->maxpkt = 64;
        else
            udev->maxpkt = 1023;
        break;

    case USB_SPEED_HIGH:
        /* High Speed. */
        if (bulk_mode)
            udev->maxpkt = 512;
        else
            udev->maxpkt = 1024;
        break;

    case USB_SPEED_SUPER:
    default:
        /* Super Speed. */
        if (bulk_mode)
            udev->maxpkt = 1024;
        else
            udev->maxpkt = 1024;
        break;
    }

    if (!dummy_data_gen_mode && !mjpeg_image && (IO_METHOD_MMAP == vdev->io)) {
        /*
         * Ensure that the V4L2 video capture device has already some
         * buffers queued.
         */
        v4l2_reqbufs(vdev, vdev->nbufs);
    }

    if (mjpeg_image)
        image_load(udev, mjpeg_image);

    /* Init UVC events. */
    uvc_events_init(udev);

    while (1) {
        if (!dummy_data_gen_mode && !mjpeg_image)
            FD_ZERO(&fdsv);

        FD_ZERO(&fdsu);

        /* We want both setup and data events on UVC interface.. */
        FD_SET(udev->uvc_fd, &fdsu);

        fd_set efds = fdsu;
        fd_set dfds = fdsu;

        /* ..but only data events on V4L2 interface */
        if (!dummy_data_gen_mode && !mjpeg_image)
            FD_SET(vdev->v4l2_fd, &fdsv);

        /* Timeout. */
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        if (!dummy_data_gen_mode && !mjpeg_image) {
            nfds = max(vdev->v4l2_fd, udev->uvc_fd);
            ret = select(nfds + 1, &fdsv, &dfds, &efds, &tv);
        } else {
            ret = select(udev->uvc_fd + 1, NULL, &dfds, &efds, NULL);
        }

        if (-1 == ret) {
            printf("select error %d, %s\n", errno, strerror(errno));
            if (EINTR == errno)
                continue;

            break;
        }

        if (0 == ret) {
            printf("select timeout\n");
            break;
        }

        if (FD_ISSET(udev->uvc_fd, &efds))
            uvc_events_process(udev);
        if (FD_ISSET(udev->uvc_fd, &dfds))
            uvc_video_process(udev);
        if (!dummy_data_gen_mode && !mjpeg_image)
            if (FD_ISSET(vdev->v4l2_fd, &fdsv))
                v4l2_process_data(vdev);
    }

    if (!dummy_data_gen_mode && !mjpeg_image && vdev->is_streaming) {
        /* Stop V4L2 streaming... */
        v4l2_stop_capturing(vdev);
        v4l2_uninit_device(vdev);
        v4l2_reqbufs(vdev, 0);
        vdev->is_streaming = 0;
    }

    if (udev->is_streaming) {
        /* ... and now UVC streaming.. */
        uvc_video_stream(udev, 0);
        uvc_uninit_device(udev);
        uvc_video_reqbufs(udev, 0);
        udev->is_streaming = 0;
    }

    if (!dummy_data_gen_mode && !mjpeg_image)
        v4l2_close(vdev);

    uvc_close(udev);
    return 0;
}
