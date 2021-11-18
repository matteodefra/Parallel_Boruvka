#include <iostream>
#include <thread>
#include "lib/components.hpp"
#include "lib/dset.hpp"
#include <algorithm>
#include <atomic>


int main(int argc, char *argv[]) {

    if (argc != 4) {
        std::cout << "Usage ./[executable] number_nodes number_edges filename" << std::endl;
        return (0);
    }

    uint num_nodes = std::atoi(argv[1]);

    uint num_edges = std::atoi(argv[2]);

    std::string filename = std::string(argv[3]);

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

    std::cout << "Total nodes in the graph: " << graph.getNumNodes() << std::endl;

    std::cout << "Total edges in the graph: " << graph.getNumEdges() << std::endl;
    
    while (graph.getNumNodes() != 1) {

        std::vector<MyEdge> global_edges (graph.originalNodes, {0, 0, 10});

        long map_time;

        {
            Utimer timer("Minimum searching", &map_time);

            for (auto &edge : graph.edges) {
                if (global_edges[edge.from].weight > edge.weight) {
                    // Update global_edges if the local_edges found by the thread i has a better weight
                    global_edges[edge.from].weight = edge.weight;
                    global_edges[edge.from].from = edge.from;
                    global_edges[edge.from].to = edge.to;
                }
            }

        }
        

        MyEdge NULL_CONN = {0, 0, 10};

        long contraction_time;

        {
            Utimer timer("Contraction time", &contraction_time);

            for (auto &edge : global_edges) {
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

        }

        std::vector<MyEdge> remaining_edges;
        std::vector<uint> remaining_nodes;

        long filtering_edge_time;

        {
            Utimer timer("Filtering edge", &filtering_edge_time);

            for (auto &edge : graph.edges) {
                if ( !initialComponents.same(edge.from, edge.to) )
                /**
                 * If the starting and the ending node of each graph's edge are not in the same component, 
                 * then we need to keep it for the next iteration.
                 * Otherwise we discard it.
                 */
                    remaining_edges.push_back(edge);
            }

        }

        long filtering_node_time;

        {
            Utimer timer("Filtering nodes", &filtering_node_time);

            for (uint i = 0; i < graph.nodes.size(); i++) {
                if ( initialComponents.parent(i) == i ) 
                    /**
                     * If the parent node is the same as the node itself, then we need to keep it also 
                     * for next iteration.
                     * Otherwise it is a child of another node and we can discard it.
                     */
                    remaining_nodes.push_back(i);
            }

        }


        total_time += map_time + contraction_time + filtering_edge_time + filtering_node_time; 
        
        std::cout << std::endl;

        std::cout << "Edges remaining" << std::endl;

        std::cout << "Size: " << remaining_edges.size() << std::endl;

        std::cout << std::endl;

        std::cout << "Node remaining" << std::endl;

        std::cout << "Size: " << remaining_nodes.size() << std::endl;


        graph.updateNodes(std::ref(remaining_nodes));
        graph.updateEdges(std::ref(remaining_edges));

        iter++;

    }   

    for (uint j = 0; j < initialComponents.mData.size(); j++) {
        if (initialComponents.parent(j) == j) {
            std::cout << "Found same parent" << std::endl;
        }
    }

    std::cout << "Computation finished" << std::endl;

    std::cout << "Total iters: " << iter << std::endl;

    std::cout << "Total time required: " << total_time << " usec" << std::endl;
     
    std::cout << "Total weight of MST: " << MST_weight.load() << std::endl;

    return (0);

}

// On the largest example:
// 562186 usec for sequential time
// 559091 usec for parallel threads with 2 workers
// 501688 usec for parallel threads with 4 workers
// 496469 usec for parallel threads with 6 workers