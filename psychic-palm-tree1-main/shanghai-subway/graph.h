#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include "station.h"

struct Edge {
    int fromId;
    int toId;
    std::string line;
    std::string direction;
    double travelTime;

    Edge() : fromId(-1), toId(-1), travelTime(1.0) {}
    Edge(int f, int t, const std::string& l = "",
        const std::string& d = "", double tm = 1.0)
        : fromId(f), toId(t), line(l), direction(d), travelTime(tm) {
    }

    bool isTransfer() const;
};

class MetroGraph {
public:
    MetroGraph(const StationManager* mgr = nullptr);
    void setStationManager(const StationManager* mgr);

    bool loadEdgesFromCSV(const std::string& filename);
    bool saveEdgesToCSV(const std::string& filename) const;

    void addEdge(int from, int to, const std::string& line = "",
        const std::string& dir = "", double time = 1.0);
    bool removeEdge(int from, int to);

    std::vector<int> getOutNeighbors(int stationId) const;
    std::vector<size_t> getOutEdgeIndices(int stationId) const;
    std::vector<const Station*> getOutNeighborStations(int stationId) const;
    std::vector<int> getInNeighbors(int stationId) const;
    std::vector<const Station*> getInNeighborStations(int stationId) const;

    const Edge* getEdge(int from, int to) const;
    double getTravelTime(int from, int to) const;
    std::string getLineBetween(int from, int to) const;
    std::string getDirection(int from, int to) const;
    bool hasEdge(int from, int to) const;

    bool isLine4(int from, int to) const;
    std::vector<Edge> getEdgesOnLine4() const;

    const std::vector<Edge>& getAllEdges() const { return edges; }
    size_t getEdgeCount() const { return edges.size(); }
    size_t getNodeCount() const { return adjacencyList.size(); }

    void displayGraphInfo() const;
    void displayAdjacencyList() const;
    void displayLine4Info() const;

private:
    const StationManager* stationMgr;
    std::vector<Edge> edges;
    std::unordered_map<int, std::vector<size_t>> adjacencyList;

    std::string trim(const std::string& str) const;
    size_t findEdgeIndex(int from, int to) const;
};
