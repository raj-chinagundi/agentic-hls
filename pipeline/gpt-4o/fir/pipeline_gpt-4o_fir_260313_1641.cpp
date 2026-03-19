#include <stdio.h>
#include <hls_stream.h>

#define NUM_TAPS 4

// Stage 1: Update Delay Line
void update_delay_line(int input, int delay_line[NUM_TAPS]) {
    for (int i = NUM_TAPS - 1; i > 0; i--) {
        #pragma HLS UNROLL
        delay_line[i] = delay_line[i - 1];
    }
    delay_line[0] = input;
}

// Stage 2: Compute Output
int compute_output(int delay_line[NUM_TAPS], int taps[NUM_TAPS]) {
    int result = 0;
    for (int i = 0; i < NUM_TAPS; i++) {
        #pragma HLS PIPELINE II=1 // Initiation interval set to 1
        result += delay_line[i] * taps[i];
    }
    return result;
}

// Top-level FIR function, which calls the split stages
void fir(int input, int *output, int taps[NUM_TAPS]) {
    #pragma HLS DATAFLOW // Enable task-level pipelining

    static int delay_line[NUM_TAPS];

    update_delay_line(input, delay_line);
    *output = compute_output(delay_line, taps);
}

int main() {
    int taps[NUM_TAPS] = {1, 2, 2, 1}; // Simple low-pass coefficients
    int output;

    for (int i = 0; i < 100000000; i++) {
        fir(i % 256, &output, taps);
    }

    printf("Last output: %d\n", output);
    return 0;
}
