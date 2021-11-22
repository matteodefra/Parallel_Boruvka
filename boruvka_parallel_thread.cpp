#include <iostream>
#include <thread>
#include "lib/components.hpp"
#include "lib/dset.hpp"
#include <algorithm>
#include <atomic>


int main(int argc, char *argv[]) {

    if (argc != 5) {
        std::cout << "Usage ./[executable] nw number_nodes number_edges filename" << std::endl;
        return (0);
    }

    short num_w = std::stoi(argv[1]);

    uint num_nodes = std::atoi(argv[2]);

    uint num_edges = std::atoi(argv[3]);

    std::string filename = std::string(argv[4]);

    Graph graph = Graph();

    if (filename.empty()) {
        graph.generateGraph(num_nodes, num_edges);
    }
    else {
        graph.loadGraph(filename);
    }

    // Disjoint Union Find structure
    DisjointSets initialComponents = { static_cast<uint32_t>(graph.getNumNodes()) };

    std::atomic<int> MST_weight;

    int iter = 0;

    long total_time = 0;

    while (graph.getNumNodes() != 1) {

        // Queues for the workers
        std::vector<MyQueue<std::pair<uint,uint>>> thread_queues (num_w);

        // Vector of local MST
        std::vector<std::vector<MyEdge>> local_edges (num_w);

        // Feedback queue
        MyQueue<short> feedbackQueue; 

        // Vector of threads
        std::vector<std::thread> map_workers (num_w);

        long map_time;

        {
            Utimer timer("Map parallel time", &map_time);

            for (short i = 0; i < num_w; i++) {

                // Initialize threads: each map worker create a local_edges structure
                std::thread worker_thread(
                    map_worker,
                    i,
                    std::ref(thread_queues[i]),
                    std::ref(feedbackQueue),
                    std::ref(local_edges[i]),
                    std::ref(graph)
                );

                // Allocate them in vector
                map_workers[i] = std::move(worker_thread);

            }

            // Make main thread the emitter
            emitter(
                std::ref(thread_queues),
                std::ref(feedbackQueue),
                graph.getNumEdges()
            );

            // Joins all the entities
            for (std::thread& w: map_workers)
                w.join();

        }

        std::vector<MyEdge> global_edges (graph.originalNodes, {0, 0, 10});

        // Vector of threads
        std::vector<std::thread> merge_workers (num_w);

        long merge_time;

        {

            Utimer timer("Merge edges", &merge_time);

            for (short i = 0; i < num_w; i++) {

                // Initialize threads: each map worker create a local_edges structure
                std::thread worker_thread(
                    merge_worker,
                    i,
                    std::ref(thread_queues[i]),
                    std::ref(feedbackQueue),
                    std::ref(local_edges),
                    std::ref(global_edges)
                );

                // Allocate them in vector
                merge_workers[i] = std::move(worker_thread);

            }

            // Make main thread the emitter
            emitter(
                std::ref(thread_queues),
                std::ref(feedbackQueue),
                local_edges[0].size()
            );

            // Joins all the entities
            for (std::thread& w: merge_workers)
                w.join();


        }

        // Vector of threads
        std::vector<std::thread> contraction_workers (num_w);

        long contraction_time;

        {

            Utimer timer("Contraction nodes", &contraction_time);

            for (short i = 0; i < num_w; i++) {

                // Initialize threads: each map worker create a local_edges structure
                std::thread worker_thread(
                    contraction_worker,
                    i,
                    std::ref(initialComponents),
                    std::ref(thread_queues[i]),
                    std::ref(feedbackQueue),
                    std::ref(global_edges),
                    std::ref(MST_weight)
                );

                // Allocate them in vector
                contraction_workers[i] = std::move(worker_thread);

            }

            // Make main thread the emitter
            emitter(
                std::ref(thread_queues),
                std::ref(feedbackQueue),
                global_edges.size()
            );

            // Joins all the entities
            for (std::thread& w: contraction_workers)
                w.join();


        }


        // After unifying components, we need to contract edges in our graph, otherwise minimum edges stays the same!

        std::vector<std::thread> filtering_workers (num_w);
       
        std::vector<std::vector<MyEdge>> selected_edges (num_w);

        std::vector<std::vector<uint>> selected_nodes (num_w);

        long filtering_edge_time;

        {

            Utimer timer("Filter edges", &filtering_edge_time);

            for (short i = 0; i < num_w; i++) {

                // Initialize threads: each map worker create a local_edges structure
                std::thread worker_thread(
                    filter_edge_worker,
                    i,
                    std::ref(initialComponents),
                    std::ref(thread_queues[i]),
                    std::ref(feedbackQueue),
                    std::ref(selected_edges[i]),
                    std::ref(graph)
                );

                // Allocate them in vector
                filtering_workers[i] = std::move(worker_thread);

            }

            // Make main thread the emitter
            emitter(
                std::ref(thread_queues),
                std::ref(feedbackQueue),
                graph.getNumEdges()
            );

            // Joins all the entities
            for (std::thread& w: filtering_workers)
                w.join();


        }

        std::vector<std::thread> filtering_workers2 (num_w);

        long filtering_node_time;

        {

            Utimer timer("Filter nodes", &filtering_node_time);

            for (short i = 0; i < num_w; i++) {

                // Initialize threads: each map worker create a local_edges structure
                std::thread worker_thread(
                    filter_node_worker,
                    i,
                    std::ref(initialComponents),
                    std::ref(thread_queues[i]),
                    std::ref(feedbackQueue),
                    std::ref(selected_nodes[i]),
                    std::ref(graph)
                );

                // Allocate them in vector
                filtering_workers2[i] = std::move(worker_thread);

            }

            // Make main thread the emitter
            emitter(
                std::ref(thread_queues),
                std::ref(feedbackQueue),
                graph.originalNodes
            );

            // Joins all the entities
            for (std::thread& w: filtering_workers2)
                w.join();


        }

        std::vector<MyEdge> remaining_edges;
        std::vector<uint> remaining_nodes;

        long filtering_time;

        {
            Utimer timer("Final filtering", &filtering_time);

            for (auto &vect : selected_edges) {
                remaining_edges.insert(remaining_edges.end(), vect.begin(), vect.end());
            }

            for (auto &vect : selected_nodes) {
                remaining_nodes.insert(remaining_nodes.end(), vect.begin(), vect.end());
            }

        }

        total_time += map_time + merge_time + contraction_time + filtering_edge_time + filtering_node_time + filtering_time; 

        graph.updateNodes(std::ref(remaining_nodes));
        graph.updateEdges(std::ref(remaining_edges));

        iter++;

    }   

    std::cout << "Total iters: " << iter << std::endl;

    std::cout << "Total time required: " << total_time << " usec" << std::endl;
     
    return (0);

}