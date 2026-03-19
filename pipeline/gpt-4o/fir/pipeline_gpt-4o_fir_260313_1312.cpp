#include <stdio.h>
#include "fir.h"

// Stage 1: Shifting the delay line
void shift_delay_line(int delay_line[NUM_TAPS]) {
    for (int i = NUM_TAPS - 1; i > 0; i--) {  // Loop handling refers to approach of splitting tasks.
        delay_line[i] = delay_line[i - 1];
    }
}

// Stage 2: Updating the delay line with the new input
void update_delay_line(int delay_line[NUM_TAPS], int input) {
    delay_line[0] = input;
}

// Stage 3: Convolution computation
int compute_convolution(const int delay_line[NUM_TAPS], const int taps[NUM_TAPS]) {
    int result = 0;
    for (int i = 0; i < NUM_TAPS; i++) {  // Unroll loop to parallelize computation.
        result += delay_line[i] * taps[i];
    }
    return result;
}

// Top-level FIR function integrating all stages
void fir(int input, int *output, int taps[NUM_TAPS]) {
    static int delay_line[NUM_TAPS] = {};

    shift_delay_line(delay_line);
    update_delay_line(delay_line, input);
    *output = compute_convolution(delay_line, taps);
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
