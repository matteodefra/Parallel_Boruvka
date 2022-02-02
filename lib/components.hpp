#if !defined(__COMPONENTS_H)
#define __COMPONENTS_H

/**
 * Implementations of the required components in the farm, such as 
 * the different emitters and the workers (OLD)
 */

#include <iostream>
#include <vector>
#include "graph.hpp"
#include "queue.hpp"
#include "utimer.hpp"
#include <algorithm>
#include <thread>
#include "dset.hpp"

#define MY_EOS std::pair<uint,uint> (0,0)


/**
 * @brief           Splits the workload among the workers: it receives a number of edges, splits them in 
 *                  a fair division, and schedule the different components in a dynamic way through a 
 *                  feedback queue.
 * 
 * @param queues    connect the emitter to all the workers. The stored type is a pair of int, indicating
 *                  the starting and ending index of the edges vector (accessible from the worker, and global).
 * @param feedback  queue connecting each worker to the emitter for feedback to receive next chunk. Contains 
 *                  the thread id of the corresponding worker.
 * @param n         number of remaining edges to process.
 */
void emitter(
    // Queues from emitter to each worker
    std::vector<MyQueue<std::pair<uint,uint>>> & queues,
    MyQueue<short>& feedback,
    const size_t& n
) {

    // Total number of workers available
    int nw = queues.size();

    // Portion of edges for each worker
    size_t chunk_dim{ n / nw };

    // The starting index will be at zero
    size_t begin = 0;

    // The ending one is n if the workers are enough, otherwise the chunk_dim computed before
    size_t end = std::min(chunk_dim, n);

    // Iterate through all the available workers
    for (short i = 0; i < nw; i++) {
        // Create pair for the index in the edges vector
        std::pair<uint, uint> chunk (begin, end);

        // Push the pair in the correspoding queue
        queues[i].push(std::move(chunk));
        // Update begin index
        begin = end;
        // Same as before for the ending index
        end = std::min(begin + chunk_dim, n);
    }
    // Until workers have not finished
    while (nw > 0) {

        // Pop the last finished thread
        short thread_id = feedback.pop();

        // We reached the end of the chunks
        if (begin == n) {
            // Pass the MY_EOS value
            queues[thread_id].push(std::move(MY_EOS));
            // Worker terminates
            nw--;
        }
        else {
            // New pair to store into the queue
            std::pair<uint, uint> chunk (begin, end);
            queues[thread_id].push(std::move(chunk));
            // Updating begin and ending indexes
            begin = end;
            end = std::min(begin + chunk_dim, n);
        }
    }

}


/**
 * @brief               Compute the minimum edge found and store it in the appropriate position.
 * 
 * @param id            The correspoding thread id of the worker.
 * @param components    The UNION-FIND data structure of components.
 * @param thread_queue  The queue where edges list indexes are stored.
 * @param feedback      Feedback queue with the emitter to notify completion of the task.
 * @param local_edges   A vector of edges where the minimum is stored in the appropriate position.
 * @param graph         The entire graph data structure.
 * 
 * @example 
 *  id = 1
 *  components = [ {0, parent=0}, {1, parent=1}, {2, parent=2}, {3, parent=0}, {4, parent=1}, {5, parent=2} ] 
 *  thread_queue = [ (0,1) ]
 *  feedback = [] when task is completed [1]
 *  local_edges = [ {0,0,10}, {0,0,10}, {0,0,10}, {0,0,10}, {0,0,10}, {0,0,10} ] will be updated by the minimum edges found
 *  graph.edges = will be accessed only in position 0 and 1 (given by pair in the thread_queue) 
 */
void map_worker(
    const short &id,
    MyQueue<std::pair<uint,uint>>& thread_queue,
    MyQueue<short>& feedback,
    std::vector<MyEdge>& local_edges,
    Graph& graph
) {

    // Initialize local edges with default values
    local_edges.assign(graph.originalNodes, {0,0,10});

    bool active = true;

    while (active) {

        std::pair<uint,uint> chunk_indexes = thread_queue.pop();

        if (chunk_indexes == MY_EOS) {
            // If MY_EOS is received, stop the thread
            active = false;
        }

        else {
            // Get the indexes of the edges array
            uint starting_index = chunk_indexes.first;

            uint ending_index = chunk_indexes.second;

            for (uint i = starting_index; i < ending_index; i++) {
                // Retrieve edge from graph
                MyEdge edge = graph.edges[i];

                if (local_edges[edge.from].weight > edge.weight) {
                    // Found edge with same starting node and minimum weight, update local_edge
                    local_edges[edge.from].weight = edge.weight;
                    local_edges[edge.from].from = edge.from;
                    local_edges[edge.from].to = edge.to;
                }
            }
            
            // When indexes are completed, push the thread id into the feedback queue
            feedback.push(id);
        }

    }

}


/**
 * @brief               Merge the local results of the threads local_edges into a global_edges array
 *                      We split the indexes in a way that each thread access specific indexes in the local_edges 
 *                      array and also in the global_edges array. 
 *                      We don't need a synchronization mechanism since there is no overlapping between the indexes.
 * 
 * @param id            The correspoding thread id of the worker.
 * @param thread_queue  The queue where indexes are stored.
 * @param feedback      Feedback queue with the emitter to notify completion of the task.
 * @param local_edges   All the vectors of local_edges of the different threads.
 * @param global_edges  The resulting vector of minimum edges.
 * 
 * @example 
 *  id = 1
 *  thread_queue = [ (0,1) ]    
 *  feedback = [] when task is completed [1]
 *  local_edges = [ 
 *                  [{1,2,4.5}, {0,0,10}, {0,0,10}, {0,0,10}], 
 *                  [...], 
 *                  [...], 
 *                  [...] 
 *                 ]        array of array filled with the minimum edges found by each thread
 * global_edges = [ ... ]   single array containing in the end the minimum edges found
 */
void merge_worker(
    const short &id,
    MyQueue<std::pair<uint,uint>>& thread_queue,
    MyQueue<short>& feedback,
    std::vector<std::vector<MyEdge>>& local_edges,
    std::vector<MyEdge>& global_edges
) {

    bool active = true;

    while (active) {

        std::pair<uint,uint> chunk_indexes = thread_queue.pop();

        if (chunk_indexes == MY_EOS) {
            // MY_EOS received, stop thread
            active = false;
        }

        else {
            // Get the indexes of the local_edges array
            uint starting_index = chunk_indexes.first;

            uint ending_index = chunk_indexes.second;

            // For each local_edge of each thread
            for (auto &local_edge : local_edges) {
                
                // Iterate through the indexes interval received
                for (uint i = starting_index; i < ending_index; i++) {
                    
                    if (local_edge[i].weight < global_edges[i].weight) {
                        // Update global_edges if the local_edges found by the thread i has a better weight
                        global_edges[i].weight = local_edge[i].weight;
                        global_edges[i].from = local_edge[i].from;
                        global_edges[i].to = local_edge[i].to;
                    }

                }

            }

            // When indexes are completed, push the thread id into the feedback queue
            feedback.push(id);
        }

    }

}


/**
 * @brief               Contraction worker responsible for updating the Union-Find data structure.
 *                      The accesses on the Union-find data structure are concurrent since by default it
 *                      is implemented with atomic primitives, so there is no problem of concurrency.
 * 
 * @param id            The correspoding thread id of the worker.
 * @param components    The UNION-FIND data structure of components.
 * @param thread_queue  The queue where indexes are stored.
 * @param feedback      Feedback queue with the emitter to notify completion of the task.
 * @param global_edges  The global_edges found in the current iteration.
 * @param MST           The current Minimum Spanning Tree.
 * 
 * @example 
 *  id = 1
 *  components = [ {0, parent=0}, {1, parent=1}, {2, parent=2}, {3, parent=0}, {4, parent=1}, {5, parent=2} ] 
 *  thread_queue = [ (0,1) ]   
 *  feedback = [] when task is completed [1]
 *  global_edges = [ ... ] single array containing in the end the minimum edges found
 *  MST = [ ... ] single array containing the minimum edges in the actual tree
 */
void contraction_worker(
    const short &id,
    DisjointSets& initialComponents,
    MyQueue<std::pair<uint,uint>>& thread_queue,
    MyQueue<short>& feedback,
    std::vector<MyEdge>& global_edges,
    std::atomic<int>& MST_weight
) {

    bool active = true;

    // Instantiate the default NULL_CONN object
    MyEdge NULL_CONN = {0, 0, 10};

    while (active) {

        std::pair<uint,uint> chunk_indexes = thread_queue.pop();

        if (chunk_indexes == MY_EOS) {
            // MY_EOS received, terminating
            active = false;
        }

        else {
            // Get the indexes of the global_edges array
            uint starting_index = chunk_indexes.first;

            uint ending_index = chunk_indexes.second;

            // Iterate through global_edges in the specific indexes
            for (uint i = starting_index; i < ending_index; i++) {

                // Retrieve the edge found
                MyEdge edge = global_edges[i];
                
                if (edge == NULL_CONN) {
                    // If edge has default value, we do nothing
                }
                else {
                    if (!initialComponents.same(edge.from, edge.to)) {
                        /**
                         * Access the UNION-FIND data structure and check if the starting node and ending node 
                         * have the same parent.
                         * If not, this means that we can unify the two trees 
                         */
                        initialComponents.unite(edge.from, edge.to);
                        // MST_weight.fetch_add(edge.weight);
                        // MST_weight += edge.weight;
                    }
                    else {
                        /**
                         * Nodes have the same parent (so they are in the same component).   
                         * We do nothing otherwise we create a cycle
                         */
                    }
                }
            }

            // When indexes are completed, push the thread id into the feedback queue
            feedback.push(id);
        }

    }

}



/**
 * @brief               Filter the edges in the graph, removing those that belongs to the same component 
 *                      (hence the have the same parent in the UNION-FIND data structure)
 * 
 * @param id            The correspoding thread id of the worker.
 * @param components    The UNION-FIND data structure of components.
 * @param thread_queue  The queue where indexes are stored.
 * @param feedback      Feedback queue with the emitter to notify completion of the task.
 * @param new_edges     The new array for the updated edges.
 * @param graph         The graph data structure.
 * 
 * @example 
 *  id = 1
 *  components = [ {0, parent=0}, {1, parent=1}, {2, parent=2}, {3, parent=0}, {4, parent=1}, {5, parent=2} ] 
 *  thread_queue = [ (0,1) ]   
 *  feedback = [] when task is completed [1]
 *  new_edges = [ ... ] single array containing in the end the remaining edges to explore
 *  graph = the graph data structure
 */
void filter_edge_worker(
    const short &id,
    DisjointSets& initialComponents,
    MyQueue<std::pair<uint,uint>>& thread_queue,
    MyQueue<short>& feedback,
    std::vector<MyEdge>& remaining_edges,
    Graph& graph
) {

    bool active = true;

    while (active) {

        std::pair<uint,uint> chunk_indexes = thread_queue.pop();

        if (chunk_indexes == MY_EOS) {
            // MY_EOS received, thread finishing
            active = false;
        }

        else {
            // Get the indexes 
            uint starting_index = chunk_indexes.first;

            uint ending_index = chunk_indexes.second;

            // Iterate through the received indexes 
            for (uint i = starting_index; i < ending_index; i++) {
                
                MyEdge edge = graph.edges[i];

                if ( !initialComponents.same(edge.from, edge.to) )
                    /**
                     * If the starting and the ending node of each graph's edge are not in the same component, 
                     * then we need to keep it for the next iteration.
                     * Otherwise we discard it.
                     */
                    remaining_edges.push_back(edge);
            }

            feedback.push(id);
        }

    }

}



/**
 * @brief               Filter the edges in the graph, removing those that belongs to the same component 
 *                      (hence the have the same parent in the UNION-FIND data structure)
 * 
 * @param id            The correspoding thread id of the worker.
 * @param components    The UNION-FIND data structure of components.
 * @param thread_queue  The queue where indexes are stored.
 * @param feedback      Feedback queue with the emitter to notify completion of the task.
 * @param new_nodes     The new array for the updated nodes.
 * @param graph         The graph data structure.
 * 
 * @example 
 *  id = 1
 *  components = [ {0, parent=0}, {1, parent=1}, {2, parent=2}, {3, parent=0}, {4, parent=1}, {5, parent=2} ] 
 *  thread_queue = [ (0,1) ]   
 *  feedback = [] when task is completed [1]
 *  new_edges = [ ... ] single array containing in the end the remaining nodes
 *  graph = the graph data structure
 */
void filter_node_worker(
    const short &id,
    DisjointSets& initialComponents,
    MyQueue<std::pair<uint,uint>>& thread_queue,
    MyQueue<short>& feedback,
    std::vector<uint>& remaining_nodes,
    Graph& graph
) {

    bool active = true;

    while (active) {

        std::pair<uint,uint> chunk_indexes = thread_queue.pop();

        if (chunk_indexes == MY_EOS) {
            // MY_EOS received, thread finishing
            active = false;
        }

        else {
            // Get the indexes 
            uint starting_index = chunk_indexes.first;

            uint ending_index = chunk_indexes.second;

            // Iterate through the received indexes 
            for (uint i = starting_index; i < ending_index; i++) {

                if ( initialComponents.parent(graph.nodes[i]) == graph.nodes[i] ) 
                    /**
                     * If the parent node is the same as the node itself, then we need to keep it also 
                     * for next iteration.
                     * Otherwise it is a child of another node and we can discard it.
                     */
                    remaining_nodes.push_back(i);
                
            }   

            feedback.push(id);
        }

    }

}


#endif 