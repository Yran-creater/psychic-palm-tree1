#include "crawler.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t total = size * nmemb;
    out->append((char*)contents, total);
    return total;
}

std::string MetroCrawler::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

// ==================== 核心 ====================
bool MetroCrawler::fetchAndGenerateCSV(const std::string& apiUrl,
    const std::string& stationFile,
    const std::string& edgeFile) {
    std::cout << "[信息] 从高德API获取数据..." << std::endl;

    CURL* curl = curl_easy_init();
    if (!curl) { std::cerr << "[错误] CURL初始化失败" << std::endl; return false; }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[错误] 网络请求失败: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    if (response.empty()) { std::cerr << "[错误] 返回数据为空" << std::endl; return false; }

    std::vector<Station> stations;
    std::vector<EdgeInfo> edgeInfos;

    if (!parseJSON(response, stations, edgeInfos)) {
        std::cerr << "[错误] JSON解析失败" << std::endl;
        return false;
    }

    std::cout << "[信息] 解析到 " << stations.size() << " 个站点，"
        << edgeInfos.size() << " 条边" << std::endl;

    if (!saveStationsToCSV(stations, stationFile)) return false;
    if (!saveEdgesToCSV(edgeInfos, stations, edgeFile)) return false;

    std::cout << "[信息] ✅ 数据生成完成！" << std::endl;
    return true;
}

// ==================== JSON解析 ====================
bool MetroCrawler::parseJSON(const std::string& jsonContent,
    std::vector<Station>& stations,
    std::vector<EdgeInfo>& edgeInfos) {
    try {
        json data = json::parse(jsonContent);
        stations.clear();
        edgeInfos.clear();

        if (!data.contains("l") || !data["l"].is_array()) {
            std::cerr << "[错误] JSON缺少 'l' 字段" << std::endl;
            return false;
        }

        int nextId = 1001;
        std::unordered_map<std::string, std::vector<int>> lineStations;

        for (const auto& line : data["l"]) {
            std::string lineName = line.value("kn", "未知线路");
            if (!line.contains("st") || !line["st"].is_array()) continue;

            std::vector<int> ids;
            for (const auto& st : line["st"]) {
                std::string name = st.value("n", "未知站点");
                std::string coord = st.value("sl", "");
                double lon = 0, lat = 0;
                size_t comma = coord.find(',');
                if (comma != std::string::npos) {
                    try {
                        lon = std::stod(coord.substr(0, comma));
                        lat = std::stod(coord.substr(comma + 1));
                    }
                    catch (...) {}
                }
                stations.emplace_back(nextId, name, lineName, "开启");
                ids.push_back(nextId);
                nextId++;
            }
            if (!ids.empty()) lineStations[lineName] = ids;
        }

        // 构建边
        const double DEFAULT_TIME = 1.5;

        for (const auto& [lineName, ids] : lineStations) {
            bool isLine4 = (lineName == "4号线");

            for (size_t i = 0; i + 1 < ids.size(); ++i) {
                int from = ids[i], to = ids[i + 1];

                if (isLine4) {
                    // 四号线：内环 + 外环
                    edgeInfos.push_back({ from, to, lineName, "内环", DEFAULT_TIME });
                    edgeInfos.push_back({ to, from, lineName, "外环", DEFAULT_TIME });
                }
                else {
                    // 普通线路：单向
                    edgeInfos.push_back({ from, to, lineName, "", DEFAULT_TIME });
                }
            }

            // 四号线首尾相连（环线闭合）
            if (isLine4 && ids.size() >= 2) {
                int first = ids.front(), last = ids.back();
                edgeInfos.push_back({ last, first, "4号线", "内环", DEFAULT_TIME });
                edgeInfos.push_back({ first, last, "4号线", "外环", DEFAULT_TIME });
            }
        }

        return true;
    }
    catch (const json::exception& e) {
        std::cerr << "[错误] JSON解析异常: " << e.what() << std::endl;
        return false;
    }
}

// ==================== 保存 ====================
bool MetroCrawler::saveStationsToCSV(const std::vector<Station>& stations,
    const std::string& filename) {
    std::ofstream file(filename, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;
    file << "\xEF\xBB\xBF";
    file << "站点ID,站点名,所属线路,运营状态\n";
    for (const auto& st : stations) {
        file << st.id << "," << st.name << "," << st.line << "," << st.status << "\n";
    }
    file.close();
    return true;
}

bool MetroCrawler::saveEdgesToCSV(const std::vector<EdgeInfo>& edgeInfos,
    const std::vector<Station>& stations,
    const std::string& filename) {
    if (edgeInfos.empty()) { std::cerr << "[警告] 无边数据" << std::endl; return false; }

    std::ofstream file(filename, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;

    file << "\xEF\xBB\xBF";
    file << "起点站ID,终点站ID,所属线路,运行方向,运行时间\n";
    for (const auto& e : edgeInfos) {
        file << e.fromId << "," << e.toId << "," << e.line << ","
            << (e.direction.empty() ? "无" : e.direction) << ","
            << std::fixed << std::setprecision(1) << e.travelTime << "\n";
    }
    file.close();
    return true;
}