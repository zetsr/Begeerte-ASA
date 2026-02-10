local WatermarkRenderer = {
    DEFAULT_CONFIG = {
        fade = 40,
        padX = -10,
        padY = 5,
        alpha = 50,
        shadowA = 127,
        lerpSpeed = 10.0,
        defaultColor = {r = 255, g = 255, b = 255, a = 200}
    },

    _state = {
        lastUpdateFPS = 0,
        lastUpdateCPU_Usage = 0,
        lastUpdateCPU_Freq = 0,
        lastUpdateGPU_Usage = 0,
        lastUpdateSV = 0,
        
        cachedFPS = 0,
        cachedCPU_Usage = 0,
        cachedCPU_Freq = 0,
        cachedGPU_Usage = 0,
        cachedSV = 0,
        cachedVAR = 0,
        
        updateRateFPS = 1.0,
        updateRateCPU_Usage = 1.0,
        updateRateCPU_Freq = 1.0,
        updateRateGPU_Usage = 1.0,
        updateRateSV = 1.0,

        frameTimeWindow = {},
        windowSize = 120,       -- 增大样本量（约2秒的数据量在60帧下）
        windowPtr = 1,
        visualVar = 0,          -- 用于显示的平滑VAR
        
        currentWidth = 0,
        currentHeight = 0,
        isInitialized = false
    }
}

--- 线性插值辅助函数
local function Lerp(start, finish, t)
    return start + (finish - start) * t
end

--- 强化版 VAR 计算：标准差 + 低通滤波
local function GetCSGOVarStable(state, dtMs)
    -- 1. 基础样本采集
    state.frameTimeWindow[state.windowPtr] = dtMs
    state.windowPtr = (state.windowPtr % state.windowSize) + 1
    
    local n = #state.frameTimeWindow
    if n < 10 then return 0 end -- 样本不足时不计算

    -- 2. 计算平均值
    local sum = 0
    for i = 1, n do sum = sum + state.frameTimeWindow[i] end
    local mean = sum / n
    
    -- 3. 计算标准差
    local sqSum = 0
    for i = 1, n do
        local diff = state.frameTimeWindow[i] - mean
        sqSum = sqSum + (diff * diff)
    end
    local rawStdDev = math.sqrt(sqSum / n)

    -- 4. 二次平滑 (这是压低数值波动的关键)
    -- 使用极小的 alpha 值，使结果向 0 靠拢，只有持续的波动才会推高数值
    state.visualVar = Lerp(state.visualVar, rawStdDev, 0.05)
    
    return state.visualVar
end

function WatermarkRenderer.DrawMultiColor(segments, x, y, customConfig)
    local cfg = WatermarkRenderer.DEFAULT_CONFIG
    if customConfig then
        cfg = setmetatable(customConfig, { __index = WatermarkRenderer.DEFAULT_CONFIG })
    end

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
    local state = WatermarkRenderer._state

    if not state.isInitialized or state.currentWidth == 0 then
        state.currentWidth = targetW
        state.currentHeight = targetH
        state.isInitialized = true
    else
        local lerpFactor = math.min(1.0, dt * cfg.lerpSpeed)
        state.currentWidth = Lerp(state.currentWidth, targetW, lerpFactor)
        state.currentHeight = Lerp(state.currentHeight, targetH, lerpFactor)
    end

    local x0 = math.floor(x + (targetW - state.currentWidth))
    local x1 = x0 + cfg.fade
    local x2 = x1 + state.currentWidth
    local x3 = x2 + cfg.fade
    local y1 = math.floor(y)
    local y2 = y1 + math.floor(state.currentHeight)

    local colT = ImGui.Color(0, 0, 0, 0)
    local colS = ImGui.Color(0, 0, 0, cfg.alpha)

    ImGui.AddRectFilledMultiColor(x0, y1, x1, y2, colT, colS, colS, colT)
    ImGui.AddRectFilled(x1, y1, x2, y2, colS, 0)
    ImGui.AddRectFilledMultiColor(x2, y1, x3, y2, colS, colT, colT, colS)

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

function WatermarkRenderer.GetSegmentsWidth(segments)
    local w = 0
    for _, seg in ipairs(segments) do
        w = w + ImGui.CalcTextSize(seg.text).x
    end
    local cfg = WatermarkRenderer.DEFAULT_CONFIG
    return w + (cfg.padX * 2) + (cfg.fade * 2)
end

local function Main()
    local screenSize = ImGui.GetScreenSize()
    if not screenSize then return end

    local pc = SDK.GetLocalPC()
    local myPawn = PC.GetPawn(pc)
    
    local ping = 0
    if pc ~= 0 and myPawn ~= 0 then
        ping = Character.GetExactPing(myPawn)
    end

    local ipStr = nil
    local portNum = nil
    local netDriver = SDK.GetNetDriver()
    if netDriver and netDriver.ServerConnection then
        local rawIp = netDriver.ServerConnection:GetFirstIP()
        if rawIp and rawIp ~= "" and rawIp ~= "N/A" then
            ipStr = rawIp
            portNum = netDriver.ServerConnection:GetPort()
        end
    end

    local cpu_usage, cpu_freq = System.GetCPUStats()
    local gpu_usage = System.GetGPUStats()
    local state = WatermarkRenderer._state
    local dt = ImGui.GetDeltaTime()
    local dtMs = dt * 1000

    -- 核心：每帧计算并平滑 VAR
    local currentVar = GetCSGOVarStable(state, dtMs)

    state.lastUpdateFPS = state.lastUpdateFPS + dt
    state.lastUpdateCPU_Usage = state.lastUpdateCPU_Usage + dt
    state.lastUpdateCPU_Freq = state.lastUpdateCPU_Freq + dt
    state.lastUpdateGPU_Usage = state.lastUpdateGPU_Usage + dt
    state.lastUpdateSV = state.lastUpdateSV + dt

    if state.lastUpdateFPS >= state.updateRateFPS then
        state.cachedFPS = ImGui.GetFPS()
        state.lastUpdateFPS = 0
    end

    if state.lastUpdateSV >= state.updateRateSV then
        state.cachedSV = dtMs
        state.cachedVAR = currentVar
        state.lastUpdateSV = 0
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

    local actors = SDK.GetActors()
    local actorCount = 0
    local charClass = SDK.GetCharacterClass()
    local dinoClass = SDK.GetDinoClass()
    if actors then
        for _, addr in ipairs(actors) do
            if Actor.IsA(addr, charClass) or Actor.IsA(addr, dinoClass) then
                actorCount = actorCount + 1
            end
        end
    end

    local dateStr = os.date("%H:%M:%S")
    local colors = {
        white  = {r = 255, g = 255, b = 255},
        orange = {r = 255, g = 99, b = 71}
    }

    local segments = {}
    table.insert(segments, { text = "Begeerte     " })
    table.insert(segments, { text = string.format("%.0f", state.cachedFPS), color = colors.orange })
    table.insert(segments, { text = " FPS    " })
    
    table.insert(segments, { text = string.format("%.0f", state.cachedSV), color = colors.orange })
    table.insert(segments, { text = " SV    " })

    table.insert(segments, { text = string.format("%.2f", state.cachedVAR), color = colors.orange })
    table.insert(segments, { text = " VAR    " })

    if ping > 0 then
        table.insert(segments, { text = string.format("%.0f", ping), color = colors.orange })
        table.insert(segments, { text = " PING    " })
    end

    table.insert(segments, { text = string.format("%.0f%%", state.cachedCPU_Usage), color = colors.orange })
    table.insert(segments, { text = " CPU    " })
    table.insert(segments, { text = string.format("%.1f", state.cachedCPU_Freq / 1000), color = colors.orange })
    table.insert(segments, { text = " GHZ    " })
    table.insert(segments, { text = string.format("%.0f%%", state.cachedGPU_Usage), color = colors.orange })
    table.insert(segments, { text = " GPU    " })

    if actorCount > 0 then
        table.insert(segments, { text = string.format("%d", actorCount), color = colors.orange })
        table.insert(segments, { text = " Actors    " })
    end

    if ipStr and portNum then
        table.insert(segments, { text = string.format("%s:%d    ", ipStr, portNum) })
    end

    table.insert(segments, { text = dateStr })

    local totalW = WatermarkRenderer.GetSegmentsWidth(segments)
    local margin = 5
    local drawX = screenSize.x - totalW - margin
    local drawY = margin

    WatermarkRenderer.DrawMultiColor(segments, drawX, drawY)
end

function OnPaint()
    local status, err = pcall(Main)
    if not status then
        ImGui.AddText(10, 10, ImGui.Color(255, 0, 0, 255), "FATAL: " .. tostring(err))
    end
end