#include <stdio.h>
#include "fir.h"

#define NUM_TAPS 4  // For demonstration purposes

// Stage 1: Update the delay line
void update_delay_line(int input, int delay_line[NUM_TAPS]) {
    #pragma HLS INLINE off
    
    for (int i = NUM_TAPS - 1; i > 0; i--) {
        #pragma HLS UNROLL // Try to unroll this loop for better performance
        delay_line[i] = delay_line[i - 1];
    }
    delay_line[0] = input;
}

// Stage 2: Compute the FIR result
void compute_result(int *result, int delay_line[NUM_TAPS], int taps[NUM_TAPS]) {
    #pragma HLS INLINE off
    
    int temp_result = 0;
    for (int i = 0; i < NUM_TAPS; i++) {
        #pragma HLS PIPELINE // Pipeline loop iterations
        temp_result += delay_line[i] * taps[i];
    }
    *result = temp_result;
}

// FIR filter function using dataflow model
void fir(int input, int *output, int taps[NUM_TAPS]) {
    #pragma HLS DATAFLOW // Enable dataflow optimization

    static int delay_line[NUM_TAPS] = {};
    int result = 0;

    // Pass data through pipeline stages
    update_delay_line(input, delay_line);
    compute_result(&result, delay_line, taps);

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
