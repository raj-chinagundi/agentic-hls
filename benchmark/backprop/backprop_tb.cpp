#include <stdio.h>
#include <stdlib.h>

#define TYPE double
#define input_dimension  13
#define possible_outputs  3
#define training_sets   163
#define nodes_per_layer  64

void backprop(TYPE weights1[input_dimension * nodes_per_layer],
              TYPE weights2[nodes_per_layer * nodes_per_layer],
              TYPE weights3[nodes_per_layer * possible_outputs],
              TYPE biases1[nodes_per_layer],
              TYPE biases2[nodes_per_layer],
              TYPE biases3[possible_outputs],
              TYPE training_data[training_sets * input_dimension],
              TYPE training_targets[training_sets * possible_outputs]);

int main() {
    static TYPE weights1[input_dimension * nodes_per_layer];
    static TYPE weights2[nodes_per_layer * nodes_per_layer];
    static TYPE weights3[nodes_per_layer * possible_outputs];
    static TYPE biases1[nodes_per_layer];
    static TYPE biases2[nodes_per_layer];
    static TYPE biases3[possible_outputs];
    static TYPE training_data[training_sets * input_dimension];
    static TYPE training_targets[training_sets * possible_outputs];
    int i, run;

    for (i = 0; i < input_dimension * nodes_per_layer; i++)
        weights1[i] = 0.01 * (i % 100 + 1);
    for (i = 0; i < nodes_per_layer * nodes_per_layer; i++)
        weights2[i] = 0.01 * (i % 100 + 1);
    for (i = 0; i < nodes_per_layer * possible_outputs; i++)
        weights3[i] = 0.01 * (i % 100 + 1);
    for (i = 0; i < nodes_per_layer; i++) {
        biases1[i] = 0.1;
        biases2[i] = 0.1;
    }
    for (i = 0; i < possible_outputs; i++)
        biases3[i] = 0.1;
    for (i = 0; i < training_sets * input_dimension; i++)
        training_data[i] = 0.5 + 0.01 * (i % 50);
    for (i = 0; i < training_sets * possible_outputs; i++)
        training_targets[i] = (i % possible_outputs == 0) ? 1.0 : 0.0;

    for (run = 0; run < 5; run++) {
        backprop(weights1, weights2, weights3,
                 biases1, biases2, biases3,
                 training_data, training_targets);
    }

    printf("weights1[0]: %f\n", weights1[0]);
    return 0;
}
