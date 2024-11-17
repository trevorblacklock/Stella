#ifndef LAYERS_H_INCLUDED
#define LAYERS_H_INCLUDED

#include "common.hpp"

namespace Stella::Features {

// Initialize the arrays to store the layers
extern int16_t L0_WEIGHT[NB_L0 * NB_L1];
extern int16_t L0_BIAS[NB_L1];
extern int16_t L1_WEIGHT[NB_L1 * 2];
extern int32_t L1_BIAS[NB_L2];

// Initializer for all the network layers
void init();

}

#endif
