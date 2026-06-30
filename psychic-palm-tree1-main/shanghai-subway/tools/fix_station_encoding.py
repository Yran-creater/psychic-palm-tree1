# -*- coding: utf-8 -*-
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

station_h = '''#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

namespace StationStatus {
    inline const char* OPEN = "\\xe5\\xbc\\x80\\xe5\\x90\\xaf";
    inline const char* CLOSED = "\\xe5\\x85\\xb3\\xe9\\x97\\xad";
}

struct Station {
    int id;
    std::string name;
    std::string line;
    std::string status;

    Station() : id(-1), name(""), line(""), status(StationStatus::OPEN) {}
    Station(int _id, const std::string& _name,
        const std::string& _line,
        const std::string& _status = StationStatus::OPEN)
        : id(_id), name(_name), line(_line), status(_status) {
    }

    void display() const;
    bool isOpen() const { return status == StationStatus::OPEN; }
};

class StationManager {
public:
    bool loadFromCSV(const std::string& filename);
    bool saveToCSV(const std::string& filename) const;

    Station* findById(int id);
    const Station* findById(int id) const;
    Station* findByName(const std::string& name);
    std::vector<Station*> findByNameLike(const std::string& keyword);
    std::vector<Station*> findByLine(const std::string& line);
    std::vector<Station*> findByStatus(const std::string& status);
    std::vector<Station*> findByNameAll(const std::string& name);
    const std::vector<Station>& getAllStations() const { return stations; }
    size_t getCount() const { return stations.size(); }

    bool isOpen(int id) const;
    bool isOpenByName(const std::string& name) const;

    bool updateStatus(int id, const std::string& newStatus,
        const std::string& operatorName = "user", bool persist = true);
    bool updateStatus(const std::string& name, const std::string& newStatus,
        const std::string& operatorName = "user");

    bool updateStationCsvStatus(const std::string& updateFile,
        const std::string& stationFile = "data/Station.csv");
    bool resetStationStatus(const std::string& initFile = "data/Station_init.csv",
        const std::string& stationFile = "data/Station.csv");
    void showClosedStations() const;
    void showInfoByLine(const std::string& line) const;

    bool closeTransferStation(const std::string& name,
        const std::string& operatorName = "user");
    bool closeLine(const std::string& line,
        const std::string& operatorName = "user");
    bool closeAllNetwork(const std::string& operatorName = "user");
    bool openAllNetwork(const std::string& operatorName = "user");

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
'''

(ROOT / 'station.h').write_text(station_h, encoding='utf-8-sig')
print('fixed station.h')
