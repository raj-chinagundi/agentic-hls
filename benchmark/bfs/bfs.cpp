#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Breadth-First Search using a circular queue
// Graph: 256 nodes, 4096 edges

#define SCALE       8
#define EDGE_FACTOR 16
#define N_NODES     (1 << SCALE)
#define N_EDGES     (N_NODES * EDGE_FACTOR)
#define N_LEVELS    10

typedef uint64_t edge_index_t;
typedef uint64_t node_index_t;

typedef struct {
    node_index_t dst;
} edge_t;

typedef struct {
    edge_index_t edge_begin;
    edge_index_t edge_end;
} node_t;

typedef int8_t level_t;
#define MAX_LEVEL INT8_MAX

#define Q_PUSH(node) { queue[q_in == 0 ? N_NODES - 1 : q_in - 1] = node; q_in = (q_in + 1) % N_NODES; }
#define Q_PEEK()     (queue[q_out])
#define Q_POP()      { q_out = (q_out + 1) % N_NODES; }
#define Q_EMPTY()    (q_in > q_out ? q_in == q_out + 1 : (q_in == 0) && (q_out == N_NODES - 1))

void bfs(node_t nodes[N_NODES], edge_t edges[N_EDGES],
         node_index_t starting_node, level_t level[N_NODES],
         edge_index_t level_counts[N_LEVELS]) {
    node_index_t queue[N_NODES];
    node_index_t q_in, q_out;
    node_index_t dummy;
    node_index_t n;
    edge_index_t e;

    q_in  = 1;
    q_out = 0;
    level[starting_node] = 0;
    level_counts[0] = 1;
    Q_PUSH(starting_node);

    loop_queue: for (dummy = 0; dummy < N_NODES; dummy++) {
        if (Q_EMPTY())
            break;
        n = Q_PEEK();
        Q_POP();
        edge_index_t tmp_begin = nodes[n].edge_begin;
        edge_index_t tmp_end   = nodes[n].edge_end;
        loop_neighbors: for (e = tmp_begin; e < tmp_end; e++) {
            node_index_t tmp_dst   = edges[e].dst;
            level_t      tmp_level = level[tmp_dst];
            if (tmp_level == MAX_LEVEL) {
                level_t new_level = level[n] + 1;
                level[tmp_dst] = new_level;
                ++level_counts[new_level];
                Q_PUSH(tmp_dst);
            }
        }
    }
}
