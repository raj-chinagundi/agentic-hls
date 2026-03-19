#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "hls_stream.h"

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

// Stream data structures for pipeline communication
typedef struct {
    node_index_t node;
    level_t node_level;
    edge_index_t edge_begin;
    edge_index_t edge_end;
    int valid;
} frontier_item_t;

typedef struct {
    node_index_t dst;
    level_t new_level;
    int valid;
} discovered_node_t;

// Stage 1: Dequeue nodes from frontier queue and read their edge ranges
void stage1_dequeue(
    node_t nodes[N_NODES],
    level_t level[N_NODES],
    hls::stream<node_index_t> &queue_in,
    hls::stream<frontier_item_t> &frontier_out,
    volatile int *done
) {
    #pragma HLS inline off
    #pragma HLS interface m_axi port=nodes offset=slave bundle=gmem0
    #pragma HLS interface m_axi port=level offset=slave bundle=gmem1
    #pragma HLS interface s_axilite port=done bundle=control
    
    while (1) {
        #pragma HLS pipeline II=1
        
        if (!queue_in.empty()) {
            node_index_t n = queue_in.read();
            frontier_item_t item;
            item.node = n;
            item.node_level = level[n];
            item.edge_begin = nodes[n].edge_begin;
            item.edge_end = nodes[n].edge_end;
            item.valid = 1;
            frontier_out.write(item);
        } else if (*done) {
            // Send termination token
            frontier_item_t end_token = {0, 0, 0, 0, 0};
            frontier_out.write(end_token);
            break;
        }
    }
}

// Stage 2: Explore neighbors of dequeued nodes
void stage2_explore(
    edge_t edges[N_EDGES],
    level_t level[N_NODES],
    hls::stream<frontier_item_t> &frontier_in,
    hls::stream<discovered_node_t> &discovered_out
) {
    #pragma HLS inline off
    #pragma HLS interface m_axi port=edges offset=slave bundle=gmem2
    #pragma HLS interface m_axi port=level offset=slave bundle=gmem3
    #pragma HLS interface s_axilite port=return bundle=control
    
    while (1) {
        #pragma HLS pipeline II=1
        
        frontier_item_t item = frontier_in.read();
        
        if (!item.valid) {
            // Propagate termination token
            discovered_node_t end_token = {0, 0, 0};
            discovered_out.write(end_token);
            break;
        }
        
        // Process all edges for this node
        for (edge_index_t e = item.edge_begin; e < item.edge_end; e++) {
            #pragma HLS pipeline II=1
            #pragma HLS dependence variable=level inter false
            
            node_index_t dst = edges[e].dst;
            level_t current_level = level[dst];
            
            if (current_level == MAX_LEVEL) {
                level_t new_level = item.node_level + 1;
                level[dst] = new_level;
                
                discovered_node_t discovered;
                discovered.dst = dst;
                discovered.new_level = new_level;
                discovered.valid = 1;
                discovered_out.write(discovered);
            }
        }
    }
}

// Stage 3: Update level counts and enqueue discovered nodes
void stage3_update_enqueue(
    edge_index_t level_counts[N_LEVELS],
    hls::stream<discovered_node_t> &discovered_in,
    hls::stream<node_index_t> &queue_out,
    volatile int *done
) {
    #pragma HLS inline off
    #pragma HLS interface m_axi port=level_counts offset=slave bundle=gmem4
    #pragma HLS interface s_axilite port=done bundle=control
    
    while (1) {
        #pragma HLS pipeline II=1
        
        discovered_node_t item = discovered_in.read();
        
        if (!item.valid) {
            *done = 1;  // Signal completion
            break;
        }
        
        // Update level count
        level_counts[item.new_level] += 1;
        
        // Enqueue discovered node
        queue_out.write(item.dst);
    }
}

// Top-level function with dataflow pragma
void bfs(
    node_t nodes[N_NODES],
    edge_t edges[N_EDGES],
    node_index_t starting_node,
    level_t level[N_NODES],
    edge_index_t level_counts[N_LEVELS]
) {
    #pragma HLS interface m_axi port=nodes offset=slave bundle=gmem0
    #pragma HLS interface m_axi port=edges offset=slave bundle=gmem1
    #pragma HLS interface m_axi port=level offset=slave bundle=gmem2
    #pragma HLS interface m_axi port=level_counts offset=slave bundle=gmem3
    #pragma HLS interface s_axilite port=starting_node bundle=control
    #pragma HLS interface s_axilite port=return bundle=control
    
    // Streams for pipeline communication
    hls::stream<node_index_t> queue_stream("queue_stream");
    hls::stream<frontier_item_t> frontier_stream("frontier_stream");
    hls::stream<discovered_node_t> discovered_stream("discovered_stream");
    
    // Control flag for pipeline termination
    volatile int pipeline_done = 0;
    
    // Initialize level array and level counts
    for (int i = 0; i < N_NODES; i++) {
        #pragma HLS pipeline II=1
        level[i] = MAX_LEVEL;
    }
    level[starting_node] = 0;
    
    for (int i = 0; i < N_LEVELS; i++) {
        #pragma HLS pipeline II=1
        level_counts[i] = 0;
    }
    level_counts[0] = 1;
    
    // Initialize queue with starting node
    queue_stream.write(starting_node);
    
    // Dataflow region - stages run concurrently
    #pragma HLS dataflow
    
    stage1_dequeue(nodes, level, queue_stream, frontier_stream, &pipeline_done);
    stage2_explore(edges, level, frontier_stream, discovered_stream);
    stage3_update_enqueue(level_counts, discovered_stream, queue_stream, &pipeline_done);
}
