#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include <cstring>
#include <immintrin.h>

namespace Stella {

namespace Features {
// Define the feature sizes of the network
constexpr int NB_L0 = 16 * 12 * 64;
constexpr int NB_L1 = 512;
constexpr int NB_L2 = 1;
}

// Setup intrinsics based on computer,
// this ensures that instructions fit into the CPU's registers
#if defined (__AVX512F__)
using vec_reg_16 = __m512i;
using vec_reg_32 = __m512i;
#define vec_load(a)      _mm512_load_si512(a)
#define vec_store(a,b)   _mm512_store_si512(a,b)
#define vec_madd_16(a,b) _mm512_madd_epi16(a,b)
#define vec_add_16(a,b)  _mm512_add_epi16(a,b)
#define vec_sub_16(a,b)  _mm512_sub_epi16(a,b)
#define vec_add_32(a,b)  _mm512_add_epi32(a,b)
#define vec_sub_32(a,b)  _mm512_sub_epi32(a,b)
#define vec_max_16(a,b)  _mm512_max_epi16(a,b)
#define BIT_ALIGNMENT 512
#define NB_REGISTER 16

#elif defined(__AVX2__)
using vec_reg_16 = __m256i;
using vec_reg_32 = __m256i;
#define vec_load(a)      _mm256_load_si256(a)
#define vec_store(a,b)   _mm256_store_si256(a,b)
#define vec_madd_16(a,b) _mm256_madd_epi16(a,b)
#define vec_add_16(a,b)  _mm256_add_epi16(a,b)
#define vec_sub_16(a,b)  _mm256_sub_epi16(a,b)
#define vec_add_32(a,b)  _mm256_add_epi32(a,b)
#define vec_sub_32(a,b)  _mm256_sub_epi32(a,b)
#define vec_max_16(a,b)  _mm256_max_epi16(a,b)
#define BIT_ALIGNMENT 256
#define NB_REGISTER 16

#elif defined(__SSE__) \
    || defined(__SSE2__) \
    || defined(__SSE3__) \
    || defined(__SSE41__) \
    || defined(__SSE42__) \
    || defined(__AVX__)
using vec_reg_16 = __m128i;
using vec_reg_32 = __m128i;
#define vec_load(a)      _mm_load_si128(a)
#define vec_store(a,b)   _mm_store_si128(a,b)
#define vec_madd_16(a,b) _mm_madd_epi16(a,b)
#define vec_add_16(a,b)  _mm_add_epi16(a,b)
#define vec_sub_16(a,b)  _mm_sub_epi16(a,b)
#define vec_add_32(a,b)  _mm_add_epi32(a,b)
#define vec_sub_32(a,b)  _mm_sub_epi32(a,b)
#define vec_max_16(a,b)  _mm_max_epi16(a,b)
#define BIT_ALIGNMENT 128
#define NB_REGISTER 16
#endif

// Define the spacing and byte alignments
constexpr int INT16_SPACING = BIT_ALIGNMENT / 16;
constexpr int BYTE_ALIGNMENT = BIT_ALIGNMENT / 8;
constexpr int CHUNK_UNROLL = BIT_ALIGNMENT;

class Position;
namespace Network {
class Evaluator;
}

}

#endif
