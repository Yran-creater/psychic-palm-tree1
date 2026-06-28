#ifndef CRAWLER_H
#define CRAWLER_H

#include <string>
#include <vector>
#include "station.h"

/**
 * 边信息（爬虫用）
 */
struct EdgeInfo {
    int fromId;
    int toId;
    std::string line;
    std::string direction;   // "内环" / "外环" / ""
    double travelTime;
};

class MetroCrawler {
public:
    bool fetchAndGenerateCSV(const std::string& apiUrl,
        const std::string& stationFile,
        const std::string& edgeFile);

private:
    bool parseJSON(const std::string& json,
        std::vector<Station>& stations,
        std::vector<EdgeInfo>& edges);

    bool saveStationsToCSV(const std::vector<Station>& stations,
        const std::string& filename);

    bool saveEdgesToCSV(const std::vector<EdgeInfo>& edges,
        const std::vector<Station>& stations,
        const std::string& filename);

    std::string trim(const std::string& str);
};

#endif