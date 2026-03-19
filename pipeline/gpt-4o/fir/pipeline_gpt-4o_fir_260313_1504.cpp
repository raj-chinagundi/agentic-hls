#include <stdio.h>
#include "fir.h"

// Stage 1: Shift Delay Line
void shift_delay_line(int delay_line[NUM_TAPS]) {
    for (int i = NUM_TAPS - 1; i > 0; i--) {
        delay_line[i] = delay_line[i - 1];
    }
}

// Stage 2: Update Delay Line
void update_delay_line(int input, int delay_line[NUM_TAPS]) {
    delay_line[0] = input;
}

// Stage 3: Compute Result
int compute_result(int delay_line[NUM_TAPS], int taps[NUM_TAPS]) {
    int result = 0;
    for (int i = 0; i < NUM_TAPS; i++) {
        result += delay_line[i] * taps[i];
    }
    return result;
}

// Top-level function: FIR filter
void fir(int input, int *output, int taps[NUM_TAPS]) {
    static int delay_line[NUM_TAPS] = {};

    // Stage 1: Shift delay line
    shift_delay_line(delay_line);

    // Stage 2: Update delay line
    update_delay_line(input, delay_line);

    // Stage 3: Compute result
    *output = compute_result(delay_line, taps);
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
