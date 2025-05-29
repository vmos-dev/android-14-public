/***************************************************************************
 *
 * Copyright 2015-2019 BES.
 * All rights reserved. All unpublished rights reserved.
 *
 * No part of this work may be used or reproduced in any form or by any
 * means, or stored in a database or retrieval system, without prior written
 * permission of BES.
 *
 * Use of this work is governed by a license granted by BES.
 * This work contains confidential and proprietary information of
 * BES. which is protected by copyright, trade secret,
 * trademark and other intellectual property rights.
 *
 ****************************************************************************/
/***
* cqueue.c - c circle queue c file
*/
#define LOG_TAG "cqueue"
    
#include <utils/Log.h>
#include <stdio.h>
#include <string.h>
#include "bes_cqueue.h"
#include "bt_vendor_bes.h"

//#define DEBUG_CQUEUE 1

int InitCQueue(CQueue *Q, unsigned int size, CQItemType *buf)
{
    Q->size = size;
    Q->base = buf;
    Q->len = 0;
    if (!Q->base)
        return CQ_ERR;

    Q->read = Q->write = 0;
    return CQ_OK;
}

int IsEmptyCQueue(CQueue *Q)
{
    if (Q->len == 0)
    {
        return CQ_OK;
    }
    else
    {
        return CQ_ERR;
    }
}

int LengthOfCQueue(CQueue *Q)
{
    return Q->len;
}

int AvailableOfCQueue(CQueue *Q)
{
    return (Q->size - Q->len);
}

int EnCQueue(CQueue *Q, CQItemType *e, unsigned int len)
{
    if (AvailableOfCQueue(Q) < len)
    {
        return CQ_ERR;
    }

    Q->len += len;
    uint32_t bytesToTheEnd = Q->size - Q->write;
    if (bytesToTheEnd > len)
    {
        if (e != NULL)
        {
            memcpy((uint8_t *)&Q->base[Q->write], (uint8_t *)e, len);
        }
        Q->write += len;
    }
    else
    {
        if (e != NULL)
        {
            memcpy((uint8_t *)&Q->base[Q->write], (uint8_t *)e, bytesToTheEnd);
            memcpy((uint8_t *)&Q->base[0], (((uint8_t *)e) + bytesToTheEnd), len - bytesToTheEnd);
        }
        Q->write = len - bytesToTheEnd;
    }

    return CQ_OK;
}

int EnCQueue_AI(CQueue *Q, CQItemType *e, unsigned int len, FRAME_LEN_GETTER_T getter)
{
    int ret = CQ_OK;

    if (AvailableOfCQueue(Q) < len)
    {
        if (getter)
        {
            do
            {
                uint16_t frameLen = getter(Q);
                DeCQueue(Q, NULL, frameLen);
                ALOGD("Warning: discard %d bytes from queue according getter", frameLen);
            } while (AvailableOfCQueue(Q) < len);
        }
        else
        {
            /// discard data from the head of the queue
            /// NOTE: may damage the frame structure if frame length is scalable
            DeCQueue(Q, NULL, len);
        }

        /// update the enqueue status
        ret = CQ_ERR;
    }

    EnCQueue(Q, e, len);

    return ret;
}

unsigned int GetCQueueReadOffset(CQueue *Q)
{
    ASSERT(Q);

    return Q->read;
}

unsigned int GetCQueueWriteOffset(CQueue *Q)
{
    ASSERT(Q);

    return Q->write;
}

int EnCQueueFront(CQueue *Q, CQItemType *e, unsigned int len)
{
    if (AvailableOfCQueue(Q) < len)
    {
        return CQ_ERR;
    }

    Q->len += len;

    /* walk to last item , revert write */
    e = e + len - 1;

    if (Q->read == 0)
    {
        Q->read = Q->size - 1;
    }
    else
    {
        Q->read--;
    }

    while (len > 0)
    {
        Q->base[Q->read] = *e;

        --Q->read;
        --e;
        --len;

        if (Q->read < 0)
            Q->read = Q->size - 1;
    }

    /* we walk one more, walk back */
    if (Q->read == Q->size - 1)
    {
        Q->read = 0;
    }
    else
    {
        ++Q->read;
    }

    return CQ_OK;
}

int DeCQueue(CQueue *Q, CQItemType *e, unsigned int len)
{
    if (LengthOfCQueue(Q) < len)
    {
        return CQ_ERR;
    }

    Q->len -= len;

    if (e != NULL)
    {
        uint32_t bytesToTheEnd = Q->size - Q->read;
        if (bytesToTheEnd > len)
        {
            memcpy((uint8_t *)e, (uint8_t *)&Q->base[Q->read], len);
            Q->read += len;
        }
        else
        {
            memcpy((uint8_t *)e, (uint8_t *)&Q->base[Q->read], bytesToTheEnd);
            memcpy((((uint8_t *)e) + bytesToTheEnd), (uint8_t *)&Q->base[0], len - bytesToTheEnd);
            Q->read = len - bytesToTheEnd;
        }
    }
    else
    {
        if (0 < Q->size)
        {
            Q->read = (Q->read + len) % Q->size;
        }
        else
        {
            Q->read = 0;
        }
    }

    return CQ_OK;
}

int PeekCQueue(CQueue *Q, unsigned int len_want, CQItemType **e1, unsigned int *len1, CQItemType **e2, unsigned int *len2)
{
    if (LengthOfCQueue(Q) < len_want)
    {
        return CQ_ERR;
    }

    *e1 = &(Q->base[Q->read]);
    if ((Q->write > Q->read) || (Q->size - Q->read >= len_want))
    {
        *len1 = len_want;
        *e2 = NULL;
        *len2 = 0;
        return CQ_OK;
    }
    else
    {
        *len1 = Q->size - Q->read;
        *e2 = &(Q->base[0]);
        *len2 = len_want - *len1;
        return CQ_OK;
    }

    return CQ_ERR;
}

int PeekCQueueToBuf(CQueue *Q, CQItemType *e, unsigned int len)
{
    int status = CQ_OK;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;

    status = PeekCQueue(Q, len, &e1, &len1, &e2, &len2);

    if (status == CQ_OK)
    {
        if (len == (len1 + len2))
        {
            memcpy(e, e1, len1);
            memcpy(e + len1, e2, len2);
        }
        else
        {
            status = CQ_ERR;
        }
    }

    return status;
}

int PeekCQueueToBufWithOffset(CQueue *Q, CQItemType *e, unsigned int len_want, unsigned int offset)
{
    ASSERT(Q);
    ASSERT(e);
    int status = CQ_OK;
    if (offset < Q->write && (offset + len_want) > Q->write)
    {
        status = CQ_ERR;
        ASSERT(0);
    }

    if ((offset > Q->write) &&
        (offset + len_want > Q->size) &&
        (offset + len_want - Q->size > Q->write))
    {
        status = CQ_ERR;
        ASSERT(0);
    }

    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;

    e1 = &(Q->base[offset]);
    if ((Q->write > offset) || (Q->size - offset >= len_want))
    {
        len1 = len_want;
        e2 = NULL;
        len2 = 0;
    }
    else
    {
        len1 = Q->size - offset;
        e2 = &(Q->base[0]);
        len2 = len_want - len1;
    }

    if (len_want == (len1 + len2))
    {
        memcpy(e, e1, len1);
        memcpy(e + len1, e2, len2);
    }
    else
    {
        status = CQ_ERR;
    }

    return status;
}

int PullCQueue(CQueue *Q, CQItemType *e, unsigned int len)
{
    int status = CQ_OK;
    unsigned char *e1 = NULL, *e2 = NULL;
    unsigned int len1 = 0, len2 = 0;

    status = PeekCQueue(Q, len, &e1, &len1, &e2, &len2);

    if (status == CQ_OK)
    {
        if (len == (len1 + len2))
        {
            memcpy(e, e1, len1);
            memcpy(e + len1, e2, len2);
            DeCQueue(Q, 0, len);
        }
        else
        {
            status = CQ_ERR;
        }
    }

    return status;
}

void ResetCQueue(CQueue *Q)
{
    Q->len = 0;
    Q->read = Q->write = 0;
}


