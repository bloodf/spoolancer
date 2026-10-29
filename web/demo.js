import { measurement, scaleCalibrationGuard, scaleZeroGuard, tagWriteGuard, validateSpool } from './logic.js';

const copy = value => structuredClone(value);
const fail = (message, status = 400) => { const error = new Error(message); error.status = status; throw error; };

export function createDemo({ now = Date.now } = {}) {
  const profiles = [
    { id: 'demo-1', name: 'Forest green PLA', brand: 'Bambu Lab', material: 'PLA', color: '#3d7058', empty_weight_g: 250, initial_weight_g: 1000, density_g_cm3: 1.24, diameter_mm: 1.75, spoolman_id: 12, remaining_weight_g: 618 },
    { id: 'demo-2', name: 'Warm white PETG', brand: 'Polymaker', material: 'PETG', color: '#d7d2bc', empty_weight_g: 185, initial_weight_g: 1000, density_g_cm3: 1.27, diameter_mm: 1.75, spoolman_id: null, remaining_weight_g: null },
    { id: 'demo-3', name: 'Signal orange PLA', brand: 'Generic', material: 'PLA', color: '#cc7938', empty_weight_g: null, initial_weight_g: 1000, density_g_cm3: 1.24, diameter_mm: 1.75, spoolman_id: null, remaining_weight_g: null },
  ];
  const status = {
    mode: 'demo', device: { name: 'Studio scale' },
    scale: { gross_g: 842, calibrated: true, zero_set: true, stable: true, fresh: true },
    nfc: { present: true, uid: '04:A1:B2:C3:D4:E5:F6', type: 'NTAG216', writable: true, spool_id: 'demo-1', last_message: 'Spoolancer inventory tag detected. The values in this preview are simulated.', last_error: '' },
    active_spool_id: 'demo-1', network: { connected: true, ip: '192.168.1.42' },
    spoolman: { configured: true, pending: false, last_message: 'Demo server configured. No network requests are sent.', last_error: '' },
  };
  const config = { device_name: 'Studio scale', wifi_ssid: 'Workshop', spoolman_url: 'http://192.168.1.50:7912', has_wifi_password: true };
  let scenario = 'ready';
  let nextId = 4;
  let nextSpoolmanId = 40;
  let paired = false;
  let pin = '12345678';
  let apExpires = 0;
  let enrollmentExpires = 0;
  const token = 'demo-session-in-memory-only';
  const active = () => profiles.find(s => s.id === status.active_spool_id);

  function setScenario(value) {
    if (value === 'headless-setup' || scenario === 'headless-setup') { paired = false; pin = '12345678'; }
    scenario = value;
    status.device.headless = false;
    delete status.setup;
    if (value === 'headless-setup') {
      status.device.headless = true;
      status.setup = { ap_active: true, open_network: true, via_ap: true, enrollment_required: true, enrollment_armed: false, enrollment_seconds: 0, ap_seconds: 600 };
      apExpires = now() + 600000;
      enrollmentExpires = 0;
    }
    status.scale = { gross_g: 842, calibrated: true, zero_set: true, stable: true, fresh: true };
    status.active_spool_id = 'demo-1';
    status.nfc = { present: true, uid: '04:A1:B2:C3:D4:E5:F6', type: 'NTAG216', writable: true, spool_id: 'demo-1', last_message: 'Spoolancer inventory tag detected. The values in this preview are simulated.', last_error: '' };
    profiles[0].empty_weight_g = 250;
    status.spoolman.last_error = '';
    status.spoolman.pending = false;
    if (value === 'moving') status.scale.stable = false;
    if (value === 'uncalibrated') Object.assign(status.scale, { calibrated: false, zero_set: false, gross_g: null });
    if (value === 'stale') status.scale.fresh = false;
    if (value === 'unknown') profiles[0].empty_weight_g = null;
    if (value === 'removed') status.nfc = { present: false, uid: '', writable: false, type: '', last_message: 'No tag present.' };
    if (value === 'readonly') status.nfc = { present: true, uid: '12:34:56:78', type: 'MIFARE Classic', writable: false, last_message: 'UID detected only. Proprietary material decoding is not implemented.', last_error: '' };
    if (value === 'sync-error') { status.spoolman.pending = true; status.spoolman.last_error = 'Demo server unavailable. Nothing was synchronized.'; }
  }

  function refreshSetup() {
    if (!status.setup) return;
    status.setup.ap_seconds = Math.max(0, Math.ceil((apExpires - now()) / 1000));
    status.setup.ap_active = status.setup.ap_seconds > 0;
    status.setup.via_ap = status.setup.ap_active;
    status.setup.enrollment_seconds = Math.max(0, Math.ceil((enrollmentExpires - now()) / 1000));
    status.setup.enrollment_armed = status.setup.enrollment_required && status.setup.ap_active && status.setup.enrollment_seconds > 0;
  }

  function armEnrollment() {
    refreshSetup();
    if (status.setup?.enrollment_required && status.setup.ap_active) enrollmentExpires = now() + 60000;
    refreshSetup();
  }

  async function request(path, { method = 'GET', body = {}, token: supplied } = {}) {
    if (scenario === 'offline') fail('The simulated device is disconnected.', 503);
    refreshSetup();
    if (path === '/api/session' && method === 'POST') {
      if (body.setup === true) {
        if (!status.setup?.enrollment_required || !status.setup?.via_ap || !status.setup?.enrollment_armed) fail('PIN enrollment requires the temporary setup network and a fresh physical button press.', 403);
        enrollmentExpires = 0;
        refreshSetup();
        if (!/^[0-9]{8}$/.test(body.pin)) fail('Choose exactly 8 digits. This attempt was consumed; simulate the button press again.');
        pin = body.pin;
        status.setup.enrollment_required = false;
      } else {
        if (status.setup?.enrollment_required) fail('Complete first setup before pairing.', 403);
        if (body.pin !== pin) fail('Incorrect device PIN.', 401);
      }
      paired = true;
      return { token };
    }
    if (path === '/api/status' && method === 'GET') return copy(status);
    if (!paired || supplied !== token) fail('Pair with the device first.', 401);
    if (path === '/api/spools' && method === 'GET') return { spools: copy(profiles) };
    if (path === '/api/config' && method === 'GET') return copy(config);
    if (method !== 'POST') fail('Unknown demo endpoint.', 404);
    if (path === '/api/spools') {
      const profile = validateSpool(body);
      const current = profiles.find(s => s.id === profile.id);
      if (profile.id && !current) fail('This spool no longer exists.');
      if (body.tag_uid !== undefined) {
        if (!status.nfc.present || status.nfc.uid !== body.tag_uid) fail('Rescan the tag before linking its UID.');
        if (profiles.some(s => s.id !== profile.id && s.tag_uid === body.tag_uid)) fail('This tag already identifies another local spool.');
        profile.tag_uid = body.tag_uid;
      }
      if (current) Object.assign(current, profile);
      else { profile.id = `demo-${nextId++}`; profile.remaining_weight_g = null; profiles.push(profile); }
      return { ok: true, id: profile.id, message: 'Spool saved in this demo session.' };
    }
    if (path === '/api/active-spool') {
      if (!profiles.some(s => s.id === body.id)) fail('Choose an existing spool.');
      status.active_spool_id = body.id;
      return { ok: true };
    }
    if (path === '/api/scale/tare') {
      const error = scaleZeroGuard(status, true);
      if (error) fail(error, 409);
      status.scale.zero_set = true;
      status.scale.gross_g = status.scale.calibrated ? 0 : null;
      status.active_spool_id = null;
      return { ok: true, message: 'Scale zeroed in this preview. Enter a reference weight in Calibration to try the next step.' };
    }
    if (path === '/api/scale/calibrate') {
      if (!Number.isFinite(body.reference_g) || body.reference_g < 100 || body.reference_g > 2000) fail('Reference mass must be 100–2,000 g.');
      const error = scaleCalibrationGuard(status, true, true);
      if (error) fail(error, 409);
      if (scenario === 'moving') fail('The simulated reference mass is moving. Keep it still before calibrating.', 409);
      status.scale.calibrated = true;
      status.scale.gross_g = body.reference_g;
      return { ok: true, message: 'Calibration updated in this preview. No physical calibration was performed.' };
    }
    if (path === '/api/nfc/read') {
      if (!status.nfc.present) fail('No tag is present.');
      const linked = profiles.find(s => s.tag_uid === status.nfc.uid);
      if (linked) status.nfc.spool_id = linked.id;
      else if (!status.nfc.spool_id) status.active_spool_id = null;
      if (status.nfc.spool_id) status.active_spool_id = status.nfc.spool_id;
      return { ok: true, nfc: copy(status.nfc), spool_id: status.nfc.spool_id || null, message: status.nfc.last_message };
    }
    if (path === '/api/nfc/write') {
      if (!body.confirm || !body.expected_uid) fail('Confirm the expected tag UID before writing.');
      const error = tagWriteGuard(status, body.expected_uid, true);
      if (error) fail(error);
      const spool = profiles.find(s => s.id === body.spool_id);
      if (!spool) fail('The selected spool no longer exists.');
      status.nfc.spool_id = spool.id;
      status.nfc.last_message = 'Demo write and read-back verification completed. No real tag was changed.';
      return { ok: true, message: status.nfc.last_message };
    }
    if (path === '/api/config') {
      const reconnect = body.wifi_ssid !== config.wifi_ssid || !!body.wifi_password || !!body.clear_wifi_password;
      config.device_name = body.device_name;
      config.wifi_ssid = body.wifi_ssid;
      config.spoolman_url = body.spoolman_url;
      if (body.clear_wifi_password) config.has_wifi_password = false;
      else if (body.wifi_password) config.has_wifi_password = true;
      status.device.name = config.device_name;
      status.spoolman.configured = !!config.spoolman_url;
      return { ok: true, reconnect_required: reconnect, ap_ssid: 'Spoolancer-demo', ip: '192.168.4.1' };
    }
    if (path === '/api/spoolman/test') {
      if (!status.spoolman.configured) fail('Configure a Spoolman server first.');
      if (scenario === 'sync-error') fail('Demo server unavailable. Nothing was synchronized.', 503);
      return { ok: true, message: 'Demo connection test completed. No real server was contacted.' };
    }
    if (path === '/api/measurements' || path === '/api/spoolman/sync') {
      if (!body.confirm || body.spool_id !== status.active_spool_id) fail('Confirm the currently selected spool.');
      const m = measurement(status, active(), true);
      if (!m.canSync) fail(m.message);
      const spool = active();
      if (path.endsWith('/sync')) {
        if (!status.spoolman.configured) fail('Configure Spoolman first.');
        if (scenario === 'sync-error') { status.spoolman.pending = true; fail('Demo server unavailable. Nothing was synchronized. Retry manually after restoring the connection.', 503); }
        if (!spool.spoolman_id) spool.spoolman_id = nextSpoolmanId++;
        status.spoolman.last_message = 'Demo synchronization completed. No real Spoolman server was changed.';
        status.spoolman.last_error = '';
        status.spoolman.pending = false;
      }
      spool.gross_g = m.gross;
      spool.remaining_weight_g = m.remaining;
      return { ok: true, gross_g: m.gross, remaining_weight_g: m.remaining, spoolman_id: spool.spoolman_id, message: path.endsWith('/sync') ? status.spoolman.last_message : 'Measurement saved in this demo session.' };
    }
    fail('Unknown demo endpoint.', 404);
  }
  return { request, setScenario, armEnrollment };
}
