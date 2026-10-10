// Sizes of the emulated GameCube memories. Shared by main.cpp (Aurora config) and the allocator glue
// (sdk/jkr_new_delete.cpp), which must recognize pointers that live inside MEM1.
#ifndef SUNSHINE_CONFIG_H
#define SUNSHINE_CONFIG_H

constexpr unsigned kSunshineMem1Size = 24u * 1024 * 1024; // main RAM: backs the OS arena the JKR heaps are carved from
constexpr unsigned kSunshineMem2Size = 16u * 1024 * 1024; // ARAM: backs JKRAram

#endif
