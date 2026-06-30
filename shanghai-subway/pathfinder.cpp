#include "pathfinder.h"
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <iostream>
#include <iomanip>
#include <limits>
#include <algorithm>

PathFinder::PathFinder(const MetroGraph* graph, const StationManager* stations)
    : graph_(graph), stations_(stations) {
}

bool PathFinder::canVisit(int nodeId) const {
    return stations_->isOpen(nodeId);
}

int PathFinder::calcTransfers(const std::vector<int>& nodes) const {
    if (nodes.size() < 2) return 0;
    int transfers = 0;
    std::string prevLine;
    for (size_t i = 0; i + 1 < nodes.size(); ++i) {
        const Edge* e = graph_->getEdge(nodes[i], nodes[i + 1]);
        if (!e) continue;
        if (!prevLine.empty() && prevLine != e->line) transfers++;
        prevLine = e->line;
    }
    return transfers;
}

int PathFinder::calcActualStops(const std::vector<int>& nodes) const {
    std::set<std::string> names;
    for (int id : nodes) {
        const Station* st = stations_->findById(id);
        if (st) names.insert(st->name);
    }
    return static_cast<int>(names.size());
}

std::vector<int> PathFinder::findTransferStations(const std::vector<int>& nodes) const {
    std::vector<int> result;
    if (nodes.size() < 2) return result;

    std::string prevLine;
    for (size_t i = 0; i + 1 < nodes.size(); ++i) {
        const Edge* e = graph_->getEdge(nodes[i], nodes[i + 1]);
        if (!e) continue;
        if (!prevLine.empty() && prevLine != e->line) {
            result.push_back(nodes[i]);
        }
        prevLine = e->line;
    }
    return result;
}

PathResult PathFinder::buildPathResult(const std::vector<int>& nodes) const {
    PathResult res;
    res.nodes = nodes;
    if (nodes.empty()) return res;

    for (size_t i = 0; i + 1 < nodes.size(); ++i) {
        const Edge* e = graph_->getEdge(nodes[i], nodes[i + 1]);
        if (e) res.totalTime += e->travelTime;
    }
    res.transferCount = calcTransfers(nodes);
    res.transferStations = findTransferStations(nodes);
    res.actualStopCount = calcActualStops(nodes);
    return res;
}

PathResult PathFinder::dijkstraByTime(int fromId, int toId,
    const std::vector<std::pair<int, int>>& bannedEdges) const {
    PathResult empty;
    if (!canVisit(fromId) || !canVisit(toId)) return empty;

    auto isBanned = [&](int u, int v) {
        for (const auto& p : bannedEdges) {
            if (p.first == u && p.second == v) return true;
        }
        return false;
    };

    struct PQNode {
        double time;
        int transfers;
        int node;
        std::string line;
        bool operator>(const PQNode& other) const {
            if (time != other.time) return time > other.time;
            return transfers > other.transfers;
        }
    };

    std::priority_queue<PQNode, std::vector<PQNode>, std::greater<PQNode>> pq;
    std::unordered_map<StateKey, std::pair<double, int>, StateKeyHash> best;
    std::unordered_map<StateKey, StateKey, StateKeyHash> parent;
    std::unordered_map<StateKey, int, StateKeyHash> parentNode;

    StateKey start{ fromId, "" };
    pq.push({ 0.0, 0, fromId, "" });
    best[start] = { 0.0, 0 };

    StateKey targetState;
    bool found = false;
    double bestTime = std::numeric_limits<double>::infinity();
    int bestTransfers = std::numeric_limits<int>::max();

    while (!pq.empty()) {
        auto cur = pq.top(); pq.pop();
        StateKey key{ cur.node, cur.line };
        auto it = best.find(key);
        if (it == best.end() || it->second.first < cur.time - 1e-9 ||
            (std::abs(it->second.first - cur.time) < 1e-9 && it->second.second < cur.transfers)) {
            continue;
        }

        if (cur.node == toId) {
            if (cur.time < bestTime - 1e-9 ||
                (std::abs(cur.time - bestTime) < 1e-9 && cur.transfers < bestTransfers)) {
                bestTime = cur.time;
                bestTransfers = cur.transfers;
                targetState = key;
                found = true;
            }
            continue;
        }

        for (size_t eidx : graph_->getOutEdgeIndices(cur.node)) {
            const Edge& e = graph_->getAllEdges()[eidx];
            if (isBanned(e.fromId, e.toId)) continue;
            if (!canVisit(e.toId)) continue;

            int newTransfers = cur.transfers;
            if (!cur.line.empty() && cur.line != e.line) newTransfers++;
            double newTime = cur.time + e.travelTime;

            StateKey next{ e.toId, e.line };
            auto bit = best.find(next);
            if (bit != best.end()) {
                if (bit->second.first < newTime - 1e-9) continue;
                if (std::abs(bit->second.first - newTime) < 1e-9 && bit->second.second <= newTransfers) continue;
            }

            best[next] = { newTime, newTransfers };
            parent[next] = key;
            parentNode[next] = cur.node;
            pq.push({ newTime, newTransfers, e.toId, e.line });
        }
    }

    if (!found) return empty;

    std::vector<int> path;
    StateKey cur = targetState;
    while (true) {
        path.push_back(cur.node);
        if (cur.node == fromId) break;
        auto pit = parent.find(cur);
        if (pit == parent.end()) break;
        cur = pit->second;
    }
    std::reverse(path.begin(), path.end());
    return buildPathResult(path);
}

PathResult PathFinder::dijkstraByTransfer(int fromId, int toId,
    const std::vector<std::pair<int, int>>& bannedEdges) const {
    PathResult empty;
    if (!canVisit(fromId) || !canVisit(toId)) return empty;

    auto isBanned = [&](int u, int v) {
        for (const auto& p : bannedEdges) {
            if (p.first == u && p.second == v) return true;
        }
        return false;
    };

    struct PQNode {
        int transfers;
        double time;
        int node;
        std::string line;
        bool operator>(const PQNode& other) const {
            if (transfers != other.transfers) return transfers > other.transfers;
            return time > other.time;
        }
    };

    std::priority_queue<PQNode, std::vector<PQNode>, std::greater<PQNode>> pq;
    std::unordered_map<StateKey, std::pair<int, double>, StateKeyHash> best;
    std::unordered_map<StateKey, StateKey, StateKeyHash> parent;

    StateKey start{ fromId, "" };
    pq.push({ 0, 0.0, fromId, "" });
    best[start] = { 0, 0.0 };

    StateKey targetState;
    bool found = false;
    int bestTransfers = std::numeric_limits<int>::max();
    double bestTime = std::numeric_limits<double>::infinity();

    while (!pq.empty()) {
        auto cur = pq.top(); pq.pop();
        StateKey key{ cur.node, cur.line };
        auto it = best.find(key);
        if (it == best.end() || it->second.first < cur.transfers ||
            (it->second.first == cur.transfers && it->second.second < cur.time - 1e-9)) {
            continue;
        }

        if (cur.node == toId) {
            if (cur.transfers < bestTransfers ||
                (cur.transfers == bestTransfers && cur.time < bestTime - 1e-9)) {
                bestTransfers = cur.transfers;
                bestTime = cur.time;
                targetState = key;
                found = true;
            }
            continue;
        }

        for (size_t eidx : graph_->getOutEdgeIndices(cur.node)) {
            const Edge& e = graph_->getAllEdges()[eidx];
            if (isBanned(e.fromId, e.toId)) continue;
            if (!canVisit(e.toId)) continue;

            int newTransfers = cur.transfers;
            if (!cur.line.empty() && cur.line != e.line) newTransfers++;
            double newTime = cur.time + e.travelTime;

            StateKey next{ e.toId, e.line };
            auto bit = best.find(next);
            if (bit != best.end()) {
                if (bit->second.first < newTransfers) continue;
                if (bit->second.first == newTransfers && bit->second.second <= newTime - 1e-9) continue;
            }

            best[next] = { newTransfers, newTime };
            parent[next] = key;
            pq.push({ newTransfers, newTime, e.toId, e.line });
        }
    }

    if (!found) return empty;

    std::vector<int> path;
    StateKey cur = targetState;
    while (true) {
        path.push_back(cur.node);
        if (cur.node == fromId) break;
        auto pit = parent.find(cur);
        if (pit == parent.end()) break;
        cur = pit->second;
    }
    std::reverse(path.begin(), path.end());
    return buildPathResult(path);
}

PathResult PathFinder::shortestPath(int fromId, int toId) const {
    return dijkstraByTime(fromId, toId);
}

PathResult PathFinder::minTransferPath(int fromId, int toId) const {
    return dijkstraByTransfer(fromId, toId);
}

std::vector<std::string> PathFinder::pathLineSequence(const std::vector<int>& nodes) const {
    std::vector<std::string> seq;
    for (size_t i = 0; i + 1 < nodes.size(); ++i) {
        const Edge* e = graph_->getEdge(nodes[i], nodes[i + 1]);
        if (e) seq.push_back(e->line);
    }
    return seq;
}

bool PathFinder::isDuplicatePath(const std::vector<PathResult>& paths,
    const std::vector<int>& nodes) const {
    auto seq = pathLineSequence(nodes);
    for (const auto& p : paths) {
        if (p.nodes == nodes) return true;
        if (pathLineSequence(p.nodes) == seq && !seq.empty()) return true;
    }
    return false;
}

std::vector<PathResult> PathFinder::yenKPaths(
    int fromId, int toId, int k, PathMetric metric,
    PathResult(PathFinder::*finder)(int, int,
        const std::vector<std::pair<int, int>>&) const) const {
    std::vector<PathResult> results;
    PathResult first = (this->*finder)(fromId, toId, {});
    if (first.empty()) return results;
    results.push_back(first);

    auto better = [](const PathResult& a, const PathResult& b, PathMetric m) {
        if (m == PathMetric::Time) {
            if (a.totalTime != b.totalTime) return a.totalTime < b.totalTime;
            return a.transferCount < b.transferCount;
        }
        if (a.transferCount != b.transferCount) return a.transferCount < b.transferCount;
        return a.totalTime < b.totalTime;
    };

    for (int ki = 1; ki < k; ++ki) {
        const PathResult& prev = results[ki - 1];
        std::vector<PathResult> candidates;

        for (size_t i = 0; i + 1 < prev.nodes.size(); ++i) {
            int spurNode = prev.nodes[i];
            std::vector<int> rootPath(prev.nodes.begin(), prev.nodes.begin() + static_cast<int>(i) + 1);

            std::vector<std::pair<int, int>> banned;
            for (int j = 0; j < ki - 1; ++j) {
                const auto& p = results[j];
                if (p.nodes.size() > i && std::equal(rootPath.begin(), rootPath.end(), p.nodes.begin())) {
                    if (i + 1 < p.nodes.size()) {
                        banned.push_back({ p.nodes[i], p.nodes[i + 1] });
                    }
                }
            }
            for (size_t j = 0; j + 1 < rootPath.size(); ++j) {
                banned.push_back({ rootPath[j], rootPath[j + 1] });
            }

            PathResult spur = (this->*finder)(spurNode, toId, banned);
            if (spur.empty()) continue;

            std::vector<int> totalPath = rootPath;
            if (spur.nodes.size() > 1) {
                totalPath.insert(totalPath.end(), spur.nodes.begin() + 1, spur.nodes.end());
            }

            PathResult candidate = buildPathResult(totalPath);
            if (!candidate.empty() && !isDuplicatePath(results, candidate.nodes)) {
                bool inCand = false;
                for (const auto& c : candidates) {
                    if (c.nodes == candidate.nodes) { inCand = true; break; }
                }
                if (!inCand) candidates.push_back(candidate);
            }
        }

        if (candidates.empty()) break;

        std::sort(candidates.begin(), candidates.end(),
            [&](const PathResult& a, const PathResult& b) { return better(a, b, metric); });

        results.push_back(candidates.front());
    }

    return results;
}

std::vector<PathResult> PathFinder::kShortestTimePaths(int fromId, int toId, int k) const {
    return yenKPaths(fromId, toId, k, PathMetric::Time, &PathFinder::dijkstraByTime);
}

std::vector<PathResult> PathFinder::kMinTransferPaths(int fromId, int toId, int k) const {
    return yenKPaths(fromId, toId, k, PathMetric::Transfer, &PathFinder::dijkstraByTransfer);
}

void PathFinder::displayPath(const PathResult& path) const {
    if (path.empty()) {
        std::cout << "[提示] 未找到可行路径（站点可能关闭或网络不连通）" << std::endl;
        return;
    }

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  路径详情" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "总时间: " << path.totalTime << " 分钟" << std::endl;
    std::cout << "换乘次数: " << path.transferCount << std::endl;
    std::cout << "实际经过站数(不含重复换乘节点): " << path.actualStopCount << std::endl;

    if (!path.transferStations.empty()) {
        std::cout << "换乘站点: ";
        for (size_t i = 0; i < path.transferStations.size(); ++i) {
            const Station* st = stations_->findById(path.transferStations[i]);
            std::cout << (st ? st->name : "未知");
            if (i + 1 < path.transferStations.size()) std::cout << " -> ";
        }
        std::cout << std::endl;
    }

    std::cout << "\n【路径可视化】" << std::endl;
    for (size_t i = 0; i < path.nodes.size(); ++i) {
        const Station* st = stations_->findById(path.nodes[i]);
        std::cout << "  " << (i + 1) << ". "
            << (st ? st->name : "未知") << " (ID=" << path.nodes[i];
        if (st) std::cout << ", " << st->line;
        std::cout << ")";

        if (i + 1 < path.nodes.size()) {
            const Edge* e = graph_->getEdge(path.nodes[i], path.nodes[i + 1]);
            if (e) {
                std::cout << "\n      --[" << e->line << ", " << e->travelTime << "min]-->";
                if (e->isTransfer()) std::cout << " (换乘)";
            }
        }
        std::cout << std::endl;
    }
}

void PathFinder::displayPaths(const std::vector<PathResult>& paths) const {
    if (paths.empty()) {
        std::cout << "[提示] 未找到可行路径" << std::endl;
        return;
    }
    for (size_t i = 0; i < paths.size(); ++i) {
        std::cout << "\n>>> 方案 " << (i + 1) << " <<<" << std::endl;
        displayPath(paths[i]);
    }
}

std::vector<int> PathFinder::analyzeAffectedArea(int closedStationId) const {
    std::vector<int> affected;
    if (!stations_->findById(closedStationId)) return affected;

    std::unordered_set<int> openNodes;
    for (const auto& st : stations_->getAllStations()) {
        if (st.isOpen()) openNodes.insert(st.id);
    }
    openNodes.erase(closedStationId);

    std::unordered_set<int> visited;
    std::vector<std::vector<int>> components;

    auto bfs = [&](int start) {
        std::vector<int> comp;
        std::queue<int> q;
        q.push(start);
        visited.insert(start);
        while (!q.empty()) {
            int u = q.front(); q.pop();
            comp.push_back(u);
            for (size_t eidx : graph_->getOutEdgeIndices(u)) {
                const Edge& e = graph_->getAllEdges()[eidx];
                if (e.toId == closedStationId || !openNodes.count(e.toId) || visited.count(e.toId)) continue;
                visited.insert(e.toId);
                q.push(e.toId);
            }
            for (int v : graph_->getInNeighbors(u)) {
                if (v == closedStationId || !openNodes.count(v) || visited.count(v)) continue;
                visited.insert(v);
                q.push(v);
            }
        }
        return comp;
    };

    for (int id : openNodes) {
        if (visited.count(id)) continue;
        components.push_back(bfs(id));
    }

    if (components.size() <= 1) return affected;

    size_t largestIdx = 0;
    for (size_t i = 1; i < components.size(); ++i) {
        if (components[i].size() > components[largestIdx].size()) largestIdx = i;
    }

    for (size_t i = 0; i < components.size(); ++i) {
        if (i == largestIdx) continue;
        for (int id : components[i]) affected.push_back(id);
    }
    return affected;
}

void PathFinder::displayAffectedArea(int closedStationId) const {
    const Station* closed = stations_->findById(closedStationId);
    if (!closed) {
        std::cout << "[错误] 站点不存在" << std::endl;
        return;
    }

    auto affected = analyzeAffectedArea(closedStationId);
    std::cout << "\n关闭站点 [" << closed->name << " ID=" << closedStationId
        << "] 受影响区域分析:" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    std::cout << "受影响站点数: " << affected.size() << std::endl;
    if (affected.empty()) {
        std::cout << "  该站点关闭未导致网络断连，或网络仍保持连通" << std::endl;
        return;
    }
    for (int id : affected) {
        const Station* st = stations_->findById(id);
        if (st) st->display();
    }
}

void PathFinder::analyzeConnectivity() const {
    std::unordered_set<int> openNodes;
    for (const auto& st : stations_->getAllStations()) {
        if (st.isOpen()) openNodes.insert(st.id);
    }

    if (openNodes.empty()) {
        std::cout << "[信息] 无开放站点" << std::endl;
        return;
    }

    std::unordered_set<int> visited;
    std::vector<std::vector<int>> components;

    auto bfs = [&](int start) {
        std::vector<int> comp;
        std::queue<int> q;
        q.push(start);
        visited.insert(start);
        while (!q.empty()) {
            int u = q.front(); q.pop();
            comp.push_back(u);
            for (size_t eidx : graph_->getOutEdgeIndices(u)) {
                const Edge& e = graph_->getAllEdges()[eidx];
                if (!openNodes.count(e.toId) || visited.count(e.toId)) continue;
                visited.insert(e.toId);
                q.push(e.toId);
            }
            for (int v : graph_->getInNeighbors(u)) {
                if (!openNodes.count(v) || visited.count(v)) continue;
                visited.insert(v);
                q.push(v);
            }
        }
        return comp;
    };

    for (int id : openNodes) {
        if (visited.count(id)) continue;
        components.push_back(bfs(id));
    }

    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "  网络连通性分析" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "开放站点总数: " << openNodes.size() << std::endl;
    std::cout << "连通分量数: " << components.size() << std::endl;

    int largest = 0;
    for (size_t i = 0; i < components.size(); ++i) {
        largest = std::max(largest, static_cast<int>(components[i].size()));
        std::cout << "  分量 " << (i + 1) << ": " << components[i].size() << " 个站点" << std::endl;
    }
    std::cout << "最大连通分量: " << largest << " 个站点" << std::endl;

    if (components.size() > 1) {
        std::cout << "\n[警告] 网络存在断连，部分开放站点之间不可达" << std::endl;
    }
    else {
        std::cout << "\n[信息] 所有开放站点处于同一连通分量" << std::endl;
    }
}
