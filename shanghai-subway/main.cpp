#include "station.h"
#include "graph.h"
#include "pathfinder.h"
#include "menu.h"
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#define MKDIR(cmd) system(("mkdir " + std::string(cmd) + " 2>nul").c_str())
#else
#define MKDIR(cmd) system(("mkdir -p " + std::string(cmd)).c_str())
#endif

static void setupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

int main() {
    setupConsole();
    MKDIR("data");

    StationManager stationMgr;
    MetroGraph graph(&stationMgr);
    PathFinder pathfinder(&graph, &stationMgr);
    Menu menu(&stationMgr, &graph, &pathfinder);

    std::cout << "正在加载地铁数据..." << std::endl;
    if (!stationMgr.loadFromCSV("data/Station.csv")) {
        std::cerr << "[错误] 无法加载 data/Station.csv，请确保数据文件存在" << std::endl;
        return 1;
    }
    if (!graph.loadEdgesFromCSV("data/Edge.csv")) {
        std::cerr << "[错误] 无法加载 data/Edge.csv，请确保数据文件存在" << std::endl;
        return 1;
    }
    menu.run();
    return 0;
}

