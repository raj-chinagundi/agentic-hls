#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Breadth-First Search using pipelined stages
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

// Stream and queue structures for pipelining
typedef struct {
    node_index_t data[N_NODES];
    node_index_t in;
    node_index_t out;
} queue_t;

typedef struct {
    node_index_t node;
    edge_index_t edge_begin;
    edge_index_t edge_end;
} frontier_t;

// Helper functions for circular queue
static inline bool q_empty(queue_t *q) {
    return (q->in > q->out) ? (q->in == q->out + 1) : 
           (q->in == 0 && q->out == N_NODES - 1);
}

static inline void q_push(queue_t *q, node_index_t node) {
    q->data[q->in == 0 ? N_NODES - 1 : q->in - 1] = node;
    q->in = (q->in + 1) % N_NODES;
}

static inline node_index_t q_pop(queue_t *q) {
    node_index_t node = q->data[q->out];
    q->out = (q->out + 1) % N_NODES;
    return node;
}

// ========== PIPELINE STAGES ==========

// Stage 1: Frontier Extraction
// Dequeues nodes and reads their edge ranges
void stage_frontier_extract(
    queue_t *current_queue,
    hls::stream<frontier_t> &frontier_stream,
    node_t nodes[N_NODES],
    bool *queue_empty
) {
    #pragma HLS INTERFACE ap_ctrl_none port=return
    #pragma HLS INTERFACE axis register both port=frontier_stream
    #pragma HLS INTERFACE bram port=nodes
    
    *queue_empty = q_empty(current_queue);
    
    while (!q_empty(current_queue)) {
        #pragma HLS PIPELINE II=1
        
        node_index_t node = q_pop(current_queue);
        frontier_t frontier;
        frontier.node = node;
        frontier.edge_begin = nodes[node].edge_begin;
        frontier.edge_end = nodes[node].edge_end;
        
        frontier_stream.write(frontier);
    }
}

// Stage 2: Edge Traversal
// Processes each edge in the frontier, checks visitation status
void stage_edge_traversal(
    hls::stream<frontier_t> &frontier_stream,
    hls::stream<node_index_t> &neighbor_stream,
    hls::stream<node_index_t> &level_stream,
    edge_t edges[N_EDGES],
    level_t level[N_NODES],
    level_t current_level
) {
    #pragma HLS INTERFACE ap_ctrl_none port=return
    #pragma HLS INTERFACE axis register both port=frontier_stream
    #pragma HLS INTERFACE axis register both port=neighbor_stream
    #pragma HLS INTERFACE axis register both port=level_stream
    #pragma HLS INTERFACE bram port=edges
    #pragma HLS INTERFACE bram port=level
    
    while (!frontier_stream.empty()) {
        #pragma HLS PIPELINE II=1
        
        frontier_t frontier = frontier_stream.read();
        
        // Process all edges for this frontier node
        for (edge_index_t e = frontier.edge_begin; e < frontier.edge_end; e++) {
            #pragma HLS PIPELINE II=1
            #pragma HLS LOOP_FLATTEN
            
            node_index_t neighbor = edges[e].dst;
            level_t neighbor_level = level[neighbor];
            
            // Check if neighbor is unvisited
            if (neighbor_level == MAX_LEVEL) {
                neighbor_stream.write(neighbor);
                level_stream.write(frontier.node);  // Pass parent level info
            }
        }
    }
}

// Stage 3: Level Update & Enqueue
// Atomically updates levels and enqueues newly discovered nodes
void stage_level_update(
    hls::stream<node_index_t> &neighbor_stream,
    hls::stream<node_index_t> &level_stream,
    queue_t *next_queue,
    level_t level[N_NODES],
    edge_index_t level_counts[N_LEVELS],
    level_t current_level
) {
    #pragma HLS INTERFACE ap_ctrl_none port=return
    #pragma HLS INTERFACE axis register both port=neighbor_stream
    #pragma HLS INTERFACE axis register both port=level_stream
    #pragma HLS INTERFACE bram port=level
    #pragma HLS INTERFACE bram port=level_counts
    
    while (!neighbor_stream.empty()) {
        #pragma HLS PIPELINE II=1
        
        node_index_t neighbor = neighbor_stream.read();
        node_index_t parent = level_stream.read();
        level_t new_level = level[parent] + 1;
        
        // Atomic check-and-set (using HLS mutex or separate buffer)
        // For simplicity, we assume no conflicts within same level
        // In practice, use atomic operations or separate buffers per thread
        if (level[neighbor] == MAX_LEVEL) {
            level[neighbor] = new_level;
            level_counts[new_level]++;
            q_push(next_queue, neighbor);
        }
    }
}

// ========== TOP-LEVEL FUNCTION ==========

void bfs(
    node_t nodes[N_NODES],
    edge_t edges[N_EDGES],
    node_index_t starting_node,
    level_t level[N_NODES],
    edge_index_t level_counts[N_LEVELS]
) {
    #pragma HLS INTERFACE ap_ctrl_none port=return
    #pragma HLS INTERFACE bram port=nodes
    #pragma HLS INTERFACE bram port=edges
    #pragma HLS INTERFACE bram port=level
    #pragma HLS INTERFACE bram port=level_counts
    
    // Initialize
    #pragma HLS ARRAY_PARTITION variable=level complete dim=1
    
    for (node_index_t i = 0; i < N_NODES; i++) {
        #pragma HLS PIPELINE II=1
        level[i] = MAX_LEVEL;
    }
    
    for (int i = 0; i < N_LEVELS; i++) {
        #pragma HLS PIPELINE II=1
        level_counts[i] = 0;
    }
    
    level[starting_node] = 0;
    level_counts[0] = 1;
    
    // Dual-queue ping-pong buffer
    queue_t queues[2];
    #pragma HLS ARRAY_PARTITION variable=queues complete dim=1
    
    queue_t *current_queue = &queues[0];
    queue_t *next_queue = &queues[1];
    
    // Initialize queues
    current_queue->in = 1;
    current_queue->out = 0;
    next_queue->in = 0;
    next_queue->out = 0;
    
    q_push(current_queue, starting_node);
    
    // Streams for inter-stage communication
    hls::stream<frontier_t> frontier_stream;
    hls::stream<node_index_t> neighbor_stream;
    hls::stream<node_index_t> level_stream;
    
    #pragma HLS STREAM variable=frontier_stream depth=32
    #pragma HLS STREAM variable=neighbor_stream depth=256
    #pragma HLS STREAM variable=level_stream depth=256
    
    level_t current_level = 0;
    
    // Main BFS loop with pipelined stages
    bfs_level_loop: while (current_level < N_LEVELS && !q_empty(current_queue)) {
        #pragma HLS PIPELINE II=1
        
        bool queue_empty;
        
        // Dataflow region: stages run concurrently
        #pragma HLS DATAFLOW
        
        stage_frontier_extract(
            current_queue, 
            frontier_stream, 
            nodes,
            &queue_empty
        );
        
        stage_edge_traversal(
            frontier_stream, 
            neighbor_stream, 
            level_stream, 
            edges, 
            level, 
            current_level
        );
        
        stage_level_update(
            neighbor_stream, 
            level_stream, 
            next_queue, 
            level, 
            level_counts, 
            current_level
        );
        
        // Swap queues for next level
        if (queue_empty || neighbor_stream.empty()) {
            queue_t *temp = current_queue;
            current_queue = next_queue;
            next_queue = temp;
            
            // Reset next queue pointers
            next_queue->in = 0;
            next_queue->out = 0;
            
            current_level++;
        }
    }
}
