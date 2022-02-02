#include <iostream>
#include <thread>
#include "lib/dset.hpp"
#include <algorithm>
#include <atomic>
#include <future>
#include "lib/queue.hpp"
#include "lib/graph.hpp"
#include "lib/utils.hpp"
#include "lib/utimer.hpp"
#include "lib/threadpool.hpp"

#define MY_EOS std::pair<uint,uint> (0,0)


/**
 * @brief Compute minimum edges of the graph
 * 
 * @param local_edges Vector of vector of edges to modify saving the minimum edges found
 * @param graph The graph accessed concurrently
 * @param chunk_indexes The <starting,ending> integer pair of graph edges to inspect
 * @param index The index of the corresponding thread
 * @return int 
 * 
 * Loop through the assigned indexes chunk_indexes and modify the local_edges at the given index thread with the minimum edges found
 */
int mapwork(std::vector<std::vector<MyEdge>> &local_edges, Graph &graph, std::pair<uint, uint> chunk_indexes, uint index) {

    // local_edges.resize(graph.originalNodes);
    local_edges[index].assign(graph.originalNodes, {0,0,10});
    // std::cout << local_edges[index].size() << std::endl;

    // Get the indexes of the edges array
    uint starting_index = chunk_indexes.first;

    uint ending_index = chunk_indexes.second;

    // std::cout << starting_index << "," << ending_index << std::endl;

    for (uint i = starting_index; i < ending_index; i++) {
        // Retrieve edge from graph
        MyEdge &edge = graph.edges[i];

        if (local_edges[index][edge.from].weight > edge.weight) {
            // Found edge with same starting node and minimum weight, update local_edge
            local_edges[index][edge.from].weight = edge.weight;
            local_edges[index][edge.from].from = edge.from;
            local_edges[index][edge.from].to = edge.to;
        }
    }

    return 1;

}


/**
 * @brief Merge the previously local_edges
 * 
 * @param local_edges Vector of vector of minimum edges found by previous threads
 * @param global_edges Global vector of minimum edges
 * @param chunk_indexes The <starting,ending> integer pair to be modified by the thread
 * @return int 
 * 
 * Each thread inspect the local_edges and update the global_edges vector with the minimum edge found previously 
 */
int mergework(std::vector<std::vector<MyEdge>> &local_edges, std::vector<MyEdge> &global_edges, std::pair<uint, uint> chunk_indexes) {

    // Get the indexes of the local_edges array
    uint starting_index = chunk_indexes.first;

    uint ending_index = chunk_indexes.second;

    // std::cout << starting_index << "," << ending_index << std::endl;

    // For each local_edge of each thread
    for (auto local_edge : local_edges) {
        
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

    return 1;

}


/**
 * @brief Contract the union find data structure
 * 
 * @param global_edges The minimum vector of edges found in this iteration
 * @param initialComponents The disjoint sets data structure
 * @param graph The graph data structure
 * @param chunk_indexes The <starting, ending> integer pair to inspect in the global_edges vector
 * @return int 
 * 
 * Contract the union find data structure by calling the method same and unite. If the minimum edge found among node x and y have already the same 
 * parent, then we don't do nothing. 
 * Otherwise, we call unite to fuse together the two subtrees
 */
int contractionwork(std::vector<MyEdge> &global_edges, DisjointSets &initialComponents, Graph &graph, std::pair<uint, uint> chunk_indexes) {

    MyEdge NULL_CONN = {0,0,10};

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

    return 1;

}


/**
 * @brief Filter the edges found previously
 * 
 * @param remaining_edges Vector of vector of edges to modify 
 * @param initialComponents The disjoint set data structure
 * @param graph The graph data structure
 * @param chunk_indexes The <starting,ending> integer pair to inspect in the graph edges
 * @param index The index of the corresponding thread
 * @return int 
 * 
 * Loop through the edges of the graph and append the edge into the corresponding remaining_edges index if the node x and y linking the current edge does 
 * not belong to the same component
 */
int filteringedgework(std::vector<std::vector<MyEdge>>& remaining_edges, DisjointSets &initialComponents, Graph &graph, std::pair<uint, uint> chunk_indexes, int index) {

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
            remaining_edges[index].push_back(edge);
    }

    return 1;

}


/**
 * @brief Filter the nodes found previously
 * 
 * @param remaining_nodes Vector of vector of nodes to modify
 * @param initialComponents The disjoint set data structure
 * @param graph The graph data structure
 * @param chunk_indexes The <starting,ending> integer pair to inspect in the graph nodes
 * @param index The index of the corresponding thread
 * @return int 
 * 
 * Inspect the given nodes indexes in the graph and check if the current node is itself a parent. 
 * If it is so, we save it into the remanining_nodes (only the parent node matters)
 */
int filteringnodework(std::vector<std::vector<uint>>& remaining_nodes, DisjointSets &initialComponents, Graph &graph, std::pair<uint, uint> chunk_indexes, int index) {

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
            remaining_nodes[index].push_back(i);
        
    }

    return 1;

}


int main(int argc, char *argv[]) {

    if (argc != 6) {
        std::cout << "Usage ./[executable] nw number_nodes number_edges filename iters" << std::endl;
        return (0);
    }

    short num_w = std::stoi(argv[1]);

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

    std::cout << "parallel thread; read time: " << loading_time << " usec" << std::endl;

    Graph copy_graph = graph;    

    for (int nw = 1; nw <= num_w; nw++) {

        // Instantiate the threadpool
        ThreadPool pool{nw};

        while (iters > 0) {

            // Disjoint Union Find structure
            DisjointSets initialComponents = { static_cast<uint32_t>(graph.getNumNodes()) };

            std::atomic<int> MST_weight;

            int iter = 0;

            long total_time = 0;

            while (graph.getNumNodes() != 1) {

                // Vector of local MST
                std::vector<std::vector<MyEdge>> local_edges (nw);

                std::vector<MyEdge> global_edges;
                global_edges.assign(graph.originalNodes, {0, 0, 10});

                std::vector<std::future<int>> mapfutures;

                long map_time;

                {

                    Utimer timer("Map parallel time", &map_time);

                    uint n = graph.getNumEdges();

                    // Portion of edges for each worker
                    size_t chunk_dim{ n / nw };

                    // The starting index will be at zero
                    size_t begin = 0;

                    // The ending one is n if the workers are enough, otherwise the chunk_dim computed before
                    size_t end = nw != 1 ? std::min(chunk_dim, static_cast<size_t>(n)) : n;


                    if (nw == 1) {
                        std::pair<uint, uint> chunk_indexes = {begin, end};
                        auto f1 = pool.enqueue([&, chunk_indexes]() -> int {
                            return mapwork(std::ref(local_edges), std::ref(graph), std::move( chunk_indexes ), 0);
                        }, 0);

                        mapfutures.push_back(std::move(f1));
                    }
                    else {
                        for (int i = 0; i < nw; i++) {

                            // Compute the indexes and enqueue the task into the thread pool
                            std::pair<uint, uint> chunk_indexes = {begin, end};
                            auto f1 = pool.enqueue([&, chunk_indexes, i]() -> int {
                                return mapwork(std::ref(local_edges), std::ref(graph), std::move( chunk_indexes ), i);
                            }, i);
                            
                            mapfutures.push_back(std::move(f1));

                            if (i == nw-2) {
                                // Last chunk
                                begin = end;
                                end = n;
                            }
                            else {
                                begin = end;
                                end = std::min(begin + chunk_dim, static_cast<size_t>(n));
                            }

                        }
                    }

                    // Wait for all the thread to finish
                    for (auto &fut : mapfutures) {
                        int val = fut.get();
                    }

                    
                }
    
                long merge_time;

                std::vector<std::future<int>> mergefutures;

                {

                    Utimer timer("Merge time", &merge_time);

                    uint n = local_edges[0].size();

                    // Portion of edges for each worker
                    size_t chunk_dim{ n / nw };

                    // The starting index will be at zero
                    size_t begin = 0;

                    // The ending one is n if the workers are enough, otherwise the chunk_dim computed before
                    size_t end = nw != 1 ? std::min(chunk_dim, static_cast<size_t>(n)) : n;

                    if (nw == 1) {
                        std::pair<uint, uint> chunk_indexes = {begin, end};
                        auto f1 = pool.enqueue([&, chunk_indexes]() -> int {
                            return mergework(std::ref(local_edges), std::ref(global_edges), std::move( chunk_indexes ));
                        }, 0);

                        mergefutures.push_back(std::move(f1));
                    }
                    else {
                        for (int i = 0; i < nw; i++) {
                            
                            std::pair<uint, uint> chunk_indexes = {begin, end};
                            auto f1 = pool.enqueue([&, chunk_indexes]() -> int {
                                return mergework(std::ref(local_edges), std::ref(global_edges), std::move(chunk_indexes));
                            }, std::move(i));

                            mergefutures.push_back(std::move(f1));
                            
                            if (i == nw-2) {
                                // Last chunk
                                begin = end;
                                end = n;
                            }
                            else {
                                begin = end;
                                end = std::min(begin + chunk_dim, static_cast<size_t>(n));
                            }

                        }
                    }

                    for (auto &fut : mergefutures) {
                        int val = fut.get();
                    }
    
                }

                long contraction_time;

                std::vector<std::future<int>> contractionfutures;

                {

                    Utimer timer("Contraction time", &contraction_time);

                    uint n = global_edges.size();

                    // Portion of edges for each worker
                    size_t chunk_dim{ n / nw };

                    // The starting index will be at zero
                    size_t begin = 0;

                    // The ending one is n if the workers are enough, otherwise the chunk_dim computed before
                    size_t end = nw != 1 ? std::min(chunk_dim, static_cast<size_t>(n)) : n;

                    if (nw == 1) {
                        std::pair<uint, uint> chunk_indexes = {begin, end};
                        auto f1 = pool.enqueue([&, chunk_indexes]() -> int {
                            return contractionwork(std::ref(global_edges), std::ref(initialComponents), std::ref(graph), std::move( chunk_indexes ));
                        }, 0);

                        contractionfutures.push_back(std::move(f1));
                    }
                    else {
                        for (int i = 0; i < nw; i++) {
                            
                            std::pair<uint, uint> chunk_indexes = {begin, end};
                            auto f1 = pool.enqueue([&, chunk_indexes]() -> int {
                                return contractionwork(std::ref(global_edges), std::ref(initialComponents), std::ref(graph), std::move( chunk_indexes ));
                            }, std::move(i));

                            contractionfutures.push_back(std::move(f1));
                            
                            if (i == nw-2) {
                                // Last chunk
                                begin = end;
                                end = n;
                            }
                            else {
                                begin = end;
                                end = std::min(begin + chunk_dim, static_cast<size_t>(n));
                            }

                        }
                    }

                    for (auto &fut : contractionfutures) {
                        int val = fut.get();
                    }
    
                }

                long filtering_edge_time;

                std::vector<std::future<int>> filtering_edgefutures;

                std::vector<std::vector<MyEdge>> selected_edges (nw);

                std::vector<std::vector<uint>> selected_nodes (nw);

                {

                    Utimer timer("Filtering edges time", &filtering_edge_time);

                    uint n = graph.getNumEdges();

                    // Portion of edges for each worker
                    size_t chunk_dim{ n / nw };

                    // The starting index will be at zero
                    size_t begin = 0;

                    // The ending one is n if the workers are enough, otherwise the chunk_dim computed before
                    size_t end = nw != 1 ? std::min(chunk_dim, static_cast<size_t>(n)) : n;

                    if (nw == 1) {
                        std::pair<uint, uint> chunk_indexes = {begin, end};
                        auto f1 = pool.enqueue([&, chunk_indexes]() -> int {
                            return filteringedgework(std::ref(selected_edges), std::ref(initialComponents), std::ref(graph), std::move( chunk_indexes ), 0);
                        }, 0);

                        filtering_edgefutures.push_back(std::move(f1));
                    }
                    else {
                        for (int i = 0; i < nw; i++) {
                            
                            std::pair<uint, uint> chunk_indexes = {begin, end};
                            auto f1 = pool.enqueue([&, chunk_indexes, i]() -> int {
                                return filteringedgework(std::ref(selected_edges), std::ref(initialComponents), std::ref(graph), std::move( chunk_indexes ), i);
                            }, std::move(i));

                            filtering_edgefutures.push_back(std::move(f1));
                            
                            if (i == nw-2) {
                                // Last chunk
                                begin = end;
                                end = n;
                            }
                            else {
                                begin = end;
                                end = std::min(begin + chunk_dim, static_cast<size_t>(n));
                            }

                        }
                    }

                    for (auto &fut : filtering_edgefutures) {
                        int val = fut.get();
                    }
    
                }

                long filtering_node_time;

                std::vector<std::future<int>> filtering_nodefutures;

                {

                    Utimer timer("Filtering nodes time", &filtering_node_time);

                    uint n = graph.originalNodes;

                    // Portion of edges for each worker
                    size_t chunk_dim{ n / nw };

                    // The starting index will be at zero
                    size_t begin = 0;

                    // The ending one is n if the workers are enough, otherwise the chunk_dim computed before
                    size_t end = nw != 1 ? std::min(chunk_dim, static_cast<size_t>(n)) : n;

                    if (nw == 1) {
                        std::pair<uint, uint> chunk_indexes = {begin, end};
                        auto f1 = pool.enqueue([&, chunk_indexes]() -> int {
                            return filteringnodework(std::ref(selected_nodes), std::ref(initialComponents), std::ref(graph), std::move( chunk_indexes ), 0);
                        }, 0);

                        filtering_nodefutures.push_back(std::move(f1));
                    }
                    else {
                        for (int i = 0; i < nw; i++) {
                            
                            std::pair<uint, uint> chunk_indexes = {begin, end};
                            auto f1 = pool.enqueue([&, chunk_indexes, i]() -> int {
                                return filteringnodework(std::ref(selected_nodes), std::ref(initialComponents), std::ref(graph), std::move( chunk_indexes ), i);
                            }, std::move(i));

                            filtering_nodefutures.push_back(std::move(f1));
                            
                            if (i == nw-2) {
                                // Last chunk
                                begin = end;
                                end = n;
                            }
                            else {
                                begin = end;
                                end = std::min(begin + chunk_dim, static_cast<size_t>(n));
                            }

                        }
                    }

                    for (auto &fut : filtering_nodefutures) {
                        int val = fut.get();
                    }
    
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

        // Close threadpool
        

    }

    return (0);


}