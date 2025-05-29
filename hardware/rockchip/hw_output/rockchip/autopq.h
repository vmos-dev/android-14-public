#ifndef AUTO_PQ_INTERFACE_H
#define AUTO_PQ_INTERFACE_H

#include <errno.h>
#include <inttypes.h>
#include <log/log.h>
#include <stdint.h>
#include <string>

#include "baseparameter_api.h"

namespace android {

#define AUTOPQ_PATH "/dev/block/by-name/autopq"

struct white_balance_data{
    unsigned short rgain;
    unsigned short ggain;
    unsigned short bgain;
};

struct auto_pq_info{
    char head_flag[6]; /* 头标识， "AUTOPQ" */
    unsigned short major_version; /* Auto PQ 大版本信息 */
    unsigned short minor_version; /* Auto PQ 小版本信息 */
    struct white_balance_data white_balance[4];
    struct gamma_lut_data gamma[8];
    struct cubic_lut_data cubic[6];
};

class AutoPQ {
public:
    AutoPQ();
    ~AutoPQ();
    static int get_auto_white_balance(unsigned short index, struct white_balance_data *data);
    static int set_auto_white_balance(unsigned short index, struct white_balance_data *data);
    static int get_auto_gamma(unsigned short index, struct gamma_lut_data *data);
    static int set_auto_gamma(unsigned short index, struct gamma_lut_data *data);
    static int get_auto_3d_lut(unsigned short index, struct cubic_lut_data *data);
    static int set_auto_3d_lut(unsigned short index, struct cubic_lut_data *data);
};

}
#endif
