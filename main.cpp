#include <iostream>
#include "lib/utimer.hpp"
#include "lib/queue.hpp"
#include "lib/graph.hpp"
#include <vector>
#include <utility>


int main(int argc, char* argv[]) {

    Graph graph = Graph();

    graph.createGraph();

    std::unordered_map<uint, std::vector<Edge>> map = graph.getGraph();

    for (auto pair : map) {
        std::cout << "Node: " << pair.first << " ";
        for (auto edge : pair.second) {
            std::cout << "(" << edge.adjacent_node << "," << edge.weight << "),";
        }
        std::cout << std::endl;
        std::cout << std::endl;
    }

    std::cout << "Number of nodes: " << graph.getNumNodes() << std::endl;
    std::cout << "Number of edges: " << graph.getNumEdges() << std::endl;

    
    

    return (0);

}