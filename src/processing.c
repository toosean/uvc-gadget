
#include "headers.h"
#include "processing_fb_uvc.h"
#include "processing_image_uvc.h"
#include "processing_v4l2_uvc.h"
#include "processing_actions.h"

static bool terminate = 0;

void term(int signum)
{
    printf("\nTERMINATE: Signal %s\n", (signum == SIGTERM) ? "SIGTERM" : "SIGINT");
    terminate = true;
}

void processing_loop(struct processing *processing)
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));

    action.sa_handler = term;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    processing->terminate = &terminate;

    if (processing->source.type == ENDPOINT_FB && processing->target.type == ENDPOINT_UVC)
    {
        processing_loop_fb_uvc(processing);
    }
    else if (processing->source.type == ENDPOINT_IMAGE && processing->target.type == ENDPOINT_UVC)
    {
        processing_loop_image_uvc(processing);
    }
    else if (processing->source.type == ENDPOINT_V4L2 && processing->target.type == ENDPOINT_UVC)
    {
        processing_loop_v4l2_uvc(processing);
    }
    else
    {
        printf("PROCESSING: ERROR - Missing loop\n");
    }
}
