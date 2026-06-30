#include "station.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <set>

void Station::display() const {
    std::cout << "ID:" << id << " | " << name << " (" << line << ") | 状态:" << status << std::endl;
}

std::string StationManager::trim(const std::string& str) const {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::string StationManager::getCurrentTime() const {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    struct tm lt_buf {};
#ifdef _WIN32
    localtime_s(&lt_buf, &t);
#else
    localtime_r(&t, &lt_buf);
#endif
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &lt_buf);
    return std::string(buf);
}

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

std::vector<Station*> StationManager::findByNameAll(const std::string& name) {
    std::vector<Station*> res;
    for (auto& st : stations) if (st.name == name) res.push_back(&st);
    return res;
}

bool StationManager::isOpen(int id) const {
    const Station* st = findById(id);
    return st && st->isOpen();
}

bool StationManager::isOpenByName(const std::string& name) const {
    for (const auto& st : stations) {
        if (st.name == name && st.isOpen()) return true;
    }
    return false;
}

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
    std::string name = st ? st->name : "未知";
    file << id << "," << name << "," << oldStatus << "," << newStatus << ","
        << getCurrentTime() << "," << operatorName << "\n";
    file.close();
    return true;
}

bool StationManager::updateStatus(int id, const std::string& newStatus,
    const std::string& operatorName, bool persist) {
    Station* st = findById(id);
    if (!st) { std::cerr << "[错误] 未找到站点 ID=" << id << std::endl; return false; }
    if (st->status == newStatus) return true;

    std::string old = st->status;
    st->status = newStatus;
    std::cout << "[信息] " << st->name << "(" << st->line << "): "
        << old << " -> " << newStatus << std::endl;

    if (persist) {
        saveToCSV("data/Station.csv");
        appendUpdateRecord(id, old, newStatus, operatorName);
    }
    return true;
}

bool StationManager::updateStatus(const std::string& name, const std::string& newStatus,
    const std::string& operatorName) {
    auto matches = findByNameAll(name);
    if (matches.empty()) matches = findByNameLike(name);
    if (matches.empty()) { std::cerr << "[错误] 未找到站点" << std::endl; return false; }
    if (matches.size() == 1) return updateStatus(matches[0]->id, newStatus, operatorName);

    std::cout << "[提示] 找到 " << matches.size() << " 个匹配站点:" << std::endl;
    for (size_t i = 0; i < matches.size(); ++i) {
        std::cout << "  [" << i + 1 << "] " << matches[i]->name
            << " (" << matches[i]->line << ") ID=" << matches[i]->id << std::endl;
    }
    std::cout << "请选择 (1-" << matches.size() << ", 0取消): ";
    int choice; std::cin >> choice; std::cin.ignore(10000, '\n');
    if (choice <= 0 || choice > (int)matches.size()) return false;
    return updateStatus(matches[choice - 1]->id, newStatus, operatorName);
}

bool StationManager::updateStationCsvStatus(const std::string& updateFile,
    const std::string& stationFile) {
    std::ifstream file(updateFile);
    if (!file.is_open()) {
        std::cerr << "[错误] 无法打开批量更新文件: " << updateFile << std::endl;
        return false;
    }

    std::string line;
    bool first = true;
    int updated = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (first) { first = false; continue; }

        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(ss, token, ',')) tokens.push_back(trim(token));
        if (tokens.empty()) continue;

        try {
            int id = std::stoi(tokens[0]);
            std::string newStatus;
            if (tokens.size() >= 4) newStatus = tokens[3];
            else if (tokens.size() >= 2) newStatus = tokens[1];
            else continue;

            if (updateStatus(id, newStatus, "batch", false)) updated++;
        }
        catch (...) {}
    }
    file.close();
    saveToCSV(stationFile);
    std::cout << "[信息] 批量更新完成，共更新 " << updated << " 个站点" << std::endl;
    return true;
}

bool StationManager::resetStationStatus(const std::string& initFile,
    const std::string& stationFile) {
    if (!loadFromCSV(initFile)) return false;
    return saveToCSV(stationFile);
}

void StationManager::showClosedStations() const {
    std::cout << "\n当前关闭站点:" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    int count = 0;
    std::set<std::string> shown;
    for (const auto& st : stations) {
        if (st.status != StationStatus::CLOSED) continue;
        std::string key = st.name + "|" + st.line;
        if (shown.count(key)) continue;
        shown.insert(key);
        st.display();
        count++;
    }
    std::cout << "共 " << count << " 个关闭站点" << std::endl;
}

void StationManager::showInfoByLine(const std::string& line) const {
    std::cout << "\n线路 [" << line << "] 站点信息:" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    int count = 0;
    for (const auto& st : stations) {
        if (st.line == line) {
            st.display();
            count++;
        }
    }
    if (count == 0) std::cout << "  未找到该线路站点" << std::endl;
    else std::cout << "共 " << count << " 个站点" << std::endl;
}

bool StationManager::closeTransferStation(const std::string& name,
    const std::string& operatorName) {
    auto matches = findByNameAll(name);
    if (matches.empty()) {
        std::cerr << "[错误] 未找到换乘站: " << name << std::endl;
        return false;
    }
    std::cout << "[信息] 关闭换乘站 [" << name << "] 全部 " << matches.size() << " 个节点" << std::endl;
    for (auto* st : matches) updateStatus(st->id, StationStatus::CLOSED, operatorName, false);
    saveToCSV("data/Station.csv");
    return true;
}

bool StationManager::closeLine(const std::string& line,
    const std::string& operatorName) {
    auto list = findByLine(line);
    if (list.empty()) {
        std::cerr << "[错误] 未找到线路: " << line << std::endl;
        return false;
    }
    std::cout << "[信息] 线路 [" << line << "] 停运，关闭 " << list.size() << " 个站点" << std::endl;
    for (auto* st : list) updateStatus(st->id, StationStatus::CLOSED, operatorName, false);
    saveToCSV("data/Station.csv");
    return true;
}

bool StationManager::closeAllNetwork(const std::string& operatorName) {
    std::cout << "[信息] 全网停运" << std::endl;
    for (auto& st : stations) {
        if (st.status != StationStatus::CLOSED) updateStatus(st.id, StationStatus::CLOSED, operatorName, false);
    }
    saveToCSV("data/Station.csv");
    return true;
}

bool StationManager::openAllNetwork(const std::string& operatorName) {
    std::cout << "[信息] 全网恢复运营" << std::endl;
    for (auto& st : stations) {
        if (st.status != StationStatus::OPEN) updateStatus(st.id, StationStatus::OPEN, operatorName, false);
    }
    saveToCSV("data/Station.csv");
    return true;
}

void StationManager::showUpdateHistory(int id) const {
    std::ifstream file("data/update_station_status.csv");
    if (!file.is_open()) { std::cout << "[信息] 无历史记录" << std::endl; return; }

    std::string line;
    bool first = true, found = false;
    std::cout << "\n站点 " << id << " 修改历史:" << std::endl;
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
                    << tokens[2] << " -> " << tokens[3]
                    << " | 操作人: " << tokens[5] << std::endl;
            }
        }
        catch (...) {}
    }
    file.close();
    if (!found) std::cout << "  无修改记录" << std::endl;
}

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
