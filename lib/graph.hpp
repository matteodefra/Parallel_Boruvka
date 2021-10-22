#include <utility>
#include <iostream>
#include <vector>
#include <fstream>
#include <set>

class Graph {

    private: 

        // Vector of edges (implemented as a pair of <node, node>)
        std::vector<std::pair<uint,uint>> edges;
        // Vector of nodes
        std::set<uint> nodes;

    public:

        Graph() {}

        ~Graph() {}


        std::vector<std::pair<uint, uint>> getEdges() {

            return this->edges;

        }


        std::set<uint> getNodes() {

            return this->nodes;

        }


        /**
         * @brief Create graph object, by filling the vector of edges above
         */ 
        void createGraph() {

            std::ifstream infile("data/roadNet-CA.txt");

            // These values are fixed
            uint numberNodes = 1965206;
            uint numberEdges = 5533214;

            uint a, b;

            // Deleted first four lines of file, contains the information above
            while (infile >> a >> b) {

                std::pair<uint, uint> pair (a,b);

                this->edges.push_back(pair);

                this->nodes.insert(a);
                this->nodes.insert(b);

            }

        }
        


};