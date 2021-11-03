#include <iostream>
#include <thread>
#include "lib/components.hpp"
#include "lib/dset.hpp"


int main(int argc, char *argv[]) {

    // if (argc < 3) {
        // std::cout << "Usage ./[executable] nw" << std::endl;
        // return (0);
    // }

    short num_w = std::stoi(argv[1]);

    std::cout << "Num workers: " << num_w << std::endl;

    Graph graph = Graph();

    graph.createGraph();

    std::vector<uint> nodes = graph.getNodes();

    std::vector<MyEdge> edges = graph.getEdges();

    // Disjoint Union Find structure
    DisjointSets initialComponents = { static_cast<uint32_t>(graph.getNumNodes()) };

    // std::cout << initialComponents << std::endl;
    
    while (initialComponents.size() != 1) {

        // Queues for the workers
        std::vector<MyQueue<std::pair<uint,uint>>> thread_queues (num_w);

        // Vector of local MST
        std::vector<std::vector<MyEdge>> local_edges (num_w);

        // Feedback queue
        MyQueue<short> feedbackQueue; 

        // Vector of threads
        std::vector<std::thread> map_workers (num_w);

        {
            Utimer timer("Map parallel time");

            for (short i = 0; i < num_w; i++) {

                // Initialize threads: each map worker create a local_edges structure
                std::thread worker_thread(
                    map_worker,
                    i,
                    std::ref(initialComponents),
                    std::ref(thread_queues[i]),
                    std::ref(feedbackQueue),
                    std::ref(local_edges[i]),
                    std::ref(graph)
                );

                // Allocate them in vector
                map_workers[i] = std::move(worker_thread);

            }

            std::cout << "Number of edges: " << graph.getNumEdges()*2 << std::endl;

            // Make main thread the emitter
            emitter(
                std::ref(thread_queues),
                std::ref(feedbackQueue),
                graph.getNumEdges()*2
            );

            // Joins all the entities
            for (std::thread& w: map_workers)
                w.join();

        }

        std::vector<MyEdge> global_edges (graph.getNumNodes(), {0, 0, 10});

        // MAYBE CAN PARALLELIZE ALSO THIS
        // Before going on, we must merge together the local_edges found by the different threads, and then reduce
        for (uint index = 0; index < local_edges.size(); index++) {
            
            for (uint i = 0; i < local_edges[index].size(); i++) {

                if (local_edges[index][i].weight < global_edges[i].weight) {
                    std::swap(local_edges[index][i], global_edges[i]);
                }

            }

        }


        MyEdge NULL_CONN = {0, 0, 10};

        // Contraction time
        for (auto edge : global_edges) {
            if (edge == NULL_CONN) {
                // std::cout << "Edge not connected" << std::endl;
            }
            else {
                if (!initialComponents.same(edge.from, edge.to)) {
                    initialComponents.unite(edge.from, edge.to);
                }
                else {
                    std::cout << "Same component!" << std::endl;
                }
            }
        }

        std::cout << initialComponents << std::endl;


        // After unifying components, we need to contract edges in our graph, otherwise minimum edges stays the same!

    

        // // Queues for the reduce workers
        // std::vector<MyQueue<std::pair<uint,uint>>> thread_queues_workers (num_w);

        // // Vector of threads
        // std::vector<std::thread> reduce_workers (num_w);


    }

    std::cout << "MST found!" << std::endl;
    std::cout << initialComponents << std::endl;

    return (0);

}