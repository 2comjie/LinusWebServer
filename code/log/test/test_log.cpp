#include <stdio.h>

#include "log.h"

int main(void) {
    char buf[1024];
    Log::instance()->init(1);
    while (true) {
        scanf("%s", buf);
        if (strcmp("q", buf) == 0) {
            break;
        }
        LOG_INFO("%s", buf);
    }
    return 0;
}