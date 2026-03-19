#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <hls_stream.h>
#include <hls_vector.h>

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

// Stream data structures for inter-stage communication
typedef struct {
    node_index_t node_idx;
    level_t node_level;
} node_stream_t;

typedef struct {
    edge_index_t edge_begin;
    edge_index_t edge_end;
    level_t parent_level;
} edge_range_stream_t;

// Stage 1: Queue management - pop nodes from current level queue
void stage1_queue_pop(
    node_index_t current_queue[N_NODES],
    node_index_t *current_in,
    node_index_t *current_out,
    hls::stream<node_stream_t> &node_stream,
    level_t level_array[N_NODES]
) {
    #pragma HLS INLINE off
    #pragma HLS INTERFACE ap_ctrl_none port=return
    
    while (*current_out != *current_in) {
        #pragma HLS PIPELINE II=1
        
        node_index_t node_idx = current_queue[*current_out];
        *current_out = (*current_out + 1) % N_NODES;
        
        node_stream_t item;
        item.node_idx = node_idx;
        item.node_level = level_array[node_idx];
        node_stream.write(item);
    }
    
    // Write end-of-level marker
    node_stream_t end_marker;
    end_marker.node_idx = N_NODES;  // Invalid index as marker
    end_marker.node_level = 0;
    node_stream.write(end_marker);
}

// Stage 2: Edge range lookup - get neighbor ranges for each node
void stage2_edge_lookup(
    hls::stream<node_stream_t> &node_stream,
    hls::stream<edge_range_stream_t> &edge_range_stream,
    node_t nodes[N_NODES]
) {
    #pragma HLS INLINE off
    #pragma HLS INTERFACE ap_ctrl_none port=return
    
    while (true) {
        #pragma HLS PIPELINE II=1
        
        node_stream_t node_item = node_stream.read();
        
        // Check for end-of-level marker
        if (node_item.node_idx == N_NODES) {
            edge_range_stream_t end_marker;
            end_marker.edge_begin = N_EDGES;  // Invalid index as marker
            end_marker.edge_end = 0;
            end_marker.parent_level = 0;
            edge_range_stream.write(end_marker);
            break;
        }
        
        edge_range_stream_t range_item;
        range_item.edge_begin = nodes[node_item.node_idx].edge_begin;
        range_item.edge_end = nodes[node_item.node_idx].edge_end;
        range_item.parent_level = node_item.node_level;
        edge_range_stream.write(range_item);
    }
}

// Stage 3: Neighbor processing - check/update levels and push to next queue
void stage3_neighbor_process(
    hls::stream<edge_range_stream_t> &edge_range_stream,
    node_index_t next_queue[N_NODES],
    node_index_t *next_in,
    edge_t edges[N_EDGES],
    level_t level_array[N_NODES],
    edge_index_t level_counts[N_LEVELS]
) {
    #pragma HLS INLINE off
    #pragma HLS INTERFACE ap_ctrl_none port=return
    
    while (true) {
        #pragma HLS PIPELINE II=1
        
        edge_range_stream_t range_item = edge_range_stream.read();
        
        // Check for end-of-level marker
        if (range_item.edge_begin == N_EDGES) {
            break;
        }
        
        // Process all neighbors for this edge range
        for (edge_index_t e = range_item.edge_begin; e < range_item.edge_end; e++) {
            #pragma HLS PIPELINE II=1
            #pragma HLS LOOP_FLATTEN
            
            node_index_t neighbor = edges[e].dst;
            level_t neighbor_level = level_array[neighbor];
            
            if (neighbor_level == MAX_LEVEL) {
                level_t new_level = range_item.parent_level + 1;
                level_array[neighbor] = new_level;
                level_counts[new_level]++;
                
                // Push to next level queue
                next_queue[*next_in] = neighbor;
                *next_in = (*next_in + 1) % N_NODES;
            }
        }
    }
}

// Top-level function with dataflow optimization
void bfs(
    node_t nodes[N_NODES],
    edge_t edges[N_EDGES],
    node_index_t starting_node,
    level_t level[N_NODES],
    edge_index_t level_counts[N_LEVELS]
) {
    #pragma HLS INTERFACE ap_ctrl_none port=return
    #pragma HLS INTERFACE m_axi port=nodes offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=edges offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi port=level offset=slave bundle=gmem2
    #pragma HLS INTERFACE m_axi port=level_counts offset=slave bundle=gmem3
    
    // Dual-queue implementation for level-by-level processing
    node_index_t current_queue[N_NODES];
    node_index_t next_queue[N_NODES];
    node_index_t current_in, current_out, next_in, next_out;
    level_t current_level;
    
    // Initialize all levels to unvisited
    for (node_index_t i = 0; i < N_NODES; i++) {
        #pragma HLS PIPELINE II=1
        level[i] = MAX_LEVEL;
    }
    
    // Initialize starting node
    level[starting_node] = 0;
    level_counts[0] = 1;
    current_queue[0] = starting_node;
    current_in = 1;
    current_out = 0;
    next_in = 0;
    next_out = 0;
    current_level = 0;
    
    // Create streams for inter-stage communication
    hls::stream<node_stream_t> node_stream;
    hls::stream<edge_range_stream_t> edge_range_stream;
    
    // Main BFS loop - process level by level
    while (current_out != current_in) {
        #pragma HLS DATAFLOW
        
        // Stage 1: Pop nodes from current level queue
        stage1_queue_pop(
            current_queue, &current_in, &current_out,
            node_stream, level
        );
        
        // Stage 2: Lookup edge ranges for nodes
        stage2_edge_lookup(
            node_stream, edge_range_stream, nodes
        );
        
        // Stage 3: Process neighbors and build next level queue
        stage3_neighbor_process(
            edge_range_stream, next_queue, &next_in,
            edges, level, level_counts
        );
        
        // Swap queues for next level
        node_index_t *temp_queue = current_queue;
        current_queue = next_queue;
        next_queue = temp_queue;
        current_in = next_in;
        current_out = 0;
        next_in = 0;
        next_out = 0;
        current_level++;
        
        // Safety check for maximum levels
        if (current_level >= N_LEVELS) {
            break;
        }
    }
}
