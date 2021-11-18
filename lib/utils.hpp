#if !defined(__UTILS_H)
#define __UTILS_H

#include <iostream>


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

#endif