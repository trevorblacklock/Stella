#ifndef MISC_H_INCLUDED
#define MISC_H_INCLUDED

#include "types.hpp"

#include <string_view>
#include <vector>
#include <sstream>
#include <string>
#include <chrono>

namespace Stella {

// Convert a piece to a string
constexpr std::string_view pieceChar(".PNBRQK  pnbrqk ");

// Convert a square to a string
inline const std::string square(Square s) {
  return std::string{ char('a' + file_of(s)), char('1' + rank_of(s)) };
}

// Convert a move into a string
inline const std::string move(Move m, bool isChess960) {
  // Get the move squares
  Square from = m.from();
  Square to = m.to();

  // For special cases in castling not in Chess960 change to square
  if (m.type() == CASTLING && !isChess960) to = make_square(rank_of(from), to > from ? FILE_G : FILE_C);

  std::string s = square(from) + square(to);
  // Check for a promotion
  if (m.type() == PROMOTION) s += std::tolower(pieceChar[m.promotion()]);
  return s;
}

// Used for splitting a string into a vector
inline std::vector<std::string> split(const std::string& s, char delimiter) {
  // Create a new vector
  std::vector<std::string> result;
  // Create a stream
  std::stringstream ss(s);
  // Store seperated strings in here
  std::string f;

  // Loop through input
  while (getline(ss, f, delimiter)) result.push_back(f);

  return result;
}

// Class used to generate the time spent given a start point
class Timer {
public:
  // Functions for saving the start and end times
  void start() { start_time = std::chrono::high_resolution_clock::now(); }
  void end() { end_time = std::chrono::high_resolution_clock::now(); }
  // Returns the elapsed time between start and end call in milliseconds
  uint64_t elapsed() { return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count(); }
private:
  // Store the start and end times
  std::chrono::high_resolution_clock::time_point start_time;
  std::chrono::high_resolution_clock::time_point end_time;
};

// Class to generate pseudo random numbers.
// Referenced from Sebastiano Vigna at http://vigna.di.unimi.it/ftp/papers/xorshift.pdf
class Random {
private:
  uint64_t m_seed;
  uint64_t random_u64() {
    m_seed ^= m_seed >> 12;
    m_seed ^= m_seed << 25;
    m_seed ^= m_seed >> 27;
    return m_seed * 2685821657736338717LL;
  }
public:
  // Constructor for RNG given a seed
  Random(uint64_t seed) :
    m_seed(seed) { assert(seed); }
  // Return a random u64
  template<typename T>
  T random() { return T(random_u64()); }
  // Return a sparse random u64 for magic numbers
  template<typename T>
  T random_sparse() { return T(random_u64() & random_u64() & random_u64()); }
};

}

#endif
