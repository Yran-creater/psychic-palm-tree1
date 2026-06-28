#include "station.h"
#include "graph.h"
#include "crawler.h"
#include <iostream>
#include <limits>
#include <cstdlib>

#ifdef _WIN32
#define MKDIR(cmd) system(("mkdir " + std::string(cmd) + " 2>nul").c_str())
#else
#define MKDIR(cmd) system(("mkdir -p " + std::string(cmd)).c_str())
#endif
int main() {

}