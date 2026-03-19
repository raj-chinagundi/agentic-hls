cpp
#include <stdio.h>
#include "fir.h"

// Function to handle shifting in the delay line
void shift_delay_line(int input, int delay_line[NUM_TAPS]) {
    #pragma HLS INLINE
    for (int i = NUM_TAPS - 1; i > 0; i--) {
        delay_line[i] = delay_line[i - 1];
    }
    delay_line[0] = input;
}

// Function to compute the dot product with taps
int compute_dot_product(int delay_line[NUM_TAPS], int taps[NUM_TAPS]) {
    #pragma HLS INLINE
    int result = 0;
    for (int i = 0; i < NUM_TAPS; i++) {
        result += delay_line[i] * taps[i];
    }
    return result;
}

// Top-level FIR function
void fir(int input, int *output, int taps[NUM_TAPS]) {
    #pragma HLS DATAFLOW
    
    static int delay_line[NUM_TAPS] = {};
    
    shift_delay_line(input, delay_line);
    *output = compute_dot_product(delay_line, taps);
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
