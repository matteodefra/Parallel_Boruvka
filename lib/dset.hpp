#if !defined(__UNIONFIND_H)
#define __UNIONFIND_H

#include <vector>
#include <atomic>
#include <iostream>

/**
 * Lock-free parallel disjoint set data structure (aka UNION-FIND)
 * with path compression and union by rank
 *
 * Supports concurrent find(), same() and unite() calls as described
 * in the paper
 *
 * "Wait-free Parallel Algorithms for the Union-Find Problem"
 * by Richard J. Anderson and Heather Woll
 *
 */
class DisjointSets {

    public:
    
        /**
         * @brief Construct a new Disjoint Sets object
         * 
         * @param size size of the union find data structure (aka number of nodes)
         * 
         * Initialize each parent with given node number, i.e. from 0 to size-1
         */
        DisjointSets(uint32_t size) : mData(size) {
            for (uint32_t i=0; i<size; ++i)
                mData[i] = (uint32_t) i;
        }

        /**
         * @brief 
         * 
         * @param id 
         * @return uint32_t 
         */
        uint32_t find(uint32_t id) const {
            while (id != parent(id)) {
                uint64_t value = mData[id];
                uint32_t new_parent = parent((uint32_t) value);
                uint64_t new_value =
                    (value & 0xFFFFFFFF00000000ULL) | new_parent;
                /* Try to update parent (may fail, that's ok) */
                if (value != new_value)
                    mData[id].compare_exchange_weak(value, new_value);
                id = new_parent;
            }
            return id;
        }

        /**
         * @brief Check if two node belongs to the same tree
         * 
         * @param id1 the first node
         * @param id2 the second node
         * @return true if the two id's belongs to the same subtree
         * @return false if the two id's does not belong to the same subtree
         * 
         * Iterative loop that calls find to get the parent of each node. If the parent are the same, 
         * the loop exit with true, otherwise false.
         * It is an iterative loop since atomic values can be changed at any time
         */
        bool same(uint32_t id1, uint32_t id2) const {
            for (;;) {
                id1 = find(id1);
                id2 = find(id2);
                if (id1 == id2)
                    return true;
                if (parent(id1) == id1)
                    return false;
            }
        }

        /**
         * @brief Unite two node under a given subtree
         * 
         * @param id1 first node
         * @param id2 second node
         * @return uint32_t 
         * 
         * Unite two different node under the same parent.
         * Iterative loop that find the parent of both nodes, swap the rank of both nodes and return the new parent
         */
        uint32_t unite(uint32_t id1, uint32_t id2) {
            for (;;) {
                id1 = find(id1);
                id2 = find(id2);

                if (id1 == id2)
                    return id1;

                uint32_t r1 = rank(id1), r2 = rank(id2);

                if (r1 > r2 || (r1 == r2 && id1 < id2)) {
                    std::swap(r1, r2);
                    std::swap(id1, id2);
                }

                uint64_t oldEntry = ((uint64_t) r1 << 32) | id1;
                uint64_t newEntry = ((uint64_t) r1 << 32) | id2;

                if (!mData[id1].compare_exchange_strong(oldEntry, newEntry))
                    continue;

                if (r1 == r2) {
                    oldEntry = ((uint64_t) r2 << 32) | id2;
                    newEntry = ((uint64_t) (r2+1) << 32) | id2;
                    /* Try to update the rank (may fail, that's ok) */
                    mData[id2].compare_exchange_weak(oldEntry, newEntry);
                }

                break;
            }
            return id2;
        }
        
        // Return the size of the data structure
        uint32_t size() const { return (uint32_t) mData.size(); }

        // Return the rank of a given parent node
        uint32_t rank(uint32_t id) const {
            return ((uint32_t) (mData[id] >> 32)) & 0x7FFFFFFFu;
        }

        /**
         * @brief Return the parent of a given node
         * 
         * @param id the given node
         * @return uint32_t the parent node
         * 
         * Return the parent corresponding to the given id
         */
        uint32_t parent(uint32_t id) const {
            return (uint32_t) mData[id];
        }

        friend std::ostream &operator<<(std::ostream &os, const DisjointSets &f) {
            for (size_t i=0; i<f.mData.size(); ++i)
                os << i << ": parent=" << f.parent(i) << ", rank=" << f.rank(i) << std::endl;
            return os;
        }

        mutable std::vector<std::atomic<uint64_t>> mData;

};

#endif /* __UNIONFIND_H */
