local WatermarkRenderer = {
    -- 默认视觉配置
    DEFAULT_CONFIG = {
        fade = 40,         -- 渐变宽度
        padX = -10,        -- 横向内边距
        padY = 5,         -- 纵向内边距
        alpha = 50,        -- 背景透明度
        shadowA = 127,     -- 文本阴影透明度
        lerpSpeed = 10.0,  -- 平滑改变尺寸的速度 (数值越大越快)
        defaultColor = {r = 255, g = 255, b = 255, a = 200}
    },

    _state = {
        lastUpdateFPS = 0,
        lastUpdateCPU_Usage = 0,
        lastUpdateCPU_Freq = 0,
        lastUpdateGPU_Usage = 0,
        cachedFPS = 0,
        cachedCPU_Usage = 0,
        cachedCPU_Freq = 0,
        cachedGPU_Usage = 0,
        updateRateFPS = 1.0,
        updateRateCPU_Usage = 1.0,
        updateRateCPU_Freq = 1.0,
        updateRateGPU_Usage = 1.0,
        
        -- 新增：用于平滑过渡的状态变量
        currentWidth = 0,  -- 当前背景宽度
        currentHeight = 0, -- 当前背景高度
        isInitialized = false -- 是否已初始化尺寸
    }
}

--- 线性插值辅助函数
local function Lerp(start, finish, t)
    return start + (finish - start) * t
end

--- 内部渲染函数：支持多颜色片段与平滑背景
--- @param segments table 格式：{{text="xxx", color={r,g,b,a}}, ...}
--- @param x number 目标起始X坐标 (这通常是计算出的对齐位置)
--- @param y number 目标起始Y坐标
--- @param customConfig table 可选配置
function WatermarkRenderer.DrawMultiColor(segments, x, y, customConfig)
    -- 1. 配置合并
    local cfg = WatermarkRenderer.DEFAULT_CONFIG
    if customConfig then
        cfg = setmetatable(customConfig, { __index = WatermarkRenderer.DEFAULT_CONFIG })
    end

    -- 2. 计算实时文本总尺寸 (目标尺寸)
    local targetTextW = 0
    local targetTextH = 0
    for _, seg in ipairs(segments) do
        local size = ImGui.CalcTextSize(seg.text)
        seg._w = size.x 
        targetTextW = targetTextW + size.x
        if size.y > targetTextH then targetTextH = size.y end
    end

    local targetW = targetTextW + cfg.padX * 2
    local targetH = targetTextH + cfg.padY * 2
    local dt = ImGui.GetDeltaTime()

    -- 3. 平滑尺寸处理 (关键点)
    local state = WatermarkRenderer._state
    if not state.isInitialized or state.currentWidth == 0 then
        state.currentWidth = targetW
        state.currentHeight = targetH
        state.isInitialized = true
    else
        -- 使用指数衰减插值，实现平滑移动
        local lerpFactor = math.min(1.0, dt * cfg.lerpSpeed)
        state.currentWidth = Lerp(state.currentWidth, targetW, lerpFactor)
        state.currentHeight = Lerp(state.currentHeight, targetH, lerpFactor)
    end

    -- 4. 布局坐标计算
    -- 注意：由于宽度在变，为了保持右侧对齐，我们需要重新计算起始点坐标
    local x0 = math.floor(x + (targetW - state.currentWidth))
    local x1 = x0 + cfg.fade
    local x2 = x1 + state.currentWidth
    local x3 = x2 + cfg.fade
    local y1 = math.floor(y)
    local y2 = y1 + math.floor(state.currentHeight)

    -- 颜色对象
    local colT = ImGui.Color(0, 0, 0, 0)
    local colS = ImGui.Color(0, 0, 0, cfg.alpha)

    -- 5. 绘制背景装饰
    ImGui.AddRectFilledMultiColor(x0, y1, x1, y2, colT, colS, colS, colT) -- 左侧渐变
    ImGui.AddRectFilled(x1, y1, x2, y2, colS, 0)                          -- 中间块
    ImGui.AddRectFilledMultiColor(x2, y1, x3, y2, colS, colT, colT, colS) -- 右侧渐变

    -- 6. 逐段渲染文字
    -- 文字渲染通常紧贴实时内容，所以起点基于 x1 (背景起始点) 加上 padX
    local currentX = x1 + cfg.padX
    local textPosY = y1 + cfg.padY

    for _, seg in ipairs(segments) do
        local c = seg.color or cfg.defaultColor
        local colMain = ImGui.Color(c.r, c.g, c.b, c.a or 255)
        local colShadow = ImGui.Color(0, 0, 0, cfg.shadowA)

        ImGui.AddText(currentX + 1, textPosY + 1, colShadow, seg.text)
        ImGui.AddText(currentX, textPosY, colMain, seg.text)
        currentX = currentX + seg._w
    end
end

--- 辅助函数：计算片段组的总渲染宽度 (包含固定渐变部分)
function WatermarkRenderer.GetSegmentsWidth(segments)
    local w = 0
    for _, seg in ipairs(segments) do
        w = w + ImGui.CalcTextSize(seg.text).x
    end
    local cfg = WatermarkRenderer.DEFAULT_CONFIG
    return w + (cfg.padX * 2) + (cfg.fade * 2)
end

-- 业务模块
local function Main()
    local screenSize = ImGui.GetScreenSize()
    if not screenSize then return end

    local pc = SDK.GetLocalPC()
    local myPawn = PC.GetPawn(pc)
    local ping = (pc == 0 or myPawn == 0) and 0 or Character.GetExactPing(myPawn)

    -- 数据更新逻辑
    local cpu_usage, cpu_freq = System.GetCPUStats()
    local gpu_usage = System.GetGPUStats()
    local state = WatermarkRenderer._state
    local dt = ImGui.GetDeltaTime()

    state.lastUpdateFPS = state.lastUpdateFPS + dt
    state.lastUpdateCPU_Usage = state.lastUpdateCPU_Usage + dt
    state.lastUpdateCPU_Freq = state.lastUpdateCPU_Freq + dt
    state.lastUpdateGPU_Usage = state.lastUpdateGPU_Usage + dt

    if state.lastUpdateFPS >= state.updateRateFPS then
        state.cachedFPS = ImGui.GetFPS()
        state.lastUpdateFPS = 0
    end
    if state.lastUpdateCPU_Usage >= state.updateRateCPU_Usage then
        state.cachedCPU_Usage = cpu_usage
        state.lastUpdateCPU_Usage = 0
    end
    if state.lastUpdateCPU_Freq >= state.updateRateCPU_Freq then
        state.cachedCPU_Freq = cpu_freq
        state.lastUpdateCPU_Freq = 0
    end
    if state.lastUpdateGPU_Usage >= state.updateRateGPU_Usage then
        state.cachedGPU_Usage = gpu_usage
        state.lastUpdateGPU_Usage = 0
    end

    -- 统计玩家和生物数量
    local actors = SDK.GetActors()
    local actorCount = 0
    local charClass = SDK.GetCharacterClass()
    local dinoClass = SDK.GetDinoClass()

    for _, addr in ipairs(actors) do
        if Actor.IsA(addr, charClass) or Actor.IsA(addr, dinoClass) then
            actorCount = actorCount + 1
        end
    end

    local dateStr = os.date("%H:%M:%S")

    local colors = {
        white  = {r = 255, g = 255, b = 255},
        orange = {r = 255, g = 99, b = 71}
    }

    local segments = {
        { text = "Begeerte     " },
        { text = string.format("%.0f", state.cachedFPS), color = colors.orange },
        { text = " FPS    " },
        { text = string.format("%.0f", ping), color = colors.orange },
        { text = " PING    " },
        { text = string.format("%.0f%%", state.cachedCPU_Usage), color = colors.orange },
        { text = " CPU    " },
        { text = string.format("%.1f", state.cachedCPU_Freq / 1000), color = colors.orange },
        { text = " GHZ    " },
        { text = string.format("%.0f%%", state.cachedGPU_Usage), color = colors.orange },
        { text = " GPU    " },
        { text = string.format("%d", actorCount), color = colors.orange },
        { text = " Actors    " },
        { text = dateStr }
    }

    -- 布局计算
    -- 注意：drawX 是目标文本应该在的位置，DrawMultiColor 内部会处理背景的平滑尺寸
    local totalW = WatermarkRenderer.GetSegmentsWidth(segments)
    local margin = 5
    local drawX = screenSize.x - totalW - margin
    local drawY = margin

    -- 最终渲染
    WatermarkRenderer.DrawMultiColor(segments, drawX, drawY)
end

-- 渲染回调入口
function OnPaint()
    local status, err = pcall(Main)
    if not status then
        ImGui.AddText(10, 10, ImGui.Color(255, 0, 0, 255), "FATAL: " .. tostring(err))
    end
end