#include <stdio.h>
#include "fir.h"

#define STREAM_BUFFER_SIZE 256

// First stage: Shift the delay line.
void shift_delay_line(int input, int delay_line[NUM_TAPS]) {
    for (int i = NUM_TAPS - 1; i > 0; i--) {
        delay_line[i] = delay_line[i - 1];
    }
    delay_line[0] = input;
}

// Second stage: Compute the FIR result.
int compute_result(int delay_line[NUM_TAPS], int taps[NUM_TAPS]) {
    int result = 0;
    for (int i = 0; i < NUM_TAPS; i++) {
        result += delay_line[i] * taps[i];
    }
    return result;
}

// Top-level function that coordinates the stages.
void fir(int input, int *output, int taps[NUM_TAPS]) {
    static int delay_line[NUM_TAPS] = {};
    
    // Shift delay line stage
    shift_delay_line(input, delay_line);
    
    // Compute result stage
    int result = compute_result(delay_line, taps);

    // Output the result
    *output = result;
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
