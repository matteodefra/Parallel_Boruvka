#include <iostream>

struct Edge {
    uint adjacent_node;
    float weight;
};

struct LightestConnection {
    float weight;
    uint startingNode;
    uint endingNode;
};
