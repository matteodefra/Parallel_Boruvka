#if !defined(__UTILS_H)
#define __UTILS_H

#include <iostream>


struct MyEdge {
    uint from;
    uint to;
    float weight;

    bool operator==(const MyEdge& conn) const {

        return (conn.from == from && conn.to == to) || (conn.from == to && conn.to == from); 

    }
};

#endif