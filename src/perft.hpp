#ifndef PERFT_H_INCLUDED
#define PERFT_H_INCLUDED

#include "movegen.hpp"
#include "bitboard.hpp"
#include "position.hpp"
#include "misc.hpp"

namespace Stella {

// A tool used to verify move generation. Given a depth will traverse every legal move
// up to that depth and return the number of nodes traversed.
template<bool root>
uint64_t perft(Position *pos, Depth depth) {
  // Set initial count to zero
  uint64_t cnt,nodes = 0;
  // Initialize move
  Move m;

  // Check if depth is 2, if so can implement a trick during search
  const bool leaf = (depth == 2);
  // Initialize a move generator for the position
  Generator gen(pos);

  // Loop through every legal move
  while ((m = gen.next_best<LEGAL>()) != Move::none()) {

    // If at the root just count the nodes, don't bother calling recursively
    if (root && depth <= 1)
      cnt = 1, nodes++;

    else {
      // Make the move
      pos->do_move<false>(m);
      // If the depth is two can count all root moves from the next position, otherwise recursively call perft
      cnt = leaf ? Generator(pos).count<LEGAL>() : perft<false>(pos, depth - 1);
      // Count the nodes
      nodes += cnt;
      // Undo the move
      pos->undo_move(m);
    }

    if (root) {
      std::cout << from_move(m, pos->is_chess960()) << ": " << cnt << std::endl;
    }
  }
  // Return the total node count
  return nodes;
}

}

#endif
