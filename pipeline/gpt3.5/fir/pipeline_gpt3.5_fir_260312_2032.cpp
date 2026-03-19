c
#include <stdio.h>
#include "fir.h"

// Stage 1: Update shift register with new sample
void shift_update(int input, int *delay_line) {
    #pragma HLS inline
    for (int i = NUM_TAPS - 1; i > 0; i--) {
        delay_line[i] = delay_line[i - 1];
    }
    delay_line[0] = input;
}

// Stage 2: Compute first half of dot product
void multiply_half1(int *delay_line, int taps[NUM_TAPS], int *sum1) {
    #pragma HLS inline
    int s = 0;
    for (int i = 0; i < NUM_TAPS/2; i++) {
        s += delay_line[i] * taps[i];
    }
    *sum1 = s;
}

// Stage 3: Compute second half of dot product
void multiply_half2(int *delay_line, int taps[NUM_TAPS], int *sum2) {
    #pragma HLS inline
    int s = 0;
    for (int i = NUM_TAPS/2; i < NUM_TAPS; i++) {
        s += delay_line[i] * taps[i];
    }
    *sum2 = s;
}

// Top-level function with dataflow pipelining
void fir(int input, int *output, int taps[NUM_TAPS]) {
    static int delay_line0[NUM_TAPS] = {0};
    static int delay_line1[NUM_TAPS] = {0};
    static int flag = 0;

    int *current_delay_line = (flag == 0) ? delay_line0 : delay_line1;
    int *next_delay_line = (flag == 0) ? delay_line1 : delay_line0;

    int sum1, sum2;

    #pragma HLS dataflow
    shift_update(input, next_delay_line);          // Stage 1
    multiply_half1(current_delay_line, taps, &sum1); // Stage 2
    multiply_half2(current_delay_line, taps, &sum2); // Stage 3

    *output = sum1 + sum2;
    flag = 1 - flag; // Toggle for next iteration
}

int main() {
    int taps[NUM_TAPS] = {1, 2, 2, 1};  // simple low-pass coefficients
    int output;

    // Run one extra iteration to account for 1-cycle latency
    for (int i = 0; i < 100000001; i++) {
        fir(i % 256, &output, taps);
    }

    printf("Last output: %d\n", output);
    return 0;
}
