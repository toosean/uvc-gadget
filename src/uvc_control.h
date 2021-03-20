
#ifndef UVC_CONTROL
#define UVC_CONTROL

#include "headers.h"

void uvc_fill_streaming_control(struct processing *processing,
                                struct uvc_streaming_control *ctrl,
                                enum stream_control_action action,
                                int iformat,
                                int iframe);

#endif // end UVC_CONTROL
