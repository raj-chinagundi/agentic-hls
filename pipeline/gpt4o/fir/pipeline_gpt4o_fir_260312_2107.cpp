#include <stdio.h>
#include "fir.h"

#define STREAM_DEPTH 1

// Function prototypes
void update_delay_line(int input, int delay_line[NUM_TAPS]);
void compute_convolution(int delay_line[NUM_TAPS], int taps[NUM_TAPS], int *result);

// Top-level FIR function
void fir(int input, int *output, int taps[NUM_TAPS]) {
    static int delay_line[NUM_TAPS] = {};

    // Stage 1: Update Delay Line
    update_delay_line(input, delay_line);

    // Stage 2: Compute Convolution
    int result;
    compute_convolution(delay_line, taps, &result);

    *output = result;
}

// Stage 1: Update Delay Line
// This function shifts the delay line and inserts the new input
void update_delay_line(int input, int delay_line[NUM_TAPS]) {
    // Reference Example 1: Iterations considered as stages.
    Shift_DelayLine:
    for (int i = NUM_TAPS - 1; i > 0; i--) {
#pragma HLS UNROLL
        delay_line[i] = delay_line[i - 1];
    }
    delay_line[0] = input;
}

// Stage 2: Compute Convolution
// This function computes the convolution based on the current state of the delay line and filter taps
void compute_convolution(int delay_line[NUM_TAPS], int taps[NUM_TAPS], int *result) {
    // Reference Example 1: Iterations considered as stages.
    int acc = 0;
    Compute_Accumulation:
    for (int i = 0; i < NUM_TAPS; i++) {
#pragma HLS PIPELINE
        acc += delay_line[i] * taps[i];
    }
    *result = acc;
}

int main() {
    int taps[NUM_TAPS] = {1, 2, 2, 1};  // Simple low-pass coefficients
    int output;

    for (int i = 0; i < 100000000; i++) {
        fir(i % 256, &output, taps);
    }

    printf("Last output: %d\n", output);
    return 0;
}
