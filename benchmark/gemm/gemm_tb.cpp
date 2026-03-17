#include <stdio.h>
#include <stdlib.h>

#define TYPE double
#define row_size 64
#define col_size 64
#define N (row_size * col_size)

void gemm(TYPE m1[N], TYPE m2[N], TYPE prod[N]);

int main() {
    static TYPE m1[N], m2[N], prod[N];
    int i;

    for (i = 0; i < N; i++) {
        m1[i] = (TYPE)(i % 64) * 0.01 + 0.1;
        m2[i] = (TYPE)(i % 64) * 0.01 + 0.2;
        prod[i] = 0;
    }

    for (i = 0; i < 30; i++) {
        gemm(m1, m2, prod);
    }

    printf("prod[0]: %f\n", prod[0]);
    return 0;
}
