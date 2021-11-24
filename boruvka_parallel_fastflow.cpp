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

    // Setting up initial stage
    if (argc != 6) {
        std::cout << "Usage ./[executable] nw number_nodes number_edges filename iters" << std::endl;
        return (0);
    }

    int num_w = std::stoi(argv[1]);

    uint num_nodes = std::atoi(argv[2]);

    uint num_edges = std::atoi(argv[3]);

    std::string filename = std::string(argv[4]);

    int iters = std::atoi(argv[5]);

    long loading_time = 0;

    Graph graph;// = Graph();

    {

        Utimer read_time("loading graph",&loading_time);

        // UNCOMMENT FOR V_E and sc-rel9
        if (filename.empty()) {
            graph.generateGraph(num_nodes, num_edges);
        }
        else {
            graph.loadGraph(filename);
        }
        // UNCOMMENT FOR soc-youtube
        // graph.loadGraphUnweighted(filename);

    }

    std::cout << "fastflow; read time: " << loading_time << " usec" << std::endl;

    Graph copy_graph = graph;

    for (int nw = 1; nw <= num_w; nw++) {

        while (iters > 0) {
        
            // Disjoint Union Find structure
            DisjointSets initialComponents = { static_cast<uint32_t>(graph.getNumNodes()) };

            std::atomic<int> MST_weight;

            long total_time = 0;        

            int iter = 0;

            while (graph.getNumNodes() != 1) {

                uint num_edge = graph.getNumEdges();
                
                Emitter emitter(num_edge, nw, graph);

                std::vector<std::unique_ptr<ff::ff_node>> workers; 

                for (short i = 0; i < nw; i++) {

                    workers.push_back(ff::make_unique<MapWorker>());
                }

                ff::ff_Farm<> myFarm(std::move(workers));
                
                myFarm.remove_collector();

                myFarm.add_emitter(emitter);

                myFarm.wrap_around();

                long map_time;

                /**
                 * Farm for the map phase
                 */
                {
                    Utimer timer("map ff", &map_time);

                    if (myFarm.run_and_wait_end() < 0) {

                        std::cout << "error farm" << std::endl;
                        return -1;
                    
                    }

                }

                MergeEmitter merge_emitter(graph.originalNodes, nw, emitter.local_edges);

                std::vector<std::unique_ptr<ff::ff_node>> merge_workers; 

                for (short i = 0; i < nw; i++) {

                    merge_workers.push_back(ff::make_unique<MergeWorker>());
                }

                ff::ff_Farm<> mergeFarm(std::move(merge_workers));
                
                mergeFarm.remove_collector();

                mergeFarm.add_emitter(merge_emitter);

                mergeFarm.wrap_around();

                long merge_time;

                /**
                 * Farm for the merge phase
                 */
                {
                    Utimer timer("merge ff", &merge_time);

                    if (mergeFarm.run_and_wait_end() < 0) {

                        std::cout << "error farm" << std::endl;
                        return -1;
                    
                    }

                }

                ContractionEmitter contraction_emitter(merge_emitter.global_edges.size(), nw, merge_emitter.global_edges, std::ref(initialComponents));

                std::vector<std::unique_ptr<ff::ff_node>> contraction_workers; 

                for (short i = 0; i < nw; i++) {

                    contraction_workers.push_back(ff::make_unique<ContractionWorker>());
                }

                ff::ff_Farm<> contractionFarm(std::move(contraction_workers));
                
                contractionFarm.remove_collector();

                contractionFarm.add_emitter(contraction_emitter);

                contractionFarm.wrap_around();

                long contraction_time;

                /**
                 * Farm for the contraction phase
                 */
                {
                    Utimer timer("contraction ff", &contraction_time);

                    if (contractionFarm.run_and_wait_end() < 0) {

                        std::cout << "error farm" << std::endl;
                        return -1;
                    
                    }

                }

                FilterEdgeEmitter filter_edge_emitter(graph.getNumEdges(), nw, std::ref(graph), std::ref(initialComponents));

                std::vector<std::unique_ptr<ff::ff_node>> filtering_edge_workers; 

                for (short i = 0; i < nw; i++) {

                    filtering_edge_workers.push_back(ff::make_unique<FilterEdgeWorker>());
                }

                ff::ff_Farm<> filteringEdgeFarm(std::move(filtering_edge_workers));
                
                filteringEdgeFarm.remove_collector();

                filteringEdgeFarm.add_emitter(filter_edge_emitter);

                filteringEdgeFarm.wrap_around();

                long filtering_edge_time;

                /**
                 * Farm for the edge filtering phase
                 */
                {
                    Utimer timer("filtering edges ff", &filtering_edge_time);

                    if (filteringEdgeFarm.run_and_wait_end() < 0) {

                        std::cout << "error farm" << std::endl;
                        return -1;
                    
                    }

                }

                FilterNodeEmitter filter_node_emitter(graph.originalNodes, nw, std::ref(graph), std::ref(initialComponents));

                std::vector<std::unique_ptr<ff::ff_node>> filtering_node_workers; 

                for (short i = 0; i < nw; i++) {

                    filtering_node_workers.push_back(ff::make_unique<FilterNodeWorker>());
                }

                ff::ff_Farm<> filteringNodeFarm(std::move(filtering_node_workers));
                
                filteringNodeFarm.remove_collector();

                filteringNodeFarm.add_emitter(filter_node_emitter);

                filteringNodeFarm.wrap_around();

                long filtering_node_time;

                /**
                 * Farm for the node filtering phase
                 */
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

                graph.updateNodes(std::ref(remaining_nodes));
                graph.updateEdges(std::ref(remaining_edges));

                iter += 1;

            }
            
            std::cout << "workers: " << nw << "; iters: " << iter << "; time " << total_time << " usec" << std::endl;

            graph = copy_graph;

            iters--;

        }

        iters = std::atoi(argv[5]);

    }

    return (0);

}


#endif