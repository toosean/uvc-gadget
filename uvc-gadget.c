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

#include "uvc-gadget.h"

/* Enable debug prints. */
#undef ENABLE_BUFFER_DEBUG
#undef ENABLE_USB_REQUEST_DEBUG

static void control_mappig_init() {
    printf ("control_mappig_init: %d", control_mapping_size);
    int i;
    for (i = 0; i < control_mapping_size; i++) {
        control_mapping[i].enabled = false;
    }
}

static char * uvc_request_code_name(unsigned int uvc_control)
{
    switch (uvc_control) {
    case UVC_RC_UNDEFINED:
        return "RC_UNDEFINED";
    case UVC_SET_CUR:
        return "SET_CUR";
    case UVC_GET_CUR:
        return "GET_CUR";
    case UVC_GET_MIN:
        return "GET_MIN";
    case UVC_GET_MAX:
        return "GET_MAX";
    case UVC_GET_RES:
        return "GET_RES";
    case UVC_GET_LEN:
        return "GET_LEN";
    case UVC_GET_INFO:
        return "GET_INFO";
    case UVC_GET_DEF:
        return "GET_DEF";
    default:
        return "UNKNOWN";
    }
}

struct v4l2_device * v4l2_open(char *devname, enum device_type type)
{
    struct v4l2_device * dev;
    struct v4l2_capability cap;
    int fd;
    const char * type_name = (type == DEVICE_TYPE_UVC) ? "DEVICE_UVC" : "DEVICE_V4L2";

    printf("%s: Opening %s device\n", type_name, devname);

    fd = open(devname, O_RDWR | O_NONBLOCK, 0);
    if (fd == -1) {
        printf("%s: Device open failed: %s (%d).\n", type_name, strerror(errno), errno);
        return NULL;
    }

    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        printf("%s: VIDIOC_QUERYCAP failed: %s (%d).\n", type_name, strerror(errno), errno);
        goto err;
    }

    switch (type) {
    case DEVICE_TYPE_V4L2:
        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
            printf("%s: %s is no video capture device\n", type_name, devname);
            goto err;
        }

        if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
            printf("%s: %s does not support streaming i/o\n", type_name, devname);
            goto err;
        }
        break;

    case DEVICE_TYPE_UVC:
    default:
        if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
            printf("%s: %s is no video output device\n", type_name, devname);
            goto err;
        }
        break;
    }

    dev = calloc(1, sizeof *dev);
    if (dev == NULL) {
        goto err;
    }

    printf("%s: Device is %s on bus %s\n", type_name, cap.card, cap.bus_info);

    dev->fd = fd;
    dev->device_type = type;
    dev->device_type_name = type_name;
    return dev;

err:
    close(fd);
    return NULL;
}

/* forward declarations */
// static bool uvc_implemented_pu_controls(unsigned int uvc_control);
// static bool uvc_implemented_it_controls(unsigned int uvc_control);

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
                printf("V4L2: %s munmap failed\n", dev->device_type_name);
                return ret;
            }
        }
        free(dev->mem);
        break;

    case IO_METHOD_USERPTR:
    default:
        if (dev->device_type == DEVICE_TYPE_UVC && dev->run_standalone) {
            for (i = 0; i < dev->nbufs; ++i)
                free(dev->dummy_buf[i].start);

            free(dev->dummy_buf);
        }
        break;
    }

    return 0;
}

static int v4l2_video_stream(struct v4l2_device * dev, int enable)
{
    int type = (dev->device_type == DEVICE_TYPE_UVC) ? V4L2_BUF_TYPE_VIDEO_OUTPUT : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret;

    if (enable) {
        ret = ioctl(dev->fd, VIDIOC_STREAMON, &type);
        if (ret < 0) {
            printf("%s: STREAM ON failed: %s (%d).\n", dev->device_type_name, strerror(errno), errno);
            return ret;
        }

        printf("%s: STREAM ON success\n", dev->device_type_name);
        dev->is_streaming = 1;
        dev->uvc_shutdown_requested = 0;

    } else if (dev->is_streaming) {
        ret = ioctl(dev->fd, VIDIOC_STREAMOFF, &type);
        if (ret < 0) {
            printf("%s: STREAM OFF failed: %s (%d).\n", dev->device_type_name, strerror(errno), errno);
            return ret;
        }

        printf("%s: STREAM OFF success\n", dev->device_type_name);
        dev->is_streaming = 0;
    }

    return 0;
}

static int v4l2_reqbufs_mmap(struct v4l2_device *dev, int nbufs)
{
    struct v4l2_requestbuffers req;
    unsigned int i = 0;
    int ret;
    int type = (dev->device_type == DEVICE_TYPE_V4L2) ? V4L2_BUF_TYPE_VIDEO_CAPTURE : V4L2_BUF_TYPE_VIDEO_OUTPUT;

    CLEAR(req);

    req.count = nbufs;
    req.type = type;
    req.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(dev->fd, VIDIOC_REQBUFS, &req);
    if (ret < 0) {
        if (ret == -EINVAL) {
            printf("%s: Does not support memory mapping\n", dev->device_type_name);
        } else {
            printf("%s: VIDIOC_REQBUFS error %s (%d).\n", dev->device_type_name, strerror(errno), errno);
        }
        goto err;
    }

    if (!req.count) {
        return 0;
    }

    if (req.count < 2) {
        printf("%s: Insufficient buffer memory.\n", dev->device_type_name);
        ret = -EINVAL;
        goto err;
    }

    /* Map the buffers. */
    dev->mem = calloc(req.count, sizeof dev->mem[0]);
    if (!dev->mem) {
        printf("%s: Out of memory\n", dev->device_type_name);
        ret = -ENOMEM;
        goto err;
    }

    for (i = 0; i < req.count; ++i) {
        memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));

        dev->mem[i].buf.type = type;
        dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
        dev->mem[i].buf.index = i;

        ret = ioctl(dev->fd, VIDIOC_QUERYBUF, &(dev->mem[i].buf));
        if (ret < 0) {
            printf("%s: VIDIOC_QUERYBUF failed for buf %d: %s (%d).\n", dev->device_type_name, i, strerror(errno), errno);
            ret = -EINVAL;
            goto err_free;
        }

        dev->mem[i].start =
            mmap(NULL /* start anywhere */,
                dev->mem[i].buf.length,
                PROT_READ | PROT_WRITE /* required */,
                MAP_SHARED /* recommended */,
                dev->fd, dev->mem[i].buf.m.offset
            );

        if (MAP_FAILED == dev->mem[i].start) {
            printf("%s: Unable to map buffer %u: %s (%d).\n", dev->device_type_name, i, strerror(errno), errno);
            dev->mem[i].length = 0;
            ret = -EINVAL;
            goto err_free;
        }

        dev->mem[i].length = dev->mem[i].buf.length;
        printf("%s: Buffer %u mapped at address %p, length %d.\n", dev->device_type_name, i, dev->mem[i].start, dev->mem[i].length);
    }

    dev->nbufs = req.count;
    printf("%s: %u buffers allocated.\n", dev->device_type_name, req.count);

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
    unsigned int i, j, bpl, payload_size;
    CLEAR(req);

    req.count = nbufs;
    req.type = (dev->device_type == DEVICE_TYPE_V4L2) ? V4L2_BUF_TYPE_VIDEO_CAPTURE : V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_USERPTR;

    ret = ioctl(dev->fd, VIDIOC_REQBUFS, &req);
    if (ret < 0) {
        if (ret == -EINVAL) {
            printf("%s: Does not support user pointer i/o\n", dev->device_type_name);
        } else {
            printf("%s: VIDIOC_REQBUFS error %s (%d).\n", dev->device_type_name, strerror(errno), errno);
        }
        return ret;
    }

    if (!req.count) {
        return 0;
    }

    dev->nbufs = req.count;
    printf("%s: %u buffers allocated.\n", dev->device_type_name, req.count);

    if (dev->device_type == DEVICE_TYPE_UVC && dev->run_standalone) {
        /* Allocate buffers to hold dummy data pattern. */
        dev->dummy_buf = calloc(req.count, sizeof dev->dummy_buf[0]);
        if (!dev->dummy_buf) {
            printf("%s: Out of memory\n", dev->device_type_name);
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

        for (i = 0; i < req.count; ++i) {
            dev->dummy_buf[i].length = payload_size;
            dev->dummy_buf[i].start = malloc(payload_size);
            if (!dev->dummy_buf[i].start) {
                printf("%s: Out of memory\n", dev->device_type_name);
                ret = -ENOMEM;
                goto err;
            }

            if (V4L2_PIX_FMT_YUYV == dev->fcc) {
                for (j = 0; j < dev->height; ++j) {
                    memset(dev->dummy_buf[i].start + j * bpl, dev->color++, bpl);
                }
            }

            if (V4L2_PIX_FMT_MJPEG == dev->fcc) {
                memcpy(dev->dummy_buf[i].start, dev->imgdata, dev->imgsize);
            }
        }

        dev->mem = dev->dummy_buf;
    }

    return 0;

err:
    return ret;
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

static int v4l2_qbuf_mmap(struct v4l2_device * dev)
{
    unsigned int i;
    int ret;

    for (i = 0; i < dev->nbufs; ++i) {
        memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));

        dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
        dev->mem[i].buf.index = i;

        ret = ioctl(dev->fd, VIDIOC_QBUF, &(dev->mem[i].buf));
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
    if (!dev->is_streaming) {
        return 0;
    }

    if (dev->udev->first_buffer_queued) {
        if (dev->dqbuf_count >= dev->qbuf_count) {
            return 0;
        }
    }

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

    ret = ioctl(dev->fd, VIDIOC_DQBUF, &vbuf);
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

    ret = ioctl(dev->udev->fd, VIDIOC_QBUF, &ubuf);
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
        v4l2_video_stream(dev->udev, 1);
        dev->udev->first_buffer_queued = 1;
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

    ret = ioctl(dev->fd, VIDIOC_G_FMT, &fmt);
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

    ret = ioctl(dev->fd, VIDIOC_S_FMT, fmt);
    if (ret < 0) {
        printf("V4L2: Unable to set format %s (%d).\n", strerror(errno), errno);
        return ret;
    }

    printf("V4L2: Setting format to: %c%c%c%c %ux%u\n", pixfmtstr(fmt->fmt.pix.pixelformat), fmt->fmt.pix.width,
           fmt->fmt.pix.height);

    return 0;
}

static int v4l2_set_ctrl_value(struct v4l2_device *dev, struct control_mapping_pair ctrl, unsigned int ctrl_v4l2, int v4l2_ctrl_value)
{
    struct v4l2_queryctrl queryctrl;
    struct v4l2_control control;
    int ret;
    CLEAR(queryctrl);

    queryctrl.id = ctrl_v4l2;
    ret = ioctl(dev->fd, VIDIOC_QUERYCTRL, &queryctrl);
    if (-1 == ret) {
        if (errno != EINVAL) {
            printf("V4L2: VIDIOC_QUERYCTRL failed: %s (%d).\n", strerror(errno), errno);
        } else {
            printf("v4l2: %s is not supported: %s (%d).\n", ctrl.v4l2_name, strerror(errno), errno);
        }

        return ret;

    } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
        printf("V4L2: %s is disabled.\n", ctrl.v4l2_name);
        ret = -EINVAL;
        return ret;

    } else {
        CLEAR(control);
        control.id = ctrl.v4l2;
        control.value = v4l2_ctrl_value;

        ret = ioctl(dev->fd, VIDIOC_S_CTRL, &control);
        if (-1 == ret) {
            printf("V4L2: VIDIOC_S_CTRL failed: %s (%d).\n", strerror(errno), errno);
            return ret;
        }
    }

    printf("V4L2: %s changed value (V4L2: %d, UVC: %d)\n", ctrl.v4l2_name, v4l2_ctrl_value, ctrl.value);
    return 0;
}

static void v4l2_set_ctrl(struct v4l2_device *dev, struct control_mapping_pair ctrl)
{
    int v4l2_ctrl_value = 0;
    if (ctrl.value < ctrl.minimum) {
        ctrl.value = ctrl.minimum;
    }

    if (ctrl.value > ctrl.maximum) {
        ctrl.value = ctrl.maximum;
    }

    v4l2_ctrl_value = (ctrl.value - ctrl.minimum) * (ctrl.v4l2_maximum - ctrl.v4l2_minimum) / (ctrl.maximum - ctrl.minimum) + ctrl.v4l2_minimum;

    v4l2_set_ctrl_value(dev, ctrl, ctrl.v4l2, v4l2_ctrl_value);

    if (ctrl.v4l2 == V4L2_CID_RED_BALANCE) {
        v4l2_set_ctrl_value(dev, ctrl, V4L2_CID_BLUE_BALANCE, v4l2_ctrl_value);
    }
}

static void v4l2_apply_camera_control(struct control_mapping_pair * mapping, struct v4l2_queryctrl queryctrl, struct v4l2_control control)
{
    mapping->enabled = true;
    mapping->control_type = queryctrl.type;
    mapping->v4l2_minimum = queryctrl.minimum;
    mapping->v4l2_maximum = queryctrl.maximum;
    mapping->minimum = 0;
    mapping->maximum = (0 - queryctrl.minimum) + queryctrl.maximum;
    mapping->step = queryctrl.step;
    mapping->default_value = (0 - queryctrl.minimum) + queryctrl.default_value;
    mapping->value = (0 - queryctrl.minimum) + control.value;
    
    printf("V4L2: Supported control %s (%s = %s)\n", queryctrl.name, mapping->v4l2_name, mapping->uvc_name);

    printf("V4L2:   V4L2: min: %d, max: %d, step: %d, default: %d, value: %d\n",
        queryctrl.minimum,
        queryctrl.maximum,
        queryctrl.step,
        queryctrl.default_value,
        control.value
    );

    printf("V4L2:   UVC: min: %d, max: %d, step: %d, default: %d, value: %d\n",
        mapping->minimum,
        mapping->maximum,
        queryctrl.step,
        mapping->default_value,
        mapping->value
    );
}

static void v4l2_get_controls(struct v4l2_device *dev)
{
    int i;
    struct v4l2_queryctrl queryctrl;
    struct v4l2_control control;
    unsigned int id;
    const unsigned next_fl = V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND;
    memset (&queryctrl, 0, sizeof (queryctrl));

    queryctrl.id = next_fl;
    while (0 == ioctl (dev->fd, VIDIOC_QUERYCTRL, &queryctrl)) {
        
        id = queryctrl.id;
        queryctrl.id |= next_fl;

        if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
            continue;
        }

        if (id && V4L2_CTRL_CLASS_USER) {

            for (i = 0; i < control_mapping_size; i++) {
                if (control_mapping[i].v4l2 == id) {
                    control.id = queryctrl.id;
                    if (0 == ioctl (dev->fd, VIDIOC_G_CTRL, &control)) {
                        v4l2_apply_camera_control(&control_mapping[i], queryctrl, control);
                    }
                }
            }

            // else {
            //     printf("control id %02x", id);
            //     printf ("V4L2: Unsupported control %s\n", queryctrl.name);
            //     printf ("V4L2:   V4L2: min: %d, max: %d, step: %d, default: %d, value: %d\n",
            //     	queryctrl.minimum,
            //     	queryctrl.maximum,
            //     	queryctrl.step,
            //     	queryctrl.default_value,
            //     	control.value
            //     );
            // }
        }
    }
}

static int v4l2_init (struct v4l2_device * dev, struct v4l2_format *s_fmt) {
    int ret = -EINVAL;

    /* Get the default image format supported. */
    ret = v4l2_get_format(dev);
    if (ret < 0) {
        goto err_free;
    }

    /*
     * Set the desired image format.
     * Note: VIDIOC_S_FMT may change width and height.
     */
    ret = v4l2_set_format(dev, s_fmt);
    if (ret < 0) {
        goto err_free;
    }

    /* Get the changed image format. */
    ret = v4l2_get_format(dev);
    if (ret < 0) {
        goto err_free;
    }

    v4l2_get_controls(dev);

    return 0;

err_free:
    free(dev);
    close(dev->fd);

    return ret;
}

static void v4l2_close(struct v4l2_device *dev)
{
    close(dev->fd);
    if (dev->device_type == DEVICE_TYPE_UVC) {
       free(dev->imgdata); 
    }
    free(dev);
}

/* ---------------------------------------------------------------------------
 * UVC generic stuff
 */

/* ---------------------------------------------------------------------------
 * UVC streaming related
 */

static void uvc_video_fill_buffer(struct v4l2_device *dev, struct v4l2_buffer *buf)
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

static int uvc_video_process(struct v4l2_device *dev)
{
    struct v4l2_buffer ubuf;
    struct v4l2_buffer vbuf;
    unsigned int i;
    int ret;
    /*
     * Return immediately if UVC video output device has not started
     * streaming yet.
     */
    if (!dev->is_streaming) {
        return 0;
    }
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
        ret = ioctl(dev->fd, VIDIOC_DQBUF, &ubuf);
        if (ret < 0) {
            return ret;
        }

        dev->dqbuf_count++;
#ifdef ENABLE_BUFFER_DEBUG
        printf("DeQueued buffer at UVC side = %d\n", ubuf.index);
#endif
        uvc_video_fill_buffer(dev, &ubuf);

        ret = ioctl(dev->fd, VIDIOC_QBUF, &ubuf);
        if (ret < 0) {
            return ret;
        }

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
        if (!dev->vdev->is_streaming || !dev->first_buffer_queued) {
            return 0;
        }

        /*
         * Do not dequeue buffers from UVC side until there are atleast
         * 2 buffers available at UVC domain.
         */
        if (!dev->uvc_shutdown_requested) {
            if ((dev->dqbuf_count + 1) >= dev->qbuf_count) {
                return 0;
            }
        }

        /* Dequeue the spent buffer from UVC domain */
        ret = ioctl(dev->fd, VIDIOC_DQBUF, &ubuf);
        if (ret < 0) {
            printf("UVC: Unable to dequeue buffer: %s (%d).\n", strerror(errno), errno);
            return ret;
        }

        if (dev->io == IO_METHOD_USERPTR) {
            for (i = 0; i < dev->nbufs; ++i) {
                if (ubuf.m.userptr == (unsigned long)dev->vdev->mem[i].start && ubuf.length == dev->vdev->mem[i].length) {
                    break;
                }
            }
        }

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

        ret = ioctl(dev->vdev->fd, VIDIOC_QBUF, &vbuf);
        if (ret < 0) {
            return ret;
        }

        dev->vdev->qbuf_count++;

#ifdef ENABLE_BUFFER_DEBUG
        printf("ReQueueing buffer at V4L2 side = %d\n", vbuf.index);
#endif
    }

    return 0;
}

static int uvc_video_qbuf_mmap(struct v4l2_device *dev)
{
    unsigned int i;
    int ret;

    for (i = 0; i < dev->nbufs; ++i) {
        memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));

        dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
        dev->mem[i].buf.index = i;

        /* UVC standalone setup. */
        if (dev->run_standalone) {
            uvc_video_fill_buffer(dev, &(dev->mem[i].buf));
        }

        ret = ioctl(dev->fd, VIDIOC_QBUF, &(dev->mem[i].buf));
        if (ret < 0) {
            printf("UVC: VIDIOC_QBUF failed : %s (%d).\n", strerror(errno), errno);
            return ret;
        }

        dev->qbuf_count++;
    }

    return 0;
}

static int uvc_video_qbuf_userptr(struct v4l2_device *dev)
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

            ret = ioctl(dev->fd, VIDIOC_QBUF, &buf);
            if (ret < 0) {
                printf("UVC: VIDIOC_QBUF failed : %s (%d).\n", strerror(errno), errno);
                return ret;
            }

            dev->qbuf_count++;
        }
    }

    return 0;
}

static int uvc_video_qbuf(struct v4l2_device *dev)
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

/*
 * This function is called in response to either:
 * 	- A SET_ALT(interface 1, alt setting 1) command from USB host,
 * 	  if the UVC gadget supports an ISOCHRONOUS video streaming endpoint
 * 	  or,
 *
 *	- A UVC_VS_COMMIT_CONTROL command from USB host, if the UVC gadget
 *	  supports a BULK type video streaming endpoint.
 */
static int uvc_handle_streamon_event(struct v4l2_device *dev)
{
    int ret;

    ret = v4l2_reqbufs(dev, dev->nbufs);
    if (ret < 0) {
        goto err;
    }

    if (!dev->run_standalone) {
        /* UVC - V4L2 integrated path. */
        if (IO_METHOD_USERPTR == dev->vdev->io) {
            /*
             * Ensure that the V4L2 video capture device has already
             * some buffers queued.
             */
            ret = v4l2_reqbufs(dev->vdev, dev->vdev->nbufs);
            if (ret < 0) {
                goto err;
            }
        }

        ret = v4l2_qbuf(dev->vdev);
        if (ret < 0) {
            goto err;
        }

        /* Start V4L2 capturing now. */
        ret = v4l2_video_stream(dev->vdev, 1);
        if (ret < 0) {
            goto err;
        }

    }

    /* Common setup. */

    /* Queue buffers to UVC domain and start streaming. */
    ret = uvc_video_qbuf(dev);
    if (ret < 0) {
        goto err;
    }

    if (dev->run_standalone) {
        v4l2_video_stream(dev, 1);
        dev->first_buffer_queued = 1;
    }

    return 0;

err:
    return ret;
}

/* ---------------------------------------------------------------------------
 * UVC Request processing
 */

static void uvc_fill_streaming_control(struct v4l2_device *dev, struct uvc_streaming_control *ctrl, int iframe, int iformat)
{
    const struct uvc_format_info *format;
    const struct uvc_frame_info *frame;
    unsigned int nframes;

    if (iformat < 0) {
        iformat = ARRAY_SIZE(uvc_formats) + iformat;
    }
    if (iformat < 0 || iformat >= (int)ARRAY_SIZE(uvc_formats)) {
        return;
    }
    format = &uvc_formats[iformat];

    nframes = 0;
    while (format->frames[nframes].width != 0)
        ++nframes;

    if (iframe < 0) {
        iframe = nframes + iframe;
    }
    if (iframe < 0 || iframe >= (int)nframes) {
        return;
    }
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
    if (!settings.bulk_mode) {
        ctrl->dwMaxPayloadTransferSize = (settings.usb_maxpkt) * (settings.usb_mult + 1) * (settings.usb_burst + 1);
    } else {
        ctrl->dwMaxPayloadTransferSize = ctrl->dwMaxVideoFrameSize;
    }

    ctrl->bmFramingInfo = 3;
    ctrl->bPreferedVersion = 1;
    ctrl->bMaxVersion = 1;
}

// static void uvc_events_process_standard(struct v4l2_device *dev, struct usb_ctrlrequest *ctrl, struct uvc_request_data *resp)
// {
//     printf("standard request\n");
//     (void)dev;
//     (void)ctrl;
//     (void)resp;
// }

static void uvc_interface_control(unsigned int interface, struct v4l2_device *dev, uint8_t req, uint8_t cs, uint8_t len, struct uvc_request_data *resp)
{
    int i;
    bool found = false;
    const char * request_code_name = uvc_request_code_name(req);
    const char * interface_name = (interface == UVC_VC_INPUT_TERMINAL) ? "INPUT_TERMINAL" : "PROCESSING_UNIT";

    for (i = 0; i < control_mapping_size; i++) {
        if (control_mapping[i].type == interface && control_mapping[i].uvc == cs) {
            found = true;
            break;
        }
    }
 
    if (!found) {
        printf("UVC: %s - %s - %02x - UNSUPPORTED\n", interface_name, request_code_name, cs);
        resp->length = -EL2HLT;
        dev->request_error_code = REQEC_INVALID_CONTROL;
        return;
    } 

    if (!control_mapping[i].enabled) {
        printf("UVC: %s - %s - %s - DISABLED\n", interface_name, request_code_name, control_mapping[i].uvc_name);
        resp->length = -EL2HLT;
        dev->request_error_code = REQEC_INVALID_CONTROL;
        return;
    }

    printf("UVC: %s - %s - %s - OK\n", interface_name, request_code_name, control_mapping[i].uvc_name);

    switch (req) {
    case UVC_SET_CUR:
        resp->data[0] = 0x0;
        resp->length = len;
        dev->control_interface = interface;
        dev->control_type = cs;
        dev->request_error_code = REQEC_NO_ERROR;
        break;

    case UVC_GET_MIN:
        resp->length = 4;
        memcpy(&resp->data[0], &control_mapping[i].minimum, resp->length);
        dev->request_error_code = REQEC_NO_ERROR;
        break;

    case UVC_GET_MAX:
        resp->length = 4;
        memcpy(&resp->data[0], &control_mapping[i].maximum, resp->length);
        dev->request_error_code = REQEC_NO_ERROR;
        break;

    case UVC_GET_CUR:
        resp->length = 4;
        memcpy(&resp->data[0], &control_mapping[i].value, resp->length);
        dev->request_error_code = REQEC_NO_ERROR;
        break;

    case UVC_GET_INFO:
        resp->data[0] = (uint8_t)(UVC_CONTROL_CAP_GET | UVC_CONTROL_CAP_SET);
        resp->length = 1;
        dev->request_error_code = REQEC_NO_ERROR;
        break;

    case UVC_GET_DEF:
        resp->length = 4;
        memcpy(&resp->data[0], &control_mapping[i].default_value, resp->length);
        dev->request_error_code = REQEC_NO_ERROR;
        break;

    case UVC_GET_RES:
        resp->length = 4;
        memcpy(&resp->data[0], &control_mapping[i].step, resp->length);
        dev->request_error_code = REQEC_NO_ERROR;
        break;

    default:
        resp->length = -EL2HLT;
        dev->request_error_code = REQEC_INVALID_REQUEST;
        break;

    }
    return;
}

static char * uvc_vs_interface_control_name(unsigned int interface)
{
    switch (interface) {
    case UVC_VS_CONTROL_UNDEFINED:
        return "CONTROL_UNDEFINED";
    case UVC_VS_PROBE_CONTROL:
        return "PROBE";
    case UVC_VS_COMMIT_CONTROL:
        return "COMMIT";
    case UVC_VS_STILL_PROBE_CONTROL:
        return "STILL_PROBE";
    case UVC_VS_STILL_COMMIT_CONTROL:
        return "STILL_COMMIT";
    case UVC_VS_STILL_IMAGE_TRIGGER_CONTROL:
        return "STILL_IMAGE_TRIGGER";
    case UVC_VS_STREAM_ERROR_CODE_CONTROL:
        return "STREAM_ERROR_CODE";
    case UVC_VS_GENERATE_KEY_FRAME_CONTROL:
        return "GENERATE_KEY_FRAME";
    case UVC_VS_UPDATE_FRAME_SEGMENT_CONTROL:
        return "UPDATE_FRAME_SEGMENT";
    case UVC_VS_SYNC_DELAY_CONTROL:
        return "SYNC_DELAY";
    default:
        return "UNKNOWN";
    }
}

static void uvc_events_process_streaming(struct v4l2_device *dev, uint8_t req, uint8_t cs, struct uvc_request_data *resp)
{
    printf("UVC: streaming request CS: %s, REQ: %s\n", uvc_vs_interface_control_name(cs), uvc_request_code_name(req));

    if (cs != UVC_VS_PROBE_CONTROL && cs != UVC_VS_COMMIT_CONTROL) {
        return;
    }

    struct uvc_streaming_control * ctrl = (struct uvc_streaming_control *)&resp->data;
    struct uvc_streaming_control * target = (cs == UVC_VS_PROBE_CONTROL) ? &dev->probe : &dev->commit;

    int ctrl_length = sizeof * ctrl;
    resp->length = ctrl_length;

    switch (req) {
    case UVC_SET_CUR:
        dev->control = cs;
        resp->length = ctrl_length;
        break;

    case UVC_GET_MAX:
        uvc_fill_streaming_control(dev, ctrl, -1, -1);
        break;

    case UVC_GET_MIN:
    case UVC_GET_CUR:
        memcpy(ctrl, target, ctrl_length);
        break;

    case UVC_GET_DEF:
        uvc_fill_streaming_control(dev, ctrl, 0, 0);
        break;

    case UVC_GET_RES:
        CLEAR(ctrl);
        break;

    case UVC_GET_LEN:
        resp->data[0] = 0x00;
        resp->data[1] = ctrl_length;
        resp->length = 2;
        break;

    case UVC_GET_INFO:
        resp->data[0] = 0x03;
        resp->length = 1;
        break;
    }
}

static void uvc_events_process_class(struct v4l2_device *dev, struct usb_ctrlrequest *ctrl, struct uvc_request_data *resp)
{
    uint8_t type = ctrl->wIndex & 0xff;
    uint8_t interface = ctrl->wIndex >> 8;
    uint8_t control = ctrl->wValue >> 8;
    uint8_t length = ctrl->wLength;

    if ((ctrl->bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE) {
        return;
    }

    switch (type) {
    case UVC_INTF_CONTROL:
        switch (interface) {
            case 0:
                if (control == UVC_VC_REQUEST_ERROR_CODE_CONTROL) {
                    resp->data[0] = dev->request_error_code;
                    resp->length = 1;
                }
                break;
            case 1:
                uvc_interface_control(UVC_VC_INPUT_TERMINAL, dev, ctrl->bRequest, control, length, resp);
                break;
            case 2:
                uvc_interface_control(UVC_VC_PROCESSING_UNIT, dev, ctrl->bRequest, control, length, resp);
                break;
            default:
                break;
        }
        break;

    case UVC_INTF_STREAMING:
        uvc_events_process_streaming(dev, ctrl->bRequest, control, resp);
        break;

    default:
        break;
    }
}

static void uvc_events_process_setup(struct v4l2_device *dev, struct usb_ctrlrequest *ctrl, struct uvc_request_data *resp)
{
    dev->control = 0;

#ifdef ENABLE_USB_REQUEST_DEBUG
    printf("\nbRequestType %02x bRequest %02x wValue %04x wIndex %04x wLength %04x\n",
        ctrl->bRequestType, ctrl->bRequest, ctrl->wValue, ctrl->wIndex, ctrl->wLength);
#endif

    switch (ctrl->bRequestType & USB_TYPE_MASK) {
    // case USB_TYPE_STANDARD:
    //     uvc_events_process_standard(dev, ctrl, resp);
    //     break;

    case USB_TYPE_CLASS:
        uvc_events_process_class(dev, ctrl, resp);
        break;

    default:
        break;
    }
}

static int uvc_events_process_data(struct v4l2_device *dev, struct uvc_request_data *data)
{
    struct uvc_streaming_control *target;
    struct uvc_streaming_control *ctrl;
    //struct v4l2_format fmt;
    const struct uvc_format_info *format;
    const struct uvc_frame_info *frame;
    const unsigned int *interval;
    unsigned int iformat, iframe;
    unsigned int nframes;
    int i;
    
    // printf("uvc_events_process_data %02x\n", dev->control);
    // printf("uvc_events_process_data DATA %02x, LENGTH %02x\n", data->data, data->length);

    printf("uvc_events_process_data CONTROL %s, DATA %02x%02x%02x%02x, LENGTH %02x\n",
        uvc_vs_interface_control_name(dev->control),
        data->data[0],
        data->data[1],
        data->data[2],
        data->data[3],
        data->length
    );

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
        printf("UVC: Set control, length = %d\n", data->length);

        if (data->length > 0 && data->length <= 4) {
            for (i = 0; i < control_mapping_size; i++) {
                if (control_mapping[i].type == dev->control_interface &&
                    control_mapping[i].uvc == dev->control_type &&
                    control_mapping[i].enabled
                ) {
                    control_mapping[i].value = 0x00000000;
                    control_mapping[i].length = data->length;
                    memcpy(&control_mapping[i].value, data->data, data->length);
                    v4l2_set_ctrl(dev->vdev, control_mapping[i]);
                    break;
                }
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
        if (dev->imgsize == 0) {
            printf("WARNING: MJPEG requested and no image loaded.\n");
        }
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

static void uvc_events_process(struct v4l2_device *dev)
{
    struct v4l2_event v4l2_event;
    struct uvc_event *uvc_event = (void *)&v4l2_event.u.data;
    struct uvc_request_data resp;
    int ret;

    ret = ioctl(dev->fd, VIDIOC_DQEVENT, &v4l2_event);
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
        if (ret < 0) {
            break;
        }
        return;

    case UVC_EVENT_STREAMON:
        if (!settings.bulk_mode) {
            uvc_handle_streamon_event(dev);
        }
        return;

    case UVC_EVENT_STREAMOFF:
        /* Stop V4L2 streaming... */
        if (!dev->run_standalone && dev->vdev->is_streaming) {
            /* UVC - V4L2 integrated path. */
            v4l2_video_stream(dev->vdev, 0);
        }

        /* ... and now UVC streaming.. */
        if (dev->is_streaming) {
            v4l2_video_stream(dev, 0);
            v4l2_uninit_device(dev);
            v4l2_reqbufs(dev, 0);
            dev->first_buffer_queued = 0;
        }

        return;
    }

    ret = ioctl(dev->fd, UVCIOC_SEND_RESPONSE, &resp);
    if (ret < 0) {
        printf("UVCIOC_S_EVENT failed: %s (%d)\n", strerror(errno), errno);
        return;
    }
}

static void uvc_events_init(struct v4l2_device *dev)
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

    if (settings.bulk_mode) {
        /* FIXME Crude hack, must be negotiated with the driver. */
        dev->probe.dwMaxPayloadTransferSize = dev->commit.dwMaxPayloadTransferSize = payload_size;
    }

    memset(&sub, 0, sizeof sub);
    sub.type = UVC_EVENT_SETUP;
    ioctl(dev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_DATA;
    ioctl(dev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_STREAMON;
    ioctl(dev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_STREAMOFF;
    ioctl(dev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
}

/* ---------------------------------------------------------------------------
 * main
 */

static void processing_loop_image(struct v4l2_device * udev) 
{
    int activity;
    fd_set fdsu;

    while (1) {
        FD_ZERO(&fdsu);
        FD_SET(udev->fd, &fdsu);

        fd_set efds = fdsu;
        fd_set dfds = fdsu;

        activity = select(udev->fd + 1, NULL, &dfds, &efds, NULL);

        if (activity == -1) {
            printf("select error %d, %s\n", errno, strerror(errno));
            if (EINTR == errno) {
                continue;
            }
            break;
        }

        if (activity == 0) {
            printf("select timeout\n");
            break;
        }

        if (FD_ISSET(udev->fd, &efds)) {
            uvc_events_process(udev);
        }
        if (FD_ISSET(udev->fd, &dfds)) {
            uvc_video_process(udev);
        }
    }
}

static void processing_loop_video(struct v4l2_device * udev, struct v4l2_device * vdev)
{
    struct timeval tv;
    int activity;
    fd_set fdsv, fdsu;
    int nfds;

    while (1) {
        FD_ZERO(&fdsv);
        FD_ZERO(&fdsu);

        /* We want both setup and data events on UVC interface.. */
        FD_SET(udev->fd, &fdsu);

        fd_set efds = fdsu;
        fd_set dfds = fdsu;

        /* ..but only data events on V4L2 interface */
        FD_SET(vdev->fd, &fdsv);

        /* Timeout. */
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        nfds = max(vdev->fd, udev->fd);
        activity = select(nfds + 1, &fdsv, &dfds, &efds, &tv);
        if (activity == -1) {
            printf("select error %d, %s\n", errno, strerror(errno));
            if (EINTR == errno) {
                continue;
            }
            break;
        }

        if (activity == 0) {
            printf("select timeout\n");
            break;
        }

        if (FD_ISSET(udev->fd, &efds)) {
            uvc_events_process(udev);
        }
        if (FD_ISSET(udev->fd, &dfds)) {
            uvc_video_process(udev);
        }
        if (FD_ISSET(vdev->fd, &fdsv)) {
            v4l2_process_data(vdev);
        }
    }
}

static int image_load(struct v4l2_device * dev, const char * img)
{
    int fd = -1;

    if (img == NULL) {
        return STATE_FAILED;
    }

    fd = open(img, O_RDONLY);
    if (fd == -1) {
        printf("ERROR: Unable to open MJPEG image '%s'\n", img);
        return STATE_FAILED;
    }

    dev->imgsize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    dev->imgdata = malloc(dev->imgsize);
    if (dev->imgdata == NULL) {
        printf("ERROR: Unable to allocate memory for MJPEG image\n");
        dev->imgsize = 0;
        return STATE_FAILED;
    }

    read(fd, dev->imgdata, dev->imgsize);
    close(fd);

    printf("SETTINGS: Image %s loaded successfully\n", img);
    return STATE_OK;
}

int init()
{
    int ret;
    struct v4l2_device *udev;
    struct v4l2_device *vdev;

    struct v4l2_format fmt;

    switch (settings.usb_speed) {
    case USB_SPEED_FULL:
        /* Full Speed. */
        if (settings.bulk_mode) {
            settings.usb_maxpkt = 64;
        } else {
            settings.usb_maxpkt = 1023;
        }
        break;

    case USB_SPEED_HIGH:
        /* High Speed. */
        if (settings.bulk_mode) {
            settings.usb_maxpkt = 512;
        } else {
            settings.usb_maxpkt = 1024;
        }
        break;

    case USB_SPEED_SUPER:
    default:
        /* Super Speed. */
        if (settings.bulk_mode) {
            settings.usb_maxpkt = 1024;
        } else {
            settings.usb_maxpkt = 1024;
        }
        break;
    }

    /* Open the UVC device. */

    udev = v4l2_open(settings.uvc_devname, DEVICE_TYPE_UVC);
    if (udev == NULL) {
        return 1;
    }

    if (settings.mjpeg_image) {
        if (image_load(udev, settings.mjpeg_image) == STATE_FAILED) {
            return 1;
        }
        /* UVC standalone setup. */
        udev->run_standalone = 1;

    } else {
        /*
         * Try to set the default format at the V4L2 video capture
         * device as requested by the user.
         */
        CLEAR(fmt);
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = (settings.default_resolution == 0) ? WIDTH1 : WIDTH2;
        fmt.fmt.pix.height = (settings.default_resolution == 0) ? HEIGHT1 : HEIGHT2;
        fmt.fmt.pix.sizeimage = (settings.default_format == V4L2_PIX_FMT_YUYV) ? (fmt.fmt.pix.width * fmt.fmt.pix.height * 2)
                                                      : (fmt.fmt.pix.width * fmt.fmt.pix.height * 1.5);
        fmt.fmt.pix.pixelformat = settings.default_format;
        fmt.fmt.pix.field = V4L2_FIELD_ANY;

        /* Open the V4L2 device. */
        vdev = v4l2_open(settings.v4l2_devname, DEVICE_TYPE_V4L2);
        if (vdev == NULL) {
            return 1;
        }

        ret = v4l2_init(vdev, &fmt);
        if (ret < 0) {
            return 1;
        }
    }

    if (!settings.mjpeg_image) {
        /* Bind UVC and V4L2 devices. */
        udev->vdev = vdev;
        vdev->udev = udev;
    }

    /* Set parameters as passed by user. */
    udev->width = (settings.default_resolution == 0) ? WIDTH1 : WIDTH2;
    udev->height = (settings.default_resolution == 0) ? HEIGHT1 : HEIGHT2;
    udev->imgsize = (settings.default_format == V4L2_PIX_FMT_YUYV) ? (udev->width * udev->height * 2) : (udev->width * udev->height * 1.5);
    udev->fcc = settings.default_format;
    
    udev->io = settings.uvc_io_method;

    udev->nbufs = settings.nbufs;

    if (!settings.mjpeg_image) {
        /* UVC - V4L2 integrated path */
        vdev->nbufs = settings.nbufs;

        /*
         * IO methods used at UVC and V4L2 domains must be
         * complementary to avoid any memcpy from the CPU.
         */
        switch (settings.uvc_io_method) {
        case IO_METHOD_MMAP:
            vdev->io = IO_METHOD_USERPTR;
            break;

        case IO_METHOD_USERPTR:
        default:
            vdev->io = IO_METHOD_MMAP;
            break;
        }
    }

    if (!settings.mjpeg_image && (IO_METHOD_MMAP == vdev->io)) {
        /*
         * Ensure that the V4L2 video capture device has already some
         * buffers queued.
         */
        v4l2_reqbufs(vdev, vdev->nbufs);
    }

    /* Init UVC events. */
    uvc_events_init(udev);

    if (settings.mjpeg_image) {
        processing_loop_image(udev);

    } else {
        processing_loop_video(udev, vdev);
 
    } 

    if (!settings.mjpeg_image && vdev->is_streaming) {
        /* Stop V4L2 streaming... */
        v4l2_video_stream(vdev, 0);
        v4l2_uninit_device(vdev);
        v4l2_reqbufs(vdev, 0);
    }

    if (udev->is_streaming) {
        /* ... and now UVC streaming.. */
        v4l2_video_stream(udev, 0);
        v4l2_uninit_device(udev);
        v4l2_reqbufs(udev, 0);
    }

    if (!settings.mjpeg_image) {
        v4l2_close(vdev);
    }

    v4l2_close(udev);
    return 0;
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

static void check_settings()
{
    printf("SETTINGS: Bulk mode: %s\n", (settings.bulk_mode) ? "DISABLED": "ENABLED");
    printf("SETTINGS: Number of buffers requested: %d\n", settings.nbufs);
    if (settings.mjpeg_image) {
        printf("SETTINGS: MJPEG image: %s\n", settings.mjpeg_image);
    }
    printf("SETTINGS: Video format: %s\n", (settings.default_format == V4L2_PIX_FMT_YUYV) ? "V4L2_PIX_FMT_YUYV": "V4L2_PIX_FMT_MJPEG");
    
    // !!! add usb speed
    printf("SETTINGS: USB mult: %d\n", settings.usb_mult);
    printf("SETTINGS: USB burst: %d\n", settings.usb_burst);
    
    printf("SETTINGS: IO method requested: %s\n", (settings.uvc_io_method == IO_METHOD_MMAP) ? "MMAP" : "USER_PTR");
    printf("SETTINGS: Default resolution: %d\n", settings.default_resolution);
    printf("SETTINGS: UVC device name: %s\n", settings.uvc_devname);
    printf("SETTINGS: V4L2 device name: %s\n", settings.v4l2_devname);
}

int main(int argc, char *argv[])
{
    int opt;

    while ((opt = getopt(argc, argv, "bdf:hi:m:n:o:r:s:t:u:v:")) != -1) {
        switch (opt) {
        case 'b':
            settings.bulk_mode = 1;
            break;

        case 'f':
            if (atoi(optarg) < 0 || atoi(optarg) > 1) {
                fprintf(stderr, "ERROR: format value out of range\n");
                goto err;
            }
            settings.default_format = (atoi(optarg) == 0) ? V4L2_PIX_FMT_YUYV : V4L2_PIX_FMT_MJPEG;
            break;

        case 'h':
            usage(argv[0]);
            return 1;

        case 'i':
            settings.mjpeg_image = optarg;
            break;

        case 'm':
            if (atoi(optarg) < 0 || atoi(optarg) > 2) {
                fprintf(stderr, "ERROR: usb mult value out of range\n");
                goto err;
            }
            settings.usb_mult = atoi(optarg);
            break;

        case 'n':
            if (atoi(optarg) < 2 || atoi(optarg) > 32) {
                fprintf(stderr, "ERROR: Number of Video buffers value out of range\n");
                goto err;
            }
            settings.nbufs = atoi(optarg);
            break;

        case 'o':
            if (atoi(optarg) < 0 || atoi(optarg) > 1) {
                fprintf(stderr, "ERROR: UVC IO method value out of range\n");
                goto err;
            }
            settings.uvc_io_method = (atoi(optarg) == 0) ? IO_METHOD_MMAP : IO_METHOD_USERPTR;
            break;

        case 'r':
            if (atoi(optarg) < 0 || atoi(optarg) > 1) {
                fprintf(stderr, "ERROR: Frame resolution value out of range\n");
                goto err;
            }
            settings.default_resolution = atoi(optarg);
            break;

        case 's':
            if (atoi(optarg) < 0 || atoi(optarg) > 2) {
                fprintf(stderr, "ERROR: USB speed value out of range\n");
                goto err;
            }
            settings.usb_speed = atoi(optarg);
            break;

        case 't':
            if (atoi(optarg) < 0 || atoi(optarg) > 15) {
                fprintf(stderr, "ERROR: USB burst value out of range\n");
                goto err;
            }
            settings.usb_burst = atoi(optarg);
            break;

        case 'u':
            settings.uvc_devname = optarg;
            break;

        case 'v':
            settings.v4l2_devname = optarg;
            break;

        default:
            printf("Invalid option '-%c'\n", opt);
            goto err;
        }
    }

    check_settings();
    control_mappig_init();    

    return init();

err:
    usage(argv[0]);
    return 1;
}
