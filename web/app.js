import { validateSpool, measurement, tagWriteGuard, grams, validSpoolmanUrl, COMMON_MATERIALS, COLOR_SWATCHES, normalizeHex, materialSelection, resolveMaterial, THEME_STORAGE_KEY, normalizeTheme, resolveTheme, readThemePreference, saveThemePreference, pairingState, sessionBody, usesOpenSetup, scaleZeroGuard, scaleCalibrationGuard } from './logic.js';

const $ = selector => document.querySelector(selector);
const state = { token: '', status: null, spools: [], lastSeen: 0, busy: false, configDirty: false, privateLoaded: false, action: null, demo: false, lastPrivate: 0, calibrationZeroed: false, calibrationReference: null };
const isFresh = () => Date.now() - state.lastSeen < 4500;
const activeSpool = () => state.spools.find(s => String(s.id) === String(state.status?.active_spool_id));
const currentMeasurement = () => measurement(state.status, activeSpool(), isFresh());
let transport = deviceRequest;
let polling = false;

function node(tag, text, className) {
  const element = document.createElement(tag);
  if (text !== undefined) element.textContent = text;
  if (className) element.className = className;
  return element;
}

async function deviceRequest(path, { method = 'GET', body } = {}) {
  const controller = new AbortController();
  const duration = ['/api/nfc/write', '/api/spoolman/sync'].includes(path) ? 30000 : ['/api/nfc/read', '/api/spoolman/test'].includes(path) ? 20000 : 8000;
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
      throw error;
    }
    return data;
  } catch (error) {
    if (error.name === 'AbortError') throw new Error(method === 'POST' ? 'The device did not confirm completion in time. The action may have finished. Reconnect and inspect the tag or saved record before retrying.' : 'The device did not respond. Check the connection before retrying.');
    throw error;
  } finally { clearTimeout(timeout); }
}

async function api(path, options) {
  try {
    const result = await transport(path, { ...options, token: state.token });
    if (options?.method === 'POST' && path !== '/api/session' && result?.ok !== true) throw new Error('The device did not confirm this action. Check its state before repeating it.');
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
  if (!preserveEdits || !state.configDirty) { state.configDirty = false; $('#config-form').reset(); }
  if (!preserveEdits) $('#spool-form').reset();
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
  if (!requirePair()) return null;
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

async function loadPrivate(reportError = true) {
  if (!state.token) return;
  const token = state.token;
  const results = await Promise.allSettled([api('/api/spools'), api('/api/config')]);
  if (!state.token || state.token !== token) return;
  if (results[0].status === 'fulfilled' && Array.isArray(results[0].value.spools)) {
    state.spools = results[0].value.spools;
    state.privateLoaded = true;
    state.lastPrivate = Date.now();
    renderInventory();
    populateSelector();
  }
  if (results[1].status === 'fulfilled' && !state.configDirty) {
    const config = results[1].value;
    const form = $('#config-form');
    for (const field of ['device_name', 'wifi_ssid', 'spoolman_url']) form.elements[field].value = config[field] || '';
    form.elements.wifi_password.value = '';
    form.elements.clear_wifi_password.checked = false;
    $('#password-status').textContent = config.has_wifi_password ? 'A Wi-Fi password is saved. Leave the field blank to keep it.' : 'No Wi-Fi password is saved.';
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
    if (state.token && Date.now() - state.lastPrivate > 15000 && !state.busy) await loadPrivate(false);
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
  $('#device-name').textContent = status?.device?.name || 'Spool scale';
  $('#connection').className = `connection ${fresh ? 'online' : 'offline'}`;
  $('#connection span').textContent = state.busy ? 'Action in progress' : fresh ? (state.demo ? 'Demo connected' : 'Device connected') : 'Device disconnected';
  $('#pair-button').textContent = state.token ? 'Lock session' : pairingState(status, fresh).required ? 'Set up device' : 'Pair device';
  renderPairing();
  $('#session-state').textContent = state.token ? 'Paired in this tab' : 'View only';
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
  $('#active-select').disabled = !state.token || !fresh || state.busy;
  $('#select-spool').disabled = !state.token || !fresh || !$('#active-select').value || state.busy;
  const tag = fresh ? status?.nfc || {} : {};
  $('#tag-state').textContent = !fresh ? 'Unavailable' : tag.present ? tag.writable ? 'Writable' : 'Read-only' : 'No tag';
  $('#tag-state').className = `pill ${tag.present && tag.writable ? '' : 'neutral'}`;
  $('#tag-type').textContent = tag.type || '—';
  $('#tag-uid').textContent = tag.uid || '—';
  $('#tag-message').textContent = tag.last_error || tag.last_message || tag.message || 'Position the tag over the reader. Only writable inventory tags can be edited.';
  $('#read-tag').disabled = !fresh || state.busy;
  $('#write-tag').disabled = !spool || !!tagWriteGuard(status, null, fresh) || state.busy;
  $('#link-tag').disabled = !spool || !tag.present || !tag.uid || state.busy;
  const sm = status?.spoolman || {};
  $('#sync-state').textContent = !fresh ? 'Unavailable' : sm.last_error ? 'Needs attention' : sm.pending ? 'Pending' : sm.configured ? 'Configured' : 'Not configured';
  $('#sync-state').className = `pill ${sm.last_error ? 'error' : sm.pending ? 'warning' : 'neutral'}`;
  $('#sync-message').textContent = sm.last_error || sm.last_message || (sm.configured ? 'Send the stable measurement for the selected spool. The device checks the server response.' : 'Connect your server in Settings. Local weighing works without it.');
  $('#linked-spool').textContent = spool?.spoolman_id ? `Spool #${spool.spoolman_id}` : 'Not linked';
  $('#sync-weight').textContent = m.canSync ? `${grams(m.remaining)} g remaining` : 'No valid measurement';
  $('#sync-button').textContent = sm.pending ? 'Resolve the pending link in Edit' : spool?.spoolman_id ? 'Send measurement' : 'Create in Spoolman & send';
  $('#sync-button').disabled = !m.canSync || !sm.configured || !!sm.pending || state.busy;
  $('#save-measurement').disabled = !m.canSync || state.busy;
  $('#device-ip').textContent = fresh ? status?.network?.ip || 'Not assigned' : 'Unavailable';
  $('#wifi-state').textContent = fresh ? status?.network?.connected ? 'Connected' : status?.setup?.ap_active ? 'Temporary setup network' : 'Not connected' : 'Unknown';
  $('#tare-button').disabled = !!scaleZeroGuard(status, fresh) || state.busy;
  for (const input of $('#config-form').querySelectorAll('input,button')) input.disabled = !state.token || !fresh || state.busy;
  renderCalibration();
  if ($('#action-dialog').open && state.action) {
    const issue = state.action.guard?.() || '';
    $('#action-error').textContent = issue || state.action.error || '';
    $('#action-confirm').disabled = !!issue || state.busy;
  }
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
  $('#calibration-reading-state').textContent = !fresh ? 'No live connection.' : state.busy ? 'Waiting for the action to finish.' : !sensorFresh ? 'Waiting for new sensor readings.' : !calibrated ? 'Complete steps 1 and 2 to see grams.' : `${state.demo ? 'Simulated' : 'Live'} gross weight · ${scale.stable === true ? 'stable' : 'settling'}${state.calibrationReference === null ? '. Uses the previously stored calibration.' : '.'}`;
}

function calibrationResult(message, error = false) {
  $('#calibration-result').textContent = `${state.demo ? 'Demo: ' : ''}${message}`;
  $('#calibration-result').className = error ? 'form-error' : 'inline-result';
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
  $('#pair-description').textContent = mode.required
    ? mode.eligible ? 'Choose an 8-digit PIN and keep it somewhere safe. With the scale running, hold and release BOOT for 2 seconds, then submit within 60 seconds. Holding it for 8 seconds resets setup instead.' : 'Join the temporary setup Wi-Fi and open http://192.168.4.1. If that network has closed, hold BOOT for 8 seconds while the scale is running to reopen setup.'
    : openSetup ? 'Enter your saved 8-digit PIN to enable changes in this browser tab. If you forgot it, hold BOOT for 8 seconds while the scale is running and set up a new PIN on the temporary Wi-Fi.' : 'Enter the 8-digit PIN shown on the device or in its Serial output. Pairing enables changes in this browser tab.';
  $('#enrollment-warning').hidden = !mode.eligible;
  $('#enrollment-status').hidden = !mode.required;
  $('#enrollment-status').textContent = !fresh ? 'Waiting for current device status.' : !mode.eligible ? 'PIN enrollment is available only through the temporary setup network.' : mode.armed ? `BOOT confirmed. One enrollment attempt is available for ${setup.enrollment_seconds} seconds.` : 'Waiting for a 2-second BOOT press and release. Each submission uses one attempt, even if the device rejects the PIN.';
  $('#pin-field').hidden = mode.required && !mode.eligible;
  $('#pin-label').textContent = mode.required ? 'Choose an 8-digit PIN' : 'Device PIN';
  $('#pair-form [name=pin]').disabled = state.busy || (mode.required && !mode.eligible);
  $('#pair-submit').textContent = mode.required ? 'Save PIN & pair' : 'Pair device';
  $('#pair-submit').disabled = state.busy || !mode.canSubmit;
  $('#pairing-help').textContent = openSetup ? 'Use your chosen PIN. For first setup or a forgotten PIN, hold BOOT for 8 seconds while the scale is running, join its temporary Wi-Fi, then open http://192.168.4.1. Sessions stay in this tab’s memory and clear on reload.' : 'Pair using the PIN shown by your device or Serial output. The session stays in this tab’s memory and is cleared on reload.';
  $('#wifi-help').textContent = openSetup ? 'Wi-Fi credentials are sent over local HTTP. The temporary setup network closes after 10 minutes, or 60 seconds after connecting to your Wi-Fi. If connection fails after it closes, hold BOOT for 8 seconds while the scale is running to reopen setup.' : 'A Wi-Fi change can disconnect this page. Reopen it at the device’s new address. The device keeps its setup access point available if connection fails.';
  if (state.demo) $('#demo-arm').hidden = !mode.eligible;
}

function populateSelector(followActive = false) {
  const select = $('#active-select');
  const previous = select.value;
  select.replaceChildren(node('option', 'Choose a spool…'));
  select.firstChild.value = '';
  for (const spool of state.spools) {
    const option = node('option', `${spool.name} · ${spool.material}`);
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
  $('#spool-count').textContent = state.token ? state.spools.length : '—';
  $('#inventory-total').textContent = !state.token ? 'Pair to load private inventory' : `${matches.length} of ${state.spools.length} spools`;
  $('#inventory-empty').hidden = matches.length > 0;
  $('#inventory-empty h2').textContent = !state.token ? 'Your inventory is private' : query ? 'No matching spools' : 'No spools yet';
  $('#inventory-empty p').textContent = !state.token ? 'Pair with your scale to see and edit its saved spool profiles.' : query ? 'Try another name, brand or material.' : 'Add a spool and its known empty weight to calculate remaining filament.';
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
    details(values, [['Empty spool', typeof spool.empty_weight_g === 'number' ? `${grams(spool.empty_weight_g)} g` : 'Unknown'], ['Last saved remaining', typeof spool.remaining_weight_g === 'number' ? `${grams(spool.remaining_weight_g)} g` : 'No measurement'], ['Spoolman', spool.spoolman_id ? `#${spool.spoolman_id}` : 'Not linked']]);
    const actions = node('div', undefined, 'button-row');
    const use = node('button', active ? 'Selected' : 'Use on scale', 'button secondary');
    use.disabled = active;
    use.addEventListener('click', () => chooseSpool(spool.id));
    const edit = node('button', 'Edit', 'button secondary');
    edit.addEventListener('click', () => editSpool(spool));
    actions.append(use, edit);
    card.append(top, identity, values, actions);
    list.append(card);
  }
}

function editSpool(spool) {
  if (!requirePair()) return;
  const form = $('#spool-form');
  form.reset();
  const defaults = { id: '', name: '', brand: '', material: '', color: '#386c56', empty_weight_g: '', initial_weight_g: 1000, density_g_cm3: 1.24, diameter_mm: 1.75, spoolman_id: '' };
  for (const [key, fallback] of Object.entries(defaults)) form.elements[key].value = spool?.[key] ?? fallback;
  const material = materialSelection(spool?.material || '');
  $('#material-select').value = material.choice;
  $('#material-custom').value = material.custom;
  syncMaterialEditor();
  setEditorColor(spool?.color || defaults.color);
  $('#spool-dialog-title').textContent = spool ? 'Edit spool' : 'Add a spool';
  $('#spool-error').textContent = '';
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

function measurementGuard(id) {
  if (!state.token) return 'Pair with the device before saving.';
  if (String(state.status?.active_spool_id) !== String(id)) return 'The active spool changed. Cancel and review the current spool.';
  const m = currentMeasurement();
  return m.canSync ? '' : m.message;
}

function showTare() {
  confirmAction({ title: 'Zero the empty scale', description: 'Remove the spool and anything else resting on the cradle. Leave the printed cradle, reader and wiring installed. This changes scale zero, not the saved empty-spool weight.', label: 'Scale is empty · zero now', guard: () => scaleZeroGuard(state.status, isFresh()), run: async () => {
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
  confirmAction({ title: 'Write this inventory tag?', description: 'This replaces the supported tag’s inventory record with the selected profile. Keep the same tag still at the reader until the device verifies it. This format does not create tags for stock printers.', rows: [['Spool', spool.name], ['Material / color', `${spool.material} / ${spool.color}`], ['Expected tag UID', uid || 'No tag'], ['Format', 'Spoolancer inventory · schema 1']], label: 'Write & verify tag', guard: () => !state.token ? 'Pair with the device first.' : String(state.status?.active_spool_id) !== String(spool.id) ? 'The active spool changed. Cancel and review it again.' : tagWriteGuard(state.status, uid, isFresh()), run: () => mutation('/api/nfc/write', { spool_id: spool.id, expected_uid: uid, confirm: true }, 'The device completed the write and read-back check.') });
}

function showSync() {
  const spool = activeSpool();
  if (!spool) return;
  const m = currentMeasurement();
  const linked = !!spool.spoolman_id;
  confirmAction({ title: linked ? 'Send this measurement?' : 'Create a Spoolman spool?', description: linked ? 'The device will update the linked spool with the current stable remaining weight. It will check the measurement again before sending.' : 'The device will create the required vendor, filament and spool entries, then send the current stable remaining weight. Link an existing spool ID in Edit if it already exists. Verify the density and diameter below.', rows: [['Spool', spool.name], ['Remaining now', `${grams(m.remaining)} g`], ['Spoolman ID', linked ? String(spool.spoolman_id) : 'Create a new entry'], ...(linked ? [] : [['Density / diameter', `${spool.density_g_cm3} g/cm³ / ${spool.diameter_mm} mm`]])], label: linked ? 'Send to Spoolman' : 'Create & send', guard: () => measurementGuard(spool.id) || (!state.status?.spoolman?.configured ? 'Configure Spoolman first.' : state.status.spoolman.pending ? 'Check the server and resolve the pending spool link in Edit before retrying.' : ''), run: () => mutation('/api/spoolman/sync', { spool_id: spool.id, confirm: true }, 'The device received confirmation from Spoolman.') });
}

function setView() {
  const requested = location.hash.slice(1);
  const view = ['scale', 'calibration', 'inventory', 'settings'].includes(requested) ? requested : 'scale';
  for (const section of document.querySelectorAll('.view')) section.hidden = section.id !== `view-${view}`;
  for (const link of document.querySelectorAll('[data-view]')) {
    if (link.dataset.view === view) link.setAttribute('aria-current', 'page');
    else link.removeAttribute('aria-current');
  }
  if (view === 'inventory') renderInventory();
}

$('#notice button').addEventListener('click', () => { $('#notice').hidden = true; });
$('#pair-button').addEventListener('click', () => { if (state.token) { lock(); notice('Session locked. Pair again to make changes.'); } else requirePair(); });
for (const button of document.querySelectorAll('[data-close]')) button.addEventListener('click', () => button.closest('dialog').close());
$('#action-dialog').addEventListener('close', () => { state.action = null; });
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
  if (body.setup) { state.status.setup.enrollment_armed = false; state.status.setup.enrollment_seconds = 0; }
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
  $('#spool-error').textContent = '';
  try {
    const fields = Object.fromEntries(new FormData(event.currentTarget));
    fields.material = resolveMaterial($('#material-select').value, $('#material-custom').value);
    fields.color = normalizeHex($('#color-hex').value);
    const spool = validateSpool(fields);
    const result = await mutation('/api/spools', spool, 'Spool saved locally.');
    if (result) $('#spool-dialog').close();
  } catch (error) { $('#spool-error').textContent = error.message; }
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
$('#link-tag').addEventListener('click', () => {
  const spool = activeSpool();
  const uid = state.status?.nfc?.uid;
  if (!spool || !uid) return;
  confirmAction({ title: 'Link this tag locally?', description: 'Save this tag UID as an identifier for the selected spool. Future reads on this device will use this profile. The tag itself is unchanged, including original printer tags.', rows: [['Spool', spool.name], ['Material', spool.material], ['Tag UID', uid]], label: 'Link tag to spool', guard: () => !isFresh() || !state.status?.nfc?.present || state.status.nfc.uid !== uid ? 'The tag is missing or changed. Cancel and scan it again.' : String(state.status?.active_spool_id) !== String(spool.id) ? 'The active spool changed. Cancel and review it.' : '', run: () => mutation('/api/spools', { ...validateSpool(spool), tag_uid: uid }, 'Tag UID linked to this local spool. The tag was not modified.') });
});
$('#sync-button').addEventListener('click', showSync);
$('#save-measurement').addEventListener('click', () => {
  const spool = activeSpool();
  if (!spool) return;
  confirmAction({ title: 'Save this measurement?', description: 'Save the current stable gross and remaining weight to this local spool record. Spoolman is not required. The device checks the reading again before saving.', rows: [['Spool', spool.name], ['Remaining now', `${grams(currentMeasurement().remaining)} g`]], label: 'Save locally', guard: () => measurementGuard(spool.id), run: () => mutation('/api/measurements', { spool_id: spool.id, confirm: true }, 'Measurement saved on the device.') });
});
$('#read-tag').addEventListener('click', async () => {
  try {
    const result = await mutation('/api/nfc/read', {}, 'Tag read completed.');
    const profile = result?.nfc?.metadata;
    if (profile?.name) notice(`${result.message || 'Tag read.'} Tag profile: ${profile.name}, ${profile.brand || 'brand unknown'}, ${profile.material || 'material unknown'}.`);
  } catch (error) { notice(error.message, true); }
});
$('#refresh-button').addEventListener('click', async () => {
  try { await refreshStatus(); await loadPrivate(); } catch (error) { state.lastSeen = 0; render(); notice(error.message, true); }
});
$('#config-form').addEventListener('input', () => { state.configDirty = true; });
$('#config-form').addEventListener('submit', async event => {
  event.preventDefault();
  if (!requirePair()) return;
  const form = event.currentTarget;
  const body = { device_name: form.elements.device_name.value.trim(), wifi_ssid: form.elements.wifi_ssid.value, spoolman_url: form.elements.spoolman_url.value.trim(), clear_wifi_password: form.elements.clear_wifi_password.checked };
  if (form.elements.wifi_password.value) body.wifi_password = form.elements.wifi_password.value;
  $('#config-result').textContent = '';
  setBusy(true);
  try {
    if (!body.device_name) throw new Error('Enter a device name.');
    if (!validSpoolmanUrl(body.spoolman_url)) throw new Error('Use a numeric IPv4 address, such as http://192.168.1.50:7912. Hostnames, HTTPS, credentials, queries and fragments are not supported.');
    if (body.wifi_password && !body.clear_wifi_password && (new TextEncoder().encode(body.wifi_password).length < 8 || new TextEncoder().encode(body.wifi_password).length > 63)) throw new Error('Wi-Fi passwords must be 8–63 bytes. Use the clear-password checkbox for an open network.');
    const result = await api('/api/config', { method: 'POST', body });
    form.elements.wifi_password.value = '';
    state.configDirty = false;
    const message = result.reconnect_required ? usesOpenSetup(state.status) ? 'Settings saved. Wi-Fi is reconnecting. Reopen this page at the device’s new address. The temporary setup network closes after 10 minutes, or 60 seconds after connecting. If it has closed and Wi-Fi fails, hold BOOT for 8 seconds while the scale is running to reopen setup.' : `Settings saved. Wi-Fi is reconnecting. Reopen this page at the device's new address. If it cannot connect, join ${result.ap_ssid || 'its setup access point'} and open ${result.ip || '192.168.4.1'}.` : 'Settings saved on the device.';
    $('#config-result').textContent = `${state.demo ? 'Demo: ' : ''}${message}`;
    notice(message);
    if (result.reconnect_required) { state.lastSeen = 0; render(); }
    else { await refreshStatus(); await loadPrivate(false); }
  } catch (error) { $('#config-result').textContent = error.message; notice(error.message, true); }
  finally { setBusy(false); }
});
$('#test-spoolman').addEventListener('click', async () => {
  try {
    if (state.configDirty) throw new Error('Save the server settings before testing them.');
    await mutation('/api/spoolman/test', {}, 'The device reached the configured Spoolman server.');
  } catch (error) { notice(error.message, true); }
});
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
window.addEventListener('hashchange', setView);

setupTheme();
setupEditorControls();
const demoRequested = new URLSearchParams(location.search).get('demo') === '1';
if (demoRequested && ['localhost', '127.0.0.1', '[::1]'].includes(location.hostname)) {
  const { createDemo } = await import('./demo.js');
  const demo = createDemo();
  state.demo = true;
  transport = demo.request;
  $('#demo-banner').hidden = false;
  $('#footer-mode').textContent = 'Demo only · PIN 12345678';
  $('#pair-form [name=pin]').placeholder = 'Demo PIN: 12345678';
  $('#demo-scenario').addEventListener('change', async event => {
    const resetSession = event.target.value === 'headless-setup' || state.status?.device?.headless;
    demo.setScenario(event.target.value);
    state.calibrationZeroed = false;
    state.calibrationReference = null;
    $('#calibration-result').textContent = '';
    if (resetSession) lock();
    $('#pair-form [name=pin]').placeholder = event.target.value === 'headless-setup' ? 'Choose 8 digits' : 'Demo PIN: 12345678';
    $('#footer-mode').textContent = event.target.value === 'headless-setup' ? 'Demo only · choose your own preview PIN' : 'Demo only · PIN 12345678';
    await poll(); await loadPrivate(false);
  });
  $('#demo-arm').addEventListener('click', async () => { demo.armEnrollment(); await poll(); });
} else if (demoRequested) notice('Demo mode is available only on a local desktop preview. This page will use the real device API.', true);
setView();
renderInventory();
render();
await poll();
setInterval(poll, 1500);
setInterval(render, 1000);
