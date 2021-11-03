#if !defined(__GRAPH_H)
#define __GRAPH_H

#include <utility>
#include <iostream>
#include <vector>
#include <fstream>
#include <set>
#include <unordered_map>
#include <random>
#include "utils.hpp"
#include <vector>

class Graph {

    private: 

        // Vector of nodes
        std::vector<uint> nodes;
        // Total number of nodes
        uint num_nodes;
        // Total number of edges
        uint num_edges;
        // List of edges
        std::vector<MyEdge> edges; 

    public:

        Graph() {
            this->num_edges = 0;
            this->num_nodes = 0;
        }

        ~Graph() {}


        std::vector<uint> getNodes() {
            return this->nodes;
        }

        uint getNumNodes() {
            return this->num_nodes;
        }

        uint getNumEdges() {
            return this->num_edges;
        }

        std::vector<MyEdge> getEdges() {
            return this->edges;
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

            std::set<uint> nodes;

            // Deleted first four lines of file, contains the information above
            while (infile >> a >> b >> c) {

                float variance = MIN + (double)(rand()) / ((double)(RAND_MAX/(MAX - MIN)));

                float weight = c + variance;

                edges.push_back({a, b, weight});

                this->num_edges += 1;

                nodes.insert(a);
                nodes.insert(b);

            }

            this->nodes.assign(nodes.begin(), nodes.end());

            this->num_nodes = this->nodes.size();

            this->num_edges = this->num_edges / 2;

        }
        


};

#endif