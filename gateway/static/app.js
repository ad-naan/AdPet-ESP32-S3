/**
 * AdPet Gateway 管理界面 — 前端逻辑
 */

let selectedDeviceId = null;

// ─── 工具函数 ───

function $(sel) { return document.querySelector(sel); }
function $$(sel) { return document.querySelectorAll(sel); }

function toast(msg, type = 'success') {
  const el = document.createElement('div');
  el.className = `toast ${type}`;
  el.textContent = msg;
  $('#toastContainer').appendChild(el);
  setTimeout(() => el.remove(), 3000);
}

async function api(method, url, body) {
  const opts = { method, headers: {} };
  if (body) {
    opts.headers['Content-Type'] = 'application/json';
    opts.body = JSON.stringify(body);
  }
  const res = await fetch(url, opts);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}

function fmtTime(iso) {
  if (!iso) return '-';
  const d = new Date(iso);
  return d.toLocaleString('zh-CN', { month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit' });
}

// ─── 标签切换 ───

$$('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    $$('.tab-btn').forEach(b => b.classList.remove('active'));
    $$('.tab-panel').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    $(`#panel-${btn.dataset.tab}`).classList.add('active');
    if (btn.dataset.tab === 'config') loadConfig();
  });
});

// ─── 设备管理 ───

async function loadDevices() {
  try {
    const devices = await api('GET', '/api/devices');
    renderDevices(devices);
    // 更新统计
    $('#statDevices').textContent = devices.length;
    const totalMsgs = devices.reduce((s, d) => s + (d.message_count || 0), 0);
    $('#statMessages').textContent = totalMsgs;
    $('#statUptime').textContent = '正常';
  } catch (e) {
    console.error(e);
    toast('加载设备失败', 'error');
  }
}

function renderDevices(devices) {
  const list = $('#deviceList');
  if (!devices.length) {
    list.innerHTML = '<div class="empty-state"><div class="empty-state-icon">📡</div><div class="empty-state-title">暂无设备</div><p>ESP32 首次请求后自动注册</p></div>';
    return;
  }
  list.innerHTML = devices.map(d => `
    <div class="device-item ${d.device_id === selectedDeviceId ? 'selected' : ''}"
         onclick="selectDevice('${d.device_id}')">
      <div class="device-avatar">🤖</div>
      <div class="device-info">
        <div class="device-name">${d.alias || d.device_id}</div>
        <div class="device-meta">
          <span>💬 ${d.message_count || 0}</span>
          <span>📅 ${fmtTime(d.last_active)}</span>
        </div>
      </div>
    </div>
  `).join('');
}

async function selectDevice(deviceId) {
  selectedDeviceId = deviceId;
  // 更新选中状态
  $$('.device-item').forEach(el => {
    el.classList.toggle('selected', el.onclick.toString().includes(deviceId));
  });
  $('#chatTitle').textContent = deviceId;
  $('#chatSubtitle').textContent = '对话历史';
  $('#btnClear').style.display = '';
  $('#btnDelete').style.display = '';

  try {
    const msgs = await api('GET', `/api/devices/${deviceId}/messages`);
    renderMessages(msgs);
  } catch (e) {
    toast('加载对话失败', 'error');
  }
}

function renderMessages(msgs) {
  const container = $('#chatMessages');
  if (!msgs.length) {
    container.innerHTML = '<div class="chat-empty"><div class="chat-empty-icon">💬</div><p>暂无对话记录</p></div>';
    return;
  }
  container.innerHTML = msgs.map(m => `
    <div class="msg ${m.role}">
      <div class="msg-role">${m.role === 'user' ? '👤 用户' : '🐾 AdPet'}</div>
      <div>${escHtml(m.content)}</div>
      <div class="msg-time">${fmtTime(m.created_at)}</div>
    </div>
  `).join('');
  container.scrollTop = container.scrollHeight;
}

function escHtml(s) {
  const d = document.createElement('div');
  d.textContent = s;
  return d.innerHTML;
}

async function clearMessages() {
  if (!selectedDeviceId) return;
  if (!confirm(`确定清空 ${selectedDeviceId} 的所有对话？`)) return;
  try {
    await api('DELETE', `/api/devices/${selectedDeviceId}/messages`);
    toast('对话已清空');
    selectDevice(selectedDeviceId);
    loadDevices();
  } catch (e) {
    toast('清空失败', 'error');
  }
}

async function deleteDevice() {
  if (!selectedDeviceId) return;
  if (!confirm(`确定删除设备 ${selectedDeviceId}？此操作不可恢复。`)) return;
  try {
    await api('DELETE', `/api/devices/${selectedDeviceId}`);
    toast('设备已删除');
    selectedDeviceId = null;
    $('#chatTitle').textContent = '对话记录';
    $('#chatSubtitle').textContent = '选择设备查看';
    $('#btnClear').style.display = 'none';
    $('#btnDelete').style.display = 'none';
    $('#chatMessages').innerHTML = '<div class="chat-empty"><div class="chat-empty-icon">💬</div><p>选择左侧设备查看对话</p></div>';
    loadDevices();
  } catch (e) {
    toast('删除失败', 'error');
  }
}

// ─── 配置管理 ───

const CONFIG_KEYS = [
  'mimo_api_key', 'mimo_base_url',
  'asr_model', 'llm_model', 'tts_model', 'tts_voice',
  'max_history_turns', 'max_wav_size',
  'server_system_prompt', 'gateway_api_key',
];

async function loadConfig() {
  try {
    const cfg = await api('GET', '/api/config');
    CONFIG_KEYS.forEach(k => {
      const el = $(`#cfg-${k}`);
      if (el && cfg[k] !== undefined) el.value = cfg[k];
    });
  } catch (e) {
    console.error(e);
  }
}

async function saveConfig() {
  const data = {};
  CONFIG_KEYS.forEach(k => {
    const el = $(`#cfg-${k}`);
    if (el && el.value.trim()) data[k] = el.value.trim();
  });
  try {
    await api('PUT', '/api/config', data);
    toast('配置已保存');
  } catch (e) {
    toast('保存失败', 'error');
  }
}

// ─── 初始化 ───

loadDevices();
// 定时刷新
setInterval(loadDevices, 30000);
