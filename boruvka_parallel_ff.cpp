#if !defined(__PARALLEL_FF_SHORT_H)
#define __PARALLEL_FF_SHORT_H

#include <iostream>
#include <thread>
#include "lib/components.hpp"
#include "lib/dset.hpp"
#include <ff/ff.hpp>
#include <ff/farm.hpp>
#include <ff/parallel_for.hpp>


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

    MyEdge NULL_CONN = {0,0,10};

    for (int nw = 1; nw <= num_w; nw++) {

        // Instantiate a ParallelFor
        ff::ParallelFor pf(nw);

        while (iters > 0) {
        
            // Disjoint Union Find structure
            DisjointSets initialComponents = { static_cast<uint32_t>(graph.getNumNodes()) };

            std::atomic<int> MST_weight;

            long total_time = 0;        

            int iter = 0;

            while (graph.getNumNodes() != 1) {

                uint num_edge = graph.getNumEdges();

                std::vector<std::vector<MyEdge>> local_edges (nw);

                for (auto &local_edge : local_edges) {
                    local_edge.assign(graph.originalNodes, {0,0,10});
                }

                long map_time;

                {

                    Utimer timer("Map parallel time", &map_time);
                
                    pf.parallel_for_idx(0, num_edge, 1, 0, [&](const long start, const long stop, const int thid) {

                        for (uint i = start; i < stop; i++) {
                            // Retrieve edge from graph
                            MyEdge edge = graph.edges[i];

                            if (local_edges[thid][edge.from].weight > edge.weight) {
                                // Found edge with same starting node and minimum weight, update local_edge
                                local_edges[thid][edge.from].weight = edge.weight;
                                local_edges[thid][edge.from].from = edge.from;
                                local_edges[thid][edge.from].to = edge.to;
                            }
                        }
                    });

                }

                std::vector<MyEdge> global_edges (graph.originalNodes, {0, 0, 10});

                long merge_time;

                {
                    Utimer timer("Merge edges", &merge_time);

                    pf.parallel_for_idx(0, local_edges[0].size(), 1, 0, [&](const long start, const long stop, const int thid) {

                        // For each local_edge of each thread
                        for (auto &local_edge : local_edges) {
                            // Iterate through the indexes interval received
                            for (uint i = start; i < stop; i++) {
                                if (local_edge[i].weight < global_edges[i].weight) {
                                    // Update global_edges if the local_edges found by the thread i has a better weight
                                    global_edges[i].weight = local_edge[i].weight;
                                    global_edges[i].from = local_edge[i].from;
                                    global_edges[i].to = local_edge[i].to;
                                }
                            }
                        }
                    });

                }

                
                long contraction_time;

                {

                    Utimer timer("Contraction nodes", &contraction_time);

                    pf.parallel_for_idx(0, global_edges.size(), 1, 0, [&](const long start, const long stop, const int thid) {

                        // Iterate through global_edges in the specific indexes
                        for (uint i = start; i < stop; i++) {
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
                    });
                }

                std::vector<std::vector<MyEdge>> selected_edges (nw);
                std::vector<std::vector<uint>> selected_nodes (nw);

                long filtering_edge_time;

                {

                    Utimer timer("Filter edges", &filtering_edge_time);

                    pf.parallel_for_idx(0, num_edge, 1, 0, [&](const long start, const long stop, const int thid) {

                        // Iterate through the received indexes 
                        for (uint i = start; i < stop; i++) {
                            
                            MyEdge edge = graph.edges[i];

                            if ( !initialComponents.same(edge.from, edge.to) )
                                /**
                                 * If the starting and the ending node of each graph's edge are not in the same component, 
                                 * then we need to keep it for the next iteration.
                                 * Otherwise we discard it.
                                 */
                                selected_edges[thid].push_back(edge);
                        }
                    });

                }

                long filtering_node_time;

                {

                    Utimer timer("Filter nodes", &filtering_node_time);

                    pf.parallel_for_idx(0, graph.originalNodes, 1, 0, [&](const long start, const long stop, const int thid) {
                        // Iterate through the received indexes 
                        for (uint i = start; i < stop; i++) {
                            if ( initialComponents.parent(graph.nodes[i]) == graph.nodes[i] ) 
                                /**
                                 * If the parent node is the same as the node itself, then we need to keep it also 
                                 * for next iteration.
                                 * Otherwise it is a child of another node and we can discard it.
                                 */
                                selected_nodes[thid].push_back(i);
                        }  
                    });

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
            
            std::cout << "workers: " << nw << "; iters: " << iter << "; time " << total_time << " usec" << std::endl;

            graph = copy_graph;

            iters--;

        }

        iters = std::atoi(argv[5]);

    }

    return (0);

}


#endif