"""FastAPI 入口：为前端提供 REST API，与 C++ 控制台程序完全分离。"""

from pathlib import Path
from typing import List, Literal, Optional

from fastapi import FastAPI, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field

from metro_service import CLOSED, OPEN, metro

FRONTEND_DIR = Path(__file__).resolve().parent.parent / "frontend"

app = FastAPI(title="上海地铁演示 API", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)


class PathRequest(BaseModel):
    fromId: int
    toId: int
    mode: Literal["time", "transfer", "time_k", "transfer_k"] = "time"
    k: int = Field(default=3, ge=1, le=5)


class StatusRequest(BaseModel):
    status: Literal["开启", "关闭"]


@app.get("/api/health")
def health():
    return {"ok": True, "dataDir": str(metro.data_dir)}


@app.get("/api/stats")
def stats():
    return metro.stats()


@app.get("/api/connectivity")
def connectivity():
    return metro.connectivity()


@app.get("/api/stations/search")
def search_stations(q: str = Query("", min_length=0), limit: int = Query(20, ge=1, le=50)):
    return metro.search_stations(q, limit)


@app.get("/api/stations/by-line")
def stations_by_line(line: str):
    return metro.stations_by_line(line)


@app.patch("/api/stations/{station_id}/status")
def update_status(station_id: int, body: StatusRequest):
    result = metro.set_status(station_id, body.status)
    if result is None:
        raise HTTPException(404, "站点不存在")
    return result


@app.post("/api/stations/reload")
def reload_data():
    metro.reload()
    return {"ok": True, **metro.stats()}


@app.post("/api/path")
def find_path(body: PathRequest):
    if body.fromId == body.toId:
        raise HTTPException(400, "起点与终点不能相同")

    if body.mode == "time":
        paths = [metro.shortest_path(body.fromId, body.toId)]
    elif body.mode == "transfer":
        paths = [metro.min_transfer_path(body.fromId, body.toId)]
    elif body.mode == "time_k":
        paths = metro.k_paths(body.fromId, body.toId, body.k, "time")
    else:
        paths = metro.k_paths(body.fromId, body.toId, body.k, "transfer")

    return {
        "mode": body.mode,
        "count": len(paths),
        "paths": [metro.path_to_json(p) for p in paths if not p.empty],
    }


if FRONTEND_DIR.is_dir():
    app.mount("/", StaticFiles(directory=str(FRONTEND_DIR), html=True), name="frontend")
