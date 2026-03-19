#include <stdint.h>
#include <limits.h>

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
#define SCALE       8
#define EDGE_FACTOR 16
#define N_NODES     (1 << SCALE)
#define N_EDGES     (N_NODES * EDGE_FACTOR)
#define N_LEVELS    10

void queue_push(node_index_t queue[N_NODES], node_index_t &q_in, node_index_t node) {
    queue[q_in == 0 ? N_NODES - 1 : q_in - 1] = node;
    q_in = (q_in + 1) % N_NODES;
}

node_index_t queue_peek(node_index_t queue[N_NODES], node_index_t q_out) {
    return queue[q_out];
}

void queue_pop(node_index_t &q_out) {
    q_out = (q_out + 1) % N_NODES;
}

bool queue_empty(node_index_t q_in, node_index_t q_out) {
    return (q_in > q_out ? q_in == q_out + 1 : (q_in == 0) && (q_out == N_NODES - 1));
}

void process_node(const node_t nodes[N_NODES], const edge_t edges[N_EDGES], node_index_t n,
                  level_t level[N_NODES], level_t current_level, edge_index_t level_counts[N_LEVELS],
                  node_index_t queue[N_NODES], node_index_t &q_in) {
    edge_index_t tmp_begin = nodes[n].edge_begin;
    edge_index_t tmp_end = nodes[n].edge_end;
    
    for (edge_index_t e = tmp_begin; e < tmp_end; e++) {
        node_index_t tmp_dst = edges[e].dst;
        if (level[tmp_dst] == MAX_LEVEL) {
            level[tmp_dst] = current_level + 1;
            ++level_counts[current_level + 1];
            queue_push(queue, q_in, tmp_dst);
        }
    }
}

void bfs(node_t nodes[N_NODES], edge_t edges[N_EDGES],
         node_index_t starting_node, level_t level[N_NODES],
         edge_index_t level_counts[N_LEVELS]) {
    node_index_t queue[N_NODES];
    node_index_t q_in, q_out;
    node_index_t dummy;
    
    q_in = 1;
    q_out = 0;
    
    // Initialize starting node
    level[starting_node] = 0;
    level_counts[0] = 1;
    queue_push(queue, q_in, starting_node);
    
    level_t current_level = 0;
    
    loop_queue: for (dummy = 0; dummy < N_NODES; dummy++) {
        if (queue_empty(q_in, q_out)) {
            break;
        }

        node_index_t n = queue_peek(queue, q_out);
        queue_pop(q_out);
        
        process_node(nodes, edges, n, level, current_level, level_counts, queue, q_in);
        
        // Check for level completion
        if (queue_empty(q_in, q_out)) {
            current_level++;
        }
    }
}
