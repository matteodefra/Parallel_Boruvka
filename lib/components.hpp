
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
// #include "utils.hpp"

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


void findMinimumConnection(
    std::set<uint> component,
    std::vector<LightestConnection>& local_MST,
    std::unordered_map<uint, std::vector<Edge>> graph
) {
    float minimum = INFINITY;
    uint startingNode = 0;
    uint endingNode = 0;

    // Loop through node for each component
    for (auto node : component) {

        // Retrieve the list of edges
        std::vector<Edge> edges = graph[node];

        // Loop through list of edges
        for (Edge edge : edges) {

            // If adjacent node is not present in the same component, there no cycle are guaranteed
            if (component.find(edge.adjacent_node) == component.end()) {

                // Update minimum and edge for this component
                if (edge.weight < minimum) {

                    minimum = edge.weight;
                    startingNode = node;
                    endingNode = edge.adjacent_node;

                }

            }

        }

    }
    // At this point, minimum contains the lightest weight and the pair shortest_edge the starting and ending node

    LightestConnection lightest = {minimum, startingNode, endingNode};

    // std::cout << "Finding lightest edge: (" << lightest.startingNode << "," << lightest.endingNode << ") with weight: " << lightest.weight << std::endl;

    local_MST.push_back(lightest);

}


/**
 * @brief               Compute the minimum outgoing edge from each component
 * 
 * @param id            The correspoding thread id of the worker
 * @param components    The vector of components (vector of list of nodes)
 * @param edges         The list of edges where the minimum is stored
 * @param thread_queue  The queue where components indexes are stored
 * @param feedback      Feedback queue with the emitter to notify completion of the task
 * 
 * @example 
 *  id = 1
 *  components = [ {1, 3}, {2, 7}, {5, 4}, {6, 10}, {8, 9} ] 
 *  edges = [ (1,3),(2,7),(5,4),(6,10),(8,9) ]
 *  thread_queue = [ (0,1) ]    if nw = 3 then n/nw \approx 2 
 *  feedback = [] when task is completed [1]
 */
void map_worker(
    const short &id,
    std::vector<std::set<uint>>& components,
    MyQueue<std::pair<uint,uint>>& thread_queue,
    MyQueue<short>& feedback,
    std::vector<LightestConnection>& local_MST,
    std::unordered_map<uint, std::vector<Edge>> graph
) {

    std::cout << "Thread " << id << " starting" << std::endl;

    bool active = true;

    while (active) {

        std::pair<uint,uint> chunk_indexes = thread_queue.pop();

        // if (chunk_indexes.first == 0 && chunk_indexes.second == 0) {
        if (chunk_indexes == EOS) {
            std::cout << "Received EOS" << std::endl;
            active = false;
        }

        else {
            // Get the indexes
            uint starting_index = chunk_indexes.first;

            uint ending_index = chunk_indexes.second;

            // Extract portion of the vector required
            std::vector<std::set<uint>> components_to_analyze = {components.begin() + starting_index, components.begin() + ending_index};

            for (auto component : components_to_analyze) {
                // Find minimum spanning edge for each component
                findMinimumConnection(component, std::ref(local_MST), graph);
                // std::cout << "Local MST size: " << local_MST.size() << std::endl;
            }
            
            feedback.push(id);
        }

    }
    // Here local_MST contains all the minimum connection for each component

}


// void reduce_worker(
//     const short &id,
//     std::vector<std::vector<int>>& components,
//     std::vector<std::vector<std::pair<uint, uint>>>& edges
// ) {
    
// }