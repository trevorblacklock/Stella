#include "layers.hpp"
#include "../incbin/incbin.h"

// Define the incbin style
#define INCBIN_STYLE INCBIN_STYLE_CAMEL

namespace Stella::Features {

// Load the network
INCBIN(Stella, EVALFILE);

// Define network layers and align to the byte alignment
alignas(BYTE_ALIGNMENT) int16_t L0_WEIGHT[NB_L0 * NB_L1];
alignas(BYTE_ALIGNMENT) int16_t L0_BIAS[NB_L1];
alignas(BYTE_ALIGNMENT) int16_t L1_WEIGHT[NB_L1 * 2];
alignas(BYTE_ALIGNMENT) int32_t L1_BIAS[NB_L2];

// Define the initializer
void init() {
    // Keep track of the current index
    int idx = 0;
    // Copy in the layers and increment the index when needed
    std::memcpy(L0_WEIGHT, &gStellaData[idx], sizeof(int16_t) * NB_L0 * NB_L1);
    idx += sizeof(int16_t) * NB_L0 * NB_L1;
    std::memcpy(L0_BIAS, &gStellaData[idx], sizeof(int16_t) * NB_L1);
    idx += sizeof(int16_t) * NB_L1;
    std::memcpy(L1_WEIGHT, &gStellaData[idx], sizeof(int16_t) * NB_L1 * 2);
    idx += sizeof(int16_t) * NB_L1 * 2;
    std::memcpy(L1_BIAS, &gStellaData[idx], sizeof(int32_t));
}

}
