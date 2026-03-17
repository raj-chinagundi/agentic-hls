#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define SIZE 2048
#define TYPE int32_t

void ms_mergesort(TYPE a[SIZE]);

int main() {
    static TYPE a[SIZE];
    int i, run;

    for (run = 0; run < 500; run++) {
        for (i = 0; i < SIZE; i++) {
            a[i] = (TYPE)(SIZE - i);
        }
        ms_mergesort(a);
    }

    printf("a[0]: %d  a[SIZE-1]: %d\n", (int)a[0], (int)a[SIZE - 1]);
    return 0;
}
