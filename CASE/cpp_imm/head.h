#ifndef HEAD_H
#define HEAD_H

// --- C++ Standard Library ---
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <random>
#include <stdexcept>
#include <cassert>
#include <algorithm>

// --- System-specific includes for Linux/Ubuntu ---
#include <unistd.h>     // For close()
#include <fcntl.h>      // For open(), O_RDONLY
#include <sys/mman.h>   // For mmap(), munmap()
#include <sys/stat.h>   // For fstat()
#include <cstring>      // For memcpy()
#include <queue>
// --- Type definitions ---
typedef long long int64;
typedef std::pair<double, double> dpair;

// --- Bring standard library names into the global scope ---
// This avoids having to write std::vector, std::string, etc.
using namespace std;

#endif // HEAD_H