#include <iostream>
#include <thread>
#include "lib/components.hpp"


int main(int argc, char *argv[]) {

    // if (argc < 3) {
        // std::cout << "Usage ./[executable] nw" << std::endl;
        // return (0);
    // }

    short num_w = std::stoi(argv[1]);

    std::cout << "Num workers: " << num_w << std::endl;

    Graph graph = Graph();

    graph.createGraph();

    std::unordered_map<uint, std::vector<Edge>> map = graph.getGraph();

    std::vector<std::set<uint>> initialComponents = {};

    for (const auto& myPair : map) {
        std::set<uint> comp = {myPair.first};
        initialComponents.push_back(comp);
    }
    // Initial components ready with one node for each set

    std::cout << initialComponents.size() << std::endl;
    
    // Queues for the workers
    std::vector<MyQueue<std::pair<uint,uint>>> thread_queues (num_w);

    // Vector of local MST
    std::vector<std::vector<LightestConnection>> MSTs (num_w);

    // Feedback queue
    MyQueue<short> feedbackQueue; 

    // Vector of threads
    std::vector<std::thread> workers (num_w);

    {
        Utimer timer("Parallel time");

        for (short i = 0; i < num_w; i++) {

            // Initialize threads
            std::thread worker_thread(
                map_worker,
                i,
                std::ref(initialComponents),
                std::ref(thread_queues[i]),
                std::ref(feedbackQueue),
                std::ref(MSTs[i]),
                map
            );

            // Allocate them in vector
            workers[i] = std::move(worker_thread);

        }

        size_t size_components = initialComponents.size();

        // Make main thread the emitter
        emitter(
            std::ref(thread_queues),
            std::ref(feedbackQueue),
            size_components
        );

        // Joins all the entities
        for (std::thread& w: workers)
            w.join();

    }

    // for (auto vec :  MSTs) {

    //     for (auto connection : vec) 
    //         std::cout << "Minimum edge found: " << connection.startingNode << "---" << connection.endingNode << " with weight " << connection.weight << std::endl; 
    
    // }

    uint dim{};
    for (auto vec : MSTs) {
        dim += vec.size();
    }

    std::cout << "Total connections: " << dim << std::endl;

    return (0);

}