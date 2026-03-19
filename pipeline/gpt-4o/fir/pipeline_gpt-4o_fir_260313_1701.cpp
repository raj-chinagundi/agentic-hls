#include <stdio.h>
#include "fir.h"

// Stage 1: Update Delay Line
void update_delay_line(int input, int delay_line[NUM_TAPS]) {
    for (int i = NUM_TAPS - 1; i > 0; i--) {
        delay_line[i] = delay_line[i - 1];
    }
    delay_line[0] = input;
}

// Stage 2: Compute FIR Output
int compute_fir_output(int delay_line[NUM_TAPS], int taps[NUM_TAPS]) {
    int result = 0;
    for (int i = 0; i < NUM_TAPS; i++) {
        result += delay_line[i] * taps[i];
    }
    return result;
}

void fir(int input, int* output, int taps[NUM_TAPS]) {
    // Delay line used for storing past input values
    static int delay_line[NUM_TAPS] = {};

    // Update delay line stage
    update_delay_line(input, delay_line);

    // Compute FIR output stage
    *output = compute_fir_output(delay_line, taps);
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
