#include <stdio.h>

#include "../threadpool.h"
int main(void) {
    ThreadPool pool = ThreadPool(10);
    for (int i = 0; i < 10; ++i) {
        pool.addTask([j = i] {
            printf("hello world %d\n", j);
        });
    }

    while (1);
    return 0;
}