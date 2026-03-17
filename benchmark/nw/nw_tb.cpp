#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALEN 128
#define BLEN 128

void needwun(char SEQA[ALEN], char SEQB[BLEN],
             char alignedA[ALEN + BLEN], char alignedB[ALEN + BLEN],
             int M[(ALEN + 1) * (BLEN + 1)], char ptr[(ALEN + 1) * (BLEN + 1)]);

int main() {
    static char SEQA[ALEN] = "tcgacgaaataggatgacagcacgttctcgtattagagggccgcggtacaaaccaaatgctgcggcgtacagggcacggggcgctgttcgggagatcgggggaatcgtggcgtgggt";
    static char SEQB[BLEN] = "ttcgagggcgcgtgtcgcggtccatcgacatgcccggtcggtgggacgtgggcgcctgatatagaggaatgcgattggaaggtcggacgggtcggcgagttgggcccggtgaatct";
    static char alignedA[ALEN + BLEN];
    static char alignedB[ALEN + BLEN];
    static int  M[(ALEN + 1) * (BLEN + 1)];
    static char ptr_mat[(ALEN + 1) * (BLEN + 1)];
    int i;

    for (i = 0; i < 200; i++) {
        memset(alignedA, 0, sizeof(alignedA));
        memset(alignedB, 0, sizeof(alignedB));
        memset(M,        0, sizeof(M));
        memset(ptr_mat,  0, sizeof(ptr_mat));
        needwun(SEQA, SEQB, alignedA, alignedB, M, ptr_mat);
    }

    printf("alignedA[0]: %c\n", alignedA[0]);
    return 0;
}
