# 上海地铁 Web 演示

与 C++ 控制台程序 **完全分离** 的前端演示层，不修改 `shanghai-subway/` 下的任何源码。

## 目录结构

```
web/
├── backend/          # Python FastAPI 后端（独立进程）
│   ├── main.py       # HTTP API + 静态文件托管
│   ├── metro_service.py  # 读取 CSV、路径规划
│   └── requirements.txt
├── frontend/         # 纯静态页面（HTML/CSS/JS）
│   ├── index.html
│   ├── css/style.css
│   └── js/app.js
└── start.bat         # 一键启动（Windows）
```

## 架构说明

| 层 | 位置 | 说明 |
|---|---|---|
| C++ 控制台 | `shanghai-subway/` | 课程设计主程序，菜单交互 |
| Web 后端 | `web/backend/` | 读取同一份 `data/*.csv`，提供 REST API |
| Web 前端 | `web/frontend/` | 浏览器演示界面 |

两端共享 **同一份 CSV 数据**，但进程与代码互不耦合。Web 端的站点状态修改仅保存在内存中，不会写回 CSV（避免与 C++ 程序冲突）。

## 快速启动

### 方式一：双击脚本

```
web/start.bat
```

### 方式二：命令行

**注意**：`web` 目录在 `psychic-palm-tree1` 下，不在 `shanghai_Subway` 根目录。

```bash
cd psychic-palm-tree1/web/backend
pip install -r requirements.txt
python -m uvicorn main:app --host 127.0.0.1 --port 8000 --reload
```

浏览器打开：**http://127.0.0.1:8000**

## 功能演示

- **路径规划**：时间最短 / 换乘最少 / K 条最优路径
- **站点管理**：搜索、按线路筛选、开启/关闭站点（内存）
- **网络分析**：连通性检查

## API 一览

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/stats` | 站点/边/线路统计 |
| GET | `/api/stations/search?q=` | 模糊搜索站点 |
| POST | `/api/path` | 路径规划 `{fromId, toId, mode}` |
| PATCH | `/api/stations/{id}/status` | 修改站点状态 |
| GET | `/api/connectivity` | 网络连通性 |
| POST | `/api/stations/reload` | 重新加载 CSV |

## 依赖

- Python 3.9+
- `fastapi`、`uvicorn`

## 注意事项

1. 需确保 `shanghai-subway/data/Station.csv` 与 `Edge.csv` 已存在
2. Web 演示与 C++ 程序可同时运行，但站点状态互不影响（Web 仅内存）
3. 若需 Web 端持久化状态，可后续扩展写回 CSV 或对接 C++ 可执行文件
