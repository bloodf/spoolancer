import { TAG_FORMATS, validateSpool, measurement, tagWriteGuard, tagLinkGuard, grams, validSpoolmanUrl, COMMON_MATERIALS, COLOR_SWATCHES, normalizeHex, materialSelection, resolveMaterial, THEME_STORAGE_KEY, normalizeTheme, resolveTheme, readThemePreference, saveThemePreference, pairingState, sessionBody, usesOpenSetup, setupState, setupBody, pinIsOptional, networkChoices, wifiCredentials, scaleZeroGuard, scaleCalibrationGuard, fullSpoolTare, inventoryState, inventoryActionSpool, cleanLocalDraft, ledSegments } from './logic.js';

const $ = selector => document.querySelector(selector);
const state = { token: '', status: null, spools: [], lastSeen: 0, busy: false, configDirty: false, connectionDirty: false, serverDirty: false, privateLoaded: false, action: null, demo: false, lastPrivate: 0, calibrationZeroed: false, calibrationReference: null, inventory: null, drafts: [], editedSpool: null };
const isFresh = () => Date.now() - state.lastSeen < 4500;
const activeSpool = () => [...state.spools, ...state.drafts].find(s => String(s.id) === String(state.status?.active_spool_id));
const currentMeasurement = () => measurement(state.status, activeSpool(), isFresh());
let transport = deviceRequest;
let polling = false;
let automaticSession = null;
let nextSessionAttempt = 0;
let setupFinished = false;
let customDemoAccess = false;
let scanState = 'idle';
let scanMessage = '';
let scanGeneration = 0;
let scannedNetworks = [];
let savedNetwork = { ssid: '', hasPassword: false };
let visibleView = '';
let discovery = { state: 'idle', candidates: [], message: '' };
let discoveryRunning = false;
let lastDiscovered = false;
let savedSpoolmanUrl = '';
let tagLink = null;
let diagnosticResults = {};
let firmwareInfo = null;
const networkPickers = ['setup', 'config'].map(name => ({ name, root: $(`#${name}-network`) }));
const needsSetup = () => state.status?.setup?.required === true;

function node(tag, text, className) {
  const element = document.createElement(tag);
  if (text !== undefined) element.textContent = text;
  if (className) element.className = className;
  return element;
}

async function deviceRequest(path, { method = 'GET', body } = {}) {
  const controller = new AbortController();
  const duration = ['/api/spools', '/api/spools/remove', '/api/nfc/write', '/api/spoolman/sync'].includes(path.split('?')[0]) ? 30000 : ['/api/nfc/read', '/api/spools/tag', '/api/spoolman/test', '/api/diagnostics', '/api/update'].includes(path) ? 20000 : 8000;
  const timeout = setTimeout(() => controller.abort(), duration);
  try {
    const response = await fetch(path, {
      method, cache: 'no-store', credentials: 'omit', signal: controller.signal,
      headers: { ...(body ? { 'Content-Type': 'application/json' } : {}), ...(state.token ? { Authorization: `Bearer ${state.token}` } : {}) },
      ...(body ? { body: JSON.stringify(body) } : {}),
    });
    let data;
    try { data = await response.json(); } catch { throw new Error('The device returned an unreadable response.'); }
    if (!response.ok || data.error) {
      const error = new Error(typeof data.error === 'string' ? data.error : `Device request failed (${response.status}).`);
      error.status = response.status;
      error.data = data;
      throw error;
    }
    return data;
  } catch (error) {
    if (error instanceof TypeError) throw new Error(method === 'POST' ? 'The connection to the device was lost. The action may have finished. Reconnect and inspect the tag or saved record before retrying.' : 'The device did not respond. Check the connection before retrying.');
    if (error.name === 'AbortError') throw new Error(method === 'POST' ? 'The device did not confirm completion in time. The action may have finished. Reconnect and inspect the tag or saved record before retrying.' : 'The device did not respond. Check the connection before retrying.');
    throw error;
  } finally { clearTimeout(timeout); }
}

async function api(path, options) {
  try {
    const result = await transport(path, { ...options, token: state.token });
    if (options?.method === 'POST' && !['/api/session', '/api/wifi/scan', '/api/spoolman/discover'].includes(path) && result?.ok !== true) throw new Error('The device did not confirm this action. Check its state before repeating it.');
    return result;
  }
  catch (error) {
    if (error.status === 401 && path !== '/api/session') {
      lock(true);
      error.message = 'Pair with the device again. Your session expired or was rejected.';
    }
    throw error;
  }
}

function notice(message, error = false) {
  const box = $('#notice');
  box.hidden = false;
  box.classList.toggle('error', error);
  box.querySelector('span').textContent = `${state.demo ? 'Demo: ' : ''}${message}`;
}

function requirePair() {
  if (state.token) return true;
  if (needsSetup()) { setView(); return false; }
  if (pinIsOptional(state.status)) { nextSessionAttempt = 0; void poll(); notice('Connecting to the scale. Please try again once connected.'); return false; }
  $('#pair-error').textContent = '';
  renderPairing();
  $('#pair-dialog').showModal();
  return false;
}

function lock(preserveEdits = false) {
  state.token = '';
  state.calibrationZeroed = false;
  state.calibrationReference = null;
  $('#calibration-result').textContent = '';
  state.spools = [];
  state.privateLoaded = false;
  if (!preserveEdits || !state.configDirty) { state.configDirty = false; state.connectionDirty = false; state.serverDirty = false; $('#config-form').reset(); }
  if (!preserveEdits) $('#spool-form').reset();
  state.drafts = []; state.inventory = null; diagnosticResults = {}; firmwareInfo = null;
  $('#config-form [name=wifi_password]').value = '';
  $('#pair-form [name=pin]').value = '';
  renderInventory();
  render();
}

function setBusy(busy) {
  state.busy = busy;
  document.body.classList.toggle('busy', busy);
  $('#spool-form button[type=submit]').disabled = busy;
  render();
}

async function mutation(path, body, success) {
  if (state.busy || !requirePair()) return null;
  setBusy(true);
  try {
    const result = await api(path, { method: 'POST', body });
    if (success) notice(result.message || success);
    try { await refreshStatus(); await loadPrivate(false); }
    catch { state.lastSeen = 0; notice('The device completed the action, but this page could not refresh. Reconnect and check its state before repeating the action.', true); }
    return result;
  } finally { setBusy(false); }
}

async function refreshStatus() {
  const result = await api('/api/status');
  if (result.mode !== (state.demo ? 'demo' : 'device') || !result.scale || !result.nfc) throw new Error('Unexpected device status. Live measurements are unavailable.');
  const changedSpool = String(state.status?.active_spool_id) !== String(result.active_spool_id);
  state.status = result;
  state.lastSeen = Date.now();
  if (changedSpool) { populateSelector(true); renderInventory(); }
  render();
  return result;
}

async function loadPrivate(reportError = true, refreshInventory = false) {
  if (!state.token) return;
  const token = state.token;
  state.lastPrivate = Date.now();
  const results = await Promise.allSettled([api(`/api/spools${refreshInventory ? '?refresh=1' : ''}`), api('/api/config')]);
  if (!state.token || state.token !== token) return;
  if (results[0].status === 'fulfilled' && Array.isArray(results[0].value.spools)) {
    state.inventory = inventoryState(results[0].value, state.status?.spoolman?.configured === true);
    state.spools = state.inventory.spools;
    state.drafts = Array.isArray(results[0].value.drafts) ? results[0].value.drafts : [];
    state.privateLoaded = true;
    state.lastPrivate = Date.now();
    renderInventory();
    populateSelector();
  }
  if (results[1].status === 'fulfilled' && !state.configDirty) {
    const config = results[1].value;
    const form = $('#config-form');
    for (const field of ['device_name', 'spoolman_url']) form.elements[field].value = config[field] || '';
    savedSpoolmanUrl = config.spoolman_url || '';
    savedNetwork = { ssid: config.wifi_ssid || '', hasPassword: config.has_wifi_password === true };
    populateNetworks(networkPickers[1], savedNetwork.ssid);
    form.elements.wifi_password.value = '';
  }
  if (results[0].status === 'rejected') {
    state.inventory = { ...(state.inventory || {}), source: state.status?.spoolman?.configured ? 'spoolman' : 'local', stale: true, cached: state.spools.length > 0, error: results[0].reason.message };
    renderInventory();
  }
  const failure = results.find(result => result.status === 'rejected');
  if (failure && reportError) notice(failure.reason.message, true);
  render();
}

async function poll() {
  if (polling || state.busy) return;
  polling = true;
  try {
    await refreshStatus();
    await ensureLocalSession();
    if (state.token && Date.now() - state.lastPrivate > 60000 && !state.busy) await loadPrivate(false);
    maybeScanNetworks();
    if (visibleView === 'settings' && state.token && !lastDiscovered && state.status?.network?.connected && !state.status?.spoolman?.configured) void discoverSpoolman();
  } catch {
    state.lastSeen = 0;
    render();
  } finally { polling = false; }
}

function render() {
  const fresh = isFresh();
  const status = state.status;
  const spool = activeSpool();
  const m = currentMeasurement();
  setView();
  $('#device-name').textContent = status?.device?.name || 'Spool scale';
  $('#connection').className = `connection ${fresh ? 'online' : 'offline'}`;
  $('#connection span').textContent = state.busy ? 'Action in progress' : fresh ? (state.demo ? 'Demo connected' : 'Device connected') : 'Device disconnected';
  $('#pair-button').textContent = state.token ? 'Lock session' : pairingState(status, fresh).required ? 'Set up device' : 'Pair device';
  $('#pair-button').hidden = !status || needsSetup() || pinIsOptional(status) || setupFinished;
  renderPairing();
  $('#session-state').textContent = pinIsOptional(status) ? state.token ? 'PIN off / local access' : 'Connecting local session' : state.token ? 'Paired in this tab' : 'View only';
  $('#settings-access').textContent = pinIsOptional(status) ? 'PIN protection is off. Anyone on your local network can manage this scale.' : state.token ? 'Paired in this tab. Changes are enabled.' : 'Pair with your device to make changes.';
  $('#weight-label').textContent = m.remaining !== null ? 'Remaining filament' : 'Gross spool weight';
  $('#weight-value').textContent = grams(m.remaining ?? m.gross);
  $('#weight-message').textContent = state.busy && !fresh ? 'Waiting for the device to finish the current action.' : fresh && !state.token && m.gross !== null ? 'Pair with the device to choose a spool and calculate remaining filament.' : m.message;
  $('#scale-state').textContent = m.label;
  $('#scale-state').className = `pill ${m.state === 'ready' ? '' : ['offline', 'invalid'].includes(m.state) ? 'error' : 'warning'}`;
  $('#gross-value').textContent = m.gross === null ? '—' : `${grams(m.gross)} g`;
  $('#tare-value').textContent = typeof spool?.empty_weight_g === 'number' ? `${grams(spool.empty_weight_g)} g` : 'Not set';
  $('#initial-value').textContent = typeof spool?.initial_weight_g === 'number' ? `${grams(spool.initial_weight_g)} g` : 'Not set';
  $('#weight-meter').hidden = m.percent === null;
  $('#weight-meter span').style.width = `${m.percent || 0}%`;
  $('#reading-time').textContent = fresh ? (state.demo ? 'Simulated reading' : 'Live device reading') : 'No live reading';
  $('#active-name').textContent = spool?.name || (state.token ? 'Choose a spool' : 'Pair to see your spools');
  $('#active-brand').textContent = spool?.brand || (spool ? 'Brand not set' : 'Enter a known empty weight to calculate remaining filament.');
  $('#active-material').textContent = spool?.material || 'No spool selected';
  $('#active-swatch').style.setProperty('--spool-color', /^#[0-9a-f]{6}$/i.test(spool?.color || '') ? spool.color : '#7c8d7a');
  $('#edit-active').disabled = !spool || state.busy;
  $('#edit-active').textContent = spool?.metadata_readonly ? 'Details' : 'Edit';
  $('#active-select').disabled = !state.token || !fresh || state.busy;
  $('#select-spool').disabled = !state.token || !fresh || !$('#active-select').value || state.busy;
  const tag = fresh ? status?.nfc || {} : {};
  $('#tag-state').textContent = !fresh ? 'Unavailable' : tag.present ? tag.writable ? 'Writable' : 'Read-only' : 'No tag';
  $('#tag-state').className = `pill ${tag.present && tag.writable ? '' : 'neutral'}`;
  $('#tag-type').textContent = tag.type || '—';
  $('#tag-uid').textContent = tag.uid || '—';
  $('#tag-message').textContent = tag.last_error || tag.last_message || tag.message || 'Position the tag over the reader. Only writable inventory tags can be edited.';
  $('#read-tag').disabled = !fresh || state.busy;
  $('#write-tag').disabled = !spool || !!tagWriteGuard(status, null, fresh, $('#tag-format').value) || state.busy;
  $('#tag-format-note').textContent = TAG_FORMATS.find(item => item.id === $('#tag-format').value)?.note || '';
  $('#link-tag').disabled = !spool || !fresh || state.busy;
  // Cloning only applies to a Bambu Classic tag on the reader.
  $('#clone-tag').disabled = !state.token || !fresh || state.busy || tag.chip !== 'classic';
  const sm = status?.spoolman || {};
  $('#sync-state').textContent = !fresh ? 'Unavailable' : sm.last_error ? 'Needs attention' : sm.pending ? 'Pending' : sm.configured ? 'Configured' : 'Not configured';
  $('#sync-state').className = `pill ${sm.last_error ? 'error' : sm.pending ? 'warning' : 'neutral'}`;
  $('#sync-message').textContent = sm.last_error || sm.last_message || (sm.configured ? 'Send the stable measurement for the selected spool. The device checks the server response.' : 'Connect your server in Settings. Local weighing works without it.');
  $('#linked-spool').textContent = spool?.spoolman_id ? `Spool #${spool.spoolman_id}` : 'Not linked';
  $('#sync-weight').textContent = m.canSync ? `${grams(m.remaining)} g remaining` : 'No valid measurement';
  $('#sync-button').textContent = sm.pending ? 'Resolve the pending link in Edit' : spool?.spoolman_id ? 'Send measurement' : 'Create in Spoolman & send';
  $('#sync-button').disabled = !m.canSync || !sm.configured || !!sm.pending || state.busy;
  $('#save-measurement').disabled = !m.canSync || state.busy || !!sm.pending || (sm.configured && state.inventory?.stale);
  setButtonText($('#save-measurement'), sm.configured ? 'Save to Spoolman' : 'Save measurement');
  $('#sync-button').hidden = sm.configured && !!spool?.spoolman_id;
  $('#spoolman-config-state').textContent = sm.last_error ? 'Needs attention' : sm.configured ? 'Configured' : 'Optional';
  if ($('#spool-dialog').open) renderSpoolMass();
  $('#device-ip').textContent = fresh ? status?.network?.ip || 'Not assigned' : 'Unavailable';
  $('#wifi-state').textContent = fresh ? status?.network?.connected ? 'Connected' : status?.setup?.ap_active ? 'Temporary setup network' : 'Not connected' : 'Unknown';
  $('#tare-button').disabled = !!scaleZeroGuard(status, fresh) || state.busy;
  for (const input of $('#config-form').querySelectorAll('input,button,select')) input.disabled = !state.token || !fresh || state.busy;
  renderDiscovery();
  renderSetup();
  for (const picker of networkPickers) renderNetwork(picker);
  renderCalibration();
  renderHardware();
  if ($('#action-dialog').open && state.action) {
    const issue = state.action.guard?.() || '';
    $('#action-error').textContent = issue || state.action.error || '';
    $('#action-confirm').disabled = !!issue || state.busy;
  }
  if ($('#tag-link-dialog').open) renderTagLink();
}

function renderHardware() {
  const fresh = isFresh(), status = state.status || {}, scale = status.scale || {};
  const running = fresh && Number(status.diagnostics?.led_test_remaining_ms) > 0;
  const supported = status.diagnostics?.leds_supported === true;
  $('#hardware-access').textContent = !fresh ? 'Reconnect to the device to run tests.' : !state.token ? 'Pair with the device to run a test.' : state.demo ? 'Demo only. Test results are simulated.' : 'Tests check signals without saving measurements or changing calibration.';
  $('#hardware-reader').textContent = !fresh ? 'Unavailable' : status.nfc?.available === true ? 'PN532 responding' : status.nfc?.available === false ? 'Not detected' : 'Run test to check';
  $('#hardware-tag').textContent = diagnosticResults.nfc?.uid || 'Not read';
  $('#hardware-gross').textContent = fresh && scale.fresh === true && scale.calibrated === true ? `${grams(scale.gross_g)} g` : 'Not available';
  $('#hardware-sensor').textContent = !fresh ? 'Unavailable' : scale.fresh === true ? `${scale.stable === true ? 'Fresh / stable' : 'Fresh / settling'}` : 'Waiting for samples';
  $('#hardware-calibrated').textContent = !fresh ? 'Unknown' : scale.calibrated === true ? 'Stored' : 'Needed';
  for (const component of ['nfc', 'leds', 'scale']) {
    const result = diagnosticResults[component], pill = $(`#test-${component}-state`);
    $(`#test-${component}`).disabled = !fresh || !state.token || state.busy || running || (component === 'leds' && !supported);
    const label = component === 'leds' && running ? `Running / ${Math.ceil(status.diagnostics.led_test_remaining_ms / 1000)}s` : component === 'leds' && !supported ? 'No LEDs in this profile' : !result ? 'Not tested' : ({pass:'Signal passed', warning:'Needs attention', fail:'Check connection', observe:'Check visually'}[result.state] || 'Not tested');
    pill.textContent = label;
    pill.className = `pill ${result?.state === 'fail' ? 'error' : result?.state === 'warning' ? 'warning' : 'neutral'}`;
    $(`#test-${component}-result`).textContent = result ? `${state.demo ? 'Demo: ' : ''}${result.message}` : '';
    $(`#test-${component}-result`).className = result?.state === 'fail' ? 'form-error' : 'inline-result';
  }
  const m = currentMeasurement(), known = fresh && scale.stable === true && m.canSync && Number.isFinite(m.percent);
  $('#hardware-percent').textContent = known ? `${Math.round(m.percent)}%` : 'Waiting for a valid weight';
  const colors = ledSegments(known ? m.percent : null);
  [...$('#hardware-led-bar').children].forEach((segment, i) => { segment.style.setProperty('--segment-color', colors[i] || 'var(--raised)'); segment.style.color = colors[i] ? '#10231e' : 'var(--ink)'; });
  renderFirmware();
}
function renderFirmware() {
  const fresh = isFresh(), status = state.status || {};
  const current = status.device?.firmware || 'Unknown';
  $('#firmware-current').textContent = current;
  $('#firmware-version').textContent = current === 'Unknown' ? '—' : current;
  const canCheck = fresh && !!state.token && !state.busy;
  $('#firmware-check').disabled = !canCheck;
  const info = firmwareInfo, apply = $('#firmware-apply'), notes = $('#firmware-notes'), result = $('#firmware-result');
  if (!info) { $('#firmware-latest').textContent = 'Not checked'; apply.hidden = true; notes.hidden = true; result.textContent = ''; return; }
  if (info.error) {
    $('#firmware-latest').textContent = 'Unavailable'; apply.hidden = true; notes.hidden = true;
    result.textContent = `${state.demo ? 'Demo: ' : ''}${info.error}`; result.className = 'form-error'; return;
  }
  $('#firmware-latest').textContent = info.latest || 'Unknown';
  const available = info.update_available === true;
  apply.hidden = !available; apply.disabled = !canCheck;
  notes.hidden = !info.notes_url; if (info.notes_url) notes.href = info.notes_url;
  result.className = 'inline-result';
  result.textContent = info.message ? `${state.demo ? 'Demo: ' : ''}${info.message}`
    : available ? `Version ${info.latest} is available.` : 'This device is up to date.';
}
async function checkFirmware() {
  if (!requirePair()) return;
  setBusy(true);
  try { firmwareInfo = { ...(await api('/api/update')), message: '' }; }
  catch (error) { firmwareInfo = { error: error.message || 'Could not check for updates.' }; }
  finally { setBusy(false); }
}
async function applyFirmware() {
  if (!requirePair() || !firmwareInfo?.update_available) return;
  if (!confirm('Install the latest firmware now? The device downloads it and reboots, and will be offline for about a minute. Keep it powered.')) return;
  setBusy(true);
  try {
    const result = await api('/api/update', { method: 'POST', body: { confirm: true } });
    firmwareInfo = { ...firmwareInfo, message: result.message || 'Update started; the device will reboot.' };
    notice(result.message || 'Update started; the device will reboot.');
  } catch (error) { notice(error.message || 'Could not start the update.', true); }
  finally { setBusy(false); }
}
async function runDiagnostic(component) {
  if (!requirePair()) return;
  setBusy(true);
  try {
    diagnosticResults[component] = await api('/api/diagnostics', {method:'POST', body:{component}});
    await refreshStatus();
  } catch (error) {
    diagnosticResults[component] = {state:'fail', message:error.message};
  } finally { setBusy(false); }
}

function renderCalibration() {
  const fresh = isFresh();
  const scale = state.status?.scale || {};
  const sensorFresh = fresh && scale.fresh === true;
  const calibrated = fresh && scale.calibrated === true;
  const zeroIssue = !state.token ? 'Pair with the device first.' : scaleZeroGuard(state.status, fresh);
  const calibrationIssue = !state.token ? 'Pair with the device first.' : scaleCalibrationGuard(state.status, fresh, state.calibrationZeroed);
  $('#calibration-status').textContent = !fresh ? 'Device unavailable' : calibrated ? 'Calibration stored' : 'Calibration needed';
  $('#calibration-status').className = `pill ${!fresh ? 'error' : calibrated ? '' : 'warning'}`;
  $('#calibration-sensor').textContent = !fresh ? 'Reconnect to get current sensor readings.' : sensorFresh ? 'Sensor readings available.' : 'Waiting for fresh sensor readings. Keep the cradle still; check the HX711 wiring if this continues.';
  $('#calibration-access').textContent = !state.token ? 'Pair with the device to calibrate.' : state.busy ? 'Waiting for the device to complete the action.' : state.demo ? 'Preview changes last only until this page is reloaded.' : 'Calibration is saved on the ESP32 and survives a restart.';
  $('#calibration-pair').hidden = !!state.token;
  $('#calibration-pair').disabled = !fresh || state.busy;
  $('#calibration-restart').disabled = state.busy;
  $('#tare-calibration').disabled = !!zeroIssue || state.busy;
  $('#calibration-zero-help').textContent = zeroIssue || (state.calibrationZeroed ? 'Empty-cradle zero saved in this session. Continue to step 2.' : 'Start here each time you calibrate.');
  $('#reference-mass').disabled = !state.token || !fresh || state.busy;
  $('#calibration-submit').disabled = !!calibrationIssue || state.busy;
  $('#calibration-reference-help').textContent = calibrationIssue || (state.calibrationReference !== null ? `Calibration saved using ${grams(state.calibrationReference)} g. Check the result in step 3.` : 'Keep the reference still. The device checks the sensor before saving.');
  $('#calibration-step-zero').classList.toggle('complete', state.calibrationZeroed);
  $('#calibration-step-reference').classList.toggle('complete', state.calibrationReference !== null);
  const hasReading = !state.busy && sensorFresh && calibrated && typeof scale.gross_g === 'number' && Number.isFinite(scale.gross_g);
  $('#calibration-reading').textContent = hasReading ? grams(scale.gross_g) : '—';
  $('#calibration-reading-state').textContent = !fresh ? 'No live connection.' : state.busy ? 'Waiting for the action to finish.' : !sensorFresh ? 'Waiting for new sensor readings.' : !calibrated ? 'Complete steps 1 and 2 to see grams.' : `${state.demo ? 'Simulated' : 'Live'} gross weight / ${scale.stable === true ? 'stable' : 'settling'}${state.calibrationReference === null ? '. Uses the previously stored calibration.' : '.'}`;
}

function calibrationResult(message, error = false) {
  $('#calibration-result').textContent = `${state.demo ? 'Demo: ' : ''}${message}`;
  $('#calibration-result').className = error ? 'form-error' : 'inline-result';
}

async function ensureLocalSession() {
  if (state.token || !pinIsOptional(state.status) || !isFresh() || Date.now() < nextSessionAttempt) return;
  if (automaticSession) return automaticSession;
  nextSessionAttempt = Date.now() + 10000;
  automaticSession = (async () => {
    try {
      const result = await api('/api/session', { method: 'POST', body: sessionBody(state.status, isFresh()) });
      if (typeof result.token !== 'string' || !result.token) throw new Error('The device did not provide local access.');
      if (!pinIsOptional(state.status)) return;
      state.token = result.token;
      await loadPrivate(false);
    } catch (error) { notice(`Could not open local access: ${error.message} Retrying shortly.`, true); }
    finally { automaticSession = null; render(); }
  })();
  return automaticSession;
}

function networkFields(picker) {
  const select = picker.root.querySelector('select');
  const manual = select.value === 'manual';
  const option = select.selectedOptions[0];
  const secure = manual ? undefined : option?.dataset.secure;
  return {
    wifi_ssid: manual ? picker.root.querySelector('[name=wifi_ssid]').value : select.value.startsWith('ssid:') ? select.value.slice(5) : '',
    wifi_password: picker.root.querySelector('[name=wifi_password]').value,
    wifi_open: secure === 'true' ? false : secure === 'false' ? true : picker.root.querySelector('[name=wifi_open]').checked,
  };
}

function populateNetworks(picker, savedSsid) {
  const select = picker.root.querySelector('select');
  const previous = savedSsid !== undefined ? savedSsid ? `ssid:${savedSsid}` : '' : select.value;
  const oldSecure = select.selectedOptions[0]?.dataset.secure;
  select.replaceChildren(new Option('Choose a network…', ''));
  for (const network of scannedNetworks) {
    const strength = network.rssi >= -60 ? 'Strong' : network.rssi >= -75 ? 'Good' : 'Weak';
    const option = new Option(`${network.ssid} / ${strength}${!network.supported ? ' / Unsupported security' : network.secure ? '' : ' / Open'}`, `ssid:${network.ssid}`);
    option.dataset.secure = String(network.secure);
    option.disabled = !network.supported;
    if (option.value === previous && (savedSsid !== undefined || oldSecure !== undefined)) {
      option.dataset.secure = savedSsid !== undefined ? String(savedNetwork.hasPassword) : oldSecure;
      option.textContent = `${network.ssid} / ${savedSsid !== undefined ? 'Saved network' : 'Selected network'}`;
      option.disabled = false;
    }
    select.add(option);
  }
  if (previous.startsWith('ssid:') && ![...select.options].some(option => option.value === previous)) {
    const option = new Option(`${previous.slice(5)} / Not in latest scan`, previous);
    if (savedSsid !== undefined) option.dataset.secure = String(savedNetwork.hasPassword);
    else if (oldSecure !== undefined) option.dataset.secure = oldSecure;
    select.add(option);
  }
  select.add(new Option('Hidden or other network…', 'manual'));
  select.value = previous;
  renderNetwork(picker);
}

function renderNetwork(picker) {
  const root = picker.root;
  const select = root.querySelector('select');
  const fields = networkFields(picker);
  const manual = select.value === 'manual';
  const ready = picker.name === 'setup' ? setupState(state.status, isFresh()).eligible && isFresh() : !!state.token && isFresh();
  const disabled = !ready || state.busy;
  const knownSecurity = select.selectedOptions[0]?.dataset.secure !== undefined && !manual;
  const keepsPassword = picker.name === 'config' && fields.wifi_ssid === savedNetwork.ssid && savedNetwork.hasPassword && !fields.wifi_open;
  root.querySelector('.manual-network').hidden = !manual;
  root.querySelector('[name=wifi_ssid]').required = manual;
  root.querySelector('.open-network').hidden = !manual && knownSecurity || !select.value;
  root.querySelector('.network-password').hidden = fields.wifi_open;
  for (const input of root.querySelectorAll('input,select')) input.disabled = disabled;
  root.querySelector('[name=wifi_ssid]').disabled = disabled || !manual;
  root.querySelector('[name=wifi_password]').disabled = disabled || fields.wifi_open;
  root.querySelector('[name=wifi_password]').placeholder = keepsPassword ? 'Leave blank to keep the saved password' : 'Network password';
  const scan = root.querySelector('.scan-button');
  scan.disabled = disabled || scanState === 'scanning';
  scan.textContent = scanState === 'scanning' ? 'Scanning…' : scanState === 'idle' ? 'Scan networks' : 'Scan again';
  root.querySelector('.network-status').textContent = !ready ? !isFresh() ? 'Reconnect to the scale to scan nearby networks.' : picker.name === 'setup' ? 'Join the temporary setup Wi-Fi to scan.' : 'Pair with the scale to scan nearby networks.' : scanMessage || 'Choose a nearby 2.4 GHz network, or enter a hidden network.';
  if (picker.name === 'config') $('#password-status').textContent = fields.wifi_open ? 'This network will be saved without a password.' : keepsPassword ? 'A password is saved for this network. Leave blank to keep it.' : 'Enter the password for the selected network. The previous network’s password will not be reused.';
}

function maybeScanNetworks() {
  if (scanState !== 'idle' || state.busy || !isFresh()) return;
  if (setupState(state.status, true).eligible || (location.hash === '#settings' && state.token)) void scanNetworks();
}

async function scanNetworks() {
  if (scanState === 'scanning' || state.busy) return;
  if (!setupState(state.status, isFresh()).eligible && !state.token) return;
  const generation = ++scanGeneration;
  scanState = 'scanning'; scanMessage = 'Scanning nearby 2.4 GHz networks…'; render();
  try {
    let result = await api('/api/wifi/scan', { method: 'POST', body: {} });
    const deadline = Date.now() + 20000;
    while (result.state === 'scanning' && Date.now() < deadline) {
      await new Promise(resolve => setTimeout(resolve, 700));
      if (generation !== scanGeneration) return;
      result = await api('/api/wifi/scan');
    }
    if (generation !== scanGeneration) return;
    if (result.state !== 'ready') throw new Error(result.message || 'Scan did not finish. Try again, or enter a network name manually.');
    scannedNetworks = networkChoices(result.networks);
    scanState = 'ready';
    scanMessage = scannedNetworks.length ? `${scannedNetworks.length} network${scannedNetworks.length === 1 ? '' : 's'} found. Choose yours from the list, or use manual entry.` : 'No networks found. Move closer to your router, scan again, or enter its name manually.';
    for (const picker of networkPickers) populateNetworks(picker);
  } catch (error) {
    if (generation !== scanGeneration) return;
    scanState = 'failed'; scanMessage = `Could not scan: ${error.message}`;
  } finally { if (generation === scanGeneration) render(); }
}

function renderSetup() {
  const form = $('#setup-form');
  const mode = setupState(state.status, isFresh());
  const pinRequired = form.elements.pin_required.checked;
  $('#setup-pin-fields').hidden = !pinRequired;
  for (const input of form.querySelectorAll('input,select')) input.disabled = state.busy || !isFresh() || !mode.eligible;
  for (const name of ['pin', 'pin_confirm']) {
    form.elements[name].required = pinRequired;
    form.elements[name].disabled = !pinRequired || state.busy || !isFresh() || !mode.eligible;
  }
  $('#setup-access-help').textContent = pinRequired ? 'Use an 8-digit PIN to manage spools and change settings. Keep it somewhere safe.' : 'No PIN: anyone on the same local network can manage spools, write tags and change settings.';
  $('#setup-authorization').textContent = !isFresh() ? 'Waiting for the scale. Keep this page open while reconnecting.' : !mode.eligible ? 'Join the temporary Spoolancer Wi-Fi and open http://192.168.4.1 to finish setup.' : mode.canSubmit ? 'Ready to save from this setup network. No button press is needed.' : 'Setup is temporarily unavailable. Reconnect to the setup network and try again.';
  $('#setup-authorization').classList.toggle('confirmed', mode.canSubmit);
  $('#setup-submit').disabled = state.busy || !mode.canSubmit || scanState === 'scanning';
  $('#setup-submit').textContent = state.busy ? 'Saving setup…' : 'Connect & finish setup';
  if (setupFinished) {
    const connected = isFresh() && state.status?.network?.connected === true;
    const ip = state.status?.network?.ip || '';
    const validIp = /^\d{1,3}(\.\d{1,3}){3}$/.test(ip) && ip.split('.').every(part => Number(part) <= 255) && ip !== '0.0.0.0';
    $('#setup-result-title').textContent = connected ? 'Your scale is connected.' : 'Setup saved. Connecting…';
    $('#setup-result-description').textContent = connected ? 'Join the same Wi-Fi on your phone or computer to use Spoolancer.' : 'Waiting for a confirmed Wi-Fi connection. You can check the password in Settings if it cannot connect.';
    $('#setup-result-network').textContent = connected ? `${state.demo ? 'Simulated device address' : 'Device address'}: http://${ip}` : isFresh() ? 'The scale has not joined the selected network yet.' : 'Connection to the scale was interrupted. Rejoin its setup Wi-Fi to check progress, or try spoolancer.local on your home network.';
    $('#setup-device-link').hidden = state.demo || !connected || !validIp;
    if (connected && validIp) $('#setup-device-link').href = `http://${ip}/`;
  }
}

function renderPairing() {
  const fresh = isFresh();
  const status = state.status;
  const setup = status?.setup || {};
  const mode = pairingState(status, fresh);
  const openSetup = usesOpenSetup(status);
  const openAp = setup.ap_active === true && setup.open_network === true && setup.via_ap === true;
  $('#setup-banner').hidden = !openAp;
  $('#setup-time').textContent = fresh && Number.isFinite(setup.ap_seconds) ? `Setup network closes in about ${Math.ceil(setup.ap_seconds / 60)} minute(s), or 60 seconds after joining your Wi-Fi.` : 'Device status is unavailable. Reconnect before continuing.';
  $('#pair-title').textContent = mode.required ? 'Set up your scale' : 'Pair with your scale';
  $('#pair-description').textContent = mode.required ? 'Complete Wi-Fi setup and choose your access preference first.' : openSetup ? 'Enter your saved 8-digit PIN to enable changes in this browser tab.' : 'Enter the 8-digit PIN shown on the device or in its Serial output.';
  $('#enrollment-warning').hidden = !mode.eligible;
  $('#enrollment-status').hidden = true;
  $('#pin-field').hidden = mode.required && !mode.eligible;
  $('#pin-label').textContent = mode.required ? 'Choose an 8-digit PIN' : 'Device PIN';
  $('#pair-form [name=pin]').disabled = state.busy || (mode.required && !mode.eligible);
  $('#pair-submit').textContent = mode.required ? 'Save PIN & pair' : 'Pair device';
  $('#pair-submit').disabled = state.busy || !mode.canSubmit;
  $('#pairing-help').textContent = openSetup ? 'Use your chosen PIN. For first setup or a forgotten PIN, hold BOOT for 8 seconds while the scale is running, join its temporary Wi-Fi, then open http://192.168.4.1. Sessions stay in this tab’s memory and clear on reload.' : 'Pair using the PIN shown by your device or Serial output. The session stays in this tab’s memory and is cleared on reload.';
  if (pinIsOptional(status)) $('#pairing-help').textContent = 'PIN protection is off. Anyone on your local network can operate the scale. To set a PIN, hold BOOT for 8 seconds while running and complete setup again. Spools and calibration are kept.';
  $('#wifi-help').textContent = openSetup ? 'Wi-Fi credentials are sent over local HTTP. The temporary setup network closes after 10 minutes, or 60 seconds after connecting to your Wi-Fi. If connection fails after it closes, hold BOOT for 8 seconds while the scale is running to reopen setup.' : 'A Wi-Fi change can disconnect this page. Reopen it at the device’s new address. The device keeps its setup access point available if connection fails.';
  if (state.demo) {
    $('#footer-mode').textContent = needsSetup() ? 'Demo only / choose PIN protection or local access' : pinIsOptional(status) ? 'Demo only / PIN protection off' : customDemoAccess ? 'Demo only / use your chosen preview PIN' : 'Demo only / PIN 12345678';
    $('#pair-form [name=pin]').placeholder = customDemoAccess ? 'Your chosen preview PIN' : 'Demo PIN: 12345678';
  }
}

function populateSelector(followActive = false) {
  const select = $('#active-select');
  const previous = select.value;
  select.replaceChildren(node('option', 'Choose a spool…'));
  select.firstChild.value = '';
  for (const spool of state.spools) {
    const option = node('option', `${spool.name} / ${spool.material}`);
    option.value = spool.id;
    select.append(option);
  }
  const active = state.status?.active_spool_id;
  select.value = !followActive && state.spools.some(s => String(s.id) === previous) ? previous : active || '';
}

function details(list, pairs) {
  list.replaceChildren();
  for (const [label, value] of pairs) {
    const row = node('div');
    row.append(node('dt', label), node('dd', value));
    list.append(row);
  }
}

function renderInventory() {
  const query = $('#inventory-search').value.trim().toLowerCase();
  const matches = state.spools.filter(s => `${s.name} ${s.brand} ${s.material}`.toLowerCase().includes(query));
  const list = $('#inventory-list');
  list.replaceChildren();
  const inventory = state.inventory;
  const remote = inventory?.source === 'spoolman' || state.status?.spoolman?.configured;
  $('#inventory-source').textContent = !state.token ? 'Pair to view inventory' : remote ? 'Spoolman inventory' : 'Stored on this scale';
  $('#inventory-message').textContent = !state.token ? 'Your spool profiles are visible after pairing.' : inventory?.error ? `${inventory.cached ? 'Showing cached server data. ' : ''}${inventory.error}` : inventory?.stale ? 'Showing the last server inventory. Refresh to check for changes.' : remote ? 'Spoolman owns these profiles. Material details are edited on your server.' : 'Connect Spoolman in Settings to use a shared inventory.';
  $('.inventory-source').classList.toggle('has-error', !!inventory?.error);
  $('#refresh-inventory').disabled = !state.token || !isFresh() || state.busy;
  const unavailable = !!inventory?.error && !inventory.cached && !state.spools.length;
  $('#spool-count').hidden = !state.token || unavailable || !state.privateLoaded;
  $('#spool-count').textContent = state.spools.length;
  $('#inventory-total').textContent = !state.token ? 'Pair to load private inventory' : !state.privateLoaded ? 'Loading inventory' : unavailable ? 'Count unavailable' : `${matches.length} of ${state.spools.length} spools`;
  $('#inventory-empty').hidden = matches.length > 0;
  $('#inventory-empty h2').textContent = !state.token ? 'Your inventory is private' : inventory?.error && !matches.length ? 'Inventory unavailable' : query ? 'No matching spools' : remote ? 'No spools on this server' : 'No spools yet';
  $('#inventory-empty p').textContent = !state.token ? 'Pair with your scale to see its spool profiles.' : inventory?.error && !matches.length ? 'Check the server connection in Settings, then refresh. Local records are not substituted.' : query ? 'Try another name, brand or material.' : 'Add a spool to start tracking remaining filament.';
  $('#new-spool-empty').textContent = state.token ? 'Add a spool' : 'Pair device';
  $('#new-spool-empty').hidden = !!query && !!state.token;
  for (const spool of matches) {
    const active = String(spool.id) === String(state.status?.active_spool_id);
    const card = node('article', undefined, `inventory-card${active ? ' active' : ''}`);
    const top = node('div', undefined, 'card-top');
    top.append(node('span', active ? 'On the scale' : 'Saved spool', 'eyebrow'), node('span', spool.material, 'pill neutral'));
    const identity = node('div', undefined, 'spool-identity');
    const picture = node('div', undefined, 'spool-drawing');
    picture.setAttribute('aria-hidden', 'true');
    if (/^#[0-9a-f]{6}$/i.test(spool.color || '')) picture.style.setProperty('--spool-color', spool.color);
    picture.append(node('i'));
    const heading = node('div');
    heading.append(node('h3', spool.name), node('p', spool.brand || 'Brand not set'));
    identity.append(picture, heading);
    const values = node('dl', undefined, 'details-list');
    details(values, [['Empty spool', typeof spool.empty_weight_g === 'number' ? `${grams(spool.empty_weight_g)} g` : 'Unknown'], ['Last saved remaining', typeof spool.remaining_weight_g === 'number' ? `${grams(spool.remaining_weight_g)} g` : 'No measurement'], ['Spoolman', spool.spoolman_id ? `#${spool.spoolman_id}` : 'Not linked'], ['RFID tag', spool.tag_uid || 'Not linked']]);
    const actions = node('div', undefined, 'button-row inventory-actions');
    const use = node('button', active ? 'Selected' : 'Use on scale', 'button secondary');
    use.disabled = active || state.busy;
    use.setAttribute('aria-label', `Use ${spool.name} on scale`);
    use.addEventListener('click', () => chooseSpool(spool.id));
    const edit = node('button', spool.metadata_readonly ? 'Details' : 'Edit', 'button secondary');
    edit.setAttribute('aria-label', `${spool.metadata_readonly ? 'Details for' : 'Edit'} ${spool.name}`);
    edit.addEventListener('click', () => editSpool(spool));
    const link = node('button', undefined, 'button secondary');
    link.append(icon('nfc'), node('span', spool.tag_uid ? 'Change RFID' : 'Add RFID'));
    link.setAttribute('aria-label', `${spool.tag_uid ? 'Change' : 'Add'} RFID for ${spool.name}`);
    link.addEventListener('click', () => showTagLink(spool));
    const remove = node('button', undefined, 'button secondary danger');
    remove.append(icon('x'), node('span', spool.removal_pending ? 'Check archive status' : 'Remove'));
    remove.setAttribute('aria-label', `Remove ${spool.name}`);
    remove.addEventListener('click', () => showRemove(spool));
    actions.append(use, edit, link, remove);
    card.append(top, identity, values, actions);
    if (spool.removal_pending) card.append(node('p', 'Archive not confirmed. Check its status before using this spool.', 'inline-result error'));
    list.append(card);
  }
  renderDrafts();
}

function editSpool(spool) {
  if (!requirePair()) return;
  const form = $('#spool-form');
  form.reset();
  state.editedSpool = spool || null;
  for (const input of form.querySelectorAll('input,select,button')) input.disabled = false;
  const defaults = { id: '', name: '', brand: '', material: '', color: '#386c56', empty_weight_g: '', initial_weight_g: 1000, density_g_cm3: 1.24, diameter_mm: 1.75, spoolman_id: '' };
  for (const [key, fallback] of Object.entries(defaults)) form.elements[key].value = spool?.[key] ?? fallback;
  const material = materialSelection(spool?.material || '');
  $('#material-select').value = material.choice;
  $('#material-custom').value = material.custom;
  syncMaterialEditor();
  setEditorColor(spool?.color || defaults.color);
  $('#spool-dialog-title').textContent = spool ? 'Edit spool' : 'Add a spool';
  $('#spool-error').textContent = '';
  $('#spool-condition').hidden = !!spool;
  $('#spool-link-fields').open = !!spool?.spoolman_id || !!spool?.recovery_required || !!spool?.sync_pending;
  const readonly = spool?.metadata_readonly === true;
  if (readonly) for (const input of form.querySelectorAll('input,select,button[data-color]')) input.disabled = true;
  $('#save-spool').hidden = readonly;
  $('#spool-destination').textContent = readonly ? 'This profile comes from Spoolman. Edit material, color and empty weight on your server.' : state.status?.spoolman?.configured ? state.inventory?.error ? 'The server is unavailable. A failed registration is kept as a recovery draft. Check the server before retrying.' : 'Save creates a spool on your Spoolman server. Check these details before saving.' : 'This profile will be saved on the scale.';
  setButtonText($('#save-spool'), state.status?.spoolman?.configured ? 'Save to Spoolman' : 'Save spool');
  renderSpoolMass();
  $('#spool-dialog').showModal();
}

function syncMaterialEditor(focusCustom = false) {
  const custom = $('#material-select').value === 'custom';
  $('#material-custom-label').hidden = !custom;
  $('#material-custom').disabled = !custom;
  $('#material-custom').required = custom;
  $('#spool-form').elements.material.value = custom ? $('#material-custom').value : $('#material-select').value;
  if (custom && focusCustom) $('#material-custom').focus();
}

function setEditorColor(value, updateHex = true) {
  const color = normalizeHex(value);
  $('#spool-color').value = color;
  if (updateHex) $('#color-hex').value = color.toUpperCase();
  $('#color-hex').setCustomValidity('');
  $('#color-hex').removeAttribute('aria-invalid');
  $('#color-description').textContent = `Selected color ${color.toUpperCase()}`;
  for (const swatch of document.querySelectorAll('[data-color]')) swatch.setAttribute('aria-pressed', String(swatch.dataset.color === color));
}

function setupEditorControls() {
  const select = $('#material-select');
  for (const material of COMMON_MATERIALS) {
    const option = node('option', material);
    option.value = material;
    select.insertBefore(option, select.lastElementChild);
  }
  for (const [name, color] of COLOR_SWATCHES) {
    const button = node('button', undefined, 'color-swatch');
    button.type = 'button';
    button.dataset.color = color;
    button.style.backgroundColor = color;
    button.setAttribute('aria-label', `${name} ${color.toUpperCase()}`);
    button.setAttribute('aria-pressed', 'false');
    button.title = `${name} ${color.toUpperCase()}`;
    button.addEventListener('click', () => setEditorColor(color));
    $('#color-swatches').append(button);
  }
  select.addEventListener('change', () => syncMaterialEditor(true));
  $('#material-custom').addEventListener('input', () => syncMaterialEditor());
  $('#spool-color').addEventListener('input', event => setEditorColor(event.target.value));
  $('#color-hex').addEventListener('input', event => {
    try { setEditorColor(event.target.value, false); }
    catch { event.target.setCustomValidity('Enter a 3- or 6-digit hex color.'); event.target.setAttribute('aria-invalid', 'true'); }
  });
  $('#color-hex').addEventListener('change', event => {
    try { setEditorColor(event.target.value); } catch { event.target.reportValidity(); }
  });
}

function icon(name) {
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.setAttribute('class', 'icon'); svg.setAttribute('aria-hidden', 'true'); svg.setAttribute('focusable', 'false');
  const use = document.createElementNS('http://www.w3.org/2000/svg', 'use');
  use.setAttribute('href', `#icon-${name}`); svg.append(use); return svg;
}
function setButtonText(button, text) {
  const label = button.querySelector('.button-label');
  if (label) label.textContent = text; else button.textContent = text;
}
function setupIcons() {
  const buttons = { '#refresh-button': 'refresh-cw', '#new-spool': 'plus', '#new-spool-scale': 'plus', '#save-measurement': 'save', '#read-tag': 'nfc', '#write-tag': 'nfc', '#link-tag': 'nfc', '#clone-tag': 'nfc', '#tag-link-scan': 'nfc', '#discover-spoolman': 'search', '#refresh-inventory': 'refresh-cw', '#test-nfc':'nfc', '#test-leds':'check', '#test-scale':'scale' };
  for (const [selector, name] of Object.entries(buttons)) { const el = $(selector); const label = node('span', el.textContent.replace(/^\+\s*/, ''), 'button-label'); el.replaceChildren(icon(name), label); }
  for (const [view, name] of Object.entries({ scale: 'scale', inventory: 'layers', settings: 'settings-2' })) $(`[data-view=${view}]`).prepend(icon(name));
}
function renderSpoolMass() {
  const form = $('#spool-form');
  const full = !form.elements.id.value && form.elements.spool_condition.value === 'full';
  $('#empty-weight-field').hidden = full;
  $('#tare-estimate').hidden = !full;
  $('#tare-help').textContent = full ? 'The estimated empty weight is gross weight minus the label’s filament weight. Packaging, moisture and manufacturing variation affect this estimate. Use a measured empty weight when available.' : 'Enter a known empty-spool weight to calculate remaining filament. Leave it blank to track gross weight only.';
  if (full) {
    try {
      const estimate = fullSpoolTare(state.status, isFresh(), form.elements.initial_weight_g.value, 'full');
      $('#tare-estimate').textContent = `${grams(estimate.gross)} g gross − ${grams(estimate.initial)} g filament = ${grams(estimate.empty)} g estimated empty spool`;
      $('#tare-estimate').classList.remove('error');
    } catch (error) { $('#tare-estimate').textContent = error.message; $('#tare-estimate').classList.add('error'); }
  }
}
function renderDrafts() {
  let section = $('#inventory-drafts');
  if (!section) { section = node('section', undefined, 'drafts-section'); section.id = 'inventory-drafts'; $('#inventory-list').after(section); }
  section.hidden = !state.token || !state.drafts.length;
  section.replaceChildren(node('h2', 'Inventory recovery'), node('p', 'Saved local records that need attention. They are separate from the active Spoolman inventory.', 'helper'));
  for (const draft of state.drafts) {
    const row = node('div', undefined, 'draft-row');
    const text = node('div'); text.append(node('strong', draft.name), node('p', draft.removal_pending ? 'The archive was not confirmed. Check its server status to finish this action.' : draft.inactive_tag_link ? `This spool is no longer active on the server. Its RFID link is still saved on this scale: ${draft.tag_uid}.` : draft.recovery_required || draft.sync_pending ? 'Check Spoolman before retrying. Link the existing server ID if it was created.' : 'This local draft has no pending server operation. Review it or discard it.', 'helper'));
    const actions = node('div', undefined, 'button-row');
    const button = node('button', draft.removal_pending ? 'Check & finish archive' : draft.inactive_tag_link ? 'Unlink RFID' : 'Review draft', 'button secondary');
    button.addEventListener('click', () => draft.removal_pending ? showRemove(draft) : draft.inactive_tag_link ? showUnlinkDraft(draft) : editSpool(draft));
    actions.append(button);
    if (cleanLocalDraft(draft)) {
      const discard = node('button', 'Discard draft', 'button secondary danger');
      discard.addEventListener('click', () => showDiscardDraft(draft)); actions.append(discard);
    }
    row.append(text, actions); section.append(row);
  }
}
function renderDiscovery() {
  const ready = state.token && isFresh() && state.status?.network?.connected;
  $('#discover-spoolman').disabled = !ready || state.busy || discoveryRunning;
  setButtonText($('#discover-spoolman'), discoveryRunning ? 'Searching network…' : 'Find on network');
  $('#discovery-status').textContent = !state.token ? 'Pair to find a Spoolman server.' : !state.status?.network?.connected ? 'Connect the scale to your local Wi-Fi before searching.' : discovery.message || 'Search your local network, or enter an address below.';
}
async function discoverSpoolman() {
  if (discoveryRunning || state.busy || !state.status?.network?.connected || !requirePair()) return;
  lastDiscovered = true; discoveryRunning = true;
  discovery = { state: 'searching', candidates: [], message: 'Looking for Spoolman on the local network. You can enter an address if it is not found.' };
  $('#discovery-results').replaceChildren(); renderDiscovery();
  try {
    let result = await api('/api/spoolman/discover', { method: 'POST', body: {} });
    const deadline = Date.now() + 120000;
    while (result.state === 'searching' && Date.now() < deadline) {
      discovery = result; renderDiscovery();
      await new Promise(resolve => setTimeout(resolve, 1200));
      result = await api('/api/spoolman/discover');
    }
    discovery = result;
    if (result.state === 'failed') throw new Error(result.message || 'Search failed. Enter the server address manually.');
    if (result.state === 'searching') throw new Error('Search is taking longer than expected. Enter the server address manually.');
    const candidates = Array.isArray(result.candidates) ? result.candidates : [];
    discovery.message = result.message || (candidates.length ? 'Choose a server, then select Save server.' : 'No Spoolman server found. Enter its address manually.');
    for (const candidate of candidates) {
      if (!validSpoolmanUrl(candidate.url) || !candidate.url) continue;
      const button = node('button', `${candidate.label || 'Spoolman'}: ${candidate.url}`, 'discovery-candidate button secondary'); button.type = 'button';
      button.addEventListener('click', () => { $('#config-form').elements.spoolman_url.value = candidate.url; state.configDirty = true; state.serverDirty = true; discovery.message = 'Server selected. Select Save server to use this inventory.'; renderDiscovery(); });
      $('#discovery-results').append(button);
    }
  } catch (error) { discovery = { state: 'failed', candidates: [], message: error.message }; }
  finally { discoveryRunning = false; renderDiscovery(); }
}

function setupTheme() {
  let storage;
  try { storage = window.localStorage; } catch { storage = null; }
  const system = window.matchMedia('(prefers-color-scheme: dark)');
  let preference = readThemePreference(storage);
  const apply = () => {
    const theme = resolveTheme(preference, system.matches);
    document.documentElement.dataset.theme = theme;
    $('meta[name=color-scheme]').content = theme;
    $('meta[name=theme-color]').content = theme === 'dark' ? '#111b18' : '#f6f7f2';
    $('#theme-select').value = preference;
  };
  $('#theme-select').addEventListener('change', event => { preference = saveThemePreference(storage, event.target.value); apply(); });
  system.addEventListener('change', apply);
  window.addEventListener('storage', event => { if (event.key === THEME_STORAGE_KEY) { preference = normalizeTheme(event.newValue); apply(); } });
  apply();
}

async function chooseSpool(id) {
  try {
    const result = await mutation('/api/active-spool', { id }, 'Spool selected.');
    if (result) { location.hash = 'scale'; populateSelector(); renderInventory(); render(); }
  } catch (error) { notice(error.message, true); }
}

function confirmAction({ title, description, rows = [], label, guard, run }) {
  if (!requirePair()) return;
  state.action = { guard, run };
  $('#action-title').textContent = title;
  $('#action-description').textContent = description;
  details($('#action-details'), rows);
  $('#action-confirm').textContent = label;
  $('#action-error').textContent = '';
  $('#action-dialog').showModal();
  render();
}

function inventoryActionGuard(id, operation = 'view') {
  if (!state.token) return 'Pair with the device first.';
  if (!isFresh()) return 'Reconnect to the scale before continuing.';
  const spool = inventoryActionSpool(state.spools, state.drafts, id, operation);
  if (!spool) return 'This spool is no longer in the inventory. Close this dialog and refresh.';
  if (spool.removal_pending && operation !== 'archive') return 'Check the pending archive before changing this spool.';
  if (spool.removal_pending && (!Number.isInteger(spool.spoolman_id) || spool.spoolman_id < 1)) return 'This pending archive has no valid Spoolman ID. Check its saved record before continuing.';
  if (!spool.removal_pending && (spool.recovery_required || spool.sync_pending)) return 'Resolve the pending Spoolman operation before changing this spool.';
  if (state.status?.spoolman?.configured && state.inventory?.stale && !['unlink', 'discard'].includes(operation) && !(operation === 'archive' && spool.removal_pending)) return 'Refresh the Spoolman inventory before continuing.';
  return '';
}

function showRemove(spool) {
  if (state.busy) return;
  const remote = !!spool.spoolman_id;
  const recovery = spool.removal_pending === true;
  confirmAction({
    title: recovery ? 'Check this archive?' : remote ? 'Archive this spool?' : 'Remove this spool?',
    description: recovery ? 'Check the current server record and finish the pending local action. The server may already have archived or removed this spool. This does not restore deleted history or change the physical RFID tag.' : remote ? 'Archive this spool in Spoolman and remove it from the active inventory on this scale. Spoolman keeps its history, and you can restore it on the server. The RFID tag is unchanged.' : 'Delete this profile, its saved measurements and RFID link from this scale. This cannot be undone. The RFID tag is unchanged.',
    rows: [['Spool', spool.name], ['Material / brand', `${spool.material} / ${spool.brand}`], ['Destination', remote ? `Spoolman #${spool.spoolman_id}` : 'This scale']],
    label: recovery ? 'Check & finish archive' : remote ? 'Archive in Spoolman' : 'Remove from scale',
    guard: () => remote && !state.status?.spoolman?.configured ? 'Reconnect the original Spoolman server in Settings to archive this linked spool.' : inventoryActionGuard(spool.id, 'archive'),
    run: async () => {
      try { return await mutation('/api/spools/remove', { id: spool.id, confirm: true }, remote ? 'Spool archived in Spoolman.' : 'Spool removed from this scale.'); }
      catch (error) {
        if (error.data?.spool?.removal_pending) {
          const saved = inventoryActionSpool(state.spools, state.drafts, spool.id, 'archive');
          if (saved) saved.removal_pending = true;
          $('#action-title').textContent = 'Archive needs checking';
          $('#action-confirm').textContent = 'Check & finish archive';
        }
        await loadPrivate(false);
        throw error;
      }
    },
  });
}

function showUnlinkDraft(spool) {
  if (state.busy) return;
  confirmAction({
    title: 'Unlink this RFID?',
    description: 'Remove this saved RFID association from the scale so the tag can identify another spool. The physical tag, Spoolman record and server history remain unchanged. The tag does not need to be at the reader.',
    rows: [['Spool', spool.name], ['Tag ID', spool.tag_uid], ['Spoolman', `#${spool.spoolman_id}`]],
    label: 'Unlink RFID',
    guard: () => inventoryActionGuard(spool.id, 'unlink') || (inventoryActionSpool(state.spools, state.drafts, spool.id, 'unlink')?.tag_uid !== spool.tag_uid ? 'The saved RFID link changed. Close this dialog and review it again.' : ''),
    run: () => mutation('/api/spools/tag', { id: spool.id, expected_uid: spool.tag_uid, unlink: true, confirm: true }, 'RFID link removed from this scale.'),
  });
}

function showDiscardDraft(spool) {
  if (state.busy) return;
  confirmAction({
    title: 'Discard this draft?',
    description: 'Delete this unfinished local profile and its saved RFID link from the scale. There is no pending server operation or linked Spoolman record. This cannot be undone.',
    rows: [['Spool', spool.name], ['Material / brand', `${spool.material} / ${spool.brand}`]],
    label: 'Discard draft',
    guard: () => inventoryActionGuard(spool.id, 'discard'),
    run: () => mutation('/api/spools/remove', { id: spool.id, confirm: true }, 'Local draft discarded.'),
  });
}

async function cloneBambuTag() {
  if (!requirePair()) return;
  let source;
  try {
    source = await mutation('/api/nfc/clone', { step: 'read' });
  } catch (error) { notice(error.message, true); return; }
  if (!source) return;
  confirmAction({
    title: 'Clone this Bambu tag?',
    description: 'The source tag has been read. Place a blank Gen2/CUID or FUID magic card on the reader and keep it still. The card will become a byte-for-byte copy, including its ID. A non-magic card is refused without changes.',
    rows: [['Source', source.label || 'Bambu Lab tag'], ['Source tag ID', source.source_uid || 'Unknown'], ['Target', 'Magic card on the reader']],
    label: 'Clone to magic card',
    guard: () => !state.token ? 'Pair with the device first.' : '',
    run: () => mutation('/api/nfc/clone', { step: 'write', confirm: true }, 'The magic card was cloned and verified.'),
  });
}
function showTagLink(spool) {
  if (state.busy || !spool || !requirePair()) return;
  tagLink = { spool, uid: '', error: '' };
  $('#tag-link-title').textContent = spool.tag_uid ? 'Change RFID tag' : 'Add RFID tag';
  details($('#tag-link-spool'), [['Spool', spool.name], ['Material / brand', `${spool.material} / ${spool.brand}`], ['Current RFID', spool.tag_uid || 'Not linked']]);
  $('#tag-link-dialog').showModal();
  renderTagLink();
}

function tagLinkIssue() {
  if (!tagLink) return 'Choose an existing spool first.';
  return inventoryActionGuard(tagLink.spool.id) || tagLinkGuard(state.status, state.spools.find(item => String(item.id) === String(tagLink.spool.id)), tagLink.uid, state.spools, isFresh());
}

function renderTagLink() {
  if (!tagLink) return;
  $('#tag-link-uid').textContent = tagLink.uid || 'Not scanned';
  $('#tag-link-scan').disabled = state.busy || !!inventoryActionGuard(tagLink.spool.id);
  setButtonText($('#tag-link-scan'), state.busy ? 'Please wait' : tagLink.uid ? 'Scan again' : 'Scan tag');
  $('#tag-link-confirm').disabled = state.busy || !!tagLinkIssue();
  $('#tag-link-error').textContent = tagLink.error || (tagLink.uid ? tagLinkIssue() : inventoryActionGuard(tagLink.spool.id));
}

function measurementGuard(id) {
  if (!state.token) return 'Pair with the device before saving.';
  if (String(state.status?.active_spool_id) !== String(id)) return 'The active spool changed. Cancel and review the current spool.';
  if (state.status?.spoolman?.configured && state.inventory?.stale) return 'Refresh the Spoolman inventory before saving a measurement.';
  const m = currentMeasurement();
  return m.canSync ? '' : m.message;
}

function showTare() {
  confirmAction({ title: 'Zero the empty scale', description: 'Remove the spool and anything else resting on the cradle. Leave the printed cradle, reader and wiring installed. This changes scale zero, not the saved empty-spool weight.', label: 'Scale is empty / zero now', guard: () => scaleZeroGuard(state.status, isFresh()), run: async () => {
    state.calibrationZeroed = false;
    state.calibrationReference = null;
    try {
      const result = await mutation('/api/scale/tare', {}, 'Scale zero updated.');
      if (!result) return;
      state.calibrationZeroed = true;
      calibrationResult('Empty-cradle zero saved. Place your known reference weight on the cradle, then complete step 2.');
      render();
      return result;
    } catch (error) { calibrationResult(error.message, true); throw error; }
  } });
}

function showWrite() {
  const spool = activeSpool();
  if (!spool) return;
  const uid = state.status?.nfc?.uid;
  const format = TAG_FORMATS.find(item => item.id === $('#tag-format').value) || TAG_FORMATS[0];
  const printer = format.id !== 'spoolancer';
  const article = /^[aeiou]/i.test(format.label) ? 'an' : 'a';
  const description = printer
    ? `This replaces the tag’s current contents with ${article} ${format.label} record so the printer’s stock firmware recognizes the material and color. ${format.note} The tag ID is then linked to this spool on the scale. Keep the tag still until the device verifies it.`
    : 'This replaces the supported tag’s inventory record with the selected profile. Keep the same tag still at the reader until the device verifies it. This format does not create tags for stock printers.';
  confirmAction({ title: printer ? `Write ${article} ${format.label} tag?` : 'Write this inventory tag?', description, rows: [['Spool', spool.name], ['Material / color', `${spool.material} / ${spool.color}`], ['Expected tag ID', uid || 'No tag'], ['Format', printer ? format.label : 'Spoolancer inventory / schema 1']], label: 'Write & verify tag', guard: () => !state.token ? 'Pair with the device first.' : String(state.status?.active_spool_id) !== String(spool.id) ? 'The active spool changed. Cancel and review it again.' : tagWriteGuard(state.status, uid, isFresh(), format.id), run: () => mutation('/api/nfc/write', { spool_id: spool.id, expected_uid: uid, format: format.id, confirm: true }, 'The device completed the write and read-back check.') });
}

function showSync() {
  const spool = activeSpool();
  if (!spool) return;
  const m = currentMeasurement();
  const linked = !!spool.spoolman_id;
  confirmAction({ title: linked ? 'Send this measurement?' : 'Create a Spoolman spool?', description: linked ? 'The device will update the linked spool with the current stable remaining weight. It will check the measurement again before sending.' : 'The device will create the required vendor, filament and spool entries, then send the current stable remaining weight. Link an existing spool ID in Edit if it already exists. Verify the density and diameter below.', rows: [['Spool', spool.name], ['Remaining now', `${grams(m.remaining)} g`], ['Spoolman ID', linked ? String(spool.spoolman_id) : 'Create a new entry'], ...(linked ? [] : [['Density / diameter', `${spool.density_g_cm3} g/cm³ / ${spool.diameter_mm} mm`]])], label: linked ? 'Send to Spoolman' : 'Create & send', guard: () => measurementGuard(spool.id) || (!state.status?.spoolman?.configured ? 'Configure Spoolman first.' : state.status.spoolman.pending ? 'Check the server and resolve the pending spool link in Edit before retrying.' : ''), run: () => mutation('/api/spoolman/sync', { spool_id: spool.id, confirm: true }, 'The device received confirmation from Spoolman.') });
}

function setView() {
  let requested = location.hash.slice(1);
  if (requested === 'calibration') { history.replaceState(null, '', '#settings/calibration'); requested = 'settings/calibration'; }
  const route = requested.startsWith('settings') ? 'settings' : requested;
  const view = !state.status ? 'loading' : needsSetup() ? 'setup' : setupFinished ? 'setup-result' : ['scale', 'inventory', 'settings'].includes(route) ? route : 'scale';
  $('.tabs').hidden = ['loading', 'setup', 'setup-result'].includes(view);
  for (const section of document.querySelectorAll('.view')) section.hidden = section.id !== `view-${view}`;
  const panel = requested === 'settings/calibration' ? 'calibration' : requested === 'settings/hardware' ? 'hardware' : 'connections';
  for (const name of ['connections', 'calibration', 'hardware']) $(`#settings-${name}`).hidden = panel !== name;
  for (const link of document.querySelectorAll('[data-settings]')) {
    if (link.dataset.settings === panel) link.setAttribute('aria-current', 'page');
    else link.removeAttribute('aria-current');
  }
  for (const link of document.querySelectorAll('[data-view]')) {
    if (link.dataset.view === view) link.setAttribute('aria-current', 'page');
    else link.removeAttribute('aria-current');
  }
  if (view === 'inventory' && visibleView !== view) renderInventory();
  visibleView = view;
}

$('#notice button').addEventListener('click', () => { $('#notice').hidden = true; });
$('#pair-button').addEventListener('click', () => { if (state.token) { lock(); notice('Session locked. Pair again to make changes.'); } else requirePair(); });
for (const button of document.querySelectorAll('[data-close]')) button.addEventListener('click', () => button.closest('dialog').close());
$('#action-dialog').addEventListener('close', () => { state.action = null; });
$('#tag-link-dialog').addEventListener('close', () => { tagLink = null; });
$('#pair-dialog').addEventListener('close', () => { $('#pair-form [name=pin]').value = ''; });
$('#pair-form').addEventListener('submit', async event => {
  event.preventDefault();
  if (state.busy) return;
  const pin = event.currentTarget.elements.pin.value;
  $('#pair-error').textContent = '';
  let body;
  try { body = sessionBody(state.status, isFresh(), pin); }
  catch (error) { $('#pair-error').textContent = error.message; return; }
  event.currentTarget.elements.pin.value = '';
  setBusy(true);
  try {
    const result = await api('/api/session', { method: 'POST', body });
    if (!result.token || typeof result.token !== 'string') throw new Error('The device did not provide a session.');
    state.token = result.token;
    $('#pair-dialog').close();
    await loadPrivate();
    notice(body.setup ? 'PIN saved on the device. Open Settings to configure your Wi-Fi. Keep your PIN safe; this page does not store it.' : 'Paired with the device. Changes are enabled in this tab.');
  } catch (error) { $('#pair-error').textContent = error.message; }
  finally {
    try { await refreshStatus(); } catch { state.lastSeen = 0; }
    setBusy(false);
  }
});
$('#spool-form').addEventListener('submit', async event => {
  event.preventDefault();
  if (state.busy || !requirePair() || state.editedSpool?.metadata_readonly) return;
  $('#spool-error').textContent = '';
  const form = event.currentTarget;
  let fields = {};
  try {
    fields = Object.fromEntries(new FormData(form));
    fields.material = resolveMaterial($('#material-select').value, $('#material-custom').value);
    fields.color = normalizeHex($('#color-hex').value);
    if (!fields.id && fields.spool_condition === 'full') fields.empty_weight_g = fullSpoolTare(state.status, isFresh(), fields.initial_weight_g, 'full').empty;
    const spool = validateSpool(fields);
    const remote = state.status?.spoolman?.configured === true;
    const result = await mutation('/api/spools', { ...spool, ...(fields.spool_condition === 'full' && !fields.id ? { infer_empty_from_measurement: true } : {}), spool_condition: fields.spool_condition || 'used', ...(remote ? { confirm: true } : {}) }, remote ? 'Spool confirmed by Spoolman.' : 'Spool saved on the scale.');
    if (result) { $('#spool-dialog').close(); if (result.spool?.id || result.id) await chooseSpool(result.spool?.id || result.id); }
  } catch (error) {
    if (error.data?.draft && error.data.spool?.id) {
      form.elements.id.value = error.data.spool.id;
      // Keep the inferred tare visible; the retry path no longer recomputes it.
      if (typeof error.data.spool.empty_weight_g === 'number') form.elements.empty_weight_g.value = error.data.spool.empty_weight_g;
      else if (typeof fields.empty_weight_g === 'number') form.elements.empty_weight_g.value = fields.empty_weight_g;
      state.editedSpool = error.data.spool;
      state.editedSpool.recovery_required = error.data.recovery_required === true;
      $('#spool-condition').hidden = true;
      $('#spool-link-fields').open = true;
      await loadPrivate(false);
    }
    $('#spool-error').textContent = error.message;
  }
});
$('#action-confirm').addEventListener('click', async () => {
  const action = state.action;
  if (!action || state.busy) return;
  const issue = action.guard?.();
  if (issue) { $('#action-error').textContent = issue; return; }
  try {
    const result = await action.run();
    if (result) $('#action-dialog').close();
  } catch (error) { if (state.action) state.action.error = error.message; $('#action-error').textContent = error.message; notice(error.message, true); }
});
for (const selector of ['#new-spool', '#new-spool-scale', '#new-spool-empty']) $(selector).addEventListener('click', () => editSpool());
$('#edit-active').addEventListener('click', () => editSpool(activeSpool()));
$('#select-spool').addEventListener('click', () => chooseSpool($('#active-select').value));
$('#active-select').addEventListener('change', render);
$('#inventory-search').addEventListener('input', renderInventory);
$('#tare-button').addEventListener('click', showTare);
$('#tare-calibration').addEventListener('click', showTare);
$('#calibration-pair').addEventListener('click', requirePair);
$('#calibration-restart').addEventListener('click', () => {
  state.calibrationZeroed = false;
  state.calibrationReference = null;
  $('#calibration-result').textContent = '';
  render();
  $('#tare-calibration').focus();
});
$('#write-tag').addEventListener('click', showWrite);
for (const format of TAG_FORMATS) $('#tag-format').append(new Option(format.label, format.id));
$('#tag-format').addEventListener('change', render);
$('#link-tag').addEventListener('click', () => showTagLink(activeSpool()));
$('#tag-link-scan').addEventListener('click', async () => {
  const pending = tagLink;
  if (!pending || state.busy || inventoryActionGuard(pending.spool.id)) return;
  pending.uid = ''; pending.error = '';
  try {
    const result = await mutation('/api/nfc/read', {});
    if (tagLink !== pending) return;
    if (result?.nfc?.present !== true || !result.nfc.uid) throw new Error('No single tag was detected. Position one tag at the reader and scan again.');
    pending.uid = result.nfc.uid;
  } catch (error) { if (tagLink === pending) pending.error = error.message; }
  renderTagLink();
});
$('#tag-link-confirm').addEventListener('click', async () => {
  const pending = tagLink;
  if (!pending || state.busy) return;
  const issue = tagLinkIssue();
  if (issue) { pending.error = issue; renderTagLink(); return; }
  pending.error = '';
  try {
    const result = await mutation('/api/spools/tag', { id: pending.spool.id, expected_uid: pending.uid, confirm: true }, 'RFID linked on this scale. The tag was not modified.');
    if (result && tagLink === pending) $('#tag-link-dialog').close();
  } catch (error) { if (tagLink === pending) { pending.error = error.message; renderTagLink(); } }
});
$('#sync-button').addEventListener('click', showSync);
$('#save-measurement').addEventListener('click', () => {
  const spool = activeSpool();
  if (!spool) return;
  if (state.status?.spoolman?.configured) { showSync(); return; }
  confirmAction({ title: 'Save this measurement?', description: 'Save the current stable gross and remaining weight to this local spool record. Spoolman is not required. The device checks the reading again before saving.', rows: [['Spool', spool.name], ['Remaining now', `${grams(currentMeasurement().remaining)} g`]], label: 'Save locally', guard: () => measurementGuard(spool.id), run: () => mutation('/api/measurements', { spool_id: spool.id, confirm: true }, 'Measurement saved on the device.') });
});
$('#read-tag').addEventListener('click', async () => {
  try {
    const result = await mutation('/api/nfc/read', {}, 'Tag read completed.');
    const profile = result?.nfc?.metadata;
    if (profile?.source === 'printer_tag') notice(`${result.message || 'Tag read.'} ${profile.brand || 'Printer'} ${profile.material || ''} ${profile.color || ''} (read-only printer tag).`);
    else if (profile?.name) notice(`${result.message || 'Tag read.'} Tag profile: ${profile.name}, ${profile.brand || 'brand unknown'}, ${profile.material || 'material unknown'}.`);
  } catch (error) { notice(error.message, true); }
});
$('#clone-tag').addEventListener('click', cloneBambuTag);
$('#refresh-button').addEventListener('click', async () => {
  try { await refreshStatus(); await loadPrivate(true, true); } catch (error) { state.lastSeen = 0; render(); notice(error.message, true); }
});
for (const picker of networkPickers) {
  picker.root.querySelector('.scan-button').addEventListener('click', scanNetworks);
  picker.root.querySelector('select').addEventListener('change', () => {
    picker.root.querySelector('[name=wifi_password]').value = '';
    picker.root.querySelector('[name=wifi_open]').checked = false;
    if (picker.name === 'config') { state.configDirty = true; state.connectionDirty = true; }
    renderNetwork(picker);
  });
  picker.root.querySelector('[name=wifi_open]').addEventListener('change', event => {
    if (event.target.checked) picker.root.querySelector('[name=wifi_password]').value = '';
    renderNetwork(picker);
  });
  picker.root.querySelector('[name=wifi_ssid]').addEventListener('input', () => renderNetwork(picker));
}
$('#setup-form [name=pin_required]').addEventListener('change', event => {
  if (!event.target.checked) { $('#setup-form').elements.pin.value = ''; $('#setup-form').elements.pin_confirm.value = ''; }
  renderSetup();
});
$('#setup-form').addEventListener('submit', async event => {
  event.preventDefault();
  if (state.busy || scanState === 'scanning') return;
  const form = event.currentTarget;
  $('#setup-error').textContent = '';
  let body;
  try {
    if (form.elements.pin_required.checked && form.elements.pin.value !== form.elements.pin_confirm.value) throw new Error('The PINs do not match. Enter the same 8 digits in both fields.');
    body = setupBody(state.status, isFresh(), { ...networkFields(networkPickers[0]), pin_required: form.elements.pin_required.checked, pin: form.elements.pin.value });
  } catch (error) { $('#setup-error').textContent = error.message; return; }
  setBusy(true);
  try {
    const result = await api('/api/setup', { method: 'POST', body });
    if (typeof result.token !== 'string' || !result.token) throw new Error('Setup may have been saved, but the device did not provide a session. Reconnect to check its state.');
    state.token = result.token;
    setupFinished = true;
    customDemoAccess = state.demo;
    form.elements.pin.value = ''; form.elements.pin_confirm.value = ''; form.elements.wifi_password.value = '';
    state.status.setup.required = false;
    state.status.setup.complete = true;
    state.status.auth = { pin_required: body.pin_required };
    try { await refreshStatus(); await loadPrivate(false); } catch { state.lastSeen = 0; }
  } catch (error) {
    $('#setup-error').textContent = error.message;
    try { await refreshStatus(); } catch { state.lastSeen = 0; }
    if (!needsSetup()) notice(error.message, true);
  } finally { setBusy(false); }
});
function finishSetupView(view) { setupFinished = false; location.hash = view; setView(); }
$('#setup-continue').addEventListener('click', () => finishSetupView('scale'));
$('#setup-edit-wifi').addEventListener('click', () => finishSetupView('settings'));
$('#config-form').addEventListener('input', event => { state.configDirty = true; if (event.target.name === 'spoolman_url') state.serverDirty = true; else state.connectionDirty = true; });
$('#config-form').addEventListener('submit', async event => {
  event.preventDefault();
  if (!requirePair()) return;
  const form = event.currentTarget;
  let body;
  $('#config-result').textContent = '';
  setBusy(true);
  try {
    const fields = networkFields(networkPickers[1]);
    const credentials = wifiCredentials(fields, { allowSavedPassword: fields.wifi_ssid === savedNetwork.ssid && savedNetwork.hasPassword });
    body = { device_name: form.elements.device_name.value.trim(), ...credentials, spoolman_url: form.elements.spoolman_url.value.trim() };
    if (!body.wifi_password && !body.wifi_open) delete body.wifi_password;
    if (!body.device_name) throw new Error('Enter a device name.');
    if (!validSpoolmanUrl(body.spoolman_url)) throw new Error('Use a numeric IPv4 address, such as http://192.168.1.50:7912. Hostnames, HTTPS, credentials, queries and fragments are not supported.');
    const result = await api('/api/config', { method: 'POST', body });
    savedNetwork = { ssid: body.wifi_ssid, hasPassword: !body.wifi_open };
    state.lastPrivate = 0;
    form.elements.wifi_password.value = '';
    state.configDirty = false; state.connectionDirty = false; state.serverDirty = false;
    savedSpoolmanUrl = body.spoolman_url;
    const message = result.reconnect_required ? usesOpenSetup(state.status) ? 'Settings saved. Wi-Fi is reconnecting. Reopen this page at the device’s new address. The temporary setup network closes after 10 minutes, or 60 seconds after connecting. If it has closed and Wi-Fi fails, hold BOOT for 8 seconds while the scale is running to reopen setup.' : `Settings saved. Wi-Fi is reconnecting. Reopen this page at the device's new address. If it cannot connect, join ${result.ap_ssid || 'its setup access point'} and open ${result.ip || '192.168.4.1'}.` : 'Settings saved on the device.';
    $('#config-result').textContent = `${state.demo ? 'Demo: ' : ''}${message}`;
    notice(message);
    if (result.reconnect_required) { state.lastSeen = 0; render(); }
    else { await refreshStatus(); await loadPrivate(false); }
  } catch (error) { $('#config-result').textContent = error.message; notice(error.message, true); }
  finally { setBusy(false); }
});
$('#save-server').addEventListener('click', async () => {
  if (!requirePair() || state.busy) return;
  const url = $('#config-form').elements.spoolman_url.value.trim();
  $('#server-result').textContent = '';
  if (!validSpoolmanUrl(url)) { $('#server-result').textContent = 'Enter an HTTP IPv4 address, including the server port.'; return; }
  setBusy(true);
  try {
    await api('/api/config', { method: 'POST', body: { spoolman_url: url } });
    savedSpoolmanUrl = url; state.serverDirty = false; state.configDirty = state.connectionDirty;
    await refreshStatus(); await loadPrivate(false, true);
    $('#server-result').textContent = state.inventory?.error ? `Address saved. ${state.inventory.error}` : url ? 'Server saved. Inventory now comes from Spoolman.' : 'Spoolman disabled. Inventory is stored on this scale.';
  } catch (error) { $('#server-result').textContent = error.message; }
  finally { setBusy(false); }
});
$('#test-spoolman').addEventListener('click', async () => {
  try {
    if ($('#config-form').elements.spoolman_url.value.trim() !== savedSpoolmanUrl) throw new Error('Save the server address before testing it.');
    await mutation('/api/spoolman/test', {}, 'The device reached the configured Spoolman server.');
  } catch (error) { notice(error.message, true); }
});
for (const component of ['nfc', 'leds', 'scale']) $(`#test-${component}`).addEventListener('click', () => { void runDiagnostic(component); });
$('#firmware-check').addEventListener('click', () => { void checkFirmware(); });
$('#firmware-apply').addEventListener('click', () => { void applyFirmware(); });

$('#calibration-form').addEventListener('submit', event => {
  event.preventDefault();
  const reference = Number(event.currentTarget.elements.reference_g.value);
  if (!Number.isFinite(reference) || reference < 100 || reference > 2000) { notice('Enter a known reference mass from 100 to 2,000 g.', true); return; }
  confirmAction({ title: 'Save this calibration?', description: 'Place only this known reference weight on the zeroed cradle and keep it still. Use its actual weight, not the load cell capacity. This changes the weight conversion for every spool.', rows: [['Reference weight', `${grams(reference)} g`]], label: 'Save calibration', guard: () => scaleCalibrationGuard(state.status, isFresh(), state.calibrationZeroed), run: async () => {
    try {
      const result = await mutation('/api/scale/calibrate', { reference_g: reference }, 'The device saved the calibration.');
      if (!result) return;
      state.calibrationReference = reference;
      calibrationResult(`Calibration saved using ${grams(reference)} g${state.demo ? ' in this preview only' : ' on the device'}. Remove the reference and check zero, then verify with another known weight.`);
      render();
      return result;
    } catch (error) { calibrationResult(error.message, true); throw error; }
  } });
});
window.addEventListener('hashchange', () => { setView(); maybeScanNetworks(); });
$('#refresh-inventory').addEventListener('click', async () => { if (!requirePair() || state.busy) return; setBusy(true); try { await loadPrivate(true, true); } finally { setBusy(false); } });
$('#discover-spoolman').addEventListener('click', discoverSpoolman);
$('#spool-form').addEventListener('input', renderSpoolMass);
$('#spool-form').addEventListener('change', renderSpoolMass);

setupTheme();
setupEditorControls();
setupIcons();
const demoRequested = new URLSearchParams(location.search).get('demo') === '1';
if (demoRequested && ['localhost', '127.0.0.1', '[::1]'].includes(location.hostname)) {
  const { createDemo } = await import('./demo.js');
  const demo = createDemo();
  state.demo = true;
  transport = demo.request;
  $('#demo-banner').hidden = false;
  $('#footer-mode').textContent = 'Demo only / PIN 12345678';
  $('#pair-form [name=pin]').placeholder = 'Demo PIN: 12345678';
  $('#demo-scenario').addEventListener('change', async event => {
    const resetSession = event.target.value === 'headless-setup' || state.status?.device?.headless;
    demo.setScenario(event.target.value);
    diagnosticResults = {};
    lastDiscovered = false; discovery = { state: 'idle', candidates: [], message: '' };
    scanGeneration++; scanState = 'idle'; scanMessage = ''; scannedNetworks = []; setupFinished = false;
    savedNetwork = { ssid: '', hasPassword: false }; nextSessionAttempt = 0;
    if (event.target.value === 'headless-setup') customDemoAccess = false;
    $('#setup-form').reset(); $('#setup-error').textContent = '';
    $('#config-result').textContent = ''; $('#notice').hidden = true;
    for (const picker of networkPickers) populateNetworks(picker, '');
    state.calibrationZeroed = false;
    state.calibrationReference = null;
    $('#calibration-result').textContent = '';
    if (resetSession) lock();
    $('#pair-form [name=pin]').placeholder = event.target.value === 'headless-setup' ? 'Choose 8 digits' : 'Demo PIN: 12345678';
    $('#footer-mode').textContent = event.target.value === 'headless-setup' ? 'Demo only / choose your own preview PIN' : 'Demo only / PIN 12345678';
    await poll(); await loadPrivate(false);
  });
} else if (demoRequested) notice('Demo mode is available only on a local desktop preview. This page will use the real device API.', true);
setView();
renderInventory();
render();
await poll();
setInterval(() => { if (!document.hidden) void poll(); }, 500);
document.addEventListener('visibilitychange', () => { if (!document.hidden) void poll(); });
setInterval(render, 1000);
