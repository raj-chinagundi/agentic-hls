#include <stdio.h>
#include "fir.h"

// Stage 1: Update the delay line
void update_delay_line(int input, int delay_lane[NUM_TAPS]) {
    for (int i = NUM_TAPS - 1; i > 0; i--) {
        delay_lane[i] = delay_lane[i - 1];
    }
    delay_lane[0] = input;
}

// Stage 2: Apply taps to the delay line
int apply_taps(int delay_lane[NUM_TAPS], int taps[NUM_TAPS]) {
    int result = 0;
    for (int i = 0; i < NUM_TAPS; i++) {
        result += delay_lane[i] * taps[i];
    }
    return result;
}

void fir(int input, int *output, int taps[NUM_TAPS]) {
    static int delay_lane[NUM_TAPS] = {};

    // Task pipelining using dataflow
    #pragma HLS dataflow

    update_delay_line(input, delay_lane);
    *output = apply_taps(delay_lane, taps);
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
