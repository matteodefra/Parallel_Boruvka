#if !defined(__PARALLEL_FF_H)
#define __PARALLEL_FF_H

#include <iostream>
#include <thread>
#include "lib/components.hpp"
#include "lib/dset.hpp"
#include "fastflow/ff/ff.hpp"
#include "fastflow/ff/farm.hpp"
#include "lib/components_ff.hpp"


int main(int argc, char *argv[]) {

    if (argc != 5) {
        std::cout << "Usage ./[executable] nw number_nodes number_edges filename" << std::endl;
        return (0);
    }

    int num_w = std::stoi(argv[1]);

    uint num_nodes = std::atoi(argv[2]);

    uint num_edges = std::atoi(argv[3]);

    std::string filename = std::string(argv[4]);

    std::cout << "Num workers: " << num_w << std::endl;

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

    long total_time = 0;        

    int iter = 0;

    while (graph.getNumNodes() != 1) {

        uint num_edge = graph.getNumEdges();
        
        Emitter emitter(num_edge, num_w, graph);

        std::vector<std::unique_ptr<ff::ff_node>> workers; 

        for (short i = 0; i < num_w; i++) {

            workers.push_back(ff::make_unique<MapWorker>());
        }

        ff::ff_Farm<> myFarm(std::move(workers));
        
        myFarm.remove_collector();

        myFarm.add_emitter(emitter);

        myFarm.wrap_around();

        long map_time;

        {
            Utimer timer("map ff", &map_time);

            if (myFarm.run_and_wait_end() < 0) {

                std::cout << "error farm" << std::endl;
                return -1;
            
            }

        }

        MergeEmitter merge_emitter(graph.originalNodes, num_w, emitter.local_edges);

        std::vector<std::unique_ptr<ff::ff_node>> merge_workers; 

        for (short i = 0; i < num_w; i++) {

            merge_workers.push_back(ff::make_unique<MergeWorker>());
        }

        ff::ff_Farm<> mergeFarm(std::move(merge_workers));
        
        mergeFarm.remove_collector();

        mergeFarm.add_emitter(merge_emitter);

        mergeFarm.wrap_around();

        long merge_time;

        {
            Utimer timer("merge ff", &merge_time);

            if (mergeFarm.run_and_wait_end() < 0) {

                std::cout << "error farm" << std::endl;
                return -1;
            
            }

        }

        std::cout << "Global edges found" << iter << std::endl;

        ContractionEmitter contraction_emitter(merge_emitter.global_edges.size(), num_w, merge_emitter.global_edges, std::ref(initialComponents));

        std::vector<std::unique_ptr<ff::ff_node>> contraction_workers; 

        for (short i = 0; i < num_w; i++) {

            contraction_workers.push_back(ff::make_unique<ContractionWorker>());
        }

        ff::ff_Farm<> contractionFarm(std::move(contraction_workers));
        
        contractionFarm.remove_collector();

        contractionFarm.add_emitter(contraction_emitter);

        contractionFarm.wrap_around();

        long contraction_time;

        {
            Utimer timer("contraction ff", &contraction_time);

            if (contractionFarm.run_and_wait_end() < 0) {

                std::cout << "error farm" << std::endl;
                return -1;
            
            }

        }

        FilterEdgeEmitter filter_edge_emitter(graph.getNumEdges(), num_w, std::ref(graph), std::ref(initialComponents));

        std::vector<std::unique_ptr<ff::ff_node>> filtering_edge_workers; 

        for (short i = 0; i < num_w; i++) {

            filtering_edge_workers.push_back(ff::make_unique<FilterEdgeWorker>());
        }

        ff::ff_Farm<> filteringEdgeFarm(std::move(filtering_edge_workers));
        
        filteringEdgeFarm.remove_collector();

        filteringEdgeFarm.add_emitter(filter_edge_emitter);

        filteringEdgeFarm.wrap_around();

        long filtering_edge_time;

        {
            Utimer timer("filtering edges ff", &filtering_edge_time);

            if (filteringEdgeFarm.run_and_wait_end() < 0) {

                std::cout << "error farm" << std::endl;
                return -1;
            
            }

        }

        FilterNodeEmitter filter_node_emitter(graph.originalNodes, num_w, std::ref(graph), std::ref(initialComponents));

        std::vector<std::unique_ptr<ff::ff_node>> filtering_node_workers; 

        for (short i = 0; i < num_w; i++) {

            filtering_node_workers.push_back(ff::make_unique<FilterNodeWorker>());
        }

        ff::ff_Farm<> filteringNodeFarm(std::move(filtering_node_workers));
        
        filteringNodeFarm.remove_collector();

        filteringNodeFarm.add_emitter(filter_node_emitter);

        filteringNodeFarm.wrap_around();

        long filtering_node_time;

        {
            Utimer timer("filtering nodes ff", &filtering_node_time);

            if (filteringNodeFarm.run_and_wait_end() < 0) {

                std::cout << "error farm" << std::endl;
                return -1;
            
            }

        }

        std::vector<MyEdge> remaining_edges;
        std::vector<uint> remaining_nodes;

        long filtering_time;

        {
            Utimer timer("Final filtering", &filtering_time);

            for (auto &vect : filter_edge_emitter.remaining_edges) {
                remaining_edges.insert(remaining_edges.end(), vect.begin(), vect.end());
            }

            for (auto &vect : filter_node_emitter.remaining_nodes) {
                remaining_nodes.insert(remaining_nodes.end(), vect.begin(), vect.end());
            }

        }

        total_time += map_time + merge_time + contraction_time + filtering_edge_time + filtering_node_time + filtering_time; 

        std::cout << std::endl;

        std::cout << "Edges remaining" << std::endl;

        std::cout << "Size: " << remaining_edges.size() << std::endl;

        std::cout << std::endl;

        std::cout << "Node remaining" << std::endl;

        std::cout << "Size: " << remaining_nodes.size() << std::endl;


        graph.updateNodes(std::ref(remaining_nodes));
        graph.updateEdges(std::ref(remaining_edges));

        iter += 1;

    }

    for (uint j = 0; j < initialComponents.mData.size(); j++) {
        if (initialComponents.parent(j) == j) {
            std::cout << "Found same parent" << std::endl;
        }
    }

    std::cout << "Computation finished" << std::endl;

    std::cout << "Total iters: " << iter << std::endl;

    std::cout << "Total time required: " << total_time << " usec" << std::endl;

    return (0);

}


#endif