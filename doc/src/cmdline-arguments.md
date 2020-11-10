# Command line arguments

## Available options are

|argument|value|description|
|:-------|:----|:----------|
|**-h**||**Print help screen and exit**|
|**-l**||**Use onboard led0 for streaming status indication**|
|**-n**|**\<buffers\>**|**Number of Video buffers (b/w 2 and 32)**|
|**-o**|**\<IO method\>**|**Select UVC IO method:**<br>0 = MMAP<br>1 = USER_PTR|
|**-p**|**\<pin_number\>**|**GPIO pin number for streaming status indication**|
|**-u**|**\<device\>**|**UVC Video Output device**<br>Output device: /dev/video1|
|**-v**|**\<device\>**|**V4L2 Video Capture device**<br>Input device: /dev/video0|


## Resources
[Raspberry Pi GPIO](https://www.raspberrypi.org/documentation/usage/gpio/)


## Differences from the original version of uvc-gadget

### New arguments - described above

    * -l
    * -p

### Removed arguments

 * -b - Use bulk mode
    - unused feature

 * -d - Do not use any real V4L2 capture device
    - unused feature

 * -f - Select frame format
    - frame format is removed from uvc-gadget.c and now is obtained from configfs and host computer

 * -i - MJPEG image
    - removed during code cleanup and simplification

 * -m - Streaming mult for ISOC (b/w 0 and 2)
    - unnecessary for new version - dwMaxPayloadTransferSize is set to streaming_maxpacket value

 * -r - Select frame resolution
    - frame resolution is removed from uvc-gadget.c and now is obtained from configfs and host computer

 * -s - Select USB bus speed (b/w 0 and 2)
    - unnecessary for new version - dwMaxPayloadTransferSize is set to streaming_maxpacket value

 * -t - Streaming burst (b/w 0 and 15)
    - unnecessary for new version - dwMaxPayloadTransferSize is set to streaming_maxpacket value

