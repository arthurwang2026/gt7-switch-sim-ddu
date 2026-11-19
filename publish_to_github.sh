#!/usr/bin/env bash
set -e

echo "=== 檢查 GitHub 登入狀態 ==="
gh auth status

REPO_NAME="gt7-switch-sim-ddu"
echo "=== 檢查 GitHub 公開儲存庫: $REPO_NAME ==="
gh repo create "$REPO_NAME" --public --source=. --remote=origin --push --description "PS5 Gran Turismo 7 Sim Racing DDU for Nintendo Switch (Atmosphère Homebrew .nro) with 8 motorsport modes & luxury EV cockpits" || true

APP_VER=$(sed -n 's/^#define GT7_APP_VERSION "_aw_\([^"]*\)"/\1/p' source/gt7_telemetry.h)
TAG_NAME="v${APP_VER}"
TITLE="GT7 Switch Sim-DDU Pro _aw_${APP_VER} (Wi-Fi Heartbeat Resilience, Forza/SimHub Telemetry Accuracy & Modular EV Cockpits)"

NOTES=$(cat << 'EOF'
## 🏁 GT7 Switch Sim-DDU Pro _aw_1.11.19.1305

### 🌟 本次雙階段優化與修復重點 (Optimization, Reliability & Telemetry Release)

1. **Wi-Fi 斷線修復、手動鎖定保護與離線子網偵測 (Network & Heartbeat Resilience)**：
   - **鎖定 IP 防覆寫**：分離 `simhub_ip` 與 `ps5_ip` 追蹤欄位，確保手動 `LOCK MANUAL IP` 模式下的 PS5 IP 絕不被連入的 PC SimHub 或雜散 UDP 封包覆蓋，且僅於狀態實際改變時寫入 `config.ini` 減少 SD 卡擦寫。
   - **Switch / POSIX 離線子網自動識別**：Switch 端透過 `nifmGetCurrentIpAddress()`、桌面端透過 `getifaddrs()` 精準取得本機 IP 與網段前綴（無須外網連線即可完成區域網偵測），並於探索輪詢時自動跳過本機 IP。
   - **智慧心跳包重連與 Socket 自動修復**：啟動立即發送首包心跳（`last_bcast_ms == 0`）、平衡 `/24` 掃描（每 250ms 4 主機 + 限流廣播）、並以 `SO_ERROR` 與 `last_ps5_seen_ms > 0` 雙重保護避免空轉或 SimHub 模式誤觸 Socket 重建。

2. **Forza Horizon 6 / PC SimHub 遙測精準修復 (Telemetry Accuracy & DRY Cleanup)**：
   - **保留真實扭力與馬力輸出**：新增 `pkt_has_torque` / `pkt_has_power` 標記，確保 Forza RWD / FWD / AWD 真實前後輪扭力分配與 kW 功率不被後續 Fallback 公式覆蓋；並將轉向角解析提前至扭力分配之前，消除 1 幀轉向延遲。
   - **SimHub JSON 狀態精準同步**：接收 JSON 遙測封包時正確解除 `GT7_FLAG_PAUSED` 狀態，並在頂部狀態列正確顯示獨立的 `simhub_ip` 位址。

3. **SDL 效能批次渲染、模組化架構與實車即時數據化 (Renderer & EV Cockpit Polish)**：
   - **SDL2 渲染批次化 (`SDL_RenderFillRects`)**：最佳化 `hud_draw_text`（直行 Run-length 合併 + 批次繪製）與 BMW Curved Crystal Glass Chevron 光條渲染，顯著降低 Switch GPU/CPU Draw Call；時鐘函式每秒僅查詢 1 次 `localtime()` 快取。
   - **模組化架構重構**：拆出 `source/hud_internal.h` 與 `source/hud_ev_cockpits.c`（Luxury EV 四大車廠儀表獨立成檔），維持純淨零重複與 `#000000` OLED 真黑基準。
   - **消除靜態佔位數值**：Benz MBUX EQ、BMW iDrive、Audi e-tron RS、Volvo EX90全面顯示即時冷卻液溫度 (`°C`/`°F`)、動態 `(P)` 手煞車/駐車狀態、即時賽道名稱與 `GT7_FLAG_IN_PIT` 動態進站限速標誌。
   - **IP 觸控視窗對齊與全協定開放**：統一 `IP_MODAL_*` 佈局座標修正鍵盤 2px 觸控偏移，並於觸控視窗完整開放全部 6 種 `gt7_heartbeat_mode_t` 模式按鈕 (`AUTO`, `PKT C`, `PKT ~`, `PC SIMHUB`, `PKT A`, `PKT B`)。

4. **自動化回歸測試驗證 (`make test`)**：
   - 新增 `tests/test_telemetry.c`（5 項 UDP 迴路與封包解析測試）與 `tests/test_hud_touch.c`（IP 觸控矩陣、6 協定切換與 8 大主題全離線渲染測試），通過 AddressSanitizer / UndefinedBehaviorSanitizer 與零編譯警告驗證。

---

### 📦 安裝方式 / Installation
- 下載最新 [`gt7_switch_hud.nro`](https://github.com/arthurwang2026/gt7-switch-sim-ddu/releases/latest/download/gt7_switch_hud.nro) 並覆蓋 Switch SD 卡之 `/switch/gt7_hud/gt7_switch_hud.nro`。
- 若遇到無線路由器高頻 UDP 斷線，建議點選底部 `IP: AUTO` -> 輸入 PS5 區網 IP -> 點選 `LOCK MANUAL IP`。
EOF
)

echo "=== 正在更新 Release $TAG_NAME ==="
gh release edit "$TAG_NAME" \
  --title "$TITLE" \
  --notes "$NOTES" || gh release create "$TAG_NAME" dist/switch/gt7_hud/gt7_switch_hud.nro --title "$TITLE" --notes "$NOTES"

echo "=== 上傳最新二進位 gt7_switch_hud.nro ==="
gh release upload "$TAG_NAME" dist/switch/gt7_hud/gt7_switch_hud.nro --clobber

echo "=== 發布完成 ==="
gh release view "$TAG_NAME"
