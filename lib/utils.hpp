#if !defined(__UTILS_H)
#define __UTILS_H

#include <iostream>
/**
 * @brief Struct consisting in an Edge object
 * 
 * Stores the starting, ending node and the weight of the given edge.
 * Override of the equality operator, the greater operator and the cout operator
 */
struct MyEdge {
    uint from;
    uint to;
    float weight;

    bool operator==(const MyEdge& conn) const {

        return (conn.from == from && conn.to == to); 

    }

    bool operator<(const MyEdge& conn) const {
        return (conn.from > from) || (conn.from == from && conn.to > to);
    }

    friend std::ostream& operator<< (std::ostream& out, const MyEdge& edge);

};


std::ostream& operator<<(std::ostream& os, const MyEdge& edge) {

    os << "Edge from: " << edge.from << " to " << edge.to << " weight: " << edge.weight; 

    return os;

}


// float compute_MST(DisjointSets &initialComponents, Graph &graph) {

//     std::vector<int> nodes;

//     float weight = 0;

//     for (int i = 0; i < initialComponents.mData.size(); i++) {
//         if (i != initialComponents.parent(i)) {
//             MyEdge edge = {i, initialComponents.parent(i), 10};
//             for (auto &_edge : graph.edges) {
//                 if (_edge == edge) {
//                     weight += _edge.weight;
//                 }
//             }
//         }
//     }

//     return weight;

// }


#endif