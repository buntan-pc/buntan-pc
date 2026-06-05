#ifndef BUNTAN_PC_VERILATOR_STDERR_H_
#define BUNTAN_PC_VERILATOR_STDERR_H_

#include <cstdio>

#ifdef VL_PRINTF
#undef VL_PRINTF
#endif

#define VL_PRINTF(...) std::fprintf(stderr, __VA_ARGS__)

#endif
