#ifndef PERFT_H_INCLUDED
#define PERFT_H_INCLUDED

#include "movegen.hpp"
#include "bitboard.hpp"
#include "position.hpp"
#include "misc.hpp"

namespace Stella {

void perft_test();
uint64_t perft(Position *pos, Depth depth);

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
      std::cout << move(m, pos->is_chess960()) << ": " << cnt << std::endl;
    }
  }
  // Return the total node count
  return nodes;
}

// Utility used for running multiple perft tests using the file perftest.epd
void perft_test() {
  // Load the data file
  static const char* data[] = {
#include "bench.epd"
  ""};
  // Iterate through the lines and perform a test for each line
  int num = 0;
  for (auto& line : data) {
    // Increment the counter per line
    num++;
    // Store the data in a vector that splits the string given a delimiter
    std::vector<std::string> splitLine = split(line, ';');
    // The FEN string in this format will be the first index
    std::string fen = splitLine.at(0);

    // Setup a position using the fen
    Position pos(fen, false);

    // Loop through every depth given per position
    for (size_t i = 1; i < splitLine.size(); ++i) {
      // Again store the data in a vector splitting the string
      std::vector<std::string> splitInfo = split(splitLine.at(i), ' ');
      // Get the depth and the verified node count respectively
      Depth d = std::stoll(splitInfo.at(0));
      uint64_t verify = std::stoll(splitInfo.at(1));

      // Print basic information about the position for debug purposes
      std::cout << std::endl << "Position " << num  << std::endl << "FEN: " << fen << std::endl;
      std::cout << "Depth: " << d << std::endl << std::endl;

      // Run a perft search on the position for a given depth
      uint64_t nodes = perft<true>(&pos, d);

      // Print the total nodes searched
      std::cout << std::endl << "Total Nodes: " << nodes << std::endl;

      // If the node count does not match the file then break the search
      if (nodes != verify) {
        std::cout << "Failed, expected: " << verify << " Got: " << nodes << std::endl;
        break;
      }
    }
  }
}



}

#endif
