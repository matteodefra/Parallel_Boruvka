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

/**
 * @brief Struct Task_t: used to pass object between farm in first Map stage.
 * 
 * @param chunk         Starting and ending indexes of chunk to compute.
 * @param graph         The graph structure.
 * @param i             The thread id.
 * @param local_edge    The local edge found to be modified
 * 
 */
struct Task_t {

    std::pair<uint, uint> chunk;
    Graph graph;
    int i;
    std::vector<MyEdge>& local_edge;

    // We allocate space for local_edge in the constructor
    Task_t(
        std::pair<uint,uint> chunk,
        Graph graph,
        int i,
        std::vector<MyEdge>& local_edge
    ) : chunk(chunk), graph(graph), i(i), local_edge(local_edge) {
        this->local_edge.assign(graph.originalNodes, {0,0,10});
    }

};

/**
 * @brief               Emitter: used to distribute edges indexes to threads and compute last chunk
 * 
 * @param list_size     Number of edges to distribute
 * @param num_w         The number of workers at our disposal
 * @param graph         The graph structure to access
 * @param local_edges   Vector of vectors storing the minimum edges found by each thread
 * 
 */
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

            // If only 1 worker, we let the emitter compute all the edges
            if (this->num_w == 1) {
                std::pair<uint, uint> chunk = std::make_pair(begin, end);

                // Dispatch task to the single node
                Task_t *task = new Task_t(chunk, graph, 0, std::ref(local_edges[0]));

                this->ff_send_out(task, 0);

                this->broadcast_task(this->EOS);

                return this->GO_ON;

            } 

            else {

                // Otherwise, distribute chunks between workers
                for (auto i = 0; i < num_w ; i++) {

                    std::pair<uint, uint> chunk = std::make_pair(begin, end);

                    // Dispatch task to thread i
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
                end = static_cast<size_t>(list_size);

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


/**
 * @brief MapWorker: fastflow worker to compute minimum local edges
 * 
 */
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
 * @brief Struct Task_t: used to pass object between farm in first Map stage.
 * 
 * @param chunk         Starting and ending indexes of chunk to compute.
 * @param graph         The graph structure.
 * @param i             The thread id.
 * @param local_edge    The local edge found to be modified
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


/**
 * @brief               MergeEmitter: used to distribute indexes to threads and compute last chunk
 * 
 * @param list_size     Number of indexes to distribute
 * @param num_w         The number of workers at our disposal
 * @param local_edges   The minimum local_edges found by each thread in the previou stage
 * @param global_edges  The final vector to store the result
 * 
 */
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
                // Dispatch task to the single node 
                MergeTask_t *task = new MergeTask_t(chunk, 0, local_edges, std::ref(this->global_edges));

                this->ff_send_out(task, 0);

                this->broadcast_task(this->EOS);

                return this->GO_ON;

            } 

            else {

                for (auto i = 0; i < num_w - 1; i++) {

                    std::pair<uint, uint> chunk = std::make_pair(begin, end);
                    
                    //Dispatch task to the worker
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


/**
 * @brief MapWorker: fastflow worker to select minimum among the local_edges indexes
 * 
 */
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
 * CONTRACTION STRUCTURES
 * 
 */
/**
 * @brief                   Struct ContractionTask_t: used to pass object between farm in the contraction phase.
 * 
 * @param chunk             Starting and ending indexes of chunk to compute.
 * @param i                 The thread id.
 * @param global_edge       The minimum global edges computed in the latter step.
 * @param initialComponents The UNION-FIND data structure to be modified
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


/**
 * @brief                   ContractionEmitter: used to distribute indexes to threads and compute last chunk
 * 
 * @param list_size         Number of indexes to distribute
 * @param num_w             The number of workers at our disposal
 * @param global_edges      The vector of minimum edges found in the previous step
 * @param initialComponents The UNION-FIND data structure to be modified
 * 
 */
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

                // Dispatch task to the single node
                ContractionTask_t *task = new ContractionTask_t(chunk, 0, this->global_edges, std::ref(initialComponents));

                this->ff_send_out(task, 0);

                this->broadcast_task(this->EOS);

                return this->GO_ON;

            } 

            else {

                for (auto i = 0; i < num_w - 1; i++) {

                    std::pair<uint, uint> chunk = std::make_pair(begin, end);

                    // Dispatch task to thread i
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


/**
 * @brief ContractionWorker: fastflow worker to unify disjoint trees
 * 
 */
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
/**
 * @brief                       Struct FilteringTask_t: used to pass object between farm in the filtering phase.
 * 
 * @param chunk                 Starting and ending indexes of chunk to compute.
 * @param i                     The thread id.
 * @param graph                 The graph data structure
 * @param remaining_local_edges The vector where to store remaning edges needed to be processed
 * @param initialComponents     The UNION-FIND data structure to be modified
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


/**
 * @brief                       FilterEdgeEmitter: used to distribute indexes to workers
 * 
 * @param list_size             Indexes to be processed
 * @param num_w                 The workers at our disposal.
 * @param graph                 The graph data structure
 * @param initialComponents     The UNION-FIND data structure to be modified
 * @param remaining_edges       The vector of vectors where to store the remaining edges
 * 
 */
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

                FilteringTask_t *task = new FilteringTask_t(chunk, 0, std::ref(graph), std::ref(this->remaining_edges[0]), std::ref(initialComponents));

                this->ff_send_out(task, 0);

                this->broadcast_task(this->EOS);

                return this->GO_ON;

            } 

            else {

                for (auto i = 0; i < num_w - 1; i++) {

                    std::pair<uint, uint> chunk = std::make_pair(begin, end);

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


/**
 * @brief FilterEdgeWorker: fastflow worker to check for remaining edge
 * 
 */
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
/**
 * @brief                       Struct FilteringNodeTask_t: used to pass object between farm in the filtering node phase.
 * 
 * @param chunk                 Starting and ending indexes of chunk to compute.
 * @param i                     The thread id.
 * @param graph                 The graph data structure
 * @param remaining_local_nodes The vector where to store remaning nodes needed to be processed
 * @param initialComponents     The UNION-FIND data structure to be modified
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


/**
 * @brief                       FilterNodeEmitter: used to distribute indexes to workers
 * 
 * @param list_size             Indexes to be processed
 * @param num_w                 The workers at our disposal.
 * @param graph                 The graph data structure
 * @param initialComponents     The UNION-FIND data structure to be modified
 * @param remaining_nodes       The vector of vectors where to store the remaining nodes
 * 
 */
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

                FilteringNodeTask_t *task = new FilteringNodeTask_t(chunk, 0, std::ref(graph), std::ref(this->remaining_nodes[0]), std::ref(initialComponents));

                this->ff_send_out(task, 0);

                this->broadcast_task(this->EOS);

                return this->GO_ON;

            } 

            else {

                for (auto i = 0; i < num_w - 1; i++) {

                    std::pair<uint, uint> chunk = std::make_pair(begin, end);

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


/**
 * @brief FilterNodeWorker: fastflow worker to compute remaining nodes
 * 
 */
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