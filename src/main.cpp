#include <iostream>

#include "uci.hpp"
#include "position.hpp"
#include "bitboard.hpp"

using namespace Stella;

int main(int argc, char* argv[]) {
  Bitboards::init();
  Position::init();

  Uci(argc, argv);
}
