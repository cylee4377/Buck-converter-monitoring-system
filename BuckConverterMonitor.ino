/*
 * Buck Converter Efficiency Monitor
 * ESP32-S3 + INA260 x2 (Wire/Wire1) + WebSocket + Chart.js
 *
 * 하드웨어 연결:
 *   INA260 #1 (입력측): SDA=GPIO8,  SCL=GPIO9  → Wire  → 0x40
 *   INA260 #2 (출력측): SDA=GPIO10, SCL=GPIO11 → Wire1 → 0x40
 *
 * 필요 라이브러리 (Arduino Library Manager):
 *   - Adafruit INA260
 *   - Adafruit BusIO
 *   - ESPAsyncWebServer  (me-no-dev/ESPAsyncWebServer)
 *   - AsyncTCP           (me-no-dev/AsyncTCP)
 */

#include <Wire.h>
#include <Adafruit_INA260.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

// ──────────────────────────────────────────
//  사용자 설정
// ──────────────────────────────────────────
const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";

#define SDA_0   8    // Wire  (INA260 #1 입력측)
#define SCL_0   9
#define SDA_1  10    // Wire1 (INA260 #2 출력측)
#define SCL_1  11

#define MEASURE_INTERVAL_MS  1000   // 측정 주기 (ms)

// ──────────────────────────────────────────
//  객체 선언
// ──────────────────────────────────────────
Adafruit_INA260 ina_in;   // 입력측 센서
Adafruit_INA260 ina_out;  // 출력측 센서

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

unsigned long lastMeasure = 0;

// ──────────────────────────────────────────
//  HTML / JS (PROGMEM에 저장)
// ──────────────────────────────────────────
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ko">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Buck Converter Monitor</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<style>
  :root {
    --bg:      #0f1117;
    --surface: #1a1d27;
    --border:  #2a2d3e;
    --text:    #e2e8f0;
    --muted:   #64748b;
    --green:   #22d3a5;
    --blue:    #60a5fa;
    --amber:   #fbbf24;
    --red:     #f87171;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Segoe UI', system-ui, sans-serif;
    min-height: 100vh;
    padding: 24px 16px;
  }
  h1 {
    font-size: 1.25rem;
    font-weight: 600;
    letter-spacing: .02em;
    margin-bottom: 24px;
    display: flex;
    align-items: center;
    gap: 10px;
  }
  h1 span.dot {
    width: 10px; height: 10px;
    border-radius: 50%;
    background: var(--red);
    display: inline-block;
    transition: background .3s;
  }
  h1 span.dot.connected { background: var(--green); }

  .cards {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
    gap: 14px;
    margin-bottom: 28px;
  }
  .card {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 18px 20px;
  }
  .card .label {
    font-size: .72rem;
    color: var(--muted);
    text-transform: uppercase;
    letter-spacing: .08em;
    margin-bottom: 8px;
  }
  .card .value {
    font-size: 2rem;
    font-weight: 700;
    line-height: 1;
  }
  .card .unit {
    font-size: .85rem;
    color: var(--muted);
    margin-left: 4px;
  }
  .card.eta  .value { color: var(--green); }
  .card.pin  .value { color: var(--blue); }
  .card.pout .value { color: var(--amber); }
  .card.loss .value { color: var(--red); }

  .chart-wrap {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 20px;
    margin-bottom: 20px;
  }
  .chart-wrap h2 {
    font-size: .85rem;
    color: var(--muted);
    text-transform: uppercase;
    letter-spacing: .08em;
    margin-bottom: 16px;
  }
  canvas { width: 100% !important; }

  .row2 {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 14px;
  }
  @media (max-width: 500px) { .row2 { grid-template-columns: 1fr; } }

  footer {
    text-align: center;
    font-size: .72rem;
    color: var(--muted);
    margin-top: 24px;
  }
</style>
</head>
<body>

<h1>
  <span class="dot" id="dot"></span>
  Buck Converter Monitor
</h1>

<div class="cards">
  <div class="card eta">
    <div class="label">효율 η</div>
    <div class="value" id="eta">—<span class="unit">%</span></div>
  </div>
  <div class="card pin">
    <div class="label">입력 전력 Pin</div>
    <div class="value" id="pin">—<span class="unit">W</span></div>
  </div>
  <div class="card pout">
    <div class="label">출력 전력 Pout</div>
    <div class="value" id="pout">—<span class="unit">W</span></div>
  </div>
  <div class="card loss">
    <div class="label">손실 전력</div>
    <div class="value" id="loss">—<span class="unit">W</span></div>
  </div>
</div>

<div class="chart-wrap">
  <h2>실시간 효율 (%)</h2>
  <canvas id="etaChart" height="120"></canvas>
</div>

<div class="row2">
  <div class="chart-wrap">
    <h2>전압 (V)</h2>
    <canvas id="vChart" height="140"></canvas>
  </div>
  <div class="chart-wrap">
    <h2>전류 (A)</h2>
    <canvas id="iChart" height="140"></canvas>
  </div>
</div>

<footer>ESP32-S3 &nbsp;·&nbsp; INA260 x2 &nbsp;·&nbsp; WebSocket</footer>

<script>
const MAX_POINTS = 60;

function makeChart(id, datasets, yLabel) {
  return new Chart(document.getElementById(id), {
    type: 'line',
    data: { labels: [], datasets },
    options: {
      animation: false,
      responsive: true,
      plugins: { legend: { labels: { color: '#94a3b8', font: { size: 11 } } } },
      scales: {
        x: { ticks: { color: '#475569', maxTicksLimit: 6 }, grid: { color: '#1e2433' } },
        y: { ticks: { color: '#94a3b8' }, grid: { color: '#1e2433' }, title: { display: false } }
      }
    }
  });
}

const etaChart = makeChart('etaChart', [{
  label: 'η (%)', data: [],
  borderColor: '#22d3a5', backgroundColor: 'rgba(34,211,165,.08)',
  borderWidth: 2, pointRadius: 0, fill: true, tension: .3
}]);

const vChart = makeChart('vChart', [
  { label: 'Vin',  data: [], borderColor: '#60a5fa', backgroundColor: 'transparent', borderWidth: 1.5, pointRadius: 0, tension: .3 },
  { label: 'Vout', data: [], borderColor: '#fbbf24', backgroundColor: 'transparent', borderWidth: 1.5, pointRadius: 0, tension: .3 }
]);

const iChart = makeChart('iChart', [
  { label: 'Iin',  data: [], borderColor: '#60a5fa', backgroundColor: 'transparent', borderWidth: 1.5, pointRadius: 0, tension: .3 },
  { label: 'Iout', data: [], borderColor: '#fbbf24', backgroundColor: 'transparent', borderWidth: 1.5, pointRadius: 0, tension: .3 }
]);

function pushData(chart, label, ...values) {
  chart.data.labels.push(label);
  values.forEach((v, i) => chart.data.datasets[i].data.push(v));
  if (chart.data.labels.length > MAX_POINTS) {
    chart.data.labels.shift();
    chart.data.datasets.forEach(d => d.data.shift());
  }
  chart.update('none');
}

function fmt(v, d=2) { return (typeof v === 'number') ? v.toFixed(d) : '—'; }

// WebSocket
const dot  = document.getElementById('dot');
let ws;

function connect() {
  ws = new WebSocket('ws://' + location.host + '/ws');

  ws.onopen = () => dot.classList.add('connected');
  ws.onclose = () => {
    dot.classList.remove('connected');
    setTimeout(connect, 2000);
  };

  ws.onmessage = (e) => {
    const d = JSON.parse(e.data);
    const now = new Date().toLocaleTimeString('ko-KR', { hour12: false });

    document.getElementById('eta').innerHTML  = fmt(d.eta, 1) + '<span class="unit">%</span>';
    document.getElementById('pin').innerHTML  = fmt(d.pin, 3) + '<span class="unit">W</span>';
    document.getElementById('pout').innerHTML = fmt(d.pout, 3) + '<span class="unit">W</span>';
    document.getElementById('loss').innerHTML = fmt(d.loss, 3) + '<span class="unit">W</span>';

    pushData(etaChart, now, d.eta);
    pushData(vChart,   now, d.vin, d.vout);
    pushData(iChart,   now, d.iin, d.iout);
  };
}

connect();
</script>
</body>
</html>
)rawliteral";

// ──────────────────────────────────────────
//  WebSocket 이벤트 핸들러
// ──────────────────────────────────────────
void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] 클라이언트 접속: #%u  IP: %s\n",
                  client->id(), client->remoteIP().toString().c_str());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] 클라이언트 종료: #%u\n", client->id());
  }
}

// ──────────────────────────────────────────
//  측정 → JSON 브로드캐스트
// ──────────────────────────────────────────
void measureAndSend() {
  // mV → V,  mA → A,  mW → W
  float vin  = ina_in.readBusVoltage()  / 1000.0f;
  float iin  = ina_in.readCurrent()     / 1000.0f;
  float pin  = ina_in.readPower()       / 1000.0f;

  float vout = ina_out.readBusVoltage() / 1000.0f;
  float iout = ina_out.readCurrent()    / 1000.0f;
  float pout = ina_out.readPower()      / 1000.0f;

  float loss = pin - pout;
  float eta  = (pin > 0.001f) ? (pout / pin * 100.0f) : 0.0f;
  eta = constrain(eta, 0.0f, 100.0f);

  // Serial 디버그
  Serial.printf("Vin=%.3fV  Iin=%.4fA  Pin=%.3fW | "
                "Vout=%.3fV  Iout=%.4fA  Pout=%.3fW | "
                "η=%.1f%%  Loss=%.3fW\n",
                vin, iin, pin, vout, iout, pout, eta, loss);

  // JSON 직렬화 (라이브러리 없이 snprintf)
  char buf[200];
  snprintf(buf, sizeof(buf),
    "{\"vin\":%.3f,\"iin\":%.4f,\"pin\":%.4f,"
     "\"vout\":%.3f,\"iout\":%.4f,\"pout\":%.4f,"
     "\"eta\":%.2f,\"loss\":%.4f}",
    vin, iin, pin, vout, iout, pout, eta, loss);

  ws.textAll(buf);
}

// ──────────────────────────────────────────
//  setup()
// ──────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Buck Converter Monitor ===");

  // I2C 초기화
  Wire.begin(SDA_0, SCL_0);
  Wire1.begin(SDA_1, SCL_1);

  // INA260 초기화
  if (!ina_in.begin(INA260_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("[ERROR] INA260 #1 (입력) 감지 실패! 배선 확인.");
    while (1) delay(10);
  }
  Serial.println("[OK] INA260 #1 (입력측) 초기화 완료");

  if (!ina_out.begin(INA260_I2CADDR_DEFAULT, &Wire1)) {
    Serial.println("[ERROR] INA260 #2 (출력) 감지 실패! 배선 확인.");
    while (1) delay(10);
  }
  Serial.println("[OK] INA260 #2 (출력측) 초기화 완료");

  // 측정 모드 설정 (연속 측정, 평균 16회)
  ina_in.setAveragingCount(INA260_COUNT_16);
  ina_in.setVoltageConversionTime(INA260_TIME_1_1_ms);
  ina_in.setCurrentConversionTime(INA260_TIME_1_1_ms);

  ina_out.setAveragingCount(INA260_COUNT_16);
  ina_out.setVoltageConversionTime(INA260_TIME_1_1_ms);
  ina_out.setCurrentConversionTime(INA260_TIME_1_1_ms);

  // WiFi 연결
  Serial.printf("[WiFi] %s 연결 중", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.printf("\n[WiFi] 연결 완료! IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("       브라우저에서 http://%s 접속\n", WiFi.localIP().toString().c_str());

  // WebSocket 등록
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // HTTP 라우트
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", INDEX_HTML);
  });

  server.begin();
  Serial.println("[HTTP] 웹서버 시작");
}

// ──────────────────────────────────────────
//  loop()
// ──────────────────────────────────────────
void loop() {
  unsigned long now = millis();
  if (now - lastMeasure >= MEASURE_INTERVAL_MS) {
    lastMeasure = now;
    measureAndSend();
  }
  ws.cleanupClients();  // 끊어진 클라이언트 정리
}
