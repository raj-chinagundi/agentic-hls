#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define TYPE int32_t
#define col_size 64
#define row_size 128
#define f_size 9

void stencil(TYPE orig[row_size * col_size], TYPE sol[row_size * col_size], TYPE filter[f_size]);

int main() {
    static TYPE orig[row_size * col_size];
    static TYPE sol[row_size * col_size];
    static TYPE filter[f_size];
    int i;

    for (i = 0; i < row_size * col_size; i++) {
        orig[i] = (TYPE)(i % 100) + 1;
        sol[i] = 0;
    }
    for (i = 0; i < f_size; i++) {
        filter[i] = 1;
    }

    for (i = 0; i < 100; i++) {
        stencil(orig, sol, filter);
    }

    printf("sol[0]: %d\n", (int)sol[0]);
    return 0;
}
