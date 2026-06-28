# 上海地铁信息管理系统

基于 C++ 的控制台桌面应用，用于管理上海地铁站点数据、构建线路有向图，并支持从高德地图 API 抓取线路信息。项目采用模块化设计，适合作为数据结构（图论）与文件 I/O 的课程或练习项目。

## 项目概述

本系统将上海地铁网络抽象为**有向图**：

- **节点**：地铁站点（含 ID、名称、所属线路、运营状态）
- **边**：相邻站点之间的运行关系（含线路名、运行方向、预计运行时间）

特别处理了 **4 号线环线**：内环与外环方向分别建模为双向有向边，并闭合首尾站点。

## 目录结构

```
psychic-palm-tree1/
├── shanghai-subway.slnx          # Visual Studio 解决方案
├── shanghai-subway/
│   ├── main.cpp                  # 程序入口（待完善）
│   ├── station.h / station.cpp   # 站点与站点管理器
│   ├── graph.h / graph.cpp       # 地铁有向图
│   ├── crawler.h / crawler.cpp   # 高德 API 数据爬虫
│   ├── pathfinder.h / pathfinder.cpp  # 路径规划（待实现）
│   ├── menu.h / menu.cpp         # 交互菜单（待实现）
│   └── shanghai-subway.vcxproj   # VS 工程文件
├── LICENSE                       # Apache License 2.0
└── README.md                     # 本说明文档
```

运行时数据文件默认放在 `data/` 目录（程序启动时会尝试创建）：

| 文件 | 说明 |
|------|------|
| `data/Station.csv` | 站点主数据 |
| 边数据 CSV | 站点间有向边数据（由爬虫或手动维护） |
| `data/update_station_status.csv` | 站点状态变更历史 |

## 技术栈

| 项目 | 说明 |
|------|------|
| 语言 | C++20 |
| 构建工具 | Visual Studio 2022+（工具集 v145） |
| 平台 | Windows x64 / x86 控制台程序 |
| 第三方库 | libcurl（HTTP 请求）、nlohmann/json（JSON 解析） |

> **注意**：当前 `.vcxproj` 尚未配置 libcurl 与 nlohmann/json 的包含路径和链接库，需自行安装并配置后才能编译 `crawler.cpp`。

## 核心模块

### 1. 站点管理（`Station` / `StationManager`）

`Station` 结构体字段：

- `id`：站点唯一编号
- `name`：站点名称
- `line`：所属线路（如「1号线」「4号线」）
- `status`：运营状态（「开启」或「关闭」）

`StationManager` 主要功能：

- 从 CSV 加载 / 保存站点数据
- 按 ID、名称、模糊名称、线路、状态查询
- 更新站点运营状态，并自动写入历史记录
- 展示统计信息与全部站点列表

### 2. 地铁有向图（`MetroGraph`）

`Edge` 结构体表示一条有向边：

- `fromId` / `toId`：起点与终点站点 ID
- `line`：所属线路
- `direction`：运行方向（「内环」「外环」，普通线路为空）
- `travelTime`：运行时间（分钟）

`MetroGraph` 使用**邻接表**存储出边，支持：

- CSV 加载 / 保存边数据
- 增删边、查询邻接站点
- 获取两站之间的线路、方向、运行时间
- 4 号线专用查询与展示（内环 / 外环统计）

### 3. 数据爬虫（`MetroCrawler`）

通过高德地图地铁 API 拉取 JSON 数据，解析后生成 CSV：

1. 使用 libcurl 发起 HTTP 请求
2. 解析 JSON 中 `l`（线路）与 `st`（站点）字段
3. 为每条线路相邻站点生成边；4 号线额外生成双向环线和首尾闭合边
4. 输出站点 CSV 与边 CSV

默认每条边运行时间为 **1.5 分钟**（可在 `crawler.cpp` 中修改 `DEFAULT_TIME`）。

使用示例：

```cpp
#include "crawler.h"

MetroCrawler crawler;
crawler.fetchAndGenerateCSV(
    "https://your-amap-api-url",   // 高德地铁线路 JSON 接口
    "data/Station.csv",
    "data/Edge.csv"
);
```

### 4. 路径规划（`Pathfinder`）— 待实现

`pathfinder.h` 与 `pathfinder.cpp` 目前为空占位，计划在 `MetroGraph` 基础上实现最短路径查询（如 Dijkstra 算法），并考虑关闭站点的绕行逻辑。

### 5. 交互菜单（`Menu`）— 待实现

`menu.cpp` 尚未实现，预期提供控制台菜单，整合站点查询、图信息展示、路径规划、数据爬取等功能。

## 数据格式

所有 CSV 文件使用 **UTF-8 BOM** 编码，便于 Excel 正确显示中文。

### Station.csv

```csv
站点ID,站点名,所属线路,运营状态
1001,人民广场,1号线,开启
1002,南京东路,2号线,开启
```

- 首行为表头，加载时会跳过
- 站点 ID 从 1001 起递增（由爬虫生成）

### 边数据 CSV

```csv
起点站ID,终点站ID,所属线路,运行方向,运行时间
1001,1002,1号线,无,1.5
1003,1004,4号线,内环,1.5
1004,1003,4号线,外环,1.5
```

- 「运行方向」为「无」表示普通单向线路
- 4 号线使用「内环」「外环」区分方向

### update_station_status.csv

```csv
站点ID,站点名,原状态,新状态,修改时间,操作人
1001,人民广场,开启,关闭,2026-06-28 10:00:00,user
```

每次调用 `StationManager::updateStatus` 时会追加一条记录。

## 构建与运行

### 环境要求

- Windows 10/11
- Visual Studio 2022 或更新版本（含「使用 C++ 的桌面开发」工作负载）
- libcurl 开发库
- nlohmann/json 头文件库

### 构建步骤

1. 用 Visual Studio 打开 `shanghai-subway.slnx`
2. 在工程属性中配置 libcurl 的**附加包含目录**与**附加库目录**，并链接 `libcurl.lib`
3. 将 nlohmann/json 头文件路径加入**附加包含目录**
4. 选择 `Debug | x64` 或 `Release | x64`，生成解决方案

推荐使用 [vcpkg](https://vcpkg.io/) 安装依赖：

```powershell
vcpkg install curl nlohmann-json
vcpkg integrate install
```

然后在 VS 工程属性中引用 vcpkg 的 include/lib 路径。

### 运行

将工作目录设为 `shanghai-subway/`（即包含 `data/` 的目录），然后运行生成的可执行文件。

若控制台中文输出乱码，可在 `main()` 开头添加：

```cpp
#ifdef _WIN32
#include <windows.h>
SetConsoleOutputCP(65001);
SetConsoleCP(65001);
#endif
```

或在运行前执行 `chcp 65001`。

### 已知构建问题

| 问题 | 说明 |
|------|------|
| `main.cpp` 为空 | 入口函数尚未编写，程序无法正常使用 |
| 第三方库未配置 | vcxproj 中缺少 curl / json 的路径与链接设置 |
| `pathfinder.cpp` / `menu.cpp` 为空 | 路径规划与交互界面待实现 |
| 源文件编码 | 建议统一为 UTF-8 with BOM，并在编译选项中添加 `/utf-8` |

## 架构示意

```
┌─────────────┐     HTTP      ┌──────────────┐
│ MetroCrawler│ ────────────? │ 高德地铁 API │
└──────┬──────┘               └──────────────┘
       │ 生成 CSV
       ▼
┌─────────────┐    加载     ┌─────────────┐
│ Station.csv │ ──────────? │StationManager│
│  Edge.csv   │ ──────────? │ MetroGraph  │
└─────────────┘             └──────┬──────┘
                                   │
                            ┌──────▼──────┐
                            │  Pathfinder │  （待实现）
                            └──────┬──────┘
                                   │
                            ┌──────▼──────┐
                            │    Menu     │  （待实现）
                            └─────────────┘
```

## 设计说明

### 4 号线环线建模

普通线路只建立相邻站点的单向边（A → B）。4 号线作为环线，每条相邻区间同时建立：

- 内环方向边（正向）
- 外环方向边（反向）

并在首尾站之间闭合，形成完整环线。

### 换乘站说明

爬虫按「线路 + 站序」为每条线路独立分配站点 ID，同名换乘站在不同线路上可能是不同 ID。后续路径规划模块需额外处理换乘逻辑（如同名站合并或换乘惩罚）。

### 关闭站点

`StationManager` 支持将站点状态设为「关闭」。路径规划实现后，应跳过关闭站点的边或增加绕行策略。

## 开发进度

| 模块 | 状态 |
|------|------|
| 站点管理（StationManager） | ? 已完成 |
| 有向图（MetroGraph） | ? 已完成 |
| 数据爬虫（MetroCrawler） | ? 已完成（需配置依赖） |
| 路径规划（Pathfinder） | ? 待实现 |
| 交互菜单（Menu） | ? 待实现 |
| 程序入口（main） | ? 待实现 |

## 许可证

本项目采用 [Apache License 2.0](LICENSE) 开源。

## 参考

- 数据来源：[高德地图](https://www.amap.com/) 地铁 API
- 仓库：[github.com/Yran-creater/psychic-palm-tree1](https://github.com/Yran-creater/psychic-palm-tree1)
