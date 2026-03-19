#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SCALE       8
#define EDGE_FACTOR 16
#define N_NODES     (1 << SCALE)
#define N_EDGES     (N_NODES * EDGE_FACTOR)
#define N_LEVELS    10
#define MAX_LEVEL INT8_MAX

typedef uint64_t edge_index_t;
typedef uint64_t node_index_t;
typedef int8_t level_t;

typedef struct {
    node_index_t dst;
} edge_t;

typedef struct {
    edge_index_t edge_begin;
    edge_index_t edge_end;
} node_t;

// Function for initializing the BFS structure
void init_bfs(node_index_t queue[N_NODES], level_t level[N_NODES],
              edge_index_t level_counts[N_LEVELS], node_index_t *q_in,
              node_index_t *q_out, node_index_t starting_node) {
    *q_in = 1;
    *q_out = 0;
    level[starting_node] = 0;
    level_counts[0] = 1;
    queue[N_NODES - 1] = starting_node; // Using the macro Q_PUSH equivalent
}

// Function for processing a node's neighbors
void process_neighbors(node_t nodes[N_NODES], edge_t edges[N_EDGES],
                       level_t level[N_NODES], edge_index_t level_counts[N_LEVELS],
                       node_index_t n, node_index_t queue[N_NODES], node_index_t *q_in) {
    edge_index_t tmp_begin = nodes[n].edge_begin;
    edge_index_t tmp_end = nodes[n].edge_end;

    for (edge_index_t e = tmp_begin; e < tmp_end; e++) {
        node_index_t tmp_dst = edges[e].dst;
        level_t tmp_level = level[tmp_dst];
        if (tmp_level == MAX_LEVEL) {
            level_t new_level = level[n] + 1;
            level[tmp_dst] = new_level;
            ++level_counts[new_level];
            queue[*q_in == 0 ? N_NODES - 1 : *q_in - 1] = tmp_dst; // Q_PUSH equivalent
            *q_in = (*q_in + 1) % N_NODES;
        }
    }
}

// Function for traversing the queue
void traverse_queue(node_t nodes[N_NODES], edge_t edges[N_EDGES],
                    node_index_t queue[N_NODES], node_index_t *q_in,
                    node_index_t *q_out, level_t level[N_NODES],
                    edge_index_t level_counts[N_LEVELS]) {
    node_index_t dummy;
    for (dummy = 0; dummy < N_NODES; dummy++) {
        if (*q_in > *q_out ? *q_in == *q_out + 1 : (*q_in == 0) && (*q_out == N_NODES - 1))  // Q_EMPTY equivalent
            break;

        node_index_t n = queue[*q_out];
        *q_out = (*q_out + 1) % N_NODES; // Q_POP equivalent

        process_neighbors(nodes, edges, level, level_counts, n, queue, q_in);
    }
}

// Top-level bfs function that calls the other stages
void bfs(node_t nodes[N_NODES], edge_t edges[N_EDGES],
         node_index_t starting_node, level_t level[N_NODES],
         edge_index_t level_counts[N_LEVELS]) {
    node_index_t queue[N_NODES];
    node_index_t q_in, q_out;

    // Initialize the BFS structure
    init_bfs(queue, level, level_counts, &q_in, &q_out, starting_node);

    // Traverse the queue and process neighbors in parallel stages
    traverse_queue(nodes, edges, queue, &q_in, &q_out, level, level_counts);
}
