#include "graph.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

// ==================== 构造 ====================
MetroGraph::MetroGraph(const StationManager* mgr) : stationMgr(mgr) {}
void MetroGraph::setStationManager(const StationManager* mgr) { stationMgr = mgr; }

// ==================== 辅助 ====================
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

// ==================== 加载 ====================
bool MetroGraph::loadEdgesFromCSV(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[错误] 无法打开: " << filename << std::endl;
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

        if (tokens.size() < 5) {
            std::cerr << "[警告] 格式错误: " << line << std::endl;
            continue;
        }

        try {
            Edge e;
            e.fromId = std::stoi(tokens[0]);
            e.toId = std::stoi(tokens[1]);
            e.line = tokens[2];
            e.direction = (tokens[3] == "无") ? "" : tokens[3];
            e.travelTime = std::stod(tokens[4]);

            size_t idx = edges.size();
            edges.push_back(e);
            adjacencyList[e.fromId].push_back(idx);  // 有向：只存出边
            loaded++;
        }
        catch (...) {
            std::cerr << "[警告] 解析失败: " << line << std::endl;
        }
    }
    file.close();
    std::cout << "[信息] 加载 " << loaded << " 条有向边" << std::endl;
    return true;
}

// ==================== 保存 ====================
bool MetroGraph::saveEdgesToCSV(const std::string& filename) const {
    std::ofstream file(filename, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;

    file << "\xEF\xBB\xBF";
    file << "起点站ID,终点站ID,所属线路,运行方向,运行时间\n";
    for (const auto& e : edges) {
        file << e.fromId << "," << e.toId << "," << e.line << ","
            << (e.direction.empty() ? "无" : e.direction) << ","
            << std::fixed << std::setprecision(1) << e.travelTime << "\n";
    }
    file.close();
    std::cout << "[信息] 保存 " << edges.size() << " 条边" << std::endl;
    return true;
}

// ==================== 操作 ====================
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

    // 重建索引
    adjacencyList.clear();
    for (size_t i = 0; i < edges.size(); ++i) {
        adjacencyList[edges[i].fromId].push_back(i);
    }
    return true;
}

// ==================== 查询 ====================
std::vector<int> MetroGraph::getOutNeighbors(int stationId) const {
    std::vector<int> res;
    auto it = adjacencyList.find(stationId);
    if (it == adjacencyList.end()) return res;
    for (size_t idx : it->second) res.push_back(edges[idx].toId);
    return res;
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
    for (const auto& e : edges) {
        if (e.toId == stationId) res.push_back(e.fromId);
    }
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

// ==================== 四号线 ====================
bool MetroGraph::isLine4(int from, int to) const {
    const Edge* e = getEdge(from, to);
    return e && (e->line.find("4号线") != std::string::npos);
}

std::vector<Edge> MetroGraph::getEdgesOnLine4() const {
    std::vector<Edge> res;
    for (const auto& e : edges) {
        if (e.line.find("4号线") != std::string::npos) res.push_back(e);
    }
    return res;
}

// ==================== 显示 ====================
void MetroGraph::displayGraphInfo() const {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "  有向图信息" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "  节点数: " << adjacencyList.size() << std::endl;
    std::cout << "  边数: " << edges.size() << std::endl;

    auto l4 = getEdgesOnLine4();
    if (!l4.empty()) {
        int inner = 0, outer = 0;
        for (const auto& e : l4) {
            if (e.direction == "内环") inner++;
            else if (e.direction == "外环") outer++;
        }
        std::cout << "  四号线: " << l4.size() << " 条边" << std::endl;
        std::cout << "    - 内环: " << inner << " 条" << std::endl;
        std::cout << "    - 外环: " << outer << " 条" << std::endl;
    }

    if (!adjacencyList.empty()) {
        int total = 0, maxDeg = 0, minDeg = 1000000;
        for (const auto& [id, list] : adjacencyList) {
            int d = list.size();
            total += d;
            if (d > maxDeg) maxDeg = d;
            if (d < minDeg) minDeg = d;
        }
        std::cout << "  平均出度: " << (double)total / adjacencyList.size() << std::endl;
        std::cout << "  最大出度: " << maxDeg << std::endl;
        std::cout << "  最小出度: " << minDeg << std::endl;
    }
}
void MetroGraph::displayAdjacencyList() const {
    if (!stationMgr) { std::cout << "[警告] 未设置站点管理器" << std::endl; return; }

    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "  有向邻接表" << std::endl;
    std::cout << std::string(70, '=') << std::endl;

    for (const auto& [id, idxs] : adjacencyList) {
        const Station* st = stationMgr->findById(id);
        std::string name = st ? st->name : "未知";
        std::cout << id << "(" << name << ") → ";

        for (size_t idx : idxs) {
            const Edge& e = edges[idx];
            const Station* ns = stationMgr->findById(e.toId);
            std::cout << e.toId << "(" << (ns ? ns->name : "未知") << ")";
            if (!e.line.empty()) std::cout << "[" << e.line << "]";
            if (!e.direction.empty()) std::cout << "<" << e.direction << ">";
            std::cout << " " << e.travelTime << "min ";
        }
        std::cout << std::endl;
    }
}

void MetroGraph::displayLine4Info() const {
    auto l4 = getEdgesOnLine4();
    if (l4.empty()) { std::cout << "[信息] 无四号线数据" << std::endl; return; }

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  四号线（环线）" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    if (!stationMgr) { std::cout << "[警告] 未设置站点管理器" << std::endl; return; }

    std::cout << "\n【内环方向】" << std::endl;
    for (const auto& e : l4) {
        if (e.direction == "内环") {
            const Station* f = stationMgr->findById(e.fromId);
            const Station* t = stationMgr->findById(e.toId);
            std::cout << "  " << (f ? f->name : "未知") << " → "
                << (t ? t->name : "未知") << " (" << e.travelTime << "min)" << std::endl;
        }
    }

    std::cout << "\n【外环方向】" << std::endl;
    for (const auto& e : l4) {
        if (e.direction == "外环") {
            const Station* f = stationMgr->findById(e.fromId);
            const Station* t = stationMgr->findById(e.toId);
            std::cout << "  " << (f ? f->name : "未知") << " → "
                << (t ? t->name : "未知") << " (" << e.travelTime << "min)" << std::endl;
        }
    }
}
