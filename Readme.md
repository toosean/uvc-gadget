## uvc-gadget



## How to use

    Usage: ./uvc-gadget [options]
    
    Available options are
        -b             Use bulk mode
        -d             Do not use any real V4L2 capture device
        -f <format>    Select frame format
                0 = V4L2_PIX_FMT_YUYV
                1 = V4L2_PIX_FMT_MJPEG
        -h             Print this help screen and exit
        -i image       MJPEG image
        -m             Streaming mult for ISOC (b/w 0 and 2)
        -n             Number of Video buffers (b/w 2 and 32)
        -o <IO method> Select UVC IO method:
                0 = MMAP
                1 = USER_PTR
        -r <resolution> Select frame resolution:
                0 = 360p, VGA (640x360)
                1 = 720p, WXGA (1280x720)
        -s <speed>     Select USB bus speed (b/w 0 and 2)
                0 = Full Speed (FS)
                1 = High Speed (HS)
                2 = Super Speed (SS)
        -t             Streaming burst (b/w 0 and 15)
        -u device      UVC Video Output device
        -v device      V4L2 Video Capture device

## Build  

- host:  
    make
- Cross compile:  
    make ARCH=arch CROSS_COMPILE=cross_compiler  
    eg:  
    make ARCH=arm CROSS_COMPILE=arm-hisiv600-linux-  
- or:  
    set ARCH, CROSS_COMPILE, KERNEL_DIR in Makefile

## Change log

- Apply patchset [Bugfixes for UVC gadget test application](https://www.spinics.net/lists/linux-usb/msg99220.html)  

- Apply patchset [UVC gadget test application enhancements](https://www.spinics.net/lists/linux-usb/msg84376.html)  

- Add Readme/.gitignore and documentations  
  Copy linux-3.18.y/drivers/usb/gadget/function/uvc.h into repository, change include path for build

## Initial

- Original [uvc-gadget.git](http://git.ideasonboard.org/uvc-gadget.git)
- Fork(copy) [uvc-gadget.git - wlhe](https://github.com/wlhe/uvc-gadget)
- Fork(copy) [uvc-gadget.git - climberhunt / Dave Hunt](https://github.com/climberhunt/uvc-gadget)
- Fork(copy) [uvc-gadget.git - peterbay / Petr Vavrin](https://github.com/peterbay/uvc-gadget)

## TODO
 * better resolution settings (maybe from uvc gadget settings - configfs)
 * setting of usb speed is messy (command line arguments don't correspond with usb speed enum = -s 2 != USB_SPEED_SUPER)
     https://github.com/raspberrypi/linux/blob/1c64f4bc22811d2d371b271daa3fb27895a8abdd/include/uapi/linux/usb/ch9.h#L1140
 * better settings for dwMaxPayloadTransferSize
 * better usb gadget configuration - maybe https://github.com/libusbgx/libusbgx

## Resources

- [Raspberry Pi Zero with Pi Camera as USB Webcam / Dave Hunt](http://www.davidhunt.ie/raspberry-pi-zero-with-pi-camera-as-usb-webcam/)
- [TC35874 to UVC gadget application / Kin Wei Lee](https://github.com/kinweilee/v4l2-mmal-uvc)

