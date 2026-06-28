#ifndef STATION_H
#define STATION_H

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

/**
 * 站点结构体
 */
struct Station {
    int id;
    std::string name;
    std::string line;
    std::string status;     // "开启" 或 "关闭"

    Station() : id(-1), name(""), line(""), status("开启") {}
    Station(int _id, const std::string& _name,
        const std::string& _line,
        const std::string& _status = "开启")
        : id(_id), name(_name), line(_line), status(_status) {
    }

    void display() const {
        std::cout << "ID:" << id << " | " << name << " (" << line << ") | 状态:" << status << std::endl;
    }
};

/**
 * 站点管理器
 */
class StationManager {
public:
    // 加载/保存
    bool loadFromCSV(const std::string& filename);
    bool saveToCSV(const std::string& filename) const;

    // 查询
    Station* findById(int id);
    const Station* findById(int id) const;
    Station* findByName(const std::string& name);
    std::vector<Station*> findByNameLike(const std::string& keyword);
    std::vector<Station*> findByLine(const std::string& line);
    std::vector<Station*> findByStatus(const std::string& status);
    const std::vector<Station>& getAllStations() const { return stations; }
    size_t getCount() const { return stations.size(); }

    // 状态更新
    bool updateStatus(int id, const std::string& newStatus,
        const std::string& operatorName = "user");
    bool updateStatus(const std::string& name, const std::string& newStatus,
        const std::string& operatorName = "user");

    // 历史
    void showUpdateHistory(int id) const;
    void showStatistics() const;
    void displayAll() const;

private:
    std::vector<Station> stations;
    std::unordered_map<int, size_t> idToIndex;

    std::string trim(const std::string& str) const;
    std::string getCurrentTime() const;
    bool appendUpdateRecord(int id, const std::string& oldStatus,
        const std::string& newStatus,
        const std::string& operatorName) const;
    void ensureHistoryFile() const;
};

#endif#pragma once
