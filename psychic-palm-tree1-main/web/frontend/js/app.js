const API = "/api";

let selectedFrom = null;
let selectedTo = null;
let allLines = [];

async function api(path, options = {}) {
  const res = await fetch(API + path, {
    headers: { "Content-Type": "application/json", ...options.headers },
    ...options,
  });
  if (!res.ok) {
    const err = await res.json().catch(() => ({}));
    throw new Error(err.detail || res.statusText);
  }
  return res.json();
}

function debounce(fn, ms) {
  let t;
  return (...args) => {
    clearTimeout(t);
    t = setTimeout(() => fn(...args), ms);
  };
}

// --- Tabs ---
document.querySelectorAll(".tab").forEach((btn) => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".tab").forEach((b) => b.classList.remove("active"));
    document.querySelectorAll(".panel").forEach((p) => p.classList.remove("active"));
    btn.classList.add("active");
    document.getElementById("panel-" + btn.dataset.tab).classList.add("active");
  });
});

// --- Stats ---
async function loadStats() {
  try {
    const s = await api("/stats");
    allLines = s.lines || [];
    document.getElementById("statsBar").textContent =
      `${s.stationCount} 站 · ${s.edgeCount} 边 · ${s.lineCount} 条线 · 开放 ${s.openStationCount} 站`;

    const sel = document.getElementById("lineFilter");
    sel.innerHTML = '<option value="">全部线路</option>';
    allLines.forEach((line) => {
      const opt = document.createElement("option");
      opt.value = line;
      opt.textContent = line;
      sel.appendChild(opt);
    });
  } catch (e) {
    document.getElementById("statsBar").textContent = "无法连接后端";
  }
}

// --- Station autocomplete ---
function setupAutocomplete(inputId, suggestId, onSelect) {
  const input = document.getElementById(inputId);
  const list = document.getElementById(suggestId);

  const search = debounce(async () => {
    const q = input.value.trim();
    if (q.length < 1) {
      list.classList.remove("show");
      return;
    }
    const items = await api("/stations/search?q=" + encodeURIComponent(q));
    list.innerHTML = "";
    if (!items.length) {
      list.classList.remove("show");
      return;
    }
    items.forEach((st) => {
      const li = document.createElement("li");
      li.innerHTML = `${st.name} <span class="meta">${st.line} · ID=${st.id} · ${st.status}</span>`;
      li.addEventListener("click", () => {
        input.value = st.name;
        list.classList.remove("show");
        onSelect(st);
      });
      list.appendChild(li);
    });
    list.classList.add("show");
  }, 250);

  input.addEventListener("input", search);
  input.addEventListener("focus", search);
  document.addEventListener("click", (e) => {
    if (!input.contains(e.target) && !list.contains(e.target)) {
      list.classList.remove("show");
    }
  });
}

setupAutocomplete("fromInput", "fromSuggest", (st) => { selectedFrom = st; });
setupAutocomplete("toInput", "toSuggest", (st) => { selectedTo = st; });

// --- Path search ---
document.getElementById("searchBtn").addEventListener("click", async () => {
  const container = document.getElementById("pathResults");
  container.innerHTML = "";

  if (!selectedFrom || !selectedTo) {
    container.innerHTML = '<p class="error">请从下拉列表中选择起点和终点站点</p>';
    return;
  }

  const mode = document.querySelector('input[name="mode"]:checked').value;
  container.innerHTML = '<p class="hint">规划中…</p>';

  try {
    const data = await api("/path", {
      method: "POST",
      body: JSON.stringify({ fromId: selectedFrom.id, toId: selectedTo.id, mode }),
    });

    if (!data.paths || !data.paths.length) {
      container.innerHTML = '<p class="error">未找到可行路径（站点可能关闭或网络不连通）</p>';
      return;
    }

    container.innerHTML = "";
    data.paths.forEach((path, idx) => {
      const card = document.createElement("div");
      card.className = "path-card";
      card.innerHTML = `
        <h3>方案 ${idx + 1}</h3>
        <div class="path-meta">
          <span>总时间：<strong>${path.totalTime.toFixed(1)}</strong> 分钟</span>
          <span>换乘：<strong>${path.transferCount}</strong> 次</span>
          <span>经过站数：<strong>${path.actualStopCount}</strong></span>
          ${path.transferStations.length ? `<span>换乘站：${path.transferStations.join(" → ")}</span>` : ""}
        </div>
        <ol class="timeline" id="timeline-${idx}"></ol>
      `;
      const ol = card.querySelector(`#timeline-${idx}`);
      path.steps.forEach((step) => {
        const li = document.createElement("li");
        li.innerHTML = `
          <div class="station-name">${step.index}. ${step.name}</div>
          <div class="station-line">ID=${step.id}${step.line ? " · " + step.line : ""}</div>
        `;
        if (step.segment) {
          const seg = document.createElement("div");
          seg.className = "segment" + (step.segment.isTransfer ? " transfer" : "");
          seg.textContent = `→ ${step.segment.line}，${step.segment.travelTime} 分钟` +
            (step.segment.isTransfer ? "（换乘）" : "");
          li.appendChild(seg);
        }
        ol.appendChild(li);
      });
      container.appendChild(card);
    });
  } catch (e) {
    container.innerHTML = `<p class="error">${e.message}</p>`;
  }
});

// --- Station management ---
let stationCache = [];

async function refreshStationTable() {
  const q = document.getElementById("stationSearch").value.trim();
  const line = document.getElementById("lineFilter").value;
  const tbody = document.getElementById("stationTable");

  if (line) {
    stationCache = await api("/stations/by-line?line=" + encodeURIComponent(line));
  } else if (q) {
    stationCache = await api("/stations/search?q=" + encodeURIComponent(q) + "&limit=50");
  } else {
    stationCache = await api("/stations/search?q=站&limit=30");
  }

  tbody.innerHTML = "";
  stationCache.forEach((st) => {
    const tr = document.createElement("tr");
    const isOpen = st.status === "开启";
    tr.innerHTML = `
      <td>${st.id}</td>
      <td>${st.name}</td>
      <td>${st.line}</td>
      <td><span class="badge ${isOpen ? "open" : "closed"}">${st.status}</span></td>
      <td></td>
    `;
    const btn = document.createElement("button");
    btn.className = "btn sm";
    btn.textContent = isOpen ? "关闭" : "开启";
    btn.addEventListener("click", async () => {
      const newStatus = isOpen ? "关闭" : "开启";
      await api(`/stations/${st.id}/status`, {
        method: "PATCH",
        body: JSON.stringify({ status: newStatus }),
      });
      await refreshStationTable();
      await loadStats();
    });
    tr.querySelector("td:last-child").appendChild(btn);
    tbody.appendChild(tr);
  });
}

document.getElementById("stationSearch").addEventListener("input", debounce(refreshStationTable, 300));
document.getElementById("lineFilter").addEventListener("change", refreshStationTable);

// --- Connectivity ---
document.getElementById("connectivityBtn").addEventListener("click", async () => {
  const box = document.getElementById("connectivityResult");
  try {
    const c = await api("/connectivity");
    const warn = !c.connected;
    box.className = "info-box" + (warn ? " warn" : "");
    box.innerHTML = `
      <p>开放站点：<strong>${c.openCount}</strong></p>
      <p>连通分量数：<strong>${c.componentCount}</strong></p>
      <p>${c.connected
        ? "✅ 所有开放站点处于同一连通分量"
        : "⚠️ 网络存在断连，部分开放站点之间不可达"}</p>
      <ul>${c.components.map((comp, i) => `<li>分量 ${i + 1}：${comp.size} 个站点</li>`).join("")}</ul>
    `;
  } catch (e) {
    box.className = "info-box warn";
    box.textContent = e.message;
  }
});

// --- Init ---
loadStats();
refreshStationTable();
