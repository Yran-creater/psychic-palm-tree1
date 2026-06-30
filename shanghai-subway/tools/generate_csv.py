# -*- coding: utf-8 -*-
"""Generate Station.csv and Edge.csv from Amap Shanghai subway JSON."""

import json
import urllib.request
from collections import defaultdict
from pathlib import Path

API_URL = "https://map.amap.com/service/subway?srhdata=3100_drw_shanghai.json"
DEFAULT_TIME = 1.5
TRANSFER_TIME = 5.0
DATA_DIR = Path(__file__).resolve().parent.parent / "data"


def fetch_json(url):
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=60) as resp:
        return json.loads(resp.read().decode("utf-8"))


def is_line4(name):
    return "4" in name and "\u7ebf" in name and name.replace("4\u53f7\u7ebf", "").replace("(", "").replace(")", "") in ("", "\u5916\u5708", "\u5185\u5708")


def normalize_line_name(name):
    if "4\u53f7\u7ebf" in name:
        return "4\u53f7\u7ebf"
    return name


def add_edge(edges, seen, f, t, line, direction, time):
    key = (f, t, line, direction)
    if key in seen:
        return
    seen.add(key)
    edges.append({"from": f, "to": t, "line": line, "direction": direction, "time": time})


def generate(data):
    stations = []
    edges = []
    line_stations = {}
    name_to_ids = defaultdict(list)
    next_id = 1001

    for line in data.get("l", []):
        line_name = normalize_line_name(line.get("kn") or line.get("ln") or "\u672a\u77e5\u7ebf\u8def")
        ids = []
        for st in line.get("st") or []:
            name = st.get("n", "\u672a\u77e5\u7ad9\u70b9")
            stations.append({"id": next_id, "name": name, "line": line_name, "status": "\u5f00\u542f"})
            name_to_ids[name].append(next_id)
            ids.append(next_id)
            next_id += 1
        if ids:
            line_stations[line_name] = ids

    seen_edge = set()
    for line_name, ids in line_stations.items():
        l4 = line_name == "4\u53f7\u7ebf"
        for i in range(len(ids) - 1):
            a, b = ids[i], ids[i + 1]
            if l4:
                add_edge(edges, seen_edge, a, b, line_name, "\u5185\u73af", DEFAULT_TIME)
                add_edge(edges, seen_edge, b, a, line_name, "\u5916\u73af", DEFAULT_TIME)
            else:
                add_edge(edges, seen_edge, a, b, line_name, "\u65e0", DEFAULT_TIME)
                add_edge(edges, seen_edge, b, a, line_name, "\u65e0", DEFAULT_TIME)
        if l4 and len(ids) >= 2:
            add_edge(edges, seen_edge, ids[-1], ids[0], "4\u53f7\u7ebf", "\u5185\u73af", DEFAULT_TIME)
            add_edge(edges, seen_edge, ids[0], ids[-1], "4\u53f7\u7ebf", "\u5916\u73af", DEFAULT_TIME)

    for name, ids in name_to_ids.items():
        if len(ids) < 2:
            continue
        for i in range(len(ids)):
            for j in range(i + 1, len(ids)):
                add_edge(edges, seen_edge, ids[i], ids[j], "\u6362\u4e58", "\u65e0", TRANSFER_TIME)
                add_edge(edges, seen_edge, ids[j], ids[i], "\u6362\u4e58", "\u65e0", TRANSFER_TIME)

    return stations, edges, name_to_ids


def write_csv(stations, edges, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    station_path = out_dir / "Station.csv"
    edge_path = out_dir / "Edge.csv"
    init_path = out_dir / "Station_init.csv"

    with station_path.open("w", encoding="utf-8-sig", newline="") as f:
        f.write("\u7ad9\u70b9ID,\u7ad9\u70b9\u540d,\u6240\u5c5e\u7ebf\u8def,\u8fd0\u8425\u72b6\u6001\n")
        for s in stations:
            f.write(f"{s['id']},{s['name']},{s['line']},{s['status']}\n")

    with init_path.open("w", encoding="utf-8-sig", newline="") as f:
        f.write("\u7ad9\u70b9ID,\u7ad9\u70b9\u540d,\u6240\u5c5e\u7ebf\u8def,\u8fd0\u8425\u72b6\u6001\n")
        for s in stations:
            f.write(f"{s['id']},{s['name']},{s['line']},{s['status']}\n")

    with edge_path.open("w", encoding="utf-8-sig", newline="") as f:
        f.write("\u8d77\u70b9\u7ad9ID,\u7ec8\u70b9\u7ad9ID,\u6240\u5c5e\u7ebf\u8def,\u8fd0\u884c\u65b9\u5411,\u8fd0\u884c\u65f6\u95f4\n")
        for e in edges:
            f.write(f"{e['from']},{e['to']},{e['line']},{e['direction']},{e['time']:.1f}\n")

    return station_path, edge_path, init_path


def main():
    print("[info] fetching:", API_URL)
    data = fetch_json(API_URL)
    print("[info] city:", data.get("s", ""))

    stations, edges, name_to_ids = generate(data)
    paths = write_csv(stations, edges, DATA_DIR)

    transfer_edges = sum(1 for e in edges if e["line"] == "\u6362\u4e58")
    unique_names = len(name_to_ids)
    multi_names = sum(1 for ids in name_to_ids.values() if len(ids) > 1)

    print("[info] stations:", len(stations))
    print("[info] unique names:", unique_names)
    print("[info] transfer hub names:", multi_names)
    print("[info] edges:", len(edges), "transfer edges:", transfer_edges)
    for p in paths:
        print("[info] wrote:", p)

    print("\n--- sample stations ---")
    for s in stations[:5]:
        print(s)
    print("\n--- sample edges ---")
    for e in edges[:8]:
        print(e)


if __name__ == "__main__":
    main()
