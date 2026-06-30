#pragma once

#include <vector>
#include <string>
#include "graph.h"
#include "station.h"

struct PathResult {
    std::vector<int> nodes;
    double totalTime = 0.0;
    int transferCount = 0;
    std::vector<int> transferStations;
    int actualStopCount = 0;

    bool empty() const { return nodes.empty(); }
};

class PathFinder {
public:
    PathFinder(const MetroGraph* graph, const StationManager* stations);

    PathResult shortestPath(int fromId, int toId) const;
    std::vector<PathResult> kShortestTimePaths(int fromId, int toId, int k = 3) const;
    PathResult minTransferPath(int fromId, int toId) const;
    std::vector<PathResult> kMinTransferPaths(int fromId, int toId, int k = 3) const;

    void displayPath(const PathResult& path) const;
    void displayPaths(const std::vector<PathResult>& paths) const;

    std::vector<int> analyzeAffectedArea(int closedStationId) const;
    void displayAffectedArea(int closedStationId) const;
    void analyzeConnectivity() const;

private:
    const MetroGraph* graph_;
    const StationManager* stations_;

    struct SearchState {
        int node;
        std::string line;
    };

    struct StateKey {
        int node;
        std::string line;
        bool operator==(const StateKey& other) const {
            return node == other.node && line == other.line;
        }
    };

    struct StateKeyHash {
        size_t operator()(const StateKey& k) const {
            return std::hash<int>()(k.node) ^ (std::hash<std::string>()(k.line) << 1);
        }
    };

    bool canVisit(int nodeId) const;
    int calcTransfers(const std::vector<int>& nodes) const;
    int calcActualStops(const std::vector<int>& nodes) const;
    std::vector<int> findTransferStations(const std::vector<int>& nodes) const;
    PathResult buildPathResult(const std::vector<int>& nodes) const;

    PathResult dijkstraByTime(int fromId, int toId,
        const std::vector<std::pair<int, int>>& bannedEdges = {}) const;
    PathResult dijkstraByTransfer(int fromId, int toId,
        const std::vector<std::pair<int, int>>& bannedEdges = {}) const;

    enum class PathMetric { Time, Transfer };

    std::vector<PathResult> yenKPaths(
        int fromId, int toId, int k, PathMetric metric,
        PathResult(PathFinder::*finder)(int, int,
            const std::vector<std::pair<int, int>>&) const) const;

    bool isDuplicatePath(const std::vector<PathResult>& paths,
        const std::vector<int>& nodes) const;
    std::vector<std::string> pathLineSequence(const std::vector<int>& nodes) const;
};
