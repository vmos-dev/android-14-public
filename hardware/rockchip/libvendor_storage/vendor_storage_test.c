/*
 *
 * Copyright 2024 Rockchip Electronics S.LSI Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vendor_storage.h"  // 确保引入正确的头文件

void usage(char *program_name) {
	fprintf(stderr, "Usage: %s w <test_id> <data>\n", program_name);
	fprintf(stderr, "       %s r <test_id>\n", program_name);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  w    Write to vendor storage\n");
	fprintf(stderr, "  r    Read from vendor storage\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        usage(argv[0]);
        return -1;
	}

    char mode = 0;  // 'w': write, 'r': read
    int test_id;

    if (strcmp(argv[1], "w") == 0 && argc == 4) {
        mode = 'w';
        test_id = atoi(argv[2]);  // 将命令行参数转换为整数
        char *data = argv[3];

        if (vendor_storage_init() < 0) {
            fprintf(stderr, "Error initializing vendor storage\n");
            return -1;
        }

        // 写入数据到vendor存储
        if (emmc_vendor_write(test_id, data, strlen(data) + 1) < 0) {  // Include null-terminator
            fprintf(stderr, "Error writing to vendor storage with ID %d\n", test_id);
            return -1;
        }
        printf("Data written successfully: %s\n", data);

    } else if (strcmp(argv[1], "r") == 0 && argc == 3) {
        mode = 'r';
        test_id = atoi(argv[2]);

        if (vendor_storage_init() < 0) {
            fprintf(stderr, "Error initializing vendor storage\n");
            return -1;
        }

        char read_buffer[256];  // Assume maximum data length for simplicity

        // 从vendor存储读取数据
        if (emmc_vendor_read(test_id, read_buffer, sizeof(read_buffer)) < 0) {
            fprintf(stderr, "Error reading from vendor storage with ID %d\n", test_id);
            return -1;
        }
        printf("Data read successfully: %s\n", read_buffer);

    } else {
        fprintf(stderr, "Invalid operation or incorrect parameters.\n");
        usage(argv[0]);
        return -1;
    }

    return 0;
}

