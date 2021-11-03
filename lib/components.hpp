#if !defined(__COMPONENTS_H)
#define __COMPONENTS_H

/**
 * Implementations of the required components in the farm, such as 
 * the emitter, collector and the workers 
 */

#include <iostream>
#include <vector>
#include "graph.hpp"
#include "queue.hpp"
#include "utimer.hpp"
#include <algorithm>
#include <thread>
#include "dset.hpp"

#define EOS std::pair<uint,uint> (0,0)



/**
 * @brief               Splits the workload among the workers: it receives a certain number of components, splits them in 
 *                      a fair division, and schedule the different components in a dynamic way through a feedback queue 
 * 
 * @param queues        connecting the emitter to all the workers. The stored type is a pair of int, indicating
 *                      the starting and ending index of the components vector (accessible from the worker, and global)
 * 
 * @param feedback      queue connecting each worker to the emitter for feedback to receive next chunk. Contains 
 *                      the thread id of the corresponding worker
 * 
 * @param n             number of remaining components to process
 */
void emitter(
    // Queues from emitter to each worker
    std::vector<MyQueue<std::pair<uint,uint>>> & queues,
    MyQueue<short>& feedback,
    const size_t& n
) {

    // Total number of workers available
    int nw = queues.size();

    // Portion of components for each worker
    size_t chunk_dim{ n / nw };

    // The starting index will be at zero
    size_t begin = 0;

    // The ending one is n if the workers are enough, otherwise the chunk_dim computed before
    size_t end = std::min(chunk_dim, n);

    std::cout << "Starting emitter" << std::endl;

    // Iterate through all the available workers
    for (short i = 0; i < nw; i++) {
        // Create pair for the index in the component vector
        std::pair<uint, uint> chunk (begin, end);

        std::cout << chunk.first << "," << chunk.second << std::endl;
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
        std::cout << "Popped thread id " << thread_id << " from queue" << std::endl; 
        std::cout << "Value of begin " << begin << std::endl;
        std::cout << "Value of end " << end << std::endl;

        // We reached the end of the chunks
        if (begin == n) {
            std::cout << "Passing EOS" << std::endl;
            // Pass the EOS value
            queues[thread_id].push(EOS);
            // queues[thread_id].push(std::pair<uint,uint> (0,0));
            // Worker terminates
            nw--;
        }
        else {
            // New pair to store into the queue
            std::pair<uint, uint> chunk (begin, end);
            std::cout << "Passing last components" << std::endl;
            queues[thread_id].push(std::move(chunk));
            begin = end;
            end = std::min(begin + chunk_dim, n);
        }
    }

}


/**
 * @brief               Compute the minimum outgoing edge from each component
 * 
 * @param id            The correspoding thread id of the worker
 * @param components    The vector of components (vector of list of nodes)
 * @param thread_queue  The queue where components indexes are stored
 * @param feedback      Feedback queue with the emitter to notify completion of the task
 * @param local_edges   The map of edges where the minimum is stored
 * @param graph         The entire graph data structure
 * 
 * @example 
 *  id = 1
 *  components = [ {1, 3}, {2, 7}, {5, 4}, {6, 10}, {8, 9} ] 
 *  thread_queue = [ (0,1) ]    if nw = 3 then n/nw \approx 2 
 *  feedback = [] when task is completed [1]
 *  local_edges = [ (1,3),(2,7),(5,4),(6,10),(8,9) ]
 */
void map_worker(
    const short &id,
    DisjointSets& components,
    MyQueue<std::pair<uint,uint>>& thread_queue,
    MyQueue<short>& feedback,
    std::vector<MyEdge>& local_edges,
    Graph& graph
) {

    std::cout << "Thread " << id << " starting" << std::endl;

    for (int i = 0; i < graph.getNumNodes(); i++) {
        local_edges.push_back({0, 0, 10});
    }

    std::cout << "Total size of local_edges: " << local_edges.size() << std::endl;

    bool active = true;

    while (active) {

        std::pair<uint,uint> chunk_indexes = thread_queue.pop();

        // if (chunk_indexes.first == 0 && chunk_indexes.second == 0) {
        if (chunk_indexes == EOS) {
            std::cout << "Received EOS" << std::endl;
            active = false;
        }

        else {
            // Get the indexes of the edge array
            uint starting_index = chunk_indexes.first;

            uint ending_index = chunk_indexes.second;

            for (uint i = chunk_indexes.first; i < chunk_indexes.second; i++) {
                // Retrieve edge
                MyEdge &edge = graph.getEdges()[i];

                if (local_edges[edge.from].weight > edge.weight) {
                    // Found edge with same starting node and minimum weight
                    local_edges[edge.from].weight = edge.weight;
                    local_edges[edge.from].from = edge.from;
                    local_edges[edge.from].to = edge.to;
                }
            }

            std::cout << "Total size of local_edges: " << local_edges.size() << std::endl;
            
            feedback.push(id);
        }

    }
    // Here local_edges contains all the minimum connection for each component

}


/**
 * @brief               Compute the minimum outgoing edge from each component
 * 
 * @param id            The correspoding thread id of the worker
 * @param components    The vector of components (vector of list of nodes)
 * @param thread_queue  The queue where components indexes are stored
 * @param feedback      Feedback queue with the emitter to notify completion of the task
 * @param local_edges   The map of edges where the minimum is stored
 * @param graph         The entire graph data structure
 * 
 * @example 
 *  id = 1
 *  components = [ {1, 3}, {2, 7}, {5, 4}, {6, 10}, {8, 9} ] 
 *  thread_queue = [ (0,1) ]    if nw = 3 then n/nw \approx 2 
 *  feedback = [] when task is completed [1]
 *  local_edges = [ (1,3),(2,7),(5,4),(6,10),(8,9) ]
 */
void reduce_worker(
    const short &id,
    std::vector<std::vector<int>>& components,
    MyQueue<std::pair<uint,uint>>& thread_queue,
    MyQueue<short>& feedback
    // std::unordered_map<uint,Edge>& local_edges,
    // std::unordered_map<uint, std::vector<Edge>> graph
) {
    
}


#endif 