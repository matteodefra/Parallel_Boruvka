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
};

#endif