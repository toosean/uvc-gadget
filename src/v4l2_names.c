#include "headers.h"

char *v4l2_control_name(unsigned int control)
{
    switch (control)
    {
    case V4L2_CID_BRIGHTNESS:
        return "V4L2_CID_BRIGHTNESS";

    case V4L2_CID_CONTRAST:
        return "V4L2_CID_CONTRAST";

    case V4L2_CID_SATURATION:
        return "V4L2_CID_SATURATION";

    case V4L2_CID_HUE:
        return "V4L2_CID_HUE";

    case V4L2_CID_AUDIO_VOLUME:
        return "V4L2_CID_AUDIO_VOLUME";

    case V4L2_CID_AUDIO_BALANCE:
        return "V4L2_CID_AUDIO_BALANCE";

    case V4L2_CID_AUDIO_BASS:
        return "V4L2_CID_AUDIO_BASS";

    case V4L2_CID_AUDIO_TREBLE:
        return "V4L2_CID_AUDIO_TREBLE";

    case V4L2_CID_AUDIO_MUTE:
        return "V4L2_CID_AUDIO_MUTE";

    case V4L2_CID_AUDIO_LOUDNESS:
        return "V4L2_CID_AUDIO_LOUDNESS";

    case V4L2_CID_BLACK_LEVEL:
        return "V4L2_CID_BLACK_LEVEL";

    case V4L2_CID_AUTO_WHITE_BALANCE:
        return "V4L2_CID_AUTO_WHITE_BALANCE";

    case V4L2_CID_DO_WHITE_BALANCE:
        return "V4L2_CID_DO_WHITE_BALANCE";

    case V4L2_CID_RED_BALANCE:
        return "V4L2_CID_RED_BALANCE";

    case V4L2_CID_BLUE_BALANCE:
        return "V4L2_CID_BLUE_BALANCE";

    case V4L2_CID_GAMMA:
        return "V4L2_CID_GAMMA";

    case V4L2_CID_EXPOSURE:
        return "V4L2_CID_EXPOSURE";

    case V4L2_CID_AUTOGAIN:
        return "V4L2_CID_AUTOGAIN";

    case V4L2_CID_GAIN:
        return "V4L2_CID_GAIN";

    case V4L2_CID_HFLIP:
        return "V4L2_CID_HFLIP";

    case V4L2_CID_VFLIP:
        return "V4L2_CID_VFLIP";

    case V4L2_CID_POWER_LINE_FREQUENCY:
        return "V4L2_CID_POWER_LINE_FREQUENCY";

    case V4L2_CID_HUE_AUTO:
        return "V4L2_CID_HUE_AUTO";

    case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
        return "V4L2_CID_WHITE_BALANCE_TEMPERATURE";

    case V4L2_CID_SHARPNESS:
        return "V4L2_CID_SHARPNESS";

    case V4L2_CID_BACKLIGHT_COMPENSATION:
        return "V4L2_CID_BACKLIGHT_COMPENSATION";

    case V4L2_CID_CHROMA_AGC:
        return "V4L2_CID_CHROMA_AGC";

    case V4L2_CID_COLOR_KILLER:
        return "V4L2_CID_COLOR_KILLER";

    case V4L2_CID_COLORFX:
        return "V4L2_CID_COLORFX";

    case V4L2_CID_AUTOBRIGHTNESS:
        return "V4L2_CID_AUTOBRIGHTNESS";

    case V4L2_CID_BAND_STOP_FILTER:
        return "V4L2_CID_BAND_STOP_FILTER";

    case V4L2_CID_ROTATE:
        return "V4L2_CID_ROTATE";

    case V4L2_CID_BG_COLOR:
        return "V4L2_CID_BG_COLOR";

    case V4L2_CID_CHROMA_GAIN:
        return "V4L2_CID_CHROMA_GAIN";

    case V4L2_CID_ILLUMINATORS_1:
        return "V4L2_CID_ILLUMINATORS_1";

    case V4L2_CID_ILLUMINATORS_2:
        return "V4L2_CID_ILLUMINATORS_2";

    case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
        return "V4L2_CID_MIN_BUFFERS_FOR_CAPTURE";

    case V4L2_CID_MIN_BUFFERS_FOR_OUTPUT:
        return "V4L2_CID_MIN_BUFFERS_FOR_OUTPUT";

    case V4L2_CID_ALPHA_COMPONENT:
        return "V4L2_CID_ALPHA_COMPONENT";

    case V4L2_CID_COLORFX_CBCR:
        return "V4L2_CID_COLORFX_CBCR";

    case V4L2_CID_CAMERA_CLASS_BASE:
        return "V4L2_CID_CAMERA_CLASS_BASE";

    case V4L2_CID_CAMERA_CLASS:
        return "V4L2_CID_CAMERA_CLASS";

    case V4L2_CID_EXPOSURE_AUTO:
        return "V4L2_CID_EXPOSURE_AUTO";

    case V4L2_CID_EXPOSURE_ABSOLUTE:
        return "V4L2_CID_EXPOSURE_ABSOLUTE";

    case V4L2_CID_EXPOSURE_AUTO_PRIORITY:
        return "V4L2_CID_EXPOSURE_AUTO_PRIORITY";

    case V4L2_CID_PAN_RELATIVE:
        return "V4L2_CID_PAN_RELATIVE";

    case V4L2_CID_TILT_RELATIVE:
        return "V4L2_CID_TILT_RELATIVE";

    case V4L2_CID_PAN_RESET:
        return "V4L2_CID_PAN_RESET";

    case V4L2_CID_TILT_RESET:
        return "V4L2_CID_TILT_RESET";

    case V4L2_CID_PAN_ABSOLUTE:
        return "V4L2_CID_PAN_ABSOLUTE";

    case V4L2_CID_TILT_ABSOLUTE:
        return "V4L2_CID_TILT_ABSOLUTE";

    case V4L2_CID_FOCUS_ABSOLUTE:
        return "V4L2_CID_FOCUS_ABSOLUTE";

    case V4L2_CID_FOCUS_RELATIVE:
        return "V4L2_CID_FOCUS_RELATIVE";

    case V4L2_CID_FOCUS_AUTO:
        return "V4L2_CID_FOCUS_AUTO";

    case V4L2_CID_ZOOM_ABSOLUTE:
        return "V4L2_CID_ZOOM_ABSOLUTE";

    case V4L2_CID_ZOOM_RELATIVE:
        return "V4L2_CID_ZOOM_RELATIVE";

    case V4L2_CID_ZOOM_CONTINUOUS:
        return "V4L2_CID_ZOOM_CONTINUOUS";

    case V4L2_CID_PRIVACY:
        return "V4L2_CID_PRIVACY";

    case V4L2_CID_IRIS_ABSOLUTE:
        return "V4L2_CID_IRIS_ABSOLUTE";

    case V4L2_CID_IRIS_RELATIVE:
        return "V4L2_CID_IRIS_RELATIVE";

    case V4L2_CID_AUTO_EXPOSURE_BIAS:
        return "V4L2_CID_AUTO_EXPOSURE_BIAS";

    case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
        return "V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE";

    case V4L2_CID_WIDE_DYNAMIC_RANGE:
        return "V4L2_CID_WIDE_DYNAMIC_RANGE";

    case V4L2_CID_IMAGE_STABILIZATION:
        return "V4L2_CID_IMAGE_STABILIZATION";

    case V4L2_CID_ISO_SENSITIVITY:
        return "V4L2_CID_ISO_SENSITIVITY";

    case V4L2_CID_ISO_SENSITIVITY_AUTO:
        return "V4L2_CID_ISO_SENSITIVITY_AUTO";

    case V4L2_CID_EXPOSURE_METERING:
        return "V4L2_CID_EXPOSURE_METERING";

    case V4L2_CID_SCENE_MODE:
        return "V4L2_CID_SCENE_MODE";

    case V4L2_CID_3A_LOCK:
        return "V4L2_CID_3A_LOCK";

    case V4L2_LOCK_EXPOSURE:
        return "V4L2_LOCK_EXPOSURE";

    case V4L2_LOCK_WHITE_BALANCE:
        return "V4L2_LOCK_WHITE_BALANCE";

    case V4L2_LOCK_FOCUS:
        return "V4L2_LOCK_FOCUS";

    default:
        return "UNKNOWN";
    }
}

void print_v4l2_capabilities(const char *title,
                             uint32_t cap)
{
    printf("%s: Device capabilities:", title);

    if (cap & V4L2_CAP_VIDEO_CAPTURE)
    {
        printf(" VIDEO_CAPTURE");
    }

    if (cap & V4L2_CAP_VIDEO_OUTPUT)
    {
        printf(" VIDEO_OUTPUT");
    }

    if (cap & V4L2_CAP_VIDEO_OVERLAY)
    {
        printf(" VIDEO_OVERLAY");
    }

    if (cap & V4L2_CAP_VBI_CAPTURE)
    {
        printf(" VBI_CAPTURE");
    }

    if (cap & V4L2_CAP_VBI_OUTPUT)
    {
        printf(" VBI_OUTPUT");
    }

    if (cap & V4L2_CAP_SLICED_VBI_CAPTURE)
    {
        printf(" SLICED_VBI_CAPTURE");
    }

    if (cap & V4L2_CAP_SLICED_VBI_OUTPUT)
    {
        printf(" SLICED_VBI_OUTPUT");
    }

    if (cap & V4L2_CAP_RDS_CAPTURE)
    {
        printf(" RDS_CAPTURE");
    }

    if (cap & V4L2_CAP_VIDEO_OUTPUT_OVERLAY)
    {
        printf(" VIDEO_OUTPUT_OVERLAY");
    }

    if (cap & V4L2_CAP_HW_FREQ_SEEK)
    {
        printf(" HW_FREQ_SEEK");
    }

    if (cap & V4L2_CAP_RDS_OUTPUT)
    {
        printf(" RDS_OUTPUT");
    }
    printf("\n");
}
