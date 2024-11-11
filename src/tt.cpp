#include <cstring>
#include <iostream>

#include "tt.hpp"
#include "types.hpp"

namespace Stella {

TTtable table;

TTtable::~TTtable() {
    dealloc();
}

template<NodeType T>
void TTtable::save(Key key, Depth depth, Value score, Value eval, Move m, Bound b) {
    // Get the entry for the given key
    TTentry* entry = &entries[(num - 1) & key];

    // Make sure depth is within bounds
    assert(depth >= 0 && depth <= MAX_PLY);
    // Make sure scores are within bounds
    assert(score > -VALUE_INFINITE && score < VALUE_INFINITE);
    assert(eval > -VALUE_INFINITE && eval < VALUE_INFINITE);
    // Make sure move exists
    assert(m.is_ok());
    // Make sure bound exists
    assert(b != BOUND_NONE);

    // Update entry info if empty
    if (entry->key32 == 0) {
        // Fill the entry
        entry->key32 = static_cast<uint32_t>(key);
        entry->score16 = static_cast<int16_t>(score);
        entry->eval16 = static_cast<int16_t>(eval);
        entry->move16 = m;
        entry->depth8 = static_cast<uint8_t>(depth);
        entry->node8 = static_cast<uint8_t>(b);
        entry->age8 = generation;
    }

    // Check for a possible replacement
    else {
        // Go from least expensive to most when checking for replacement conditions
        if (b == BOUND_EXACT
        || entry->age8 != generation
        || entry->depth8 <= depth
        || (entry->key32 == static_cast<uint32_t>(key) && entry->depth8 <= depth + 3)) {

            // Fill the entry
            entry->key32 = static_cast<uint32_t>(key);
            entry->score16 = static_cast<int16_t>(score);
            entry->eval16 = static_cast<int16_t>(eval);
            entry->move16 = m;
            entry->depth8 = static_cast<uint8_t>(depth);
            entry->node8 = static_cast<uint8_t>(T << 2 | b);
            entry->age8 = generation;
        }
    }
}

template void TTtable::save<PV>(Key key, Depth depth, Value score, Value eval, Move m, Bound b);
template void TTtable::save<NON_PV>(Key key, Depth depth, Value score, Value eval, Move m, Bound b);

TTentry* TTtable::probe(Key key, bool& found) const {
    // Get the entry for this key
    TTentry* entry = &entries[(num - 1) & key];
    // Check if entry exists or matches the given key
    if (entry->key32 == 0 || entry->key32 != static_cast<uint32_t>(key)) return found = false, entry;
    else return found = true, entry;
}

void TTtable::resize(size_t mb) {
    // Deallocate memory if entries exist
    if (entries) dealloc();

    // Calculate the new size of the table in bytes
    size_t bytes = mb * 1024 * 1024;

    // Set to allocate it aligned to powers of 2 for fastest access
    constexpr size_t alignment = 2 * 1024 * 1024;
    size = ((bytes + alignment - 1) / alignment) * alignment;

    // Allocate the memory to the entires
    entries = static_cast<TTentry*>(aligned_malloc(alignment, size));

    // Set internal variables to reflect current size of table
    num = size / sizeof(TTentry);

    // Clear the table after allocating it
    clear();
}

void TTtable::clear() {
    // Reset generation
    generation = 0;
    // Check for entries, if they exist set them to zero
    if (entries) std::memset(entries, 0, size);
}

void TTtable::dealloc() {
    // If entries exist then deallocate them
    if (entries) aligned_free(entries);
}

int TTtable::hashfull() const {
    // Start with a count of zero
    int count = 0;
    // Loop through 1000 entries as an approximate value
    for (int idx = 0; idx < 1000; ++idx)
        // Only count entries with a key that are at the current generation,
        // this is done to avoid counting older entries since they can be replaced
        count += static_cast<bool>(entries[idx].key32)
            && static_cast<bool>(entries[idx].age8 == generation);
    return count;
}

void TTtable::prefetch(Key key) const {
    // Get the entry for this key
    const TTentry* entry = &entries[(num - 1) & key];
    // Use intrinsic prefetch depending on OS
    #if defined(_WIN32) || defined(WIN32)
    _mm_prefetch(entry, _MM_HINT_T0);
    #else
    __builtin_prefetch(entry);
    #endif
}

void TTtable::new_search() {
    generation == 255 ? generation = 0 : generation++;
}

size_t TTtable::num_entries() const {
    return num;
}

size_t TTtable::size_entries() const {
    return size;
}

size_t TTtable::max_size() const {
    return maxSize;
}

}
