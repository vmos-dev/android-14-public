
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <cstring>
#include <unistd.h>

#define VENDOR_REQ_TAG  0x56524551
#define VENDOR_READ_IO  _IOW('v', 0x01, unsigned int)

#define VENDOR_WRITE_IO _IOW('v', 0x02, unsigned int)

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;

struct rk_vendor_req
{
    u32 tag;
    u16 id;
    u16 len;
    u8 data[1];
};

static void print_hex_data(const char *s, struct rk_vendor_req *req, u32 len)
{
    u32 i;
    printf("%s \n", s);
    printf("  tag=0x%x id=%d len=%d \n", req->tag, req->id, req->len);
    u8 *data = req->data;
    printf("  ");
    for (i = 0; i < len; i += 1) printf("%c", data[i]);
    printf("\n");
}

int vendor_storage_read_test(u16 id, u16 len)
{
    int ret, sys_fd;
    u8 p_buf[2048] = {0};
    ; /* malloc req buffer or used extern buffer */
    struct rk_vendor_req *req;
    req    = (struct rk_vendor_req *)p_buf;
    sys_fd = open("/dev/vendor_storage", O_RDWR, 0);
    if (sys_fd < 0)
    {
        printf("vendor_storage open fail\n");
        return -1;
    }
    req->tag = VENDOR_REQ_TAG;
    req->id  = id;
    req->len = len; /* max read length to read*/
    ret      = ioctl(sys_fd, VENDOR_READ_IO, req);
    print_hex_data("vendor read:", req, req->len);
    /* return req->len is the real data length stored in the NV-storage */
    if (ret)
    {
        printf("vendor read printf\n");
        return -1;
    }
    close(sys_fd);
    return 0;
}

int vendor_storage_write_test(u16 id, u16 len, const char *str)
{
    u32 i;
    int ret, sys_fd;
    u8 p_buf[2048] = {0}; /* malloc req buffer or used extern buffer */
    struct rk_vendor_req *req;
    req    = (struct rk_vendor_req *)p_buf;
    sys_fd = open("/dev/vendor_storage", O_RDWR, 0);
    if (sys_fd < 0)
    {
        printf("vendor_storage open fail\n");
        return -1;
    }
    req->tag = VENDOR_REQ_TAG;
    req->id  = id;
    req->len = len; /* data len */

    for (i = 0; i < strlen(str); i++) req->data[i] = str[i];

    print_hex_data("vendor write:", req, req->len);
    ret = ioctl(sys_fd, VENDOR_WRITE_IO, req);
    if (ret)
    {
        printf("vendor write error\n");
        return -1;
    }
    close(sys_fd);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 4 && argc != 3)
    {
        printf("Error argc=%d\n", argc);
        printf("Usage : vendor_storage id len [code] \n");
        printf("\tConfig Code : vendor_storage 17 50 "
               "71581714-9736-4EEE-C029-B65E54280A91 \n");
        printf("\tRead Code   : vendor_storage 17 50 \n");
        return -1;
    }

    if (argc == 4)
    {
        u16 id  = atoi(argv[1]);
        u16 len = atoi(argv[2]);

        vendor_storage_write_test(id, len, argv[3]);
        vendor_storage_read_test(id, len);
    }

    if (argc == 3)
    {
        u16 id  = atoi(argv[1]);
        u16 len = atoi(argv[2]);

        vendor_storage_read_test(id, len);
    }
    return 0;
}
