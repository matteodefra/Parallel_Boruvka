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

    public:

        // Vector of nodes
        std::vector<uint> nodes;

        // List of edges
        std::vector<MyEdge> edges; 

        uint originalNodes;

        Graph() {}

        ~Graph() {}

        // Parameterized Constructors for
        // for implementing deep copy
        Graph(Graph& sample)
        {
            this->nodes = sample.nodes;
            this->edges = sample.edges;
            this->originalNodes = sample.originalNodes;
        }


        std::vector<uint> getNodes() {
            return this->nodes;
        }

        uint getNumNodes() {
            return this->nodes.size();
        }

        uint getNumEdges() {
            return this->edges.size();
        }

        std::vector<MyEdge> getEdges() {
            return this->edges;
        }

        void updateNodes(std::vector<uint>& newNodes) {
            this->nodes.clear();
            this->nodes = newNodes;
        }

        void updateEdges(std::vector<MyEdge>& newEdges) {
            this->edges.clear();
            this->edges = newEdges;
        }

        /**
         * @brief Create graph object, by filling the vector of Edge above
         */ 
        void loadGraph(std::string filename) {

            std::ifstream infile(filename);

            const int MIN = -1;
            const int MAX = 1;

            uint a, b;
            float c;

            std::set<uint> nodes;
            std::set<MyEdge> edges;

            // Deleted first four lines of file, contains the information above
            while (infile >> a >> b >> c) {

                float variance = MIN + (double)(rand()) / ((double)(RAND_MAX/(MAX - MIN)));

                float weight = c + variance;

                if (filename == "data/sc-rel9.edges") {
                    a = a-1;
                    b = b-1;
                }

                if (a != b) {
                    edges.insert({a, b, weight});
                    edges.insert({b, a, weight});

                    nodes.insert(a);
                    nodes.insert(b);
                }

            }

            this->nodes.assign(nodes.begin(), nodes.end());
            this->edges.assign(edges.begin(), edges.end());

            this->originalNodes = this->nodes.size();

        }



        /**
         * @brief Create graph object, by filling the vector of Edge above
         */ 
        void loadGraphUnweighted(std::string filename) {

            std::ifstream infile(filename);

            const int MIN = 0;
            const int MAX = 10;

            uint a, b;

            std::set<uint> nodes;
            std::set<MyEdge> edges;

            // Deleted first four lines of file, contains the information above
            while (infile >> a >> b) {

                float weight = MIN + (double)(rand()) / ((double)(RAND_MAX/(MAX - MIN)));

                a = a-1;
                b = b-1;

                if (a != b) {

                    edges.insert({a, b, weight});
                    edges.insert({b, a, weight});

                    nodes.insert(a);
                    nodes.insert(b);

                }

            }

            this->nodes.assign(nodes.begin(), nodes.end());
            this->edges.assign(edges.begin(), edges.end());

            this->originalNodes = this->getNumNodes();

        }

        


        void generateGraph(int n, long unsigned int e /*vertices number*/) {

            const int MIN = 1;
            const int MAX = 10;

            std::set<uint> nodes;
            std::set<MyEdge> edges;

            while (edges.size() < e) {
                uint x = rand() % n;
                uint y = rand() % n;

                if (y < x) {

                    float weight = MIN + (double)(rand()) / ((double)(RAND_MAX/(MAX - MIN)));

                    edges.insert({x, y, weight});
                    edges.insert({y, x, weight});
                    
                    nodes.insert(x);
                    nodes.insert(y);

                }
            }

            this->nodes.assign(nodes.begin(), nodes.end());
            this->edges.assign(edges.begin(), edges.end());

            std::ofstream file ("graph.txt");

            if (file.is_open()) {
                for (auto &edge : this->edges) {
                    file << edge.from << " " << edge.to << " " << edge.weight << "\n";
                }
            }

            file.close();

            this->originalNodes = this->nodes.size();

        }

        friend std::ostream& operator<< (std::ostream& out, const Graph& graph);


};


std::ostream& operator<<(std::ostream& os, const Graph& graph) {

    os << "Node set" << std::endl;

    for (auto &node : graph.nodes) {
        os << node << std::endl;
    }

    os << std::endl;

    os << "Edges set" << std::endl;

    for (auto &edge : graph.edges) {
        os << "Node from " << edge.from << " to " << edge.to << " with weight " << edge.weight << std::endl;
    }

    return os;

}

#endif