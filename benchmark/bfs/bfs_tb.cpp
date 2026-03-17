#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SCALE       8
#define EDGE_FACTOR 16
#define N_NODES     (1 << SCALE)
#define N_EDGES     (N_NODES * EDGE_FACTOR)
#define N_LEVELS    10

typedef uint64_t edge_index_t;
typedef uint64_t node_index_t;
typedef int8_t   level_t;
#define MAX_LEVEL INT8_MAX

typedef struct { node_index_t dst; } edge_t;
typedef struct { edge_index_t edge_begin; edge_index_t edge_end; } node_t;

void bfs(node_t nodes[N_NODES], edge_t edges[N_EDGES],
         node_index_t starting_node, level_t level[N_NODES],
         edge_index_t level_counts[N_LEVELS]);

int main() {
    static node_t       nodes[N_NODES];
    static edge_t       edges[N_EDGES];
    static level_t      level[N_NODES];
    static edge_index_t level_counts[N_LEVELS];
    int i, run;

    for (i = 0; i < N_NODES; i++) {
        nodes[i].edge_begin = (edge_index_t)(i * EDGE_FACTOR);
        nodes[i].edge_end   = (edge_index_t)(i * EDGE_FACTOR + EDGE_FACTOR);
    }
    for (i = 0; i < N_EDGES; i++) {
        int src_node = i / EDGE_FACTOR;
        int offset   = i % EDGE_FACTOR;
        edges[i].dst = (node_index_t)((src_node + offset + 1) % N_NODES);
    }

    for (run = 0; run < 5000; run++) {
        memset(level,        MAX_LEVEL, sizeof(level));
        memset(level_counts, 0,         sizeof(level_counts));
        bfs(nodes, edges, 0, level, level_counts);
    }

    printf("level[1]: %d\n", (int)level[1]);
    return 0;
}
