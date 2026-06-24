#include "NetworkManager.h"
#include "../../core/AppConfig.h"
#include "../../core/ConfigManager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

const char CONFIG_PAGE_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width,initial-scale=1'>
  <title>AdPet 控制中心</title>
  <style>
    :root {
      --bg: #090b11;
      --card-bg: rgba(17, 22, 39, 0.75);
      --border: rgba(255, 255, 255, 0.08);
      --accent: linear-gradient(135deg, #3b82f6 0%, #8b5cf6 100%);
      --accent-hover: linear-gradient(135deg, #2563eb 0%, #7c3aed 100%);
      --text: #f3f4f6;
      --text-muted: #9ca3af;
      --success: #10b981;
      --error: #ef4444;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: var(--bg);
      background-image: radial-gradient(circle at 10% 20%, rgba(59, 130, 246, 0.15) 0%, transparent 40%),
                        radial-gradient(circle at 90% 80%, rgba(139, 92, 246, 0.12) 0%, transparent 45%);
      color: var(--text);
      min-height: 100vh;
      padding: 20px;
      display: flex;
      justify-content: center;
      align-items: center;
    }
    main {
      width: 100%;
      max-width: 680px;
      margin: auto;
    }
    .header {
      text-align: center;
      margin-bottom: 24px;
    }
    .header h1 {
      font-size: 2.2rem;
      font-weight: 800;
      background: linear-gradient(to right, #60a5fa, #c084fc);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      margin-bottom: 6px;
      letter-spacing: -0.5px;
    }
    .header p {
      color: var(--text-muted);
      font-size: 0.95rem;
    }
    .card {
      background: var(--card-bg);
      backdrop-filter: blur(20px);
      -webkit-backdrop-filter: blur(20px);
      border: 1px solid var(--border);
      border-radius: 16px;
      padding: 24px;
      margin-bottom: 20px;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.5);
    }
    h2 {
      font-size: 1.25rem;
      font-weight: 700;
      margin-bottom: 18px;
      display: flex;
      align-items: center;
      gap: 8px;
      color: #60a5fa;
      border-bottom: 1px solid rgba(255, 255, 255, 0.05);
      padding-bottom: 8px;
    }
    label {
      display: block;
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--text-muted);
      margin-top: 14px;
      margin-bottom: 6px;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .input-group {
      position: relative;
      display: flex;
      gap: 8px;
    }
    input, textarea, select {
      width: 100%;
      padding: 11px 14px;
      background: rgba(255, 255, 255, 0.03);
      border: 1px solid var(--border);
      border-radius: 8px;
      color: var(--text);
      font-size: 0.95rem;
      transition: all 0.25s ease;
    }
    input:focus, textarea:focus, select:focus {
      outline: none;
      border-color: #60a5fa;
      background: rgba(255, 255, 255, 0.06);
      box-shadow: 0 0 0 3px rgba(96, 165, 250, 0.15);
    }
    textarea {
      min-height: 80px;
      resize: vertical;
      line-height: 1.4;
    }
    .btn {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 6px;
      padding: 11px 18px;
      border: 0;
      border-radius: 8px;
      font-size: 0.95rem;
      font-weight: 700;
      cursor: pointer;
      transition: all 0.2s ease;
      color: white;
    }
    .btn-primary {
      background: var(--accent);
    }
    .btn-primary:hover {
      background: var(--accent-hover);
      transform: translateY(-1px);
    }
    .btn-secondary {
      background: rgba(255, 255, 255, 0.08);
      border: 1px solid var(--border);
    }
    .btn-secondary:hover {
      background: rgba(255, 255, 255, 0.15);
    }
    .btn-test {
      padding: 8px 12px;
      font-size: 0.85rem;
      border-radius: 6px;
      background: rgba(59, 130, 246, 0.15);
      border: 1px solid rgba(59, 130, 246, 0.3);
      color: #93c5fd;
      margin-top: 10px;
    }
    .btn-test:hover {
      background: rgba(59, 130, 246, 0.25);
    }
    .grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
    }
    .flex-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-top: 18px;
    }
    .toast {
      position: fixed;
      top: 24px;
      left: 50%;
      transform: translateX(-50%) translateY(-20px);
      padding: 12px 24px;
      border-radius: 12px;
      font-weight: 700;
      font-size: 0.95rem;
      display: flex;
      align-items: center;
      gap: 8px;
      opacity: 0;
      visibility: hidden;
      transition: all 0.3s cubic-bezier(0.68, -0.55, 0.27, 1.55);
      z-index: 1000;
      box-shadow: 0 10px 25px rgba(0, 0, 0, 0.3);
    }
    .toast.show {
      opacity: 1;
      visibility: visible;
      transform: translateX(-50%) translateY(0);
    }
    .toast-success {
      background: linear-gradient(135deg, #059669 0%, #10b981 100%);
      color: white;
    }
    .toast-error {
      background: linear-gradient(135deg, #dc2626 0%, #ef4444 100%);
      color: white;
    }
    .status-badge {
      display: inline-flex;
      align-items: center;
      gap: 5px;
      font-size: 0.8rem;
      font-weight: 700;
      padding: 4px 8px;
      border-radius: 20px;
      background: rgba(239, 68, 68, 0.15);
      color: var(--error);
    }
    .status-badge.connected {
      background: rgba(16, 185, 129, 0.15);
      color: var(--success);
    }
    .reply {
      white-space: pre-wrap;
      background: rgba(0, 0, 0, 0.2);
      color: #93c5fd;
      border-radius: 8px;
      padding: 12px;
      font-family: monospace;
      font-size: 0.9rem;
      border: 1px solid rgba(255, 255, 255, 0.03);
      margin-top: 8px;
      max-height: 150px;
      overflow-y: auto;
    }
    .loading-spinner {
      width: 14px;
      height: 14px;
      border: 2px solid rgba(255, 255, 255, 0.3);
      border-radius: 50%;
      border-top-color: white;
      animation: spin 0.8s linear infinite;
      display: inline-block;
      vertical-align: middle;
    }
    @keyframes spin { to { transform: rotate(360deg); } }
  </style>
</head>
<body>
  <div id="toast" class="toast"></div>
  <main>
    <div class="header">
      <h1>AdPet Portal</h1>
      <p>智能桌面电子宠物配置中心</p>
    </div>

    <div class="card">
      <h2>🌐 设备网络状态
        <span id="statusBadge" class="status-badge">未连接</span>
      </h2>
      <div style="font-size:0.9rem; color: var(--text-muted); display:grid; grid-template-columns:1fr 1fr; gap:8px; margin-top:8px;">
        <div>AP 模式网关IP: <span id="apIp" style="color:var(--text); font-family:monospace;">-</span></div>
        <div>局域网分配IP: <span id="staIp" style="color:var(--text); font-family:monospace;">-</span></div>
      </div>
    </div>

    <div class="card">
      <form id="configForm">
        <h2>📶 Wi-Fi 配置</h2>
        
        <label>附近 Wi-Fi 信号</label>
        <div class="input-group">
          <select id="wifiSelect">
            <option value="">点击右侧按钮扫描...</option>
          </select>
          <button type="button" id="btnScan" class="btn btn-secondary">
            <span id="scanText">🔄 扫描</span>
          </button>
        </div>

        <label>Wi-Fi SSID</label>
        <input name="wifiSsid" id="wifiSsid" value="{{WIFI_SSID}}" placeholder="选择或手动输入 SSID">

        <label>Wi-Fi 密码</label>
        <input name="wifiPassword" id="wifiPassword" type="password" value="{{WIFI_PASSWORD}}" placeholder="请输入密码">
        
        <button type="button" id="btnTestWifi" class="btn btn-test">⚡ 测试 Wi-Fi 连接</button>

        <h2 style="margin-top:24px;">🤖 智能网关连接</h2>
        
        <label>网关地址 (Base URL)</label>
        <input name="gatewayBaseUrl" id="gatewayBaseUrl" value="{{GATEWAY_URL}}" placeholder="如 https://pet.adnaan.site/">

        <label>网关 API Key</label>
        <input name="gatewayApiKey" id="gatewayApiKey" type="password" value="{{GATEWAY_KEY}}" placeholder="网关授权 Bearer Token（留空则不验证）">
        
        <button type="button" id="btnTestGateway" class="btn btn-test">⚡ 测试网关连接</button>

        <label>设备标识 (Device ID)</label>
        <input name="deviceId" id="deviceId" value="{{DEVICE_ID}}">

        <label>系统提示人设 (System Prompt)</label>
        <textarea name="systemPrompt" id="systemPrompt">{{SYSTEM_PROMPT}}</textarea>

        <h2 style="margin-top:24px;">🎙️ 语音与情感配置</h2>
        <div class="grid">
          <div>
            <label>录音时长 (ms)</label>
            <input name="recordMs" id="recordMs" type="number" value="{{RECORD_MS}}">
          </div>
          <div>
            <label>触发阈值 (RMS)</label>
            <input name="triggerRms" id="triggerRms" type="number" value="{{TRIGGER_RMS}}">
          </div>
        </div>

        <div class="grid">
          <div>
            <label>空闲表情</label>
            <input name="idleEmotion" id="idleEmotion" value="{{IDLE_EMOTION}}">
          </div>
          <div>
            <label>触发表情</label>
            <input name="triggerEmotion" id="triggerEmotion" value="{{TRIGGER_EMOTION}}">
          </div>
        </div>

        <div class="flex-row">
          <button type="submit" class="btn btn-primary" style="width:100%; font-size:1.05rem; padding:12px; margin-top:20px;">💾 保存配置并重新连接</button>
        </div>
      </form>
    </div>

    <div class="card">
      <h2>💬 文本对话测试</h2>
      <form id="chatForm">
        <label>消息内容</label>
        <div class="input-group">
          <input name="message" id="chatMsg" placeholder="给 AdPet 发送一句话...">
          <button type="submit" class="btn btn-primary">发送</button>
        </div>
      </form>
      <label>最后回复内容</label>
      <div id="replyArea" class="reply">{{LAST_REPLY}}</div>
    </div>
  </main>

  <script>
    const $ = id => document.getElementById(id);
    let toastTimeout;
    
    function showToast(text, isError = false) {
      const t = $('toast');
      t.innerText = text;
      t.className = 'toast show ' + (isError ? 'toast-error' : 'toast-success');
      clearTimeout(toastTimeout);
      toastTimeout = setTimeout(() => { t.className = 'toast'; }, 4000);
    }

    // 状态轮询
    async function updateStatus() {
      try {
        const r = await fetch('/api/status');
        const d = await r.json();
        $('apIp').innerText = d.apIp || '192.168.4.1';
        $('staIp').innerText = d.staIp && d.staIp !== '0.0.0.0' ? d.staIp : '未分配';
        
        const badge = $('statusBadge');
        if (d.connected) {
          badge.innerText = '已连接';
          badge.className = 'status-badge connected';
        } else {
          badge.innerText = '未连接';
          badge.className = 'status-badge';
        }
      } catch (e) {
        console.error("无法获取状态", e);
      }
    }

    // 扫描 WiFi
    $('btnScan').onclick = async () => {
      const btn = $('btnScan');
      const text = $('scanText');
      btn.disabled = true;
      text.innerHTML = '<span class="loading-spinner"></span> 扫描中...';
      
      try {
        const r = await fetch('/api/wifi-list');
        const list = await r.json();
        
        const select = $('wifiSelect');
        select.innerHTML = '<option value="">-- 请选择附近的 Wi-Fi --</option>';
        
        if (!list || list.length === 0) {
          select.innerHTML = '<option value="">未扫描到信号，请重试</option>';
          showToast("未发现周围的 Wi-Fi 信号", true);
        } else {
          list.forEach(w => {
            const opt = document.createElement('option');
            opt.value = w.ssid;
            opt.innerText = `${w.ssid} (${w.rssi} dBm) ${w.secure ? '🔒' : '🔓'}`;
            select.appendChild(opt);
          });
          showToast(`已成功扫描到 ${list.length} 个 Wi-Fi 信号`);
        }
      } catch (e) {
        showToast("扫描失败，请重试", true);
      } finally {
        btn.disabled = false;
        text.innerText = '🔄 扫描';
      }
    };

    $('wifiSelect').onchange = (e) => {
      const ssid = e.target.value;
      if (ssid) {
        $('wifiSsid').value = ssid;
        $('wifiPassword').focus();
      }
    };

    // 测试 Wi-Fi 连接
    $('btnTestWifi').onclick = async () => {
      const btn = $('btnTestWifi');
      const ssid = $('wifiSsid').value;
      const password = $('wifiPassword').value;
      
      if (!ssid) {
        showToast("请输入 Wi-Fi SSID", true);
        return;
      }
      
      btn.disabled = true;
      btn.innerHTML = '<span class="loading-spinner"></span> 正在连接测试...';
      
      try {
        const formData = new URLSearchParams();
        formData.append('ssid', ssid);
        formData.append('password', password);
        
        const r = await fetch('/api/test-wifi', {
          method: 'POST',
          body: formData
        });
        const res = await r.json();
        
        if (res.status === 'ok') {
          showToast(res.message);
          updateStatus();
        } else {
          showToast(res.message, true);
        }
      } catch (e) {
        showToast("测试连接失败，设备响应超时", true);
      } finally {
        btn.disabled = false;
        btn.innerText = "⚡ 测试 Wi-Fi 连接";
      }
    };

    // 测试网关连接
    $('btnTestGateway').onclick = async () => {
      const btn = $('btnTestGateway');
      const url = $('gatewayBaseUrl').value;
      const key = $('gatewayApiKey').value;
      
      if (!url) {
        showToast("请输入网关地址", true);
        return;
      }
      
      btn.disabled = true;
      btn.innerHTML = '<span class="loading-spinner"></span> 正在测试网关...';
      
      try {
        const formData = new URLSearchParams();
        formData.append('url', url);
        formData.append('key', key);
        
        const r = await fetch('/api/test-gateway', {
          method: 'POST',
          body: formData
        });
        const res = await r.json();
        
        if (res.status === 'ok') {
          showToast(res.message);
        } else {
          showToast(res.message, true);
        }
      } catch (e) {
        showToast("测试连接网关失败，请检查网关地址或网络环境", true);
      } finally {
        btn.disabled = false;
        btn.innerText = "⚡ 测试网关连接";
      }
    };

    // 保存配置
    $('configForm').onsubmit = async (e) => {
      e.preventDefault();
      const form = e.target;
      const formData = new FormData(form);
      const urlEncoded = new URLSearchParams(formData);
      
      try {
        const r = await fetch('/save', {
          method: 'POST',
          body: urlEncoded
        });
        const res = await r.json();
        showToast(res.message || "配置已成功保存！");
        setTimeout(updateStatus, 3000);
      } catch (e) {
        showToast("保存失败，设备无响应", true);
      }
    };

    // 聊天表单
    $('chatForm').onsubmit = async (e) => {
      e.preventDefault();
      const msgInput = $('chatMsg');
      const text = msgInput.value;
      if (!text) return;
      
      const replyArea = $('replyArea');
      replyArea.innerText = "⏳ 等待回复中...";
      
      try {
        const formData = new URLSearchParams();
        formData.append('message', text);
        
        await fetch('/chat', {
          method: 'POST',
          body: formData
        });
        
        showToast("发送聊天测试成功！请稍后刷新获取最后回复。");
        msgInput.value = "";
        
      } catch (e) {
        replyArea.innerText = "发送失败。";
        showToast("发送聊天测试失败", true);
      }
    };

    async function init() {
      updateStatus();
      setInterval(updateStatus, 5000);
    }
    
    window.onload = init;
  </script>
</body>
</html>
)rawliteral";

void AdPetNetworkManager::begin() {
  if (!AppConfig::Feature::NETWORK_ENABLED) {
    Serial.println("[Network] disabled");
    return;
  }

  WiFi.mode(WIFI_AP_STA);
  startAccessPoint();
  connectStation();
  setupRoutes();
  _server.begin();
  Serial.println("[Network] web config server started");
}

void AdPetNetworkManager::update() {
  if (AppConfig::Feature::NETWORK_ENABLED) {
    _server.handleClient();
  }
}

bool AdPetNetworkManager::isConnected() const {
  return _connected;
}

bool AdPetNetworkManager::hasChatRequest() const {
  return _pendingChat.length() > 0;
}

String AdPetNetworkManager::takeChatRequest() {
  String text = _pendingChat;
  _pendingChat = "";
  return text;
}

void AdPetNetworkManager::setLastReply(const String& reply) {
  _lastReply = reply;
}

void AdPetNetworkManager::startAccessPoint() {
  WiFi.softAP("AdPet-Setup", "adpet1234");
  Serial.print("[Network] setup AP: http://");
  Serial.println(WiFi.softAPIP());
}

void AdPetNetworkManager::connectStation() {
  const RuntimeConfig& config = AppConfigStore.get();
  if (config.wifiSsid.length() == 0) {
    Serial.println("[Network] Wi-Fi SSID empty, AP-only mode");
    _connected = false;
    return;
  }

  Serial.print("[Network] connecting Wi-Fi: ");
  Serial.println(config.wifiSsid);
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 8000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  _connected = WiFi.status() == WL_CONNECTED;
  if (_connected) {
    Serial.print("[Network] STA IP: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[Network] Wi-Fi connect failed, AP still available");
  }
}

void AdPetNetworkManager::setupRoutes() {
  _server.on("/", HTTP_GET, [this]() { handleRoot(); });
  _server.on("/save", HTTP_POST, [this]() { handleSave(); });
  _server.on("/chat", HTTP_POST, [this]() { handleChat(); });
  _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  _server.on("/api/wifi-list", HTTP_GET, [this]() { handleWifiList(); });
  _server.on("/api/test-gateway", HTTP_POST, [this]() { handleTestGateway(); });
  _server.on("/api/test-wifi", HTTP_POST, [this]() { handleTestWifi(); });
}

void AdPetNetworkManager::handleRoot() {
  const RuntimeConfig& c = AppConfigStore.get();
  String page = FPSTR(CONFIG_PAGE_HTML);
  page.replace("{{WIFI_SSID}}", htmlEscape(c.wifiSsid));
  page.replace("{{WIFI_PASSWORD}}", htmlEscape(c.wifiPassword));
  page.replace("{{GATEWAY_URL}}", htmlEscape(c.gatewayBaseUrl));
  page.replace("{{GATEWAY_KEY}}", htmlEscape(c.gatewayApiKey));
  page.replace("{{DEVICE_ID}}", htmlEscape(c.deviceId));
  page.replace("{{SYSTEM_PROMPT}}", htmlEscape(c.systemPrompt));
  page.replace("{{RECORD_MS}}", String(c.recordMs));
  page.replace("{{TRIGGER_RMS}}", String(c.triggerRms));
  page.replace("{{IDLE_EMOTION}}", htmlEscape(c.idleEmotion));
  page.replace("{{TRIGGER_EMOTION}}", htmlEscape(c.triggerEmotion));
  
  String lastReplyText = _lastReply.length() > 0 ? htmlEscape(_lastReply) : "暂无对话记录";
  page.replace("{{LAST_REPLY}}", lastReplyText);

  _server.send(200, "text/html; charset=utf-8", page);
}

void AdPetNetworkManager::handleSave() {
  RuntimeConfig& c = AppConfigStore.edit();
  c.wifiSsid = _server.arg("wifiSsid");
  c.wifiPassword = _server.arg("wifiPassword");
  c.gatewayBaseUrl = _server.arg("gatewayBaseUrl");
  c.gatewayApiKey = _server.arg("gatewayApiKey");
  c.deviceId = _server.arg("deviceId");
  c.systemPrompt = _server.arg("systemPrompt");
  c.idleEmotion = _server.arg("idleEmotion");
  c.triggerEmotion = _server.arg("triggerEmotion");
  c.replyEmotion = _server.arg("replyEmotion");
  
  long recordMs = _server.arg("recordMs").toInt();
  long triggerRms = _server.arg("triggerRms").toInt();
  long triggerPeak = _server.arg("triggerPeak").toInt();

  if (recordMs < 1000) recordMs = 1000;
  if (recordMs > 5000) recordMs = 5000;
  if (triggerRms < 1) triggerRms = 1;
  if (triggerPeak < 1) triggerPeak = 1;

  c.recordMs = (uint16_t)recordMs;
  c.triggerRms = (uint32_t)triggerRms;
  c.triggerPeak = (uint32_t)triggerPeak;
  
  AppConfigStore.save();
  Serial.println("[Network] config saved, re-connecting...");
  
  _server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"message\":\"配置保存成功！正在重新连接 Wi-Fi...\"}");
  
  // 保存配置后异步连接 Station，无需整机重启
  connectStation();
}

void AdPetNetworkManager::handleChat() {
  _pendingChat = _server.arg("message");
  Serial.print("[Network] chat request: ");
  Serial.println(_pendingChat);
  _server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"message\":\"测试消息已提交，AdPet 正在思考...\"}");
}

void AdPetNetworkManager::handleStatus() {
  String json = "{\"connected\":";
  json += _connected ? "true" : "false";
  json += ",\"apIp\":\"";
  json += WiFi.softAPIP().toString();
  json += "\",\"staIp\":\"";
  json += WiFi.localIP().toString();
  json += "\"}";
  _server.send(200, "application/json", json);
}

void AdPetNetworkManager::handleWifiList() {
  Serial.println("[Network] API scan wifi");
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  json += "]";
  WiFi.scanDelete();
  _server.send(200, "application/json; charset=utf-8", json);
}

void AdPetNetworkManager::handleTestGateway() {
  String url = _server.arg("url");
  String key = _server.arg("key");
  Serial.print("[Network] API test gateway: ");
  Serial.println(url);
  
  if (url.length() == 0) {
    _server.send(200, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"网关地址不能为空\"}");
    return;
  }
  
  HTTPClient http;
  WiFiClientSecure secureClient;
  String testUrl = url;
  if (!testUrl.endsWith("/")) testUrl += "/";
  testUrl += "adpet/health";
  
  if (testUrl.startsWith("https://")) {
    secureClient.setInsecure();
    http.begin(secureClient, testUrl);
  } else {
    http.begin(testUrl);
  }
  
  if (key.length() > 0) {
    http.addHeader("Authorization", "Bearer " + key);
  }
  
  int code = http.GET();
  http.end();
  
  if (code == 200) {
    _server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"message\":\"网关测试连接成功！\"}");
  } else if (code == 401) {
    _server.send(200, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"网关返回401：API Key 校验不通过\"}");
  } else {
    _server.send(200, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"网关连接失败，HTTP 状态码: " + String(code) + "\"}");
  }
}

void AdPetNetworkManager::handleTestWifi() {
  String ssid = _server.arg("ssid");
  String pass = _server.arg("password");
  Serial.print("[Network] API test wifi SSID: ");
  Serial.println(ssid);
  
  if (ssid.length() == 0) {
    _server.send(200, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"SSID不能为空\"}");
    return;
  }
  
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), pass.c_str());
  
  unsigned long startMs = millis();
  bool ok = false;
  while (millis() - startMs < 8000) {
    if (WiFi.status() == WL_CONNECTED) {
      ok = true;
      break;
    }
    delay(200);
  }
  
  if (ok) {
    String ip = WiFi.localIP().toString();
    _connected = true;
    _server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"message\":\"Wi-Fi 测试连接成功！IP地址: " + ip + "\"}");
  } else {
    connectStation(); // 重新连回原来的配置 Wi-Fi 确保不掉线
    _server.send(200, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"Wi-Fi连接超时，请检查密码。\"}");
  }
}

String AdPetNetworkManager::htmlEscape(const String& value) const {
  String out;
  out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); i++) {
    char ch = value[i];
    if (ch == '&') out += "&amp;";
    else if (ch == '<') out += "&lt;";
    else if (ch == '>') out += "&gt;";
    else if (ch == '"') out += "&quot;";
    else out += ch;
  }
  return out;
}
