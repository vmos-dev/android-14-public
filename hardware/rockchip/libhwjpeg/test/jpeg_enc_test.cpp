//#define LOG_NDEBUG 0
#define LOG_TAG "JpegEncoderTest"
#include <utils/Log.h>

#include <string.h>
#include <stdlib.h>
#include "MpiJpegEncoder.h"

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

    MpiJpegEncoder encoder;
    MpiJpegEncoder::OutputPacket_t pktOut;

    memset(&pktOut, 0, sizeof(pktOut));

    err = encoder.prepareEncoder();
    if (!err) {
        ALOGE("failed to prepare JPEG encoder");
        goto cleanUp;
    }

    err = encoder.updateEncodeCfg(
            720 /*width*/, 1080 /*height*/, MpiJpegEncoder::INPUT_FMT_YUV420SP);
    if (!err) {
        ALOGE("failed to update encode config");
        goto cleanUp;
    }

    err = encoder.encodeFrame(data, &pktOut);
    if (!err) {
        ALOGE("failed to encode packet");
        goto cleanUp;
    }

    /* TODO - Get diaplay for the PacketOut.
       * - Pakcet address: pktOut.data
       * - Pakcet size: pktOut.size */

    /* output buffer count within limits, so release frame buffer if one
       frame has been display successful. */
    encoder.deinitOutputPacket(&pktOut);

    encoder.flushBuffer();

cleanUp:
    if (data) {
        free(data);
        data = NULL;
    }

    return 0;
}
