#ifndef WEB_PAGE_H
#define WEB_PAGE_H

const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Управление насосом</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: Arial, sans-serif;
            background: #2c2c2c;
            color: #ddd;
            padding: 20px;
        }
        .container { max-width: 1400px; margin: 0 auto; }
        h1 {
            text-align: center;
            margin-bottom: 20px;
            color: #e94560;
        }

        .card, .controls-block, .settings, .sessions-section,
        .console-section, .chart-wrapper, .stm32-settings, .status-decode {
            background: #3a3a3a;
            border-radius: 10px;
            padding: 15px;
            margin-bottom: 20px;
            border-left: 4px solid #555;
        }

        .card .label { color: #aaa; font-size: 12px; text-transform: uppercase; }
        .card .value { font-size: 24px; font-weight: bold; margin-top: 5px; color: #fff; }

        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
            gap: 15px;
            margin-bottom: 20px;
        }

        .charts-container {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: 20px;
            margin: 20px 0;
        }
        .chart-wrapper h3 {
            color: #ccc;
            font-size: 14px;
            margin-bottom: 10px;
            text-align: center;
        }
        .chart-wrapper canvas {
            width: 100% !important;
            height: 200px !important;
        }

        .btn {
            padding: 10px 20px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 14px;
            font-weight: bold;
            transition: 0.3s;
        }
        .btn-primary { background: #e94560; color: white; }
        .btn-primary:hover { background: #c73652; }
        .btn-success { background: #4caf50; color: white; }
        .btn-success:hover { background: #388e3c; }
        .btn-danger { background: #f44336; color: white; }
        .btn-danger:hover { background: #d32f2f; }
        .btn-warning { background: #ff9800; color: white; }
        .btn-warning:hover { background: #e68900; }
        .btn-secondary { background: #607d8b; color: white; }
        .btn-secondary:hover { background: #455a64; }
        .btn-info { background: #2196f3; color: white; }
        .btn-info:hover { background: #1976d2; }

        .btn-group {
            display: flex;
            flex-wrap: wrap;
            gap: 10px;
        }

        .stm32-settings {
            border-left-color: #e94560;
            display: none;
        }
        .stm32-settings.active { display: block; }

        .setting-item {
            display: inline-block;
            margin: 5px 20px 5px 0;
            color: #ddd;
            font-size: 14px;
        }
        .setting-item .label { color: #888; font-size: 12px; }
        .setting-item .value { color: #fff; font-weight: bold; font-size: 16px; }

        .status-row {
            display: flex;
            flex-wrap: wrap;
            gap: 15px;
            padding: 10px 0;
        }
        .status-item {
            display: flex;
            align-items: center;
            gap: 8px;
            background: #2a2a2a;
            padding: 8px 15px;
            border-radius: 20px;
            font-size: 13px;
        }
        .status-dot {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            display: inline-block;
        }
        .status-dot.green { background: #4caf50; }
        .status-dot.red { background: #f44336; }
        .status-dot.yellow { background: #ff9800; }
        .status-dot.gray { background: #666; }

        /* Блок расшифровки статуса */
        .status-decode {
            border-left-color: #ff9800;
        }
        .status-decode .bit-row {
            display: flex;
            flex-wrap: wrap;
            gap: 15px 30px;
            margin-top: 8px;
        }
        .status-decode .bit-item {
            display: flex;
            align-items: center;
            gap: 6px;
            font-size: 14px;
        }
        .status-decode .bit-item .led {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            display: inline-block;
        }
        .status-decode .bit-item .led.on { background: #4caf50; }
        .status-decode .bit-item .led.off { background: #666; }

        .sessions-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-wrap: wrap;
            gap: 10px;
            margin-bottom: 15px;
        }
        .sessions-header h2 {
            font-size: 16px;
            color: #e94560;
        }
        .sessions-table-wrapper { overflow-x: auto; }
        .sessions-table {
            width: 100%;
            border-collapse: collapse;
            font-size: 13px;
        }
        .sessions-table th {
            background: #2a2a2a;
            color: #aaa;
            padding: 10px 12px;
            text-align: left;
            border-bottom: 2px solid #e94560;
        }
        .sessions-table td {
            padding: 8px 12px;
            border-bottom: 1px solid #2a2a2a;
            color: #ccc;
        }
        .sessions-table tr:hover td { background: #4a4a4a; }
        .sessions-table .pump1-row td:first-child { border-left: 3px solid #4caf50; }
        .sessions-table .pump2-row td:first-child { border-left: 3px solid #ff9800; }
        .sessions-count { color: #aaa; font-size: 12px; }

        .console-section { padding: 0; }
        .console-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 15px 20px;
            background: #2a2a2a;
            border-radius: 10px 10px 0 0;
            flex-wrap: wrap;
            gap: 10px;
        }
        .console-header h2 {
            font-size: 16px;
            color: #e94560;
        }
        .console-controls {
            display: flex;
            gap: 10px;
            align-items: center;
            flex-wrap: wrap;
        }
        .console-controls input {
            background: #2a2a2a;
            color: #fff;
            border: 1px solid #555;
            padding: 5px 10px;
            border-radius: 4px;
        }
        .console-controls button {
            padding: 5px 15px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 12px;
        }
        .btn-clear { background: #f44336; color: white; }
        .btn-clear:hover { background: #d32f2f; }
        .btn-pause { background: #ff9800; color: white; }
        .btn-pause:hover { background: #e68900; }
        .btn-filter { background: #4caf50; color: white; }
        .btn-filter:hover { background: #388e3c; }

        .console-output {
            height: 200px;
            overflow-y: auto;
            padding: 10px 20px;
            font-family: 'Courier New', monospace;
            font-size: 12px;
            background: #1f1f1f;
            border-radius: 0 0 10px 10px;
        }
        .console-output .log-entry { padding: 2px 0; border-bottom: 1px solid #2a2a2a; }
        .console-output .log-time { color: #666; margin-right: 10px; }
        .console-output .log-rx { color: #4caf50; }
        .console-output .log-tx { color: #ff9800; }
        .console-output .log-event { color: #2196f3; }
        .console-output .log-error { color: #f44336; }
        .console-output .log-warning { color: #ff9800; }

        #errorDescription {
            font-size: 14px;
            padding: 5px 0;
            color: #ff6b6b;
        }

        @media (max-width: 1024px) {
            .charts-container { grid-template-columns: 1fr 1fr; }
        }
        @media (max-width: 768px) {
            .charts-container { grid-template-columns: 1fr; }
            .sessions-table { font-size: 11px; }
            .sessions-table th, .sessions-table td { padding: 6px 8px; }
            .controls-block .btn-group { flex-direction: column; }
            .controls-block .btn-group .btn { width: 100%; }
        }

        ::-webkit-scrollbar { width: 8px; }
        ::-webkit-scrollbar-track { background: #1f1f1f; }
        ::-webkit-scrollbar-thumb { background: #444; border-radius: 4px; }
        ::-webkit-scrollbar-thumb:hover { background: #555; }
    </style>
</head>
<body>
<div class="container">
    <h1>🚀 Управление насосной станцией</h1>

    <!-- Статус связи -->
    <div class="status-row">
        <div class="status-item">
            <span class="status-dot" id="wsDot"></span>
            <span>WebSocket: <span id="wsStatus">Подключение...</span></span>
        </div>
        <div class="status-item">
            <span class="status-dot" id="uartDot"></span>
            <span>UART: <span id="uartStatus">Ожидание...</span></span>
        </div>
        <div class="status-item">
            <span>🟢 Насос 1:</span>
            <span id="pump1Status">—</span>
        </div>
        <div class="status-item">
            <span>🟠 Насос 2:</span>
            <span id="pump2Status">—</span>
        </div>
    </div>

    <!-- Карточки -->
    <div class="grid" id="telemetry">
        <div class="card"><div class="label">Давление (атм)</div><div class="value" id="pressure">0.00</div></div>
        <div class="card"><div class="label">Ток насоса 1 (А)</div><div class="value" id="current1">0.000</div></div>
        <div class="card"><div class="label">Ток насоса 2 (А)</div><div class="value" id="current2">0.000</div></div>
        <div class="card"><div class="label">Температура (°C)</div><div class="value" id="temperature">0.0</div></div>
        <div class="card"><div class="label">Код ошибки</div><div class="value" id="error">0</div></div>
    </div>

    <!-- Блок расшифровки статуса -->
    <div class="status-decode">
        <h3 style="color:#ff9800; margin-bottom:8px;">🔍 Расшифровка статуса (0x<span id="statusHex">00</span>)</h3>
        <div class="bit-row" id="statusBits">
            <div class="bit-item"><span class="led off" id="ledPump1"></span> Насос 1 (факт): <span id="bitPump1">ВЫКЛ</span></div>
            <div class="bit-item"><span class="led off" id="ledPump2"></span> Насос 2 (факт): <span id="bitPump2">ВЫКЛ</span></div>
            <div class="bit-item"><span class="led off" id="ledCmd1"></span> Команда насоса 1: <span id="bitCmd1">ВЫКЛ</span></div>
            <div class="bit-item"><span class="led off" id="ledCmd2"></span> Команда насоса 2: <span id="bitCmd2">ВЫКЛ</span></div>
            <div class="bit-item"><span class="led off" id="ledPriority"></span> Приоритет полива: <span id="bitPriority">НЕТ</span></div>
            <div class="bit-item"><span class="led off" id="ledButton"></span> Кнопка удержана: <span id="bitButton">НЕТ</span></div>
        </div>
    </div>

    <!-- Расшифровка ошибок -->
    <div class="controls-block">
        <h3>⚠️ Расшифровка ошибок</h3>
        <div id="errorDescription">Ошибок нет</div>
    </div>

    <!-- Графики -->
    <div class="charts-container">
        <div class="chart-wrapper">
            <h3>📊 Давление (атм) <span style="color:#888;font-size:12px;font-weight:normal;">(5 минут)</span></h3>
            <canvas id="chartPressure"></canvas>
        </div>
        <div class="chart-wrapper">
            <h3>📊 Ток насоса 1 (А) <span style="color:#888;font-size:12px;font-weight:normal;">(5 минут)</span></h3>
            <canvas id="chart1"></canvas>
        </div>
        <div class="chart-wrapper">
            <h3>📊 Ток насоса 2 (А) <span style="color:#888;font-size:12px;font-weight:normal;">(5 минут)</span></h3>
            <canvas id="chart2"></canvas>
        </div>
    </div>

    <!-- Управление -->
    <div class="controls-block">
        <h3>🔧 Управление насосом ресивера (насос 1)</h3>
        <div class="btn-group">
            <button class="btn btn-success" onclick="sendCommand('pump_on')">▶ Включить</button>
            <button class="btn btn-danger" onclick="sendCommand('pump_off')">⏹ Выключить</button>
        </div>
    </div>
    <div class="controls-block">
        <h3>💧 Управление поливом (насос 2)</h3>
        <div class="btn-group">
            <button class="btn btn-success" onclick="sendCommand('irr_on')">💧 Включить полив</button>
            <button class="btn btn-danger" onclick="sendCommand('irr_off')">💧 Выключить полив</button>
        </div>
    </div>

    <!-- Настройки -->
    <div class="controls-block">
        <h3>⚙️ Настройки</h3>
        <div class="btn-group">
            <button class="btn btn-info" onclick="requestSettings()">📥 Запросить настройки с STM32</button>
            <button class="btn btn-secondary" onclick="toggleSettingsDisplay()">👁 Показать/скрыть настройки</button>
            <button class="btn btn-warning" onclick="clearSessions()">🗑 Очистить сессии</button>
        </div>
    </div>

    <div class="stm32-settings" id="stm32Settings">
        <h4 style="color:#e94560;margin-bottom:10px;">📋 Настройки STM32</h4>
        <div class="setting-item">
            <span class="label">Давление ВКЛ:</span>
            <span class="value" id="stm32Pon">—</span>
        </div>
        <div class="setting-item">
            <span class="label">Давление ВЫКЛ:</span>
            <span class="value" id="stm32Poff">—</span>
        </div>
        <div class="setting-item">
            <span class="label">Макс. время сессии:</span>
            <span class="value" id="stm32MaxTime">—</span>
        </div>
        <div class="setting-item">
            <span class="label">Статус:</span>
            <span class="value" id="stm32Status">—</span>
        </div>
    </div>

    <div class="settings">
        <label>Давление вкл: <input type="number" step="0.01" id="pmin" value="1.5"></label>
        <label>Давление выкл: <input type="number" step="0.01" id="pmax" value="3.0"></label>
        <button class="btn btn-primary" onclick="sendSettings()">Установить давление</button>
    </div>

    <!-- Сессии -->
    <div class="sessions-section">
        <div class="sessions-header">
            <h2>⏱ История работы насосов</h2>
            <span class="sessions-count" id="sessionsCount">Всего сессий: 0</span>
        </div>
        <div class="sessions-table-wrapper">
            <table class="sessions-table" id="sessionsTable">
                <thead>
                    <tr>
                        <th>Насос</th>
                        <th>№ сессии</th>
                        <th>Время начала</th>
                        <th>Время окончания</th>
                        <th>Длительность (сек)</th>
                        <th>Средний ток (А)</th>
                        <th>Макс. ток (А)</th>
                    </tr>
                </thead>
                <tbody id="sessionsBody">
                    <tr><td colspan="7" style="text-align:center;color:#666;padding:20px;">Нет данных о сессиях</td></tr>
                </tbody>
            </table>
        </div>
    </div>

    <!-- Консоль -->
    <div class="console-section">
        <div class="console-header">
            <h2>📋 UART Консоль</h2>
            <div class="console-controls">
                <input type="text" id="filterInput" placeholder="Фильтр..." onkeyup="applyFilter()">
                <button class="btn-filter" onclick="applyFilter()">Фильтр</button>
                <button class="btn-pause" onclick="togglePause()">⏸ Пауза</button>
                <button class="btn-clear" onclick="clearConsole()">🗑 Очистить</button>
            </div>
        </div>
        <div class="console-output" id="consoleOutput"></div>
    </div>
</div>

<script>
    // ==== Константы ====
    const PRESSURE_HISTORY_SECONDS = 300;
    const MAX_DATA_POINTS = 1500;

    // ==== Данные ====
    let pressureHistory = [];
    let currentHistory1 = [];
    let currentHistory2 = [];

    // ==== Сессии ====
    let sessions = [];
    let sessionId = 0;
    let currentSession = { pump1: null, pump2: null };
    let lastPump1State = false;
    let lastPump2State = false;
    let pump1StartTime = 0;
    let pump2StartTime = 0;
    let pump1Currents = [];
    let pump2Currents = [];

    // ==== Настройки STM32 ====
    let stm32Settings = { pon: null, poff: null, maxTime: null };
    let settingsVisible = false;

    // ==== Графики ====
    const ctxPressure = document.getElementById('chartPressure').getContext('2d');
    const ctx1 = document.getElementById('chart1').getContext('2d');
    const ctx2 = document.getElementById('chart2').getContext('2d');

    const chartPressure = new Chart(ctxPressure, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Давление (атм)',
                data: [],
                borderColor: '#2196f3',
                backgroundColor: 'rgba(33, 150, 243, 0.1)',
                borderWidth: 2,
                fill: true,
                tension: 0.3,
                pointRadius: 0
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 100 },
            scales: {
                y: { min: 0, max: 5, grid: { color: 'rgba(255,255,255,0.05)' }, ticks: { color: '#aaa' } },
                x: { grid: { display: false }, ticks: { color: '#aaa', maxTicksLimit: 10, maxRotation: 0 } }
            },
            plugins: { legend: { labels: { color: '#aaa' } } }
        }
    });

    const chart1 = new Chart(ctx1, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Ток насоса 1 (А)',
                data: [],
                borderColor: '#4caf50',
                backgroundColor: 'rgba(76, 175, 80, 0.1)',
                borderWidth: 2,
                fill: true,
                tension: 0.4,
                pointRadius: 0
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 100 },
            scales: {
                y: { min: 0, max: 25, grid: { color: 'rgba(255,255,255,0.05)' }, ticks: { color: '#aaa' } },
                x: { grid: { display: false }, ticks: { color: '#aaa', maxTicksLimit: 10, maxRotation: 0 } }
            },
            plugins: { legend: { labels: { color: '#aaa' } } }
        }
    });

    const chart2 = new Chart(ctx2, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Ток насоса 2 (А)',
                data: [],
                borderColor: '#ff9800',
                backgroundColor: 'rgba(255, 152, 0, 0.1)',
                borderWidth: 2,
                fill: true,
                tension: 0.4,
                pointRadius: 0
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 100 },
            scales: {
                y: { min: 0, max: 25, grid: { color: 'rgba(255,255,255,0.05)' }, ticks: { color: '#aaa' } },
                x: { grid: { display: false }, ticks: { color: '#aaa', maxTicksLimit: 10, maxRotation: 0 } }
            },
            plugins: { legend: { labels: { color: '#aaa' } } }
        }
    });

    // ==== WebSocket с автореконнектом ====
    let ws;
    let paused = false;
    let filterText = '';
    let logCount = 0;
    const MAX_LOGS = 500;
    let wsConnected = false;

    function reconnectWebSocket() {
        setTimeout(() => {
            if (ws.readyState !== WebSocket.OPEN) {
                ws = new WebSocket('ws://' + window.location.hostname + ':81/');
                setupWebSocket();
            }
        }, 3000);
    }

    function setupWebSocket() {
        ws.onopen = () => {
            wsConnected = true;
            updateConnectionStatus();
            addLog('✅ WebSocket подключен', 'event');
        };
        ws.onclose = () => {
            wsConnected = false;
            updateConnectionStatus();
            addLog('❌ WebSocket отключен, переподключение...', 'error');
            reconnectWebSocket();
        };
        ws.onerror = () => {
            wsConnected = false;
            updateConnectionStatus();
            addLog('❌ Ошибка WebSocket, переподключение...', 'error');
            ws.close();
        };
        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if (data.type === 'telemetry') {
                    updateTelemetry(data);
                } else if (data.type === 'log') {
                    addLog(data.message, data.category || 'event');
                } else if (data.type === 'history') {
                    const points = data.data;
                    if (points.length === 0) return;
                    pressureHistory = [];
                    currentHistory1 = [];
                    currentHistory2 = [];
                    for (let p of points) {
                        const t = p.t;
                        pressureHistory.push({ time: t, value: p.p });
                        currentHistory1.push({ time: t, value: Math.abs(p.c1) });
                        currentHistory2.push({ time: t, value: Math.abs(p.c2) });
                    }
                    updateAllCharts();
                    const last = points[points.length - 1];
                    document.getElementById('pressure').textContent = last.p.toFixed(2);
                    document.getElementById('current1').textContent = Math.abs(last.c1).toFixed(3);
                    document.getElementById('current2').textContent = Math.abs(last.c2).toFixed(3);
                    document.getElementById('temperature').textContent = last.temp ? last.temp.toFixed(1) : '—';
                    document.getElementById('error').textContent = last.e;
                    updateErrorDescription(last.e);
                    // Также обновим статус, если есть поле s в истории? Мы не храним статус в истории для отображения, но можем обновить последний статус из current telemetry.
                }
            } catch(e) { console.error('Parse error:', e); }
        };
    }

    // Инициализация WebSocket
    ws = new WebSocket('ws://' + window.location.hostname + ':81/');
    setupWebSocket();

    function addLog(message, category = 'event') {
        if (paused) return;
        const output = document.getElementById('consoleOutput');
        const entry = document.createElement('div');
        entry.className = 'log-entry';
        const time = new Date().toLocaleTimeString();
        entry.innerHTML = `<span class="log-time">[${time}]</span><span class="log-${category}">${message}</span>`;
        if (filterText && !entry.textContent.toLowerCase().includes(filterText.toLowerCase())) return;
        output.appendChild(entry);
        logCount++;
        if (logCount > MAX_LOGS) { output.removeChild(output.firstChild); logCount--; }
        output.scrollTop = output.scrollHeight;
    }

    function updateConnectionStatus() {
        const dot = document.getElementById('wsDot');
        const status = document.getElementById('wsStatus');
        if (wsConnected) {
            dot.className = 'status-dot green';
            status.textContent = 'Подключен';
            status.style.color = '#4caf50';
        } else {
            dot.className = 'status-dot red';
            status.textContent = 'Отключен';
            status.style.color = '#f44336';
        }
    }

    function updateAllCharts() {
        // График давления
        const labelsP = pressureHistory.map(item => {
            const d = new Date(item.time * 1000);
            return d.getHours() + ':' + String(d.getMinutes()).padStart(2,'0') + ':' + String(d.getSeconds()).padStart(2,'0');
        });
        chartPressure.data.labels = labelsP;
        chartPressure.data.datasets[0].data = pressureHistory.map(item => item.value);
        chartPressure.update('none');

        const labelsC1 = currentHistory1.map(item => {
            const d = new Date(item.time * 1000);
            return d.getHours() + ':' + String(d.getMinutes()).padStart(2,'0') + ':' + String(d.getSeconds()).padStart(2,'0');
        });
        chart1.data.labels = labelsC1;
        chart1.data.datasets[0].data = currentHistory1.map(item => item.value);
        chart1.update('none');

        const labelsC2 = currentHistory2.map(item => {
            const d = new Date(item.time * 1000);
            return d.getHours() + ':' + String(d.getMinutes()).padStart(2,'0') + ':' + String(d.getSeconds()).padStart(2,'0');
        });
        chart2.data.labels = labelsC2;
        chart2.data.datasets[0].data = currentHistory2.map(item => item.value);
        chart2.update('none');
    }

    // ==== Обновление статуса (расшифровка битов) ====
    function updateStatusDisplay(status) {
        const hex = status.toString(16).toUpperCase().padStart(2, '0');
        document.getElementById('statusHex').textContent = hex;

        // Бит 0: насос 1 фактически включён
        const p1 = (status & 0x01) ? 1 : 0;
        const led1 = document.getElementById('ledPump1');
        const span1 = document.getElementById('bitPump1');
        led1.className = 'led ' + (p1 ? 'on' : 'off');
        span1.textContent = p1 ? 'ВКЛ' : 'ВЫКЛ';
        span1.style.color = p1 ? '#4caf50' : '#f44336';

        // Бит 1: насос 2 фактически включён
        const p2 = (status & 0x02) ? 1 : 0;
        const led2 = document.getElementById('ledPump2');
        const span2 = document.getElementById('bitPump2');
        led2.className = 'led ' + (p2 ? 'on' : 'off');
        span2.textContent = p2 ? 'ВКЛ' : 'ВЫКЛ';
        span2.style.color = p2 ? '#4caf50' : '#f44336';

        // Бит 2: команда насоса 1 активна
        const cmd1 = (status & 0x04) ? 1 : 0;
        const led3 = document.getElementById('ledCmd1');
        const span3 = document.getElementById('bitCmd1');
        led3.className = 'led ' + (cmd1 ? 'on' : 'off');
        span3.textContent = cmd1 ? 'АКТИВНА' : 'ВЫКЛ';
        span3.style.color = cmd1 ? '#ff9800' : '#888';

        // Бит 3: команда насоса 2 активна
        const cmd2 = (status & 0x08) ? 1 : 0;
        const led4 = document.getElementById('ledCmd2');
        const span4 = document.getElementById('bitCmd2');
        led4.className = 'led ' + (cmd2 ? 'on' : 'off');
        span4.textContent = cmd2 ? 'АКТИВНА' : 'ВЫКЛ';
        span4.style.color = cmd2 ? '#ff9800' : '#888';

        // Бит 4: приоритет полива
        const priority = (status & 0x10) ? 1 : 0;
        const led5 = document.getElementById('ledPriority');
        const span5 = document.getElementById('bitPriority');
        led5.className = 'led ' + (priority ? 'on' : 'off');
        span5.textContent = priority ? 'УСТАНОВЛЕН' : 'НЕТ';
        span5.style.color = priority ? '#ff9800' : '#888';

        // Бит 5: кнопка удержана
        const btn = (status & 0x20) ? 1 : 0;
        const led6 = document.getElementById('ledButton');
        const span6 = document.getElementById('bitButton');
        led6.className = 'led ' + (btn ? 'on' : 'off');
        span6.textContent = btn ? 'ДА' : 'НЕТ';
        span6.style.color = btn ? '#ff9800' : '#888';
    }

    // ==== Обновление телеметрии ====
    function updateTelemetry(data) {
        const now = Math.floor(Date.now() / 1000);

        // UART статус
        const uartDot = document.getElementById('uartDot');
        const uartStatus = document.getElementById('uartStatus');
        if (data.online) {
            uartDot.className = 'status-dot green';
            uartStatus.textContent = 'Online';
            uartStatus.style.color = '#4caf50';
        } else {
            uartDot.className = 'status-dot red';
            uartStatus.textContent = 'Offline';
            uartStatus.style.color = '#f44336';
        }

        // Состояния насосов из статуса (биты 0 и 1)
        const status = data.status || 0;
        const p1 = (status & 0x01) ? true : false;
        const p2 = (status & 0x02) ? true : false;

        document.getElementById('pump1Status').textContent = p1 ? 'ВКЛ' : 'ВЫКЛ';
        document.getElementById('pump1Status').style.color = p1 ? '#4caf50' : '#f44336';
        document.getElementById('pump2Status').textContent = p2 ? 'ВКЛ' : 'ВЫКЛ';
        document.getElementById('pump2Status').style.color = p2 ? '#4caf50' : '#f44336';

        // Расшифровка статуса
        updateStatusDisplay(status);

        // Давление
        const pressure = data.p;
        pressureHistory.push({ time: now, value: pressure });
        const cutoff = now - PRESSURE_HISTORY_SECONDS;
        pressureHistory = pressureHistory.filter(item => item.time >= cutoff);
        const labelsP = pressureHistory.map(item => {
            const d = new Date(item.time * 1000);
            return d.getHours() + ':' + String(d.getMinutes()).padStart(2,'0') + ':' + String(d.getSeconds()).padStart(2,'0');
        });
        chartPressure.data.labels = labelsP;
        chartPressure.data.datasets[0].data = pressureHistory.map(item => item.value);
        chartPressure.update('none');

        // Токи
        const current1 = Math.abs(data.c1);
        const current2 = Math.abs(data.c2);
        currentHistory1.push({ time: now, value: current1 });
        currentHistory2.push({ time: now, value: current2 });
        if (currentHistory1.length > MAX_DATA_POINTS) currentHistory1.shift();
        if (currentHistory2.length > MAX_DATA_POINTS) currentHistory2.shift();

        const labelsC1 = currentHistory1.map(item => {
            const d = new Date(item.time * 1000);
            return d.getHours() + ':' + String(d.getMinutes()).padStart(2,'0') + ':' + String(d.getSeconds()).padStart(2,'0');
        });
        chart1.data.labels = labelsC1;
        chart1.data.datasets[0].data = currentHistory1.map(item => item.value);
        chart1.update('none');

        const labelsC2 = currentHistory2.map(item => {
            const d = new Date(item.time * 1000);
            return d.getHours() + ':' + String(d.getMinutes()).padStart(2,'0') + ':' + String(d.getSeconds()).padStart(2,'0');
        });
        chart2.data.labels = labelsC2;
        chart2.data.datasets[0].data = currentHistory2.map(item => item.value);
        chart2.update('none');

        // Карточки
        document.getElementById('pressure').textContent = pressure.toFixed(2);
        document.getElementById('current1').textContent = current1.toFixed(3);
        document.getElementById('current2').textContent = current2.toFixed(3);
        document.getElementById('temperature').textContent = data.temp ? data.temp.toFixed(1) : '—';
        document.getElementById('error').textContent = data.e;
        updateErrorDescription(data.e);

        // Настройки STM32
        if (data.pon !== undefined && data.poff !== undefined) {
            stm32Settings.pon = data.pon;
            stm32Settings.poff = data.poff;
            stm32Settings.maxTime = data.tmax;
            updateSettingsDisplay();
        }

        // ==== Отслеживание сессий (по битам статуса) ====
        const pump1State = p1;
        const pump2State = p2;

        if (pump1State && !lastPump1State) {
            pump1StartTime = now;
            pump1Currents = [];
            currentSession.pump1 = {
                id: ++sessionId,
                startTime: now,
                startTimeStr: new Date().toLocaleString(),
                currents: []
            };
            addLog(`🟢 Насос 1 ВКЛЮЧЕН (сессия #${sessionId})`, 'event');
        } else if (!pump1State && lastPump1State && currentSession.pump1) {
            const duration = now - pump1StartTime;
            const avg = pump1Currents.length ? pump1Currents.reduce((a,b) => a+b, 0) / pump1Currents.length : 0;
            const max = pump1Currents.length ? Math.max(...pump1Currents) : 0;
            currentSession.pump1.endTime = now;
            currentSession.pump1.endTimeStr = new Date().toLocaleString();
            currentSession.pump1.duration = duration;
            currentSession.pump1.avgCurrent = avg;
            currentSession.pump1.maxCurrent = max;
            sessions.push({ ...currentSession.pump1, pump: 'Насос 1', pumpClass: 'pump1-row' });
            currentSession.pump1 = null;
            updateSessionsTable();
            addLog(`🔴 Насос 1 ВЫКЛЮЧЕН (длит: ${duration}с, ср.ток: ${avg.toFixed(2)}А)`, 'event');
        }

        if (pump2State && !lastPump2State) {
            pump2StartTime = now;
            pump2Currents = [];
            currentSession.pump2 = {
                id: ++sessionId,
                startTime: now,
                startTimeStr: new Date().toLocaleString(),
                currents: []
            };
            addLog(`🟢 Насос 2 ВКЛЮЧЕН (сессия #${sessionId})`, 'event');
        } else if (!pump2State && lastPump2State && currentSession.pump2) {
            const duration = now - pump2StartTime;
            const avg = pump2Currents.length ? pump2Currents.reduce((a,b) => a+b, 0) / pump2Currents.length : 0;
            const max = pump2Currents.length ? Math.max(...pump2Currents) : 0;
            currentSession.pump2.endTime = now;
            currentSession.pump2.endTimeStr = new Date().toLocaleString();
            currentSession.pump2.duration = duration;
            currentSession.pump2.avgCurrent = avg;
            currentSession.pump2.maxCurrent = max;
            sessions.push({ ...currentSession.pump2, pump: 'Насос 2', pumpClass: 'pump2-row' });
            currentSession.pump2 = null;
            updateSessionsTable();
            addLog(`🔴 Насос 2 ВЫКЛЮЧЕН (длит: ${duration}с, ср.ток: ${avg.toFixed(2)}А)`, 'event');
        }

        if (currentSession.pump1) { pump1Currents.push(current1); currentSession.pump1.currents.push(current1); }
        if (currentSession.pump2) { pump2Currents.push(current2); currentSession.pump2.currents.push(current2); }

        lastPump1State = pump1State;
        lastPump2State = pump2State;
    }

    // ==== Расшифровка ошибок ====
    function updateErrorDescription(errorCode) {
        const el = document.getElementById('errorDescription');
        if (errorCode === 0) {
            el.textContent = '✅ Ошибок нет';
            el.style.color = '#4caf50';
            return;
        }
        const messages = [];
        if (errorCode & 0x01) messages.push('⏱ Критический таймаут насоса');
        if (errorCode & 0x02) messages.push('Ошибка датчика давления');
        if (errorCode & 0x04) messages.push('⚡ Ток насоса 1 вне нормы');
        if (errorCode & 0x08) messages.push('⚡ Ток насоса 2 вне нормы');
        if (errorCode & 0x10) messages.push('⚠️ Ошибка датчика тока');
        if (errorCode & 0x20) messages.push('🔒 Залипание насоса');
        if (messages.length === 0) messages.push('Неизвестная ошибка (код ' + errorCode + ')');
        el.innerHTML = messages.join('<br>');
        el.style.color = '#ff6b6b';
    }

    // ==== Настройки STM32 ====
    function updateSettingsDisplay() {
        document.getElementById('stm32Pon').textContent = stm32Settings.pon !== null ? stm32Settings.pon.toFixed(2) + ' атм' : '—';
        document.getElementById('stm32Poff').textContent = stm32Settings.poff !== null ? stm32Settings.poff.toFixed(2) + ' атм' : '—';
        document.getElementById('stm32MaxTime').textContent = stm32Settings.maxTime !== null ? stm32Settings.maxTime + ' сек' : '—';
        document.getElementById('stm32Status').textContent = stm32Settings.pon !== null ? '✅ Получены' : '⏳ Ожидание...';
    }

    // ==== Сессии ====
    function updateSessionsTable() {
        const tbody = document.getElementById('sessionsBody');
        const count = document.getElementById('sessionsCount');
        count.textContent = 'Всего сессий: ' + sessions.length;
        if (sessions.length === 0) {
            tbody.innerHTML = '<tr><td colspan="7" style="text-align:center;color:#666;padding:20px;">Нет данных о сессиях</td></tr>';
            return;
        }
        const display = sessions.slice(-100).reverse();
        let html = '';
        for (let s of display) {
            html += `<tr class="${s.pumpClass}">
                <td>${s.pump}</td>
                <td>#${s.id}</td>
                <td>${s.startTimeStr}</td>
                <td>${s.endTimeStr || '—'}</td>
                <td>${s.duration || 0}</td>
                <td>${s.avgCurrent ? s.avgCurrent.toFixed(2) : '—'}</td>
                <td>${s.maxCurrent ? s.maxCurrent.toFixed(2) : '—'}</td>
            </tr>`;
        }
        tbody.innerHTML = html;
    }

    function clearSessions() {
        sessions = [];
        sessionId = 0;
        currentSession = { pump1: null, pump2: null };
        pump1Currents = [];
        pump2Currents = [];
        updateSessionsTable();
        addLog('🗑 История сессий очищена', 'event');
    }

    // ==== Команды ====
    function sendCommand(cmd) {
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(cmd);
            addLog('📤 Команда: ' + cmd, 'tx');
        } else {
            addLog('❌ WebSocket не подключен', 'error');
        }
    }

    function requestSettings() {
        addLog('📥 Запрос настроек STM32...', 'event');
        if (ws.readyState === WebSocket.OPEN) {
            ws.send('get_settings');
            addLog('📤 Запрос настроек отправлен', 'tx');
        }
        document.getElementById('stm32Settings').classList.add('active');
        settingsVisible = true;
    }

    function toggleSettingsDisplay() {
        settingsVisible = !settingsVisible;
        const block = document.getElementById('stm32Settings');
        block.classList.toggle('active', settingsVisible);
    }

    function sendSettings() {
        const pmin = document.getElementById('pmin').value;
        const pmax = document.getElementById('pmax').value;
        const cmd = 'set:' + pmin + ',' + pmax;
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(cmd);
            addLog('📤 Настройки давления: ' + pmin + ' / ' + pmax, 'tx');
        }
    }

    function clearConsole() {
        document.getElementById('consoleOutput').innerHTML = '';
        logCount = 0;
        addLog('⚠️ Консоль очищена', 'warning');
    }

    function togglePause() {
        paused = !paused;
        const btn = document.querySelector('.btn-pause');
        btn.textContent = paused ? '▶ Продолжить' : '⏸ Пауза';
        addLog(paused ? '⏸ Логирование приостановлено' : '▶ Логирование возобновлено', 'event');
    }

    function applyFilter() {
        filterText = document.getElementById('filterInput').value;
        const output = document.getElementById('consoleOutput');
        const entries = output.getElementsByClassName('log-entry');
        for (let entry of entries) {
            if (filterText) {
                entry.style.display = entry.textContent.toLowerCase().includes(filterText.toLowerCase()) ? '' : 'none';
            } else {
                entry.style.display = '';
            }
        }
    }

    window.onload = function() {
        updateConnectionStatus();
        addLog('🚀 Консоль UART запущена', 'event');
        addLog('⏳ Ожидание данных от STM32...', 'event');
    };
</script>
</body>
</html>
)=====";

#endif