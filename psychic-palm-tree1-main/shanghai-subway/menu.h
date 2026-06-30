#pragma once

#include "station.h"
#include "graph.h"
#include "pathfinder.h"
#include <string>

class Menu {
public:
    Menu(StationManager* stations, MetroGraph* graph, PathFinder* pathfinder);

    void run();

private:
    StationManager* stations_;
    MetroGraph* graph_;
    PathFinder* pathfinder_;

    static constexpr const char* PROMPT = "请输入选项编号：";

    void pause() const;
    int readChoice() const;
    int readInt(const std::string& prompt) const;
    std::string readLine(const std::string& prompt) const;
    int selectStation(const std::string& prompt) const;

    void showMainMenu();
    void showStatusMenu();
    void showTimePathMenu();
    void showTransferPathMenu();

    void handlePathQuery(bool byTime, bool multiple);
    void handleManualStatusUpdate();
    void handleAffectedAnalysis();
    void handleStationQuery();
};
