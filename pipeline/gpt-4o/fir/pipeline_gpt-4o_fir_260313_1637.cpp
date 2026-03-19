#include <stdio.h>
#include "fir.h"

// Stage 1: Update the delay line
// Refers to the stage where each iteration of the loop is treated as an update stage for the delay line
void update_delay_line(int delay_lane[NUM_TAPS]) {
    for (int i = NUM_TAPS - 1; i > 0; i--) {
        delay_lane[i] = delay_lane[i - 1];
    }
}

// Stage 2: Insert the new input into delay line
void insert_input(int input, int delay_lane[NUM_TAPS]) {
    delay_lane[0] = input;
}

// Stage 3: Perform the convolution MAC operations
// Can be parallelized with loop unrolling for optimal performance
void compute_convolution(int delay_lane[NUM_TAPS], int taps[NUM_TAPS], int *output) {
    int result = 0;
    for (int i = 0; i < NUM_TAPS; i++) {
        result += delay_lane[i] * taps[i];
    }
    *output = result;
}

void fir(int input, int *output, int taps[NUM_TAPS]) {
    static int delay_lane[NUM_TAPS] = {};

    #pragma HLS DATAFLOW
    update_delay_line(delay_lane);
    insert_input(input, delay_lane);
    compute_convolution(delay_lane, taps, output);
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
