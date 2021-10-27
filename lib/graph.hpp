#include <utility>
#include <iostream>
#include <vector>
#include <fstream>
#include <set>
#include <unordered_map>
#include <random>
#include "utils.hpp"

class Graph {

    private: 

        // Unordered map for fast access to adjiacency list
        std::unordered_map<uint, std::vector<Edge>> graph;
        // Total number of nodes
        uint num_nodes;
        // Total number of edges
        uint num_edges;

    public:

        Graph() {
            this->num_edges = 0;
            this->num_nodes = 0;
        }

        ~Graph() {}


        std::unordered_map<uint, std::vector<Edge>> getGraph() {

            return this->graph;

        }

        uint getNumNodes() {
            return this->num_nodes;
        }

        uint getNumEdges() {
            return this->num_edges;
        }


        /**
         * @brief Create graph object, by filling the vector of Edge above
         */ 
        void createGraph() {

            std::ifstream infile("data/bio-DM-LC.edges");

            const int MIN = -1;
            const int MAX = 1;

            uint a, b;
            float c;

            // Deleted first four lines of file, contains the information above
            while (infile >> a >> b >> c) {

                float variance = MIN + (double)(rand()) / ((double)(RAND_MAX/(MAX - MIN)));

                float weight = c + variance;

                // First insert pair (a,b)
                if (graph.find(a) == graph.end()) {
                    // Key not exist
                    // Create edge object
                    struct Edge edge;
                    edge.adjacent_node = b;
                    edge.weight = weight;

                    // Initialize vector of edges and insert (key,value) in the map
                    std::vector<Edge> edges;
                    edges.push_back(edge); 
                    graph.insert(std::make_pair(a, edges));

                    this->num_nodes++;
                    this->num_edges++;
                }
                else {

                    // Create edge object
                    struct Edge edge;
                    edge.adjacent_node = b;
                    edge.weight = weight;

                    graph[a].push_back(edge);

                    this->num_edges++;
                }

                // Then insert pair (b,a)
                if (graph.find(b) == graph.end()) {
                    // Key not exist
                    // Create edge object
                    struct Edge edge;
                    edge.adjacent_node = a;
                    edge.weight = weight;

                    // Initialize vector of edges and insert (key,value) in the map
                    std::vector<Edge> edges;
                    edges.push_back(edge); 
                    graph.insert(std::make_pair(b, edges));

                    this->num_nodes++;
                    this->num_edges++;
                }
                else {

                    // Create edge object
                    struct Edge edge;
                    edge.adjacent_node = a;
                    edge.weight = weight;

                    graph[b].push_back(edge);

                    this->num_edges++;
                }            

            }

            this->num_edges = this->num_edges / 2;

        }
        


};