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

typedef struct {
    node_index_t dst;
} edge_t;

typedef struct {
    edge_index_t edge_begin;
    edge_index_t edge_end;
} node_t;

typedef int8_t level_t;
#define MAX_LEVEL INT8_MAX

void initialize(
    node_index_t queue[N_NODES],
    level_t level[N_NODES],
    edge_index_t level_counts[N_LEVELS],
    node_index_t &q_in,
    node_index_t &q_out,
    node_index_t starting_node
) {
    q_in  = 1;
    q_out = 0;
    level[starting_node] = 0;
    level_counts[0] = 1;
    queue[0] = starting_node;
}

bool is_queue_empty(node_index_t q_in, node_index_t q_out) {
    return (q_in > q_out ? q_in == q_out + 1 : (q_in == 0) && (q_out == N_NODES - 1));
}

node_index_t queue_peek(node_index_t queue[N_NODES], node_index_t q_out) {
    return queue[q_out];
}

void queue_pop(node_index_t &q_out) {
    q_out = (q_out + 1) % N_NODES;
}

void queue_push(node_index_t queue[N_NODES], node_index_t &q_in, node_index_t node) {
    queue[q_in == 0 ? N_NODES - 1 : q_in - 1] = node;
    q_in = (q_in + 1) % N_NODES;
}

void process_neighbors(
    node_index_t n,
    node_t nodes[N_NODES],
    edge_t edges[N_EDGES],
    level_t level[N_NODES],
    edge_index_t level_counts[N_LEVELS],
    node_index_t queue[N_NODES],
    node_index_t &q_in
) {
    edge_index_t tmp_begin = nodes[n].edge_begin;
    edge_index_t tmp_end   = nodes[n].edge_end;

    for (edge_index_t e = tmp_begin; e < tmp_end; e++) {
        node_index_t tmp_dst   = edges[e].dst;
        level_t      tmp_level = level[tmp_dst];
        if (tmp_level == MAX_LEVEL) {
            level_t new_level = level[n] + 1;
            level[tmp_dst] = new_level;
            ++level_counts[new_level];
            queue_push(queue, q_in, tmp_dst);
        }
    }
}

void bfs(
    node_t nodes[N_NODES], edge_t edges[N_EDGES],
    node_index_t starting_node, level_t level[N_NODES],
    edge_index_t level_counts[N_LEVELS]
) {
    node_index_t queue[N_NODES];
    node_index_t q_in, q_out;
    node_index_t n;

    initialize(queue, level, level_counts, q_in, q_out, starting_node);

    while (true) {
        if (is_queue_empty(q_in, q_out))
            break;
        n = queue_peek(queue, q_out);
        queue_pop(q_out);
        process_neighbors(n, nodes, edges, level, level_counts, queue, q_in);
    }
}
