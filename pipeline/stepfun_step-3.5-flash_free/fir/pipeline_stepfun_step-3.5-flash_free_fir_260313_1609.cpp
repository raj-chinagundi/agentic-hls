#include <stdio.h>
#include "fir.h"

// Stage 1: Update delay line (shift and insert new sample)
void update_delay_line(
    int delay_lane_in[NUM_TAPS], 
    int input_sample, 
    int delay_lane_out[NUM_TAPS]
) {
    #pragma HLS INTERFACE ap_ctrl_none port=return
    #pragma HLS ARRAY_PARTITION variable=delay_lane_in complete
    #pragma HLS ARRAY_PARTITION variable=delay_lane_out complete
    
    // Shift operation (NUM_TAPS-1 assignments)
    for (int i = NUM_TAPS - 1; i > 0; i--) {
        delay_lane_out[i] = delay_lane_in[i - 1];
    }
    delay_lane_out[0] = input_sample;
}

// Stage 2: Compute MAC (partial sum for first half)
void compute_mac_part1(
    int delay_lane[NUM_TAPS], 
    int taps[NUM_TAPS], 
    int *partial_sum
) {
    #pragma HLS INTERFACE ap_ctrl_none port=return
    #pragma HLS ARRAY_PARTITION variable=delay_lane complete
    #pragma HLS ARRAY_PARTITION variable=taps complete
    
    int sum = 0;
    // Process first half of taps (NUM_TAPS/2 iterations)
    for (int i = 0; i < NUM_TAPS/2; i++) {
        #pragma HLS UNROLL
        sum += delay_lane[i] * taps[i];
    }
    *partial_sum = sum;
}

// Stage 3: Compute MAC (partial sum for second half + reduction)
void compute_mac_part2(
    int delay_lane[NUM_TAPS], 
    int taps[NUM_TAPS], 
    int partial_sum1, 
    int *output
) {
    #pragma HLS INTERFACE ap_ctrl_none port=return
    #pragma HLS ARRAY_PARTITION variable=delay_lane complete
    #pragma HLS ARRAY_PARTITION variable=taps complete
    
    int sum = partial_sum1;
    // Process second half of taps (NUM_TAPS/2 iterations)
    for (int i = NUM_TAPS/2; i < NUM_TAPS; i++) {
        #pragma HLS UNROLL
        sum += delay_lane[i] * taps[i];
    }
    *output = sum;
}

// Top-level function with dataflow pipelining
void fir_accelerator(
    int *input_stream, 
    int *output_stream, 
    int taps[NUM_TAPS], 
    int num_samples
) {
    #pragma HLS INTERFACE axis port=input_stream
    #pragma HLS INTERFACE axis port=output_stream
    #pragma HLS INTERFACE s_axilite port=taps bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=num_samples bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=return bundle=CTRL
    
    // Double-buffered delay lines for pipelining
    int delay_lane0[NUM_TAPS] = {0};
    int delay_lane1[NUM_TAPS] = {0};
    #pragma HLS ARRAY_PARTITION variable=delay_lane0 complete
    #pragma HLS ARRAY_PARTITION variable=delay_lane1 complete
    
    int *current_delay = delay_lane0;
    int *next_delay = delay_lane1;
    
    // Stream processing with dataflow between stages
    for (int i = 0; i < num_samples; i++) {
        #pragma HLS PIPELINE II=1
        
        int input_sample = input_stream[i];
        int partial_sum1, final_output;
        
        // Stage 1: Update delay line (produces next_delay)
        update_delay_line(current_delay, input_sample, next_delay);
        
        // Stages 2 & 3: Compute MAC (can run in parallel with Stage 1 of next iteration)
        compute_mac_part1(next_delay, taps, &partial_sum1);
        compute_mac_part2(next_delay, taps, partial_sum1, &final_output);
        
        output_stream[i] = final_output;
        
        // Swap buffers for next iteration
        int *temp = current_delay;
        current_delay = next_delay;
        next_delay = temp;
    }
}

// Modified main to use streaming interface
int main() {
    int taps[NUM_TAPS] = {1, 2, 2, 1};
    const int num_samples = 100000000;
    int input_stream[num_samples];
    int output_stream[num_samples];
    
    // Generate input stream
    for (int i = 0; i < num_samples; i++) {
        input_stream[i] = i % 256;
    }
    
    // Process entire stream
    fir_accelerator(input_stream, output_stream, taps, num_samples);
    
    printf("Last output: %d\n", output_stream[num_samples-1]);
    return 0;
}
