    //#define LOG_NDEBUG 0
#define LOG_TAG "JpegDecoderTest"
#include <utils/Log.h>

#include <string.h>
#include <stdlib.h>
#include "MpiJpegDecoder.h"

#define PACKET_SIZE             2048

int main() {
    bool err = false;
    char *data = NULL;
    size_t size = PACKET_SIZE;

    data = (char *)malloc(size);
    if (!data) {
        ALOGE("dec_test malloc failed");
        return 0;
    }

    MpiJpegDecoder decoder;
    MpiJpegDecoder::OutputFrame_t frameOut;

    memset(&frameOut, 0, sizeof(frameOut));

    err = decoder.prepareDecoder();
    if (!err) {
        ALOGE("failed to prepare JPEG decoder");
        goto cleanUp;
    }

    err = decoder.decodePacket(data, size, &frameOut);
    if (!err) {
        ALOGE("failed to decode packet");
        goto cleanUp;
    }

    /* TODO - Get diaplay for the frameOut.
          * - frame address: frameOut.MemVirAddr
          * - frame size: frameOut.OutputSize */

    /* output buffer count within limits, so release frame buffer if one
       frame has been display successfully. */
    decoder.deinitOutputFrame(&frameOut);

    decoder.flushBuffer();

cleanUp:
    if (data) {
        free(data);
        data = NULL;
    }

    return 0;
}
