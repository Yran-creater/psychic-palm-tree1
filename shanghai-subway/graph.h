#ifndef GRAPH_H
#define GRAPH_H

#include <vector>
#include <unordered_map>
#include <string>
#include "station.h"

/**
 * 边结构体（有向图）
 */
struct Edge {
    int fromId;
    int toId;
    std::string line;        // 所属线路（含换乘信息）
    std::string direction;   // "内环" / "外环" / ""（空表示普通线路）
    double travelTime;       // 运行时间（分钟）

    Edge() : fromId(-1), toId(-1), travelTime(1.0) {}
    Edge(int f, int t, const std::string& l = "",
        const std::string& d = "", double tm = 1.0)
        : fromId(f), toId(t), line(l), direction(d), travelTime(tm) {
    }
};

/**
 * 有向图（邻接表）
 */
class MetroGraph {
public:
    MetroGraph(const StationManager* mgr = nullptr);
    void setStationManager(const StationManager* mgr);

    // 加载/保存
    bool loadEdgesFromCSV(const std::string& filename);
    bool saveEdgesToCSV(const std::string& filename) const;

    // 添加/删除边
    void addEdge(int from, int to, const std::string& line = "",
        const std::string& dir = "", double time = 1.0);
    bool removeEdge(int from, int to);

    // 查询（有向）
    std::vector<int> getOutNeighbors(int stationId) const;
    std::vector<const Station*> getOutNeighborStations(int stationId) const;
    std::vector<int> getInNeighbors(int stationId) const;
    std::vector<const Station*> getInNeighborStations(int stationId) const;

    const Edge* getEdge(int from, int to) const;
    double getTravelTime(int from, int to) const;
    std::string getLineBetween(int from, int to) const;
    std::string getDirection(int from, int to) const;
    bool hasEdge(int from, int to) const;

    // 四号线特殊
    bool isLine4(int from, int to) const;
    std::vector<Edge> getEdgesOnLine4() const;

    // 获取数据
    const std::vector<Edge>& getAllEdges() const { return edges; }
    size_t getEdgeCount() const { return edges.size(); }
    size_t getNodeCount() const { return adjacencyList.size(); }

    // 显示
    void displayGraphInfo() const;
    void displayAdjacencyList() const;
    void displayLine4Info() const;

private:
    const StationManager* stationMgr;
    std::vector<Edge> edges;
    std::unordered_map<int, std::vector<size_t>> adjacencyList;  // 出边索引

    std::string trim(const std::string& str) const;
    size_t findEdgeIndex(int from, int to) const;
};

#endif#pragma once
