#if !defined(__COMPONENTS_FF_H)
#define __COMPONENTS_FF_H

#include "../fastflow/ff/ff.hpp"
#include "graph.hpp"
#include "utils.hpp"


/**
 * 
 * MAP STRUCTURES
 * 
 */

struct Task_t {

    std::pair<uint, uint> chunk;
    Graph graph;
    int i;
    std::vector<MyEdge>& local_edge;

    Task_t(
        std::pair<uint,uint> chunk,
        Graph graph,
        int i,
        std::vector<MyEdge>& local_edge
    ) : chunk(chunk), graph(graph), i(i), local_edge(local_edge) {
        this->local_edge.assign(graph.originalNodes, {0,0,10});
    }

};

//! emitter node for evaluate stage
struct Emitter : ff::ff_monode_t<Task_t> {

    uint list_size;
    int num_w;
    Graph graph;
    std::vector<std::vector<MyEdge>> local_edges;   

    Emitter(
        uint list_size,
        int num_w,
        Graph graph
        ) : list_size(list_size), num_w(num_w), graph(graph) {
            this->local_edges.assign(this->num_w, {});
        }

    Task_t *svc(Task_t *in) override {

        if (in == nullptr) {

            // Portion of edges for each worker
            size_t chunk_dim{ list_size / num_w };

            // The starting index will be at zero
            size_t begin = 0;

            // The ending one is n if the workers are enough, otherwise the chunk_dim computed before
            size_t end = std::min(chunk_dim, static_cast<size_t>(list_size));


            if (this->num_w == 1) {
                std::pair<uint, uint> chunk = std::make_pair(begin, end);
                std::cout << chunk.first << "," << chunk.second << std::endl;

                // Only one worker, simply compute locally
                Task_t *task = new Task_t(chunk, graph, 0, std::ref(local_edges[0]));

                this->ff_send_out(task, 0);

                this->broadcast_task(this->EOS);

                return this->GO_ON;

            } 

            else {

                for (auto i = 0; i < num_w ; i++) {

                    std::pair<uint, uint> chunk = std::make_pair(begin, end);
                    std::cout << chunk.first << "," << chunk.second << std::endl;

                    // Only one worker, simply compute locally
                    Task_t *task = new Task_t(chunk, graph, i, std::ref(local_edges[i]));

                    this->ff_send_out(task, i);

                    if (i < num_w - 2) {

                        begin = end;
                        // Same as before for the ending index
                        end = std::min(begin + chunk_dim, static_cast<size_t>(list_size));
                    }

                }

                this->broadcast_task(this->EOS);

                begin = end;
                // Same as before for the ending index
                end = static_cast<size_t>(list_size);

                std::cout << begin << "," << end << std::endl;

                uint last_index = this->local_edges.size()-1;

                this->local_edges[last_index].assign(this->graph.originalNodes, {0,0,10});

                // Compute last chunk in the emitter
                for (uint i = begin; i < end; i++) {
                    // Retrieve edge from graph
                    MyEdge edge = this->graph.edges[i];

                    if (this->local_edges[last_index][edge.from].weight > edge.weight) {
                        // Found edge with same starting node and minimum weight, update local_edge
                        this->local_edges[last_index][edge.from].weight = edge.weight;
                        this->local_edges[last_index][edge.from].from = edge.from;
                        this->local_edges[last_index][edge.from].to = edge.to;
                    }
                }

                return this->GO_ON;

            }

        }

        delete in;

        return this->GO_ON;

    }

};



//! worker node for Evaluate stage
struct MapWorker : ff::ff_node_t<Task_t> {

    Task_t *svc(Task_t *in) {
        
        uint start = in->chunk.first;
        
        uint end = in->chunk.second;
        
        for (uint i = start; i < end; i++) {
            // Retrieve edge from graph
            MyEdge &edge = in->graph.edges[i];

            if (in->local_edge[edge.from].weight > edge.weight) {
                // Found edge with same starting node and minimum weight, update local_edge
                in->local_edge[edge.from].weight = edge.weight;
                in->local_edge[edge.from].from = edge.from;
                in->local_edge[edge.from].to = edge.to;
            }
        }

        return in;
    }

};

/**
 * 
 * MERGE STRUCTURES
 * 
 */

struct MergeTask_t {

    std::pair<uint, uint> chunk;
    int i;
    std::vector<std::vector<MyEdge>> local_edges;
    std::vector<MyEdge>& global_edge;

    MergeTask_t(
        std::pair<uint,uint> chunk,
        int i,
        std::vector<std::vector<MyEdge>> local_edges,
        std::vector<MyEdge>& global_edge
    ) : chunk(chunk), i(i), local_edges(local_edges), global_edge(global_edge) {}

};



//! emitter node for evaluate stage
struct MergeEmitter : ff::ff_monode_t<MergeTask_t> {

    uint list_size;
    int num_w;
    std::vector<std::vector<MyEdge>> local_edges;
    std::vector<MyEdge> global_edges;

    MergeEmitter(
        uint list_size,
        int num_w,
        std::vector<std::vector<MyEdge>> local_edges
        ) : list_size(list_size), num_w(num_w), local_edges(local_edges) {

            this->global_edges.assign(local_edges[0].size(), {0,0,10});

        }

    MergeTask_t *svc(MergeTask_t *in) override {

        if (in == nullptr) {

            // Portion of edges for each worker
            size_t chunk_dim{ list_size / num_w };

            // The starting index will be at zero
            size_t begin = 0;

            // The ending one is n if the workers are enough, otherwise the chunk_dim computed before
            size_t end = std::min(chunk_dim, static_cast<size_t>(list_size));


            if (this->num_w == 1) {
                std::pair<uint, uint> chunk = std::make_pair(begin, end);
                std::cout << chunk.first << "," << chunk.second << std::endl;
                // Only one worker, simply compute locally
                MergeTask_t *task = new MergeTask_t(chunk, 0, local_edges, std::ref(this->global_edges));

                this->ff_send_out(task, 0);

                this->broadcast_task(this->EOS);

                return this->GO_ON;

            } 

            else {

                for (auto i = 0; i < num_w - 1; i++) {

                    std::pair<uint, uint> chunk = std::make_pair(begin, end);
                    std::cout << chunk.first << "," << chunk.second << std::endl;
                    
                    MergeTask_t *task = new MergeTask_t(chunk, i, local_edges, std::ref(this->global_edges));

                    this->ff_send_out(task, i);

                    if (i < num_w - 2) {

                        begin = end;
                        // Same as before for the ending index
                        end = std::min(begin + chunk_dim, static_cast<size_t>(list_size));
                    }

                }

                this->broadcast_task(this->EOS);

                begin = end;
                // Same as before for the ending index
                end = static_cast<size_t>(list_size);

                std::cout << begin << "," << end << std::endl;

                // For each local_edge of each thread
                for (auto &local_edge : local_edges) {
                    
                    // Iterate through the indexes interval received
                    for (uint i = begin; i < end; i++) {
                        
                        if (local_edge[i].weight < this->global_edges[i].weight) {
                            // Update global_edges if the local_edges found by the thread i has a better weight
                            this->global_edges[i].weight = local_edge[i].weight;
                            this->global_edges[i].from = local_edge[i].from;
                            this->global_edges[i].to = local_edge[i].to;
                        }

                    }

                }

                return this->GO_ON;

            }

        }

        delete in;
    
        return this->GO_ON;

    }

};



//! worker node for Evaluate stage
struct MergeWorker : ff::ff_node_t<MergeTask_t> {

    MergeTask_t *svc(MergeTask_t *in) {
        
        uint start = in->chunk.first;
        
        uint end = in->chunk.second;
        
        // For each local_edge of each thread
        for (auto &local_edge : in->local_edges) {
            
            // Iterate through the indexes interval received
            for (uint i = start; i < end; i++) {
                
                if (local_edge[i].weight < in->global_edge[i].weight) {
                    // Update global_edges if the local_edges found by the thread i has a better weight
                    in->global_edge[i].weight = local_edge[i].weight;
                    in->global_edge[i].from = local_edge[i].from;
                    in->global_edge[i].to = local_edge[i].to;
                }

            }

        }
    
        return in;
    }

};


/**
 * 
 * CONTRACTION STRUCTURE
 * 
 */
struct ContractionTask_t {

    std::pair<uint, uint> chunk;
    int i;
    std::vector<MyEdge> global_edge;
    DisjointSets &initialComponents;

    ContractionTask_t(
        std::pair<uint,uint> chunk,
        int i,
        std::vector<MyEdge> global_edge,
        DisjointSets &initialComponents
    ) : chunk(chunk), i(i), global_edge(global_edge), initialComponents(initialComponents) {}

};



//! emitter node for evaluate stage
struct ContractionEmitter : ff::ff_monode_t<ContractionTask_t> {

    uint list_size;
    int num_w;
    std::vector<MyEdge> global_edges;
    DisjointSets &initialComponents;

    ContractionEmitter(
        uint list_size,
        int num_w,
        std::vector<MyEdge> global_edges,
        DisjointSets &initialComponents
        ) : list_size(list_size), num_w(num_w), global_edges(global_edges), initialComponents(initialComponents) {}

    ContractionTask_t *svc(ContractionTask_t *in) override {

        MyEdge NULL_CONN = {0,0,10};

        if (in == nullptr) {

            // Portion of edges for each worker
            size_t chunk_dim{ list_size / num_w };

            // The starting index will be at zero
            size_t begin = 0;

            // The ending one is n if the workers are enough, otherwise the chunk_dim computed before
            size_t end = std::min(chunk_dim, static_cast<size_t>(list_size));


            if (this->num_w == 1) {
                std::pair<uint, uint> chunk = std::make_pair(begin, end);
                std::cout << chunk.first << "," << chunk.second << std::endl;

                // Only one worker, simply compute locally
                ContractionTask_t *task = new ContractionTask_t(chunk, 0, this->global_edges, std::ref(initialComponents));

                this->ff_send_out(task, 0);

                this->broadcast_task(this->EOS);

                return this->GO_ON;

            } 

            else {

                for (auto i = 0; i < num_w - 1; i++) {

                    std::pair<uint, uint> chunk = std::make_pair(begin, end);
                    std::cout << chunk.first << "," << chunk.second << std::endl;

                    ContractionTask_t *task = new ContractionTask_t(chunk, i, this->global_edges, std::ref(initialComponents));

                    this->ff_send_out(task, i);

                    if (i < num_w - 2) {

                        begin = end;
                        // Same as before for the ending index
                        end = std::min(begin + chunk_dim, static_cast<size_t>(list_size));
                    }

                }

                this->broadcast_task(this->EOS);

                begin = end;
                // Same as before for the ending index
                end = static_cast<size_t>(list_size);

                std::cout << begin << "," << end << std::endl;
                    
                // Iterate through the indexes interval received
                for (uint i = begin; i < end; i++) {
                    
                        // Retrieve the edge found
                    MyEdge edge = this->global_edges[i];
                    
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

                return this->GO_ON;

            }

        }

        delete in;
    
        return this->GO_ON;

    }

};



//! worker node for Evaluate stage
struct ContractionWorker : ff::ff_node_t<ContractionTask_t> {

    ContractionTask_t *svc(ContractionTask_t *in) {

        MyEdge NULL_CONN = {0,0,10};
        
        uint start = in->chunk.first;
        
        uint end = in->chunk.second;

        for (uint i = start; i < end; i++) {

            // Retrieve the edge found
            MyEdge edge = in->global_edge[i];
            
            if (edge == NULL_CONN) {
                // If edge has default value, we do nothing
            }
            else {
                if (!in->initialComponents.same(edge.from, edge.to)) {
                    /**
                     * Access the UNION-FIND data structure and check if the starting node and ending node 
                     * have the same parent.
                     * If not, this means that we can unify the two trees 
                     */
                    in->initialComponents.unite(edge.from, edge.to);
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
       
    
        return in;
    }

};



/**
 * 
 * FILTERING STRUCTURES
 * 
 */
struct FilteringTask_t {

    std::pair<uint, uint> chunk;
    int i;
    Graph &graph;
    std::vector<MyEdge> &remaining_local_edges;
    DisjointSets &initialComponents;

    FilteringTask_t(
        std::pair<uint,uint> chunk,
        int i,
        Graph &graph,
        std::vector<MyEdge> &remaining_local_edges,
        DisjointSets &initialComponents
    ) : chunk(chunk), i(i), graph(graph), remaining_local_edges(remaining_local_edges), initialComponents(initialComponents) {}

};



//! emitter node for evaluate stage
struct FilterEdgeEmitter : ff::ff_monode_t<FilteringTask_t> {

    uint list_size;
    int num_w;
    Graph &graph;
    DisjointSets &initialComponents;
    std::vector<std::vector<MyEdge>> remaining_edges;

    FilterEdgeEmitter(
        uint list_size,
        int num_w,
        Graph &graph,
        DisjointSets &initialComponents
        ) : list_size(list_size), num_w(num_w), graph(graph), initialComponents(initialComponents) {
            this->remaining_edges.assign(num_w, {});
        }

    FilteringTask_t *svc(FilteringTask_t *in) override {

        if (in == nullptr) {

            // Portion of edges for each worker
            size_t chunk_dim{ list_size / num_w };

            // The starting index will be at zero
            size_t begin = 0;

            // The ending one is n if the workers are enough, otherwise the chunk_dim computed before
            size_t end = std::min(chunk_dim, static_cast<size_t>(list_size));


            if (this->num_w == 1) {
                std::pair<uint, uint> chunk = std::make_pair(begin, end);
                std::cout << begin << "," << end << std::endl;

                // Only one worker, simply compute locally
                FilteringTask_t *task = new FilteringTask_t(chunk, 0, std::ref(graph), std::ref(this->remaining_edges[0]), std::ref(initialComponents));

                this->ff_send_out(task, 0);

                this->broadcast_task(this->EOS);

                return this->GO_ON;

            } 

            else {

                for (auto i = 0; i < num_w - 1; i++) {

                    std::pair<uint, uint> chunk = std::make_pair(begin, end);
                    std::cout << begin << "," << end << std::endl;

                    FilteringTask_t *task = new FilteringTask_t(chunk, i, std::ref(graph), std::ref(this->remaining_edges[i]), std::ref(initialComponents));

                    this->ff_send_out(task, i);

                    if (i < num_w - 2) {

                        begin = end;
                        // Same as before for the ending index
                        end = std::min(begin + chunk_dim, static_cast<size_t>(list_size));
                    }

                }

                this->broadcast_task(this->EOS);

                begin = end;
                // Same as before for the ending index
                end = static_cast<size_t>(list_size);

                std::cout << begin << "," << end << std::endl;

                uint last_index = this->remaining_edges.size() - 1;
                    
                for (uint i = begin; i < end; i++) {
                
                    MyEdge edge = graph.edges[i];

                    if ( !initialComponents.same(edge.from, edge.to) )
                        /**
                         * If the starting and the ending node of each graph's edge are not in the same component, 
                         * then we need to keep it for the next iteration.
                         * Otherwise we discard it.
                         */
                        remaining_edges[last_index].push_back(edge);
                }

                return this->GO_ON;

            }

        }

        delete in;

        return this->GO_ON;

    }

};



//! worker node for Evaluate stage
struct FilterEdgeWorker : ff::ff_node_t<FilteringTask_t> {

    FilteringTask_t *svc(FilteringTask_t *in) {
        
        uint start = in->chunk.first;
        
        uint end = in->chunk.second;

        for (uint i = start; i < end; i++) {
        
            MyEdge edge = in->graph.edges[i];

            if ( !in->initialComponents.same(edge.from, edge.to) )
                /**
                 * If the starting and the ending node of each graph's edge are not in the same component, 
                 * then we need to keep it for the next iteration.
                 * Otherwise we discard it.
                 */
                in->remaining_local_edges.push_back(edge);
        }
       
    
        return in;
    }

};



/**
 * 
 * FILTERING STRUCTURES
 * 
 */
struct FilteringNodeTask_t {

    std::pair<uint, uint> chunk;
    int i;
    Graph &graph;
    std::vector<uint> &remaining_local_nodes;
    DisjointSets &initialComponents;

    FilteringNodeTask_t(
        std::pair<uint,uint> chunk,
        int i,
        Graph &graph,
        std::vector<uint> &remaining_local_nodes,
        DisjointSets &initialComponents
    ) : chunk(chunk), i(i), graph(graph), remaining_local_nodes(remaining_local_nodes), initialComponents(initialComponents) {}

};



//! emitter node for evaluate stage
struct FilterNodeEmitter : ff::ff_monode_t<FilteringNodeTask_t> {

    uint list_size;
    int num_w;
    Graph &graph;
    DisjointSets &initialComponents;
    std::vector<std::vector<uint>> remaining_nodes;

    FilterNodeEmitter(
        uint list_size,
        int num_w,
        Graph &graph,
        DisjointSets &initialComponents
        ) : list_size(list_size), num_w(num_w), graph(graph), initialComponents(initialComponents) {
            this->remaining_nodes.assign(num_w, {});
        }

    FilteringNodeTask_t *svc(FilteringNodeTask_t *in) override {

        if (in == nullptr) {

            // Portion of edges for each worker
            size_t chunk_dim{ list_size / num_w };

            // The starting index will be at zero
            size_t begin = 0;

            // The ending one is n if the workers are enough, otherwise the chunk_dim computed before
            size_t end = std::min(chunk_dim, static_cast<size_t>(list_size));


            if (this->num_w == 1) {
                std::pair<uint, uint> chunk = std::make_pair(begin, end);
                std::cout << begin << "," << end << std::endl;

                // Only one worker, simply compute locally
                FilteringNodeTask_t *task = new FilteringNodeTask_t(chunk, 0, std::ref(graph), std::ref(this->remaining_nodes[0]), std::ref(initialComponents));

                this->ff_send_out(task, 0);

                this->broadcast_task(this->EOS);

                return this->GO_ON;

            } 

            else {

                for (auto i = 0; i < num_w - 1; i++) {

                    std::pair<uint, uint> chunk = std::make_pair(begin, end);
                    std::cout << begin << "," << end << std::endl;

                    FilteringNodeTask_t *task = new FilteringNodeTask_t(chunk, i, std::ref(graph), std::ref(this->remaining_nodes[i]), std::ref(initialComponents));

                    this->ff_send_out(task, i);

                    if (i < num_w - 2) {

                        begin = end;
                        // Same as before for the ending index
                        end = std::min(begin + chunk_dim, static_cast<size_t>(list_size));
                    }

                }

                this->broadcast_task(this->EOS);

                begin = end;
                // Same as before for the ending index
                end = static_cast<size_t>(list_size);
                std::cout << begin << "," << end << std::endl;

                uint last_index = this->remaining_nodes.size() - 1;
                    
                for (uint i = begin; i < end; i++) {
                
                    if ( initialComponents.parent(graph.nodes[i]) == graph.nodes[i] ) 
                    /**
                     * If the parent node is the same as the node itself, then we need to keep it also 
                     * for next iteration.
                     * Otherwise it is a child of another node and we can discard it.
                     */
                        this->remaining_nodes[last_index].push_back(i);

                }

                return this->GO_ON;

            }

        }   

        delete in;
    
        return this->GO_ON;

    }

};



//! worker node for Evaluate stage
struct FilterNodeWorker : ff::ff_node_t<FilteringNodeTask_t> {

    FilteringNodeTask_t *svc(FilteringNodeTask_t *in) {
        
        uint start = in->chunk.first;
        
        uint end = in->chunk.second;

        for (uint i = start; i < end; i++) {
        
            if ( in->initialComponents.parent(in->graph.nodes[i]) == in->graph.nodes[i] ) 
                /**
                 * If the parent node is the same as the node itself, then we need to keep it also 
                 * for next iteration.
                 * Otherwise it is a child of another node and we can discard it.
                 */
                in->remaining_local_nodes.push_back(i);
            
        }
       
    
        return in;
    }

};




#endif