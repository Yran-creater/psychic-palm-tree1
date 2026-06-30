#include "graph.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

static const char kTransfer[] = "\xe6\x8d\xa2\xe4\xb9\x98";
static const char kLine4[] = "4\xe5\x8f\xb7\xe7\xba\xbf";
static const char kNone[] = "\xe6\x97\xa0";
static const char kEdgeHeader[] = "\xe8\xb5\xb7\xe7\x82\xb9\xe7\xab\x99ID,\xe7\xbb\x88\xe7\x82\xb9\xe7\xab\x99ID,\xe6\x89\x80\xe5\xb1\x9e\xe7\xba\xbf\xe8\xb7\xaf,\xe8\xbf\x90\xe8\xa1\x8c\xe6\x96\xb9\xe5\x90\x91,\xe8\xbf\x90\xe8\xa1\x8c\xe6\x97\xb6\xe9\x97\xb4\n";

bool Edge::isTransfer() const { return line.find(kTransfer) != std::string::npos; }

MetroGraph::MetroGraph(const StationManager* mgr) : stationMgr(mgr) {}
void MetroGraph::setStationManager(const StationManager* mgr) { stationMgr = mgr; }

std::string MetroGraph::trim(const std::string& str) const {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

size_t MetroGraph::findEdgeIndex(int from, int to) const {
    for (size_t i = 0; i < edges.size(); ++i) {
        if (edges[i].fromId == from && edges[i].toId == to) return i;
    }
    return static_cast<size_t>(-1);
}

bool MetroGraph::loadEdgesFromCSV(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[ERR] open failed: " << filename << std::endl;
        return false;
    }
    edges.clear();
    adjacencyList.clear();
    std::string line;
    bool first = true;
    int loaded = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (first) { first = false; continue; }
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(ss, token, ',')) tokens.push_back(trim(token));
        if (tokens.size() < 5) continue;
        try {
            Edge e;
            e.fromId = std::stoi(tokens[0]);
            e.toId = std::stoi(tokens[1]);
            e.line = tokens[2];
            e.direction = (tokens[3] == kNone || tokens[3] == "none") ? "" : tokens[3];
            e.travelTime = std::stod(tokens[4]);
            if (tokens[2] == "换乘") {
                const Station* st = stationMgr->findById(e.toId);
                if (st) {
                    e.line = st->line;   // 设置为目标站点所属线路
                }
                else {
                    std::cerr << "[警告] 换乘边目标站点 " << e.toId << " 不存在，跳过" << std::endl;
                    continue;
                }
            }
            size_t idx = edges.size();
            edges.push_back(e);
            adjacencyList[e.fromId].push_back(idx);
            loaded++;
        } catch (...) {}
    }
    file.close();
    std::cout << "[INFO] loaded edges: " << loaded << std::endl;
    return true;
}

bool MetroGraph::saveEdgesToCSV(const std::string& filename) const {
    std::ofstream file(filename, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;
    file << "\xEF\xBB\xBF";
    file << kEdgeHeader;
    for (const auto& e : edges) {
        file << e.fromId << "," << e.toId << "," << e.line << ","
            << (e.direction.empty() ? kNone : e.direction) << ","
            << std::fixed << std::setprecision(1) << e.travelTime << "\n";
    }
    file.close();
    return true;
}

void MetroGraph::addEdge(int from, int to, const std::string& line,
    const std::string& dir, double time) {
    if (hasEdge(from, to)) return;
    size_t idx = edges.size();
    edges.emplace_back(from, to, line, dir, time);
    adjacencyList[from].push_back(idx);
}

bool MetroGraph::removeEdge(int from, int to) {
    size_t idx = findEdgeIndex(from, to);
    if (idx == static_cast<size_t>(-1)) return false;
    auto& list = adjacencyList[from];
    list.erase(std::remove(list.begin(), list.end(), idx), list.end());
    edges.erase(edges.begin() + idx);
    adjacencyList.clear();
    for (size_t i = 0; i < edges.size(); ++i) adjacencyList[edges[i].fromId].push_back(i);
    return true;
}

std::vector<int> MetroGraph::getOutNeighbors(int stationId) const {
    std::vector<int> res;
    auto it = adjacencyList.find(stationId);
    if (it == adjacencyList.end()) return res;
    for (size_t idx : it->second) res.push_back(edges[idx].toId);
    return res;
}

std::vector<size_t> MetroGraph::getOutEdgeIndices(int stationId) const {
    auto it = adjacencyList.find(stationId);
    if (it == adjacencyList.end()) return {};
    return it->second;
}

std::vector<const Station*> MetroGraph::getOutNeighborStations(int stationId) const {
    std::vector<const Station*> res;
    if (!stationMgr) return res;
    for (int id : getOutNeighbors(stationId)) {
        const Station* st = stationMgr->findById(id);
        if (st) res.push_back(st);
    }
    return res;
}

std::vector<int> MetroGraph::getInNeighbors(int stationId) const {
    std::vector<int> res;
    for (const auto& e : edges) if (e.toId == stationId) res.push_back(e.fromId);
    return res;
}

std::vector<const Station*> MetroGraph::getInNeighborStations(int stationId) const {
    std::vector<const Station*> res;
    if (!stationMgr) return res;
    for (int id : getInNeighbors(stationId)) {
        const Station* st = stationMgr->findById(id);
        if (st) res.push_back(st);
    }
    return res;
}

const Edge* MetroGraph::getEdge(int from, int to) const {
    size_t idx = findEdgeIndex(from, to);
    return (idx != static_cast<size_t>(-1)) ? &edges[idx] : nullptr;
}

double MetroGraph::getTravelTime(int from, int to) const {
    const Edge* e = getEdge(from, to);
    return e ? e->travelTime : -1.0;
}

std::string MetroGraph::getLineBetween(int from, int to) const {
    const Edge* e = getEdge(from, to);
    return e ? e->line : "";
}

std::string MetroGraph::getDirection(int from, int to) const {
    const Edge* e = getEdge(from, to);
    return e ? e->direction : "";
}

bool MetroGraph::hasEdge(int from, int to) const {
    return findEdgeIndex(from, to) != static_cast<size_t>(-1);
}

bool MetroGraph::isLine4(int from, int to) const {
    const Edge* e = getEdge(from, to);
    return e && e->line.find(kLine4) != std::string::npos;
}

std::vector<Edge> MetroGraph::getEdgesOnLine4() const {
    std::vector<Edge> res;
    for (const auto& e : edges) if (e.line.find(kLine4) != std::string::npos) res.push_back(e);
    return res;
}

void MetroGraph::displayGraphInfo() const {
    std::cout << "\nNodes: " << adjacencyList.size() << " Edges: " << edges.size() << std::endl;
}

void MetroGraph::displayAdjacencyList() const {
    if (!stationMgr) return;
    for (const auto& [id, idxs] : adjacencyList) {
        const Station* st = stationMgr->findById(id);
        std::cout << id << "(" << (st ? st->name : "?") << ") -> ";
        for (size_t idx : idxs) {
            const Edge& e = edges[idx];
            const Station* ns = stationMgr->findById(e.toId);
            std::cout << e.toId << "(" << (ns ? ns->name : "?") << ") ";
        }
        std::cout << std::endl;
    }
}

void MetroGraph::displayLine4Info() const {
    std::cout << "Line4 edges: " << getEdgesOnLine4().size() << std::endl;
}
