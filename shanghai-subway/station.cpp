#include "station.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>

// ==================== 辅助 ====================
std::string StationManager::trim(const std::string& str) const {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::string StationManager::getCurrentTime() const {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    struct tm* lt = localtime(&t);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt);
    return std::string(buf);
}

// ==================== 加载 ====================
bool StationManager::loadFromCSV(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[错误] 无法打开: " << filename << std::endl;
        return false;
    }

    stations.clear();
    idToIndex.clear();

    std::string line;
    bool first = true;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (first) { first = false; continue; }

        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(ss, token, ',')) tokens.push_back(trim(token));

        if (tokens.size() < 4) continue;

        try {
            Station st;
            st.id = std::stoi(tokens[0]);
            st.name = tokens[1];
            st.line = tokens[2];
            st.status = tokens[3];
            idToIndex[st.id] = stations.size();
            stations.push_back(st);
        }
        catch (...) {}
    }
    file.close();
    std::cout << "[信息] 加载 " << stations.size() << " 个站点" << std::endl;
    return true;
}

// ==================== 保存 ====================
bool StationManager::saveToCSV(const std::string& filename) const {
    std::ofstream file(filename, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;
    file << "\xEF\xBB\xBF";
    file << "站点ID,站点名,所属线路,运营状态\n";
    for (const auto& st : stations) {
        file << st.id << "," << st.name << "," << st.line << "," << st.status << "\n";
    }
    file.close();
    std::cout << "[信息] 保存 " << stations.size() << " 个站点" << std::endl;
    return true;
}

// ==================== 查询 ====================
Station* StationManager::findById(int id) {
    auto it = idToIndex.find(id);
    return (it != idToIndex.end()) ? &stations[it->second] : nullptr;
}

const Station* StationManager::findById(int id) const {
    auto it = idToIndex.find(id);
    return (it != idToIndex.end()) ? &stations[it->second] : nullptr;
}

Station* StationManager::findByName(const std::string& name) {
    for (auto& st : stations) if (st.name == name) return &st;
    return nullptr;
}

std::vector<Station*> StationManager::findByNameLike(const std::string& keyword) {
    std::vector<Station*> res;
    for (auto& st : stations) {
        if (st.name.find(keyword) != std::string::npos) res.push_back(&st);
    }
    return res;
}

std::vector<Station*> StationManager::findByLine(const std::string& line) {
    std::vector<Station*> res;
    for (auto& st : stations) if (st.line == line) res.push_back(&st);
    return res;
}

std::vector<Station*> StationManager::findByStatus(const std::string& status) {
    std::vector<Station*> res;
    for (auto& st : stations) if (st.status == status) res.push_back(&st);
    return res;
}

// ==================== 更新状态 ====================
void StationManager::ensureHistoryFile() const {
    std::string fname = "data/update_station_status.csv";
    std::ifstream f(fname);
    if (!f.is_open()) {
        std::ofstream of(fname, std::ios::out | std::ios::trunc);
        of << "\xEF\xBB\xBF";
        of << "站点ID,站点名,原状态,新状态,修改时间,操作人\n";
        of.close();
    }
    else {
        f.seekg(0, std::ios::end);
        if (f.tellg() == 0) {
            f.close();
            std::ofstream of(fname, std::ios::out | std::ios::trunc);
            of << "\xEF\xBB\xBF";
            of << "站点ID,站点名,原状态,新状态,修改时间,操作人\n";
            of.close();
        }
        else {
            f.close();
        }
    }
}

bool StationManager::appendUpdateRecord(int id, const std::string& oldStatus,
    const std::string& newStatus,
    const std::string& operatorName) const {
    ensureHistoryFile();
    std::ofstream file("data/update_station_status.csv", std::ios::out | std::ios::app);
    if (!file.is_open()) return false;

    const Station* st = findById(id);
    std::string name = (st) ? st->name : "未知";
    file << id << "," << name << "," << oldStatus << "," << newStatus << ","
        << getCurrentTime() << "," << operatorName << "\n";
    file.close();
    return true;
}

bool StationManager::updateStatus(int id, const std::string& newStatus,
    const std::string& operatorName) {
    Station* st = findById(id);
    if (!st) { std::cerr << "[错误] 未找到站点" << std::endl; return false; }
    if (st->status == newStatus) { std::cout << "[信息] 状态未变化" << std::endl; return true; }

    std::string old = st->status;
    st->status = newStatus;
    std::cout << "[信息] " << st->name << ": " << old << " → " << newStatus << std::endl;

    saveToCSV("data/Station.csv");
    appendUpdateRecord(id, old, newStatus, operatorName);
    return true;
}

bool StationManager::updateStatus(const std::string& name, const std::string& newStatus,
    const std::string& operatorName) {
    Station* st = findByName(name);
    if (st) return updateStatus(st->id, newStatus, operatorName);

    auto res = findByNameLike(name);
    if (res.empty()) { std::cerr << "[错误] 未找到" << std::endl; return false; }
    if (res.size() == 1) return updateStatus(res[0]->id, newStatus, operatorName);

    std::cout << "[提示] 找到 " << res.size() << " 个:" << std::endl;
    for (size_t i = 0; i < res.size(); ++i) {
        std::cout << "  [" << i + 1 << "] " << res[i]->name << " (" << res[i]->line << ")" << std::endl;
    }
    std::cout << "请选择 (1-" << res.size() << ", 0取消): ";
    int choice; std::cin >> choice; std::cin.ignore();
    if (choice <= 0 || choice > (int)res.size()) return false;
    return updateStatus(res[choice - 1]->id, newStatus, operatorName);
}

// ==================== 历史 ====================
void StationManager::showUpdateHistory(int id) const {
    std::ifstream file("data/update_station_status.csv");
    if (!file.is_open()) { std::cout << "[信息] 无历史记录" << std::endl; return; }

    std::string line;
    bool first = true, found = false;
    std::cout << "\n📝 站点 " << id << " 修改历史:" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    while (std::getline(file, line)) {
        if (first) { first = false; continue; }
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(ss, token, ',')) tokens.push_back(trim(token));
        if (tokens.size() < 6) continue;
        try {
            if (std::stoi(tokens[0]) == id) {
                found = true;
                std::cout << "  " << tokens[4] << " | "
                    << tokens[2] << " → " << tokens[3]
                    << " | 操作人: " << tokens[5] << std::endl;
            }
        }
        catch (...) {}
    }
    file.close();
    if (!found) std::cout << "  无修改记录" << std::endl;
}

// ==================== 统计/显示 ====================
void StationManager::showStatistics() const {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "  站点统计" << std::endl;
    std::cout << std::string(50, '=') << std::endl;

    std::unordered_map<std::string, int> lineCnt, statusCnt;
    for (const auto& st : stations) {
        lineCnt[st.line]++;
        statusCnt[st.status]++;
    }

    std::cout << "\n按线路:" << std::endl;
    for (const auto& [k, v] : lineCnt) std::cout << "  " << k << ": " << v << std::endl;
    std::cout << "\n按状态:" << std::endl;
    for (const auto& [k, v] : statusCnt) std::cout << "  " << k << ": " << v << std::endl;
    std::cout << std::string(30, '-') << std::endl;
    std::cout << "  总计: " << stations.size() << " 个站点" << std::endl;
}

void StationManager::displayAll() const {
    std::cout << "\n全部站点 (共 " << stations.size() << " 个):" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    for (const auto& st : stations) st.display();
}