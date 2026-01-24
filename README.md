# 🚀 Cheat Lua API 脚本指南 (v1.0)

基于 **Lua 5.4.8** 与 **sol2** 构建。

## 📋 全局信息

* **Lua 版本**: 5.4.8
* **sol2 版本**: 3.3.0
* **执行环境**: 脚本由 `OnPaint` 事件驱动，每帧调用。
* **入口函数**: 脚本必须定义 `function OnPaint()` 作为绘制循环的入口。

---

## 🎨 ImGui 绘制模块

`ImGui` 表提供了强大的背景绘图 API，所有坐标和颜色均基于屏幕空间。

### 基础方法

| 函数签名 | 说明 |
| --- | --- |
| `ImGui.Color(r, g, b, a)` | 创建一个 `ImU32` 颜色值 (0-255)。 |
| `ImGui.GetDeltaTime()` | 获取上一帧的间隔时间。 |
| `ImGui.GetFPS()` | 获取当前渲染帧率。 |
| `ImGui.GetScreenSize()` | 返回包含 `x, y` 的表格，表示屏幕分辨率。 |
| `ImGui.GetMousePos()` | 返回包含 `x, y` 的当前鼠标坐标。 |
| `ImGui.CalcTextSize(text)` | 计算文本在屏幕上占据的 `x, y` 尺寸。 |

### 输入检测

| 函数签名 | 说明 |
| --- | --- |
| `ImGui.IsMouseDown(button)` | 鼠标按键是否按下 (0:左键, 1:右键, 2:中键)。 |
| `ImGui.IsKeyDown(key)` | 指定按键是否按下 (使用 ImGuiKey 枚举索引)。 |

### 形状绘制

| 函数签名 | 参数说明 |
| --- | --- |
| `AddLine(x1, y1, x2, y2, col, thickness)` | 绘制直线。 |
| `AddRect(x1, y1, x2, y2, col, rounding, thickness)` | 绘制矩形边框。 |
| `AddRectFilled(x1, y1, x2, y2, col, rounding)` | 绘制实心矩形。 |
| `AddCircle(x, y, radius, col, segments, thickness)` | 绘制圆圈。 |
| `AddText(x, y, col, text)` | 在指定位置绘制文字。 |
| `AddTriangle(x1, y1, x2, y2, x3, y3, col, thick)` | 绘制三角形。 |
| `AddBezierCubic(x1, y1, ..., col, thick, seg)` | 绘制三阶贝塞尔曲线。 |

---

## 🎮 SDK 核心模块

`SDK` 提供对虚幻引擎（Unreal Engine）底层对象的直接访问。

### 基础结构 (Userdata)

#### `FVector`

* **构造函数**: `FVector(x, y, z)` 或 `FVector()`
* **属性**: `.X`, `.Y`, `.Z` (float)

### 全局方法

* **`SDK.GetLocalPC()`**: 返回本地 `PlayerController` 的内存地址 (`uintptr_t`)。
* **`SDK.GetActors()`**: 返回一个包含当前 World 中所有 Actor 地址的数组 (table)。
* **`SDK.GetXXXClass()`**: 获取特定类的 StaticClass 指针，用于 `IsA` 判断。
* `GetCharacterClass()` / `GetDinoClass()` / `GetTurretClass()` 等。



---

## 🛠️ 对象操作接口

这些模块用于处理从 `SDK.GetActors()` 获取的原始地址。

### `Actor` (通用对象)

| 函数 | 返回值 | 说明 |
| --- | --- | --- |
| `IsA(addr, class_ptr)` | `bool` | 判断对象是否属于特定类型。 |
| `GetLocation(addr)` | `FVector?` | 获取 Actor 的世界坐标（可能返回 nil）。 |
| `GetDistance(a, b)` | `float` | 计算两个 Actor 之间的距离。 |
| `IsHidden(addr)` | `bool` | 对象是否处于隐藏状态。 |
| `GetClassName(addr)` | `string` | 获取该对象的类名字符串。 |

### `Character` (生物/玩家)

* **`GetInfo(addr)`**:
* **返回**: `health, maxHealth, isDead, name`
* **说明**: 自动识别玩家名或生物描述名。


* **`GetRelation(target, local)`**:
* **返回**: `int` (关系枚举：0-中立, 1-友军, 2-敌对等)。



### `Item` (掉落物)

* **`GetDroppedInfo(addr)`**:
* **返回**: `isValid, name, quantity, rating, isBP, className`
* **说明**: 获取地面掉落包的详细属性。



### `PC` (控制器)

* **`GetPawn(pc_addr)`**: 获取当前控制的 Pawn 地址。
* **`ProjectToScreen(pc_addr, worldPos)`**:
* **返回**: `success, screenX, screenY`
* **说明**: 将世界坐标转换为屏幕坐标。



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
