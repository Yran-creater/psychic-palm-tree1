#include "menu.h"
#include <iostream>
#include <limits>

Menu::Menu(StationManager* stations, MetroGraph* graph, PathFinder* pathfinder)
    : stations_(stations), graph_(graph), pathfinder_(pathfinder) {
}

void Menu::pause() const {
    std::cout << "\n按回车继续...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int Menu::readChoice() const {
    return readInt(PROMPT);
}

int Menu::readInt(const std::string& prompt) const {
    std::cout << prompt;
    int val;
    while (!(std::cin >> val)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "[错误] 请输入有效编号\n" << prompt;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return val;
}

std::string Menu::readLine(const std::string& prompt) const {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

int Menu::selectStation(const std::string& prompt) const {
    std::string keyword = readLine(prompt);
    if (keyword.empty()) return -1;

    auto matches = stations_->findByNameLike(keyword);
    if (matches.empty()) {
        std::cout << "[错误] 未找到匹配站点" << std::endl;
        return -1;
    }

    std::cout << "找到 " << matches.size() << " 个匹配:" << std::endl;
    for (size_t i = 0; i < matches.size(); ++i) {
        std::cout << "  [" << (i + 1) << "] " << matches[i]->name
            << " (" << matches[i]->line << ") ID=" << matches[i]->id
            << " [" << matches[i]->status << "]" << std::endl;
    }

    if (matches.size() == 1) return matches[0]->id;

    int choice = readInt("请选择 (1-" + std::to_string(matches.size()) + ", 0取消): ");
    if (choice <= 0 || choice > (int)matches.size()) return -1;
    return matches[choice - 1]->id;
}

void Menu::handlePathQuery(bool byTime, bool multiple) {
    int fromId = selectStation("请输入起点站名(支持模糊搜索): ");
    if (fromId < 0) return;
    int toId = selectStation("请输入终点站名(支持模糊搜索): ");
    if (toId < 0) return;

    if (fromId == toId) {
        std::cout << "[提示] 起点与终点相同" << std::endl;
        return;
    }

    if (byTime) {
        if (multiple) {
            auto paths = pathfinder_->kShortestTimePaths(fromId, toId, 3);
            pathfinder_->displayPaths(paths);
        }
        else {
            pathfinder_->displayPath(pathfinder_->shortestPath(fromId, toId));
        }
    }
    else {
        if (multiple) {
            auto paths = pathfinder_->kMinTransferPaths(fromId, toId, 3);
            pathfinder_->displayPaths(paths);
        }
        else {
            pathfinder_->displayPath(pathfinder_->minTransferPath(fromId, toId));
        }
    }
}

void Menu::handleManualStatusUpdate() {
    int id = selectStation("请输入站点名(支持模糊搜索): ");
    if (id < 0) return;

    std::cout << "1. 开启  2. 关闭" << std::endl;
    int choice = readInt("请选择新状态: ");
    std::string status = (choice == 2) ? StationStatus::CLOSED : StationStatus::OPEN;
    stations_->updateStatus(id, status);
}

void Menu::handleAffectedAnalysis() {
    int id = selectStation("请输入已关闭站点名(支持模糊搜索): ");
    if (id < 0) return;
    pathfinder_->displayAffectedArea(id);
}

void Menu::handleStationQuery() {
    std::string keyword = readLine("请输入站点关键词: ");
    if (keyword.empty()) return;

    auto matches = stations_->findByNameLike(keyword);
    if (matches.empty()) {
        std::cout << "[提示] 未找到匹配站点" << std::endl;
        return;
    }

    std::cout << "\n查询结果 (共 " << matches.size() << " 条):" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    for (const auto* st : matches) st->display();
}

void Menu::showStatusMenu() {
    while (true) {
        std::cout << "\n-- 线路站点信息/运营状态管理 --" << std::endl;
        std::cout << "1. 从 CSV 文件批量更新站点开启/关闭状态" << std::endl;
        std::cout << "2. 手工更新站点开启/关闭状态" << std::endl;
        std::cout << "3. 显示当前关闭站点" << std::endl;
        std::cout << "4. 恢复所有站点初始状态" << std::endl;
        std::cout << "5. 显示线路站点信息" << std::endl;
        std::cout << "6. 受关闭站点影响站点分析" << std::endl;
        std::cout << "7. 站点查询" << std::endl;
        std::cout << "8. 返回上级菜单" << std::endl;

        switch (readChoice()) {
        case 1:
            stations_->updateStationCsvStatus("data/update_station_status.csv");
            pause();
            break;
        case 2:
            handleManualStatusUpdate();
            pause();
            break;
        case 3:
            stations_->showClosedStations();
            pause();
            break;
        case 4:
            stations_->resetStationStatus();
            pause();
            break;
        case 5: {
            std::string line = readLine("请输入线路名称(如 地铁1号线): ");
            stations_->showInfoByLine(line);
            pause();
            break;
        }
        case 6:
            handleAffectedAnalysis();
            pause();
            break;
        case 7:
            handleStationQuery();
            pause();
            break;
        case 8:
            return;
        default:
            std::cout << "[错误] 无效选项" << std::endl;
        }
    }
}

void Menu::showTimePathMenu() {
    while (true) {
        std::cout << "\n-- 所需时间最短路径规划 --" << std::endl;
        std::cout << "1. 单条所需时间最短路径" << std::endl;
        std::cout << "2. 3条所需时间最短路径" << std::endl;
        std::cout << "3. 返回上级菜单" << std::endl;

        switch (readChoice()) {
        case 1:
            handlePathQuery(true, false);
            pause();
            break;
        case 2:
            handlePathQuery(true, true);
            pause();
            break;
        case 3:
            return;
        default:
            std::cout << "[错误] 无效选项" << std::endl;
        }
    }
}

void Menu::showTransferPathMenu() {
    while (true) {
        std::cout << "\n-- 所需换乘次数最少路径规划 --" << std::endl;
        std::cout << "1. 单条换乘次数最少路径" << std::endl;
        std::cout << "2. 3条换乘次数最少路径" << std::endl;
        std::cout << "3. 返回主菜单" << std::endl;

        switch (readChoice()) {
        case 1:
            handlePathQuery(false, false);
            pause();
            break;
        case 2:
            handlePathQuery(false, true);
            pause();
            break;
        case 3:
            return;
        default:
            std::cout << "[错误] 无效选项" << std::endl;
        }
    }
}

void Menu::showMainMenu() {
    while (true) {
        std::cout << "\n==== 地铁路径规划系统 ====" << std::endl;
        std::cout << "1. 线路站点信息/运营状态管理" << std::endl;
        std::cout << "2. 所需时间最短路径规划" << std::endl;
        std::cout << "3. 所需换乘次数最少路径规划" << std::endl;
        std::cout << "4. 退出系统" << std::endl;

        switch (readChoice()) {
        case 1:
            showStatusMenu();
            break;
        case 2:
            showTimePathMenu();
            break;
        case 3:
            showTransferPathMenu();
            break;
        case 4:
            std::cout << "感谢使用，再见！" << std::endl;
            return;
        default:
            std::cout << "[错误] 无效选项" << std::endl;
        }
    }
}

void Menu::run() {
    showMainMenu();
}
