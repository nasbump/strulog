#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "strulog.h"

/*
$ gcc -g -MD -o t test_strulog.c strulog.c
*/

int32_t main(int32_t argc, char *argv[]) {
    if(argc == 1) {
        printf("usage: %s test-count [test-by-linked]\n", argv[0]);
        return 1;
    }

    int32_t tot;
    char *str= "abcdefg1234567890qwertyuiopasdfghjklzxcvbnm";
    char *body = "pl,okmijn uhb ygvtfc rdxeszwazq098765421";
    int16_t iv16 = 1;
    int32_t iv32 = 10;
    int64_t iv64 = 100;
    uint16_t uv16 = 2;
    uint32_t uv32 = 20;
    uint64_t uv64 = 200;

    //logger_init(LOGGER_TRACE, 1<<20, 1, NULL);
    logger_init(LOGGER_TRACE, 1<<20, 1, "/dev/null");

    tot = (int32_t)atoi(argv[1]);
    if(argc == 2) {
        for(; tot > 0; --tot) {
            logger_warn("str=%s,iv32=%"PRId32",iv64=%"PRId64
                                                ",uv32=%"PRIu32",uv64=%"PRIu64
                                                ",msg=%s",
                                                str, iv32, iv64,
                                                uv32, uv64, body);
        }
    } else {
        for(; tot > 0; --tot) {
            slogw.string("str", str).
                int32("iv32", iv32).int64("iv64", iv64).
                uint32("uv32", uv32).uint64("uv64", uv64).
                post(body);
        }
    }
    return 0;
}

