# ✨ Begeerte for ARK: Survival Ascended

基于 [MinHook](https://github.com/TsudaKageyu/minhook) [Minimal-D3D12-Hook-ImGui](https://github.com/zetsr/Minimal-D3D12-Hook-ImGui) [ImGui](https://github.com/ocornut/imgui) [Dumper-7](https://github.com/Encryqed/Dumper-7) 开发的 C++ 内部作弊

<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/485e987f-d15f-465b-97d3-ca8cb7eb27b1" />

# 🚀 Lua API

基于 **Lua 5.4.8** 与 **sol2** 构建。

## 📋 全局信息

* **Lua 版本**: 5.4.8
* **sol2 版本**: 3.3.0

---

# 💻 全局函数

| 函数签名 | 返回值 | 说明 | C++
| --- | --- | --- | --- |
| `OnPaint()` |  | 在这里使用 ImGui API 进行绘制 | `HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags)` |

---

# 🎨 ImGui

> ⚠️ **注意**
>
> **所有 ImGui 绘制 API 必须在 `OnPaint()` 全局函数中调用才会正常工作。**  
> 这些接口底层使用 `ImGui::GetBackgroundDrawList()`，仅在渲染阶段有效。
>
---

## 📦 ImGui 全局表

Lua 中通过全局表 `ImGui` 访问所有绘制与输入接口。

---

## 🎨 基础工具函数

| 函数签名 | 返回值 | 说明 |
| --- | --- | --- |
| `ImGui.Color(r, g, b, a)` | `ImU32` | 生成颜色值，参数范围 0~255 |
| `ImGui.GetDeltaTime()` | `float` | 上一帧时间间隔 |
| `ImGui.GetFPS()` | `float` | 当前帧率 |
| `ImGui.GetScreenSize()` | `table { x, y }` | 当前屏幕分辨率 |
| `ImGui.GetMousePos()` | `table { x, y }` | 当前鼠标屏幕坐标 |
| `ImGui.CalcTextSize(text)` | `table { x, y }` | 计算文本绘制尺寸 |

---

## 🖱️ 输入检测

| 函数签名 | 返回值 | 说明 |
| --- | --- | --- |
| `ImGui.IsMouseDown(button)` | `bool` | 鼠标是否按下（0 左 / 1 右 / 2 中） |
| `ImGui.IsKeyDown(key)` | `bool` | 键盘是否按下（`ImGuiKey` 枚举值） |

---

## 🖌️ 绘制 API

### 📐 线条与矩形

| 函数签名 | 说明 |
| --- | --- |
| `ImGui.AddLine(x1, y1, x2, y2, col, thickness)` | 绘制线段 |
| `ImGui.AddRect(x1, y1, x2, y2, col, rounding, thickness)` | 绘制矩形边框 |
| `ImGui.AddRectFilled(x1, y1, x2, y2, col, rounding)` | 绘制实心矩形 |
| `ImGui.AddRectFilledMultiColor(x1, y1, x2, y2, colUL, colUR, colBR, colBL)` | 多色渐变矩形 |

---

### 🔺 多边形

| 函数签名 | 说明 |
| --- | --- |
| `ImGui.AddTriangle(x1, y1, x2, y2, x3, y3, col, thickness)` | 绘制三角形 |
| `ImGui.AddTriangleFilled(x1, y1, x2, y2, x3, y3, col)` | 实心三角形 |
| `ImGui.AddQuad(x1, y1, x2, y2, x3, y3, x4, y4, col, thickness)` | 绘制四边形 |
| `ImGui.AddQuadFilled(x1, y1, x2, y2, x3, y3, x4, y4, col)` | 实心四边形 |
| `ImGui.AddNgon(x, y, radius, col, segments, thickness)` | 正多边形 |
| `ImGui.AddNgonFilled(x, y, radius, col, segments)` | 实心正多边形 |

---

### ⚪ 圆形 / 椭圆

| 函数签名 | 说明 |
| --- | --- |
| `ImGui.AddCircle(x, y, radius, col, segments, thickness)` | 圆形 |
| `ImGui.AddCircleFilled(x, y, radius, col, segments)` | 实心圆 |
| `ImGui.AddEllipse(x, y, rx, ry, col, rot, segments, thickness)` | 椭圆 |
| `ImGui.AddEllipseFilled(x, y, rx, ry, col, rot, segments)` | 实心椭圆 |

---

### 🌀 贝塞尔曲线

| 函数签名 | 说明 |
| --- | --- |
| `ImGui.AddBezierQuadratic(x1, y1, x2, y2, x3, y3, col, thickness, segments)` | 二阶贝塞尔 |
| `ImGui.AddBezierCubic(x1, y1, x2, y2, x3, y3, x4, y4, col, thickness, segments)` | 三阶贝塞尔 |

---

### 🔤 文本

| 函数签名 | 说明 |
| --- | --- |
| `ImGui.AddText(x, y, col, text)` | 绘制文本 |

---

# 🎮 SDK 核心模块

---

## 📐 基础结构（Userdata）

### `FVector`

```lua
local v = FVector(x, y, z)
````

| 成员  | 类型      |
| --- | ------- |
| `X` | `float` |
| `Y` | `float` |
| `Z` | `float` |

---

## 🌍 SDK 全局接口

| 函数                          | 返回值                | 说明                  |
| --------------------------- | ------------------ | ------------------- |
| `SDK.GetLocalPC()`          | `uintptr_t`        | 本地 PlayerController |
| `SDK.GetActors()`           | `table<uintptr_t>` | 当前 World 中所有 Actor  |
| `SDK.GetCharacterClass()`   | `uintptr_t`        | Character 类指针       |
| `SDK.GetDinoClass()`        | `uintptr_t`        | Dino 类指针            |
| `SDK.GetDroppedItemClass()` | `uintptr_t`        | 掉落物类                |
| `SDK.GetContainerClass()`   | `uintptr_t`        | 容器类                 |
| `SDK.GetTurretClass()`      | `uintptr_t`        | 炮塔类                 |

---

## 🧱 Actor 通用接口

| 函数                         | 返回值        | 说明   |
| -------------------------- | ---------- | ---- |
| `Actor.IsA(addr, class)`   | `bool`     | 类型判断 |
| `Actor.GetLocation(addr)`  | `FVector?` | 世界坐标 |
| `Actor.GetDistance(a, b)`  | `float`    | 距离   |
| `Actor.IsHidden(addr)`     | `bool`     | 是否隐藏 |
| `Actor.GetClassName(addr)` | `string`   | 类名   |

---

## 🧬 Character（玩家 / 生物）

### `Character.GetInfo(addr)`

返回：

```lua
health, maxHealth, isDead, name
```

说明：

* 自动区分玩家 / 生物
* 优先使用 `PlayerState` 名字

---

### `Character.GetRelation(target, local)`

| 返回值 | 含义 |
| --- | -- |
| `0` | 敌对 |
| `1` | 友军 |

---

## 🎒 Item（掉落物）

### `Item.GetDroppedInfo(addr)`

返回：

```lua
isValid, name, quantity, rating, isBlueprint, className
```

---

## 📦 Container（补给箱 / 容器）

### `Container.GetInfo(addr)`

返回：

```lua
name
```

---

## 🎮 PC（PlayerController）

| 函数                                 | 返回值          | 说明      |
| ---------------------------------- | ------------ | ------- |
| `PC.GetPawn(pc)`                   | `uintptr_t`  | 当前 Pawn |
| `PC.ProjectToScreen(pc, worldPos)` | `bool, x, y` | 世界 → 屏幕 |

---

## 📝 脚本示例：ESP

```lua
local ESP_CACHE = {}
local FADE_SPEED = 5.0
local LAST_TICK = 0

function OnPaint()
    local status, err = pcall(MainESP)
    if not status then
        ImGui.AddText(10, 10, ImGui.Color(255, 0, 0, 255), "LUA ERROR: " .. tostring(err))
    end
end

function MainESP()
    local pc = SDK.GetLocalPC()
    local myPawn = PC.GetPawn(pc)
    if pc == 0 or myPawn == 0 then return end

    local curTime = os.clock()
    local deltaTime = (LAST_TICK == 0) and 0.016 or (curTime - LAST_TICK)
    LAST_TICK = curTime

    local screen = ImGui.GetScreenSize()
    local actors = SDK.GetActors()
    
    local charClass = SDK.GetCharacterClass()
    local dinoClass = SDK.GetDinoClass()
    local dropClass = SDK.GetDroppedItemClass()

    for _, data in pairs(ESP_CACHE) do
        data.active = false
    end

    for i = 1, #actors do
        local actor = actors[i]
        if actor == 0 or actor == myPawn or Actor.IsHidden(actor) then goto next_actor end

        local loc = Actor.GetLocation(actor)
        local ok, sx, sy = PC.ProjectToScreen(pc, loc)
        
        if not ok or sx < 20 or sx > screen.x - 20 or sy < 20 or sy > screen.y - 20 then goto next_actor end

        local id = actor
        if not ESP_CACHE[id] then
            ESP_CACHE[id] = { alpha = 0, type = "none" }
        end
        
        local entry = ESP_CACHE[id]
        entry.active = true
        entry.sx, entry.sy = sx, sy
        entry.dist = Actor.GetDistance(myPawn, actor) / 100

        if Actor.IsA(actor, charClass) then
            local hp, maxHp, isDead, name = Character.GetInfo(actor)
            if isDead then entry.active = false; goto next_actor end
            
            entry.type = "unit"
            entry.name = name
            entry.hp, entry.maxHp = hp, maxHp
            entry.relation = Character.GetRelation(actor, myPawn)
            entry.isDino = Actor.IsA(actor, dinoClass)
        elseif Actor.IsA(actor, dropClass) then
            local valid, name, qty = Item.GetDroppedInfo(actor)
            if not valid or entry.dist > 50 then entry.active = false; goto next_actor end
            
            entry.type = "drop"
            entry.name = name
            entry.qty = qty
        end

        ::next_actor::
    end

    for id, data in pairs(ESP_CACHE) do
        local targetAlpha = data.active and 1.0 or 0.0

        if data.alpha < targetAlpha then
            data.alpha = math.min(targetAlpha, data.alpha + deltaTime * FADE_SPEED)
        elseif data.alpha > targetAlpha then
            data.alpha = math.max(targetAlpha, data.alpha - deltaTime * FADE_SPEED)
        end

        if data.alpha <= 0 and not data.active then
            ESP_CACHE[id] = nil
        elseif data.alpha > 0 then
            RenderEntity(data)
        end
    end
end

function RenderEntity(data)
    if data.type == "unit" then
        local r, g, b = 255, 75, 75
        if data.relation == 1 then
            r, g, b = 0, 255, 180
        end
        
        local typeTag = data.isDino and "生物" or "玩家"
        local mainTitle = string.upper(data.name)
        local subTitle = string.format("%s | %dM", typeTag, math.floor(data.dist))
        
        DrawSmartUnit(data.sx, data.sy, r, g, b, mainTitle, subTitle, data.hp, data.maxHp, data.alpha)

    elseif data.type == "drop" then
        local label = string.format("%s x%d [%dM]", string.upper(data.name), data.qty, math.floor(data.dist))
        DrawSmartTag(data.sx, data.sy, 255, 255, 255, label, data.alpha)
    end
end

function DrawSmartUnit(x, y, r, g, b, title, subtitle, hp, maxHp, alpha)
    local a = math.floor(alpha * 255)
    local bgCol = ImGui.Color(0, 0, 0, math.floor(alpha * 160))
    local textSubCol = ImGui.Color(180, 180, 180, a)
    local mainCol = ImGui.Color(r, g, b, a)

    local sMain = ImGui.CalcTextSize(title)
    local sSub = ImGui.CalcTextSize(subtitle)
    local padding = 6
    local contentW = math.max(sMain.x, sSub.x, 80)
    local fullW = contentW + (padding * 2)
    local fullH = sMain.y + sSub.y + 12

    local rx1, ry1 = x - (fullW / 2), y - fullH
    local rx2, ry2 = rx1 + fullW, y
    
    ImGui.AddRectFilled(rx1, ry1, rx2, ry2, bgCol, 0.0)
    ImGui.AddRectFilled(rx1, ry1, rx1 + 3, ry2, mainCol, 0.0)
    
    ImGui.AddText(rx1 + padding + 4, ry1 + 2, mainCol, title)
    ImGui.AddText(rx1 + padding + 4, ry1 + sMain.y + 2, textSubCol, subtitle)
    
    local pct = math.min(1.0, math.max(0.0, hp / maxHp))
    local barWidth = fullW - 10
    local barX, barY = rx1 + 5, ry2 - 4
    
    ImGui.AddRectFilled(barX, barY, barX + barWidth, barY + 2, ImGui.Color(40, 40, 40, a), 0.0)
    if pct > 0 then
        ImGui.AddRectFilled(barX, barY, barX + (barWidth * pct), barY + 2, mainCol, 0.0)
    end
end

function DrawSmartTag(x, y, col, text, alpha)
    local a = math.floor(alpha * 255)
    local size = ImGui.CalcTextSize(text)
    local w, h = size.x + 12, size.y + 4
    local x1 = x - (w / 2)
    local tagCol = ImGui.Color(col.r, col.g, col.b, a)

    ImGui.AddRectFilled(x1, y, x1 + w, y + h, ImGui.Color(10, 10, 10, math.floor(alpha * 140)), 0.0)
    ImGui.AddRectFilled(x1, y, x1 + w, y + 1, tagCol, 0.0)
    ImGui.AddText(x1 + 6, y + 2, tagCol, text)
end
```
---
