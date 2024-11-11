#ifndef INCLUDE_TT_HPP
#define INCLUDE_TT_HPP

#include <memory>

#include "types.hpp"

namespace Stella {

// Create a global table
class TTtable;
extern TTtable table;

// Structure for holding an entry in the hash table
//
// 32 bits -> key
// 16 bits -> score
// 16 bits -> eval
// 16 bits -> move
// 8 bits -> depth
// 8 bits -> node
// 8 bits -> age
//
// Totals to
struct alignas(16) TTentry {
public:
    // Return members of the entry
    uint32_t key() const { return key32; }
    int16_t score() const { return score16; }
    int16_t eval() const { return eval16; }
    Move move() const { return move16; }
    uint8_t depth() const { return depth8; }
    uint8_t node() const { return node8; }
    uint8_t age() const { return age8; }

    // Overload operators to allocate memory for an entry
    static void* operator new (size_t alignment, size_t size) {
        return aligned_malloc(alignment, size);
    }

    static void operator delete (void* ptr) {
        return aligned_free(ptr);
    }

private:
    friend class TTtable;
    uint32_t key32;
    uint16_t score16;
    uint16_t eval16;
    Move move16;
    uint8_t depth8;
    uint8_t node8;
    uint8_t age8;
};

// Class for a transposition table, allocates a given amount of memory
// to store TTentries that are used to quickly lookup previous scores in known positions
class TTtable {
private:
    // Table configuration
    friend struct TTentry;
    uint8_t generation;
    uint64_t num;
    uint64_t size;
    uint64_t maxSize = (1 << 12) * sizeof(TTentry);
    // Storage for the entries
    TTentry* entries;

public:
    // Destructor for the table so it cleans itself up
    ~TTtable();
    // Functions to access the transposition table

    // Used to save an entry to the transposition table, takes into account
    // the ages of stored entries aswell as the arguments given to determine
    // if an overwrite will occur.
    template<NodeType T>
    void save(Key key, Depth depth, Value score, Value eval, Move m, Bound b);

    // Function for retrieving an entry from the table, sets a given boolean to true if found
    TTentry* probe(Key key, bool& found) const;

    // Clear the table
    void clear();
    // Get the number of entries/size of the table
    size_t num_entries() const;
    size_t size_entries() const;
    size_t max_size() const;
    // Increment age when starting a new search, loop over if max is reached
    void new_search();
    // Resize/initialize the table to a given number of megabytes
    void resize(size_t mb);
    // Deallocate the memory from the table
    void dealloc();
    // Returns an approximation in the range [0, 1000] of how full the table is
    int hashfull() const;
    // Prefetch the entry for a given hash key
    void prefetch(Key key) const;
};

}

#endif
