#include <stdio.h>
#include "fir.h"

// Stage 1: Shift delay line
void shift_delay_line(int delay_line[NUM_TAPS], int input) {
    #pragma HLS INLINE
    // Shift the delay line values
    for (int i = NUM_TAPS - 1; i > 0; i--) {
        #pragma HLS UNROLL
        delay_line[i] = delay_line[i - 1];
    }
    delay_line[0] = input;
}

// Stage 2: Compute the output as a convolution
int compute_convolution(int delay_line[NUM_TAPS], int taps[NUM_TAPS]) {
    #pragma HLS INLINE
    int result = 0;
    for (int i = 0; i < NUM_TAPS; i++) {
        #pragma HLS UNROLL
        result += delay_line[i] * taps[i];
    }
    return result;
}

// Top-level FIR function
void fir(int input, int *output, int taps[NUM_TAPS]) {
    #pragma HLS DATAFLOW
    static int delay_line[NUM_TAPS] = {};

    // Use the stage functions defined above
    shift_delay_line(delay_line, input);
    *output = compute_convolution(delay_line, taps);
}

int main() {
    int taps[NUM_TAPS] = {1, 2, 2, 1};  // simple low-pass coefficients
    int output;

    for (int i = 0; i < 100000000; i++) {
        fir(i % 256, &output, taps);
    }

    printf("Last output: %d\n", output);
    return 0;
}
