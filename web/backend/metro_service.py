"""地铁数据与路径规划服务（独立模块，读取 shanghai-subway/data 下的 CSV）。"""

from __future__ import annotations

import csv
import heapq
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

OPEN = "开启"
CLOSED = "关闭"

# web/backend -> psychic-palm-tree1/shanghai-subway/data
DATA_DIR = Path(__file__).resolve().parent.parent.parent / "shanghai-subway" / "data"


@dataclass
class Station:
    id: int
    name: str
    line: str
    status: str = OPEN

    @property
    def is_open(self) -> bool:
        return self.status == OPEN

    def to_dict(self) -> dict:
        return {
            "id": self.id,
            "name": self.name,
            "line": self.line,
            "status": self.status,
            "isOpen": self.is_open,
        }


@dataclass
class Edge:
    from_id: int
    to_id: int
    line: str
    direction: str
    travel_time: float

    @property
    def is_transfer(self) -> bool:
        return "换乘" in self.line


@dataclass
class PathResult:
    nodes: List[int] = field(default_factory=list)
    total_time: float = 0.0
    transfer_count: int = 0
    transfer_stations: List[int] = field(default_factory=list)
    actual_stop_count: int = 0

    @property
    def empty(self) -> bool:
        return not self.nodes

    def to_dict(self, stations: Dict[int, Station], edge_map: Dict[Tuple[int, int], Edge]) -> dict:
        steps = []
        for i, nid in enumerate(self.nodes):
            st = stations.get(nid)
            step = {
                "index": i + 1,
                "id": nid,
                "name": st.name if st else "未知",
                "line": st.line if st else "",
            }
            if i + 1 < len(self.nodes):
                e = edge_map.get((nid, self.nodes[i + 1]))
                if e:
                    step["segment"] = {
                        "line": e.line,
                        "travelTime": e.travel_time,
                        "isTransfer": e.is_transfer,
                    }
            steps.append(step)

        transfer_names = []
        for tid in self.transfer_stations:
            st = stations.get(tid)
            transfer_names.append(st.name if st else "未知")

        return {
            "totalTime": self.total_time,
            "transferCount": self.transfer_count,
            "actualStopCount": self.actual_stop_count,
            "transferStations": transfer_names,
            "nodes": self.nodes,
            "steps": steps,
        }


class MetroService:
    def __init__(self, data_dir: Path = DATA_DIR) -> None:
        self.data_dir = data_dir
        self.stations: Dict[int, Station] = {}
        self.station_list: List[Station] = []
        self.edges: List[Edge] = []
        self.adj: Dict[int, List[int]] = {}
        self.edge_map: Dict[Tuple[int, int], Edge] = {}
        self.reload()

    def reload(self) -> None:
        self._load_stations(self.data_dir / "Station.csv")
        self._load_edges(self.data_dir / "Edge.csv")

    def _load_stations(self, path: Path) -> None:
        self.stations.clear()
        self.station_list.clear()
        with path.open(encoding="utf-8-sig", newline="") as f:
            reader = csv.DictReader(f)
            for row in reader:
                st = Station(
                    id=int(row["站点ID"]),
                    name=row["站点名"].strip(),
                    line=row["所属线路"].strip(),
                    status=row["运营状态"].strip(),
                )
                self.stations[st.id] = st
                self.station_list.append(st)

    def _load_edges(self, path: Path) -> None:
        self.edges.clear()
        self.adj.clear()
        self.edge_map.clear()
        with path.open(encoding="utf-8-sig", newline="") as f:
            reader = csv.DictReader(f)
            for row in reader:
                e = Edge(
                    from_id=int(row["起点站ID"]),
                    to_id=int(row["终点站ID"]),
                    line=row["所属线路"].strip(),
                    direction=row.get("运行方向", "").strip(),
                    travel_time=float(row["运行时间"]),
                )
                idx = len(self.edges)
                self.edges.append(e)
                self.adj.setdefault(e.from_id, []).append(idx)
                self.edge_map[(e.from_id, e.to_id)] = e

    def stats(self) -> dict:
        lines = sorted({s.line for s in self.station_list})
        open_count = sum(1 for s in self.station_list if s.is_open)
        return {
            "stationCount": len(self.station_list),
            "openStationCount": open_count,
            "edgeCount": len(self.edges),
            "lineCount": len(lines),
            "lines": lines,
        }

    def search_stations(self, keyword: str, limit: int = 20) -> List[dict]:
        kw = keyword.strip()
        if not kw:
            return []
        results = []
        for st in self.station_list:
            if kw in st.name or kw in st.line:
                results.append(st.to_dict())
                if len(results) >= limit:
                    break
        return results

    def stations_by_line(self, line: str) -> List[dict]:
        return [s.to_dict() for s in self.station_list if s.line == line]

    def set_status(self, station_id: int, status: str) -> Optional[dict]:
        if status not in (OPEN, CLOSED):
            raise ValueError("status 必须为「开启」或「关闭」")
        st = self.stations.get(station_id)
        if not st:
            return None
        st.status = status
        return st.to_dict()

    def _can_visit(self, node_id: int) -> bool:
        st = self.stations.get(node_id)
        return st is not None and st.is_open

    def _build_path_result(self, nodes: List[int]) -> PathResult:
        res = PathResult(nodes=nodes)
        if not nodes:
            return res
        for i in range(len(nodes) - 1):
            e = self.edge_map.get((nodes[i], nodes[i + 1]))
            if e:
                res.total_time += e.travel_time
        res.transfer_count = self._calc_transfers(nodes)
        res.transfer_stations = self._find_transfer_stations(nodes)
        res.actual_stop_count = len({self.stations[n].name for n in nodes if n in self.stations})
        return res

    def _calc_transfers(self, nodes: List[int]) -> int:
        if len(nodes) < 2:
            return 0
        transfers = 0
        prev_line = ""
        for i in range(len(nodes) - 1):
            e = self.edge_map.get((nodes[i], nodes[i + 1]))
            if not e:
                continue
            if prev_line and prev_line != e.line:
                transfers += 1
            prev_line = e.line
        return transfers

    def _find_transfer_stations(self, nodes: List[int]) -> List[int]:
        result: List[int] = []
        if len(nodes) < 2:
            return result
        prev_line = ""
        for i in range(len(nodes) - 1):
            e = self.edge_map.get((nodes[i], nodes[i + 1]))
            if not e:
                continue
            if prev_line and prev_line != e.line:
                result.append(nodes[i])
            prev_line = e.line
        return result

    def _dijkstra(
        self,
        from_id: int,
        to_id: int,
        metric: str,
        banned: Optional[List[Tuple[int, int]]] = None,
    ) -> PathResult:
        empty = PathResult()
        if not self._can_visit(from_id) or not self._can_visit(to_id):
            return empty

        banned_set = set(banned or [])
        StateKey = Tuple[int, str]  # (node, line)

        def is_banned(u: int, v: int) -> bool:
            return (u, v) in banned_set

        parent: Dict[StateKey, StateKey] = {}
        best: Dict[StateKey, Tuple[float, float]] = {}

        start: StateKey = (from_id, "")
        if metric == "time":
            best[start] = (0.0, 0.0)
            heap: List[Tuple[float, float, int, str]] = [(0.0, 0.0, from_id, "")]
            found = False
            target: Optional[StateKey] = None
            best_time = float("inf")
            best_transfers = float("inf")

            while heap:
                t, tr, node, line = heapq.heappop(heap)
                key = (node, line)
                b = best.get(key)
                if b is None or b[0] < t - 1e-9 or (abs(b[0] - t) < 1e-9 and b[1] < tr):
                    continue

                if node == to_id:
                    if t < best_time - 1e-9 or (abs(t - best_time) < 1e-9 and tr < best_transfers):
                        best_time, best_transfers = t, tr
                        target = key
                        found = True
                    continue

                for eidx in self.adj.get(node, []):
                    e = self.edges[eidx]
                    if is_banned(e.from_id, e.to_id) or not self._can_visit(e.to_id):
                        continue
                    new_tr = tr + (1 if line and line != e.line else 0)
                    new_t = t + e.travel_time
                    nkey = (e.to_id, e.line)
                    nb = best.get(nkey)
                    if nb is not None:
                        if nb[0] < new_t - 1e-9:
                            continue
                        if abs(nb[0] - new_t) < 1e-9 and nb[1] <= new_tr:
                            continue
                    best[nkey] = (new_t, new_tr)
                    parent[nkey] = key
                    heapq.heappush(heap, (new_t, new_tr, e.to_id, e.line))
        else:
            best[start] = (0.0, 0.0)
            heap = [(0.0, 0.0, from_id, "")]
            found = False
            target = None
            best_transfers = float("inf")
            best_time = float("inf")

            while heap:
                tr, t, node, line = heapq.heappop(heap)
                key = (node, line)
                b = best.get(key)
                if b is None or b[0] < tr or (b[0] == tr and b[1] < t - 1e-9):
                    continue

                if node == to_id:
                    if tr < best_transfers or (tr == best_transfers and t < best_time - 1e-9):
                        best_transfers, best_time = tr, t
                        target = key
                        found = True
                    continue

                for eidx in self.adj.get(node, []):
                    e = self.edges[eidx]
                    if is_banned(e.from_id, e.to_id) or not self._can_visit(e.to_id):
                        continue
                    new_tr = tr + (1 if line and line != e.line else 0)
                    new_t = t + e.travel_time
                    nkey = (e.to_id, e.line)
                    nb = best.get(nkey)
                    if nb is not None:
                        if nb[0] < new_tr:
                            continue
                        if nb[0] == new_tr and nb[1] <= new_t - 1e-9:
                            continue
                    best[nkey] = (new_tr, new_t)
                    parent[nkey] = key
                    heapq.heappush(heap, (new_tr, new_t, e.to_id, e.line))

        if not found or target is None:
            return empty

        path: List[int] = []
        cur: Optional[StateKey] = target
        while cur is not None:
            path.append(cur[0])
            if cur[0] == from_id:
                break
            cur = parent.get(cur)
        path.reverse()
        return self._build_path_result(path)

    def shortest_path(self, from_id: int, to_id: int) -> PathResult:
        return self._dijkstra(from_id, to_id, "time")

    def min_transfer_path(self, from_id: int, to_id: int) -> PathResult:
        return self._dijkstra(from_id, to_id, "transfer")

    def _path_line_sequence(self, nodes: List[int]) -> List[str]:
        seq = []
        for i in range(len(nodes) - 1):
            e = self.edge_map.get((nodes[i], nodes[i + 1]))
            if e:
                seq.append(e.line)
        return seq

    def _is_duplicate(self, paths: List[PathResult], nodes: List[int]) -> bool:
        seq = self._path_line_sequence(nodes)
        for p in paths:
            if p.nodes == nodes:
                return True
            if self._path_line_sequence(p.nodes) == seq and seq:
                return True
        return False

    def k_paths(self, from_id: int, to_id: int, k: int, metric: str) -> List[PathResult]:
        finder = lambda f, t, b: self._dijkstra(f, t, metric, b)
        results: List[PathResult] = []
        first = finder(from_id, to_id, [])
        if first.empty:
            return results
        results.append(first)

        def better(a: PathResult, b: PathResult) -> bool:
            if metric == "time":
                if a.total_time != b.total_time:
                    return a.total_time < b.total_time
                return a.transfer_count < b.transfer_count
            if a.transfer_count != b.transfer_count:
                return a.transfer_count < b.transfer_count
            return a.total_time < b.total_time

        for _ in range(1, k):
            prev = results[-1]
            candidates: List[PathResult] = []
            for i in range(len(prev.nodes) - 1):
                spur_node = prev.nodes[i]
                root_path = prev.nodes[: i + 1]
                banned: List[Tuple[int, int]] = []
                for j in range(len(results)):
                    p = results[j]
                    if len(p.nodes) > i and p.nodes[: i + 1] == root_path:
                        if i + 1 < len(p.nodes):
                            banned.append((p.nodes[i], p.nodes[i + 1]))
                for j in range(len(root_path) - 1):
                    banned.append((root_path[j], root_path[j + 1]))
                spur = finder(spur_node, to_id, banned)
                if spur.empty:
                    continue
                total = root_path + spur.nodes[1:]
                candidate = self._build_path_result(total)
                if not candidate.empty and not self._is_duplicate(results + candidates, candidate.nodes):
                    candidates.append(candidate)
            if not candidates:
                break
            candidates.sort(key=lambda c: (c.total_time, c.transfer_count) if metric == "time" else (c.transfer_count, c.total_time))
            results.append(candidates[0])
        return results

    def connectivity(self) -> dict:
        open_nodes = {s.id for s in self.station_list if s.is_open}
        if not open_nodes:
            return {"openCount": 0, "componentCount": 0, "components": [], "connected": True}

        visited = set()
        components: List[List[int]] = []

        def bfs(start: int) -> List[int]:
            from collections import deque

            comp = []
            q = deque([start])
            visited.add(start)
            while q:
                u = q.popleft()
                comp.append(u)
                for eidx in self.adj.get(u, []):
                    e = self.edges[eidx]
                    if e.to_id in open_nodes and e.to_id not in visited:
                        visited.add(e.to_id)
                        q.append(e.to_id)
                for e in self.edges:
                    if e.to_id == u and e.from_id in open_nodes and e.from_id not in visited:
                        visited.add(e.from_id)
                        q.append(e.from_id)
            return comp

        for nid in open_nodes:
            if nid not in visited:
                components.append(bfs(nid))

        return {
            "openCount": len(open_nodes),
            "componentCount": len(components),
            "components": [{"size": len(c)} for c in components],
            "connected": len(components) <= 1,
        }

    def path_to_json(self, path: PathResult) -> dict:
        return path.to_dict(self.stations, self.edge_map)


# 全局单例，供 API 使用
metro = MetroService()
