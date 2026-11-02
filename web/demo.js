import { fullSpoolTare, measurement, pinIsOptional, scaleCalibrationGuard, scaleZeroGuard, setupBody, setupState, tagWriteGuard, validateSpool, wifiCredentials } from './logic.js';

const copy = value => structuredClone(value);
const fail = (message, status = 400) => { const error = new Error(message); error.status = status; throw error; };

export function createDemo({ now = Date.now } = {}) {
  const profiles = [
    { id: 'demo-1', name: 'Forest green PLA', brand: 'Bambu Lab', material: 'PLA', color: '#3d7058', empty_weight_g: 250, initial_weight_g: 1000, density_g_cm3: 1.24, diameter_mm: 1.75, spoolman_id: 12, remaining_weight_g: 618 },
    { id: 'demo-2', name: 'Warm white PETG', brand: 'Polymaker', material: 'PETG', color: '#d7d2bc', empty_weight_g: 185, initial_weight_g: 1000, density_g_cm3: 1.27, diameter_mm: 1.75, spoolman_id: null, remaining_weight_g: null },
    { id: 'demo-3', name: 'Signal orange PLA', brand: 'Generic', material: 'PLA', color: '#cc7938', empty_weight_g: null, initial_weight_g: 1000, density_g_cm3: 1.24, diameter_mm: 1.75, spoolman_id: null, remaining_weight_g: null },
  ];
  const status = {
    mode: 'demo', device: { name: 'Studio scale', firmware: '0.5.0-demo', variant: 'headless-led' },
    diagnostics: {leds_supported:true, led_test_remaining_ms:0},
    auth: { pin_required: true, invalid: false }, setup: { required: false, complete: true, can_submit: false, physical_confirmation_required: false },
    scale: { gross_g: 842, calibrated: true, zero_set: true, stable: true, fresh: true, tare_ready: true, calibration_ready: true },
    nfc: { present: true, uid: '04:A1:B2:C3:D4:E5:F6', type: 'NTAG216', writable: true, raw_writable: true, chip: 'ntag', spool_id: 'demo-1', last_message: 'Spoolancer inventory tag detected. The values in this preview are simulated.', last_error: '' },
    active_spool_id: 'demo-1', network: { connected: true, ip: '192.168.1.42' },
    spoolman: { configured: true, pending: false, last_message: 'Demo server configured. No network requests are sent.', last_error: '' },
  };
  const config = { device_name: 'Studio scale', wifi_ssid: 'Workshop', wifi_open: false, spoolman_url: 'http://192.168.1.50:7912', has_wifi_password: true };
  let scenario = 'ready';
  let nextId = 4;
  let nextSpoolmanId = 40;
  let paired = false;
  let ledTestStarted = null;
  let pin = '12345678';
  let apExpires = 0;

  let connectAt = null;
  let scanState = 'ready';
  let scanPolls = 0;
  let discoveryPolls = 0;
  let discoveryStarted = false;
  const scanNetworks = [
    { ssid: 'Workshop', rssi: -64, secure: true, channel: 1, supported: true },
    { ssid: 'Workshop', rssi: -39, secure: true, channel: 6, supported: true },
    { ssid: '', rssi: -28, secure: true, channel: 11, supported: true },
    { ssid: 'Guest Wi-Fi', rssi: -56, secure: false, channel: 11, supported: true },
    { ssid: 'Office Enterprise', rssi: -48, secure: true, channel: 6, supported: false },
    { ssid: 'Oficina impressão', rssi: -72, secure: true, channel: 1, supported: true },
  ];
  const token = 'demo-session-in-memory-only';
  const active = () => profiles.find(s => s.id === status.active_spool_id);
  const inactiveTagLink = profile => status.spoolman.configured && profile.archived && !!profile.tag_uid
    && !profile.removal_pending && !profile.recovery_required && !profile.sync_pending;

  function setScenario(value) {
    connectAt = null;
    if (value === 'headless-setup') {
      paired = false; pin = '12345678';
      status.auth = { pin_required: true, invalid: false };
      config.wifi_ssid = ''; config.wifi_open = false; config.has_wifi_password = false;
    } else if (scenario === 'headless-setup' && status.setup.required) {
      paired = false; pin = '12345678';
      apExpires = 0;
      status.auth = { pin_required: true, invalid: false };
      status.setup = { required: false, complete: true, can_submit: false, physical_confirmation_required: false };
      config.wifi_ssid = 'Workshop'; config.wifi_open = false; config.has_wifi_password = true;
    }
    if (value === 'headless-open') {
      paired = false; pin = null;
      apExpires = 0;
      status.auth = { pin_required: false, invalid: false };
      status.setup = { required: false, complete: true, can_submit: false, physical_confirmation_required: false };
      config.wifi_ssid = 'Workshop'; config.wifi_open = false; config.has_wifi_password = true;
    }
    scenario = value;
    status.device.headless = value === 'headless-open';
    if (value !== 'headless-setup') status.network = { connected: true, ip: '192.168.1.42' };
    if (value === 'headless-setup') {
      status.device.headless = true;
      status.setup = { required: true, complete: false, ap_active: true, open_network: true, via_ap: true, can_submit: true, physical_confirmation_required: false, ap_seconds: 600 };
      status.network = { connected: false, ip: '192.168.4.1' };
      apExpires = now() + 600000;

    }
    status.scale = { gross_g: 842, calibrated: true, zero_set: true, stable: true, fresh: true, tare_ready: true, calibration_ready: true };
    status.active_spool_id = profiles.find(profile => !profile.archived)?.id || null;
    status.nfc = { present: true, uid: '04:A1:B2:C3:D4:E5:F6', type: 'NTAG216', writable: true, raw_writable: true, chip: 'ntag', spool_id: 'demo-1', last_message: 'Spoolancer inventory tag detected. The values in this preview are simulated.', last_error: '' };
    const first = profiles.find(profile => profile.id === 'demo-1');
    if (first) first.empty_weight_g = 250;
    status.spoolman.configured = value !== 'local';
    config.spoolman_url = value === 'local' ? '' : 'http://192.168.1.50:7912';
    status.spoolman.last_error = '';
    status.spoolman.pending = false;
    if (value === 'moving') { status.scale.stable = false; status.scale.tare_ready = false; status.scale.calibration_ready = false; }
    if (value === 'uncalibrated') Object.assign(status.scale, { calibrated: false, zero_set: false, gross_g: null });
    if (value === 'stale') status.scale.fresh = false;
    status.nfc.available = value !== 'reader-missing';
    if (value === 'unknown' && first) first.empty_weight_g = null;
    if (value === 'removed') status.nfc = { present: false, uid: '', writable: false, type: '', last_message: 'No tag present.' };
    if (value === 'readonly') status.nfc = { present: true, uid: '12:34:56:78', type: 'MIFARE Classic 1K', writable: false, raw_writable: false, chip: 'classic', last_message: 'UID detected only. Proprietary material decoding is not implemented.', last_error: '' };
    if (value === 'unreadable') status.nfc = { present: true, uid: '04:11:22:33:44:55:66', type: 'NFC-A', writable: false, last_message: 'Tag UID detected.', last_error: 'Tag contents could not be read. Its UID can still identify a spool.' };
    const identityTags = {
      'nfc-a4': ['11223344', 'ISO 14443-A'],
      'nfc-a7': ['04112233445566', 'ISO 14443-A'],
      'nfc-a10': ['04112233445566778899', 'ISO 14443-A'],
      'nfc-b': ['B:11223344', 'ISO 14443-B'],
      felica: ['F:0123456789ABCDEF', 'FeliCa'],
      jewel: ['J:11223344', 'Jewel / Topaz'],
    };
    if (identityTags[value]) {
      const [uid, type] = identityTags[value];
      status.nfc = { present: true, uid, type, writable: false, last_message: 'Tag ID detected. Link this ID to a spool. Memory writing is not supported for this tag.', last_error: '' };
    }
    if (value === 'multiple-tags') status.nfc = { present: false, uid: '', writable: false, type: '', last_error: 'Multiple tags detected. Keep only one tag beside the reader.' };
    if (value === 'full-spool') { status.scale.gross_g = 1248; status.active_spool_id = null; }
    if (value === 'sync-error') { status.spoolman.pending = true; status.spoolman.last_error = 'Demo server unavailable. Nothing was synchronized.'; }
    if (['archive-hidden', 'archive-missing'].includes(value) && first) {
      first.archived = true; first.removal_pending = true;
      first.remote_missing = value === 'archive-missing';
      status.active_spool_id = null; status.nfc.spool_id = null;
      status.spoolman.pending = true;
      status.spoolman.last_error = 'The server archived this spool, but confirmation was lost. Check its archive status.';
    }
    if (value === 'inactive-tag' && first) {
      first.archived = true; first.removal_pending = false; first.recovery_required = false; first.sync_pending = false;
      first.tag_uid ||= '04:A1:B2:C3:D4:E5:F6';
      status.active_spool_id = null; status.nfc.spool_id = null;
    }
    if (['clean-draft', 'pending-draft'].includes(value)) {
      const draft = profiles.find(profile => profile.id === 'demo-2');
      if (draft) { draft.draft = true; draft.recovery_required = value === 'pending-draft'; draft.sync_pending = value === 'pending-draft'; }
    }
  }

  function refreshSetup() {
    if (!status.setup) return;
    if (connectAt !== null && now() >= connectAt) {
      status.network = { connected: true, ip: '192.168.1.42' };
      apExpires = Math.min(apExpires, connectAt + 60000);
      connectAt = null;
    }
    status.setup.ap_seconds = Math.max(0, Math.ceil((apExpires - now()) / 1000));
    status.setup.ap_active = status.setup.ap_seconds > 0;
    status.setup.via_ap = status.setup.ap_active;
    status.setup.can_submit = status.setup.required === true && status.setup.ap_active && !status.network.connected;
  }

  const inventory = () => {
    const configured = status.spoolman.configured;
    const error = ['sync-error', 'remote-error'].includes(scenario) ? 'Spoolman did not respond. Check its address and connection.' : '';
    const visible = profiles.filter(p => !p.archived);
    const remote = visible.filter(p => p.spoolman_id).map(p => ({ ...p, source: 'spoolman', metadata_readonly: true }));
    return {
      spools: copy(configured ? ['remote-empty', 'remote-error'].includes(scenario) ? [] : remote : visible.map(p => ({ ...p, source: 'local', metadata_readonly: false }))),
      drafts: copy(configured ? profiles.filter(p => p.draft || p.removal_pending || inactiveTagLink(p)).map(p => ({ ...p, inactive_tag_link: !!inactiveTagLink(p) })) : []),
      inventory: { source: configured ? 'spoolman' : 'local', configured, stale: !!error, cached: scenario === 'sync-error', error, limit: 200, age_seconds: scenario === 'sync-error' ? 120 : 0 },
    };
  };

  async function request(path, { method = 'GET', body = {}, token: supplied } = {}) {
    if (scenario === 'offline') fail('The simulated device is disconnected.', 503);
    refreshSetup();
    if (path === '/api/setup' && method === 'POST') {
      if (!setupState(status, true).canSubmit) fail('Join the temporary setup network to complete first setup.', 403);
      let fields;
      try { fields = setupBody(status, true, body); }
      catch (error) { fail(error.message); }
      config.wifi_ssid = fields.wifi_ssid;
      config.wifi_open = fields.wifi_open;
      config.has_wifi_password = !fields.wifi_open;
      status.auth.pin_required = fields.pin_required;
      pin = fields.pin_required ? fields.pin : null;
      status.setup.required = false;
      status.setup.complete = true;
      status.setup.can_submit = false;
      connectAt = now() + 1500;
      apExpires = now() + 600000;
      paired = true;
      return { ok: true, token, reconnect_required: true, ap_ssid: 'Spoolancer-demo', ip: '192.168.4.1', message: 'Setup saved in this preview. No device or network was changed.' };
    }
    if (path === '/api/session' && method === 'POST') {
      if (body.setup === true) fail('Complete Wi-Fi setup using /api/setup.', 403);
      if (status.setup?.required) fail('Complete first setup before pairing.', 403);
      if (status.auth.invalid) fail('Reset Wi-Fi and setup with the physical BOOT button.', 403);
      if (!pinIsOptional(status) && body.pin !== pin) fail('Incorrect device PIN.', 401);
      paired = true;
      return { token };
    }
    if (path === '/api/status' && method === 'GET') {
      status.diagnostics.led_test_remaining_ms = ledTestStarted === null ? 0 : Math.max(0, 12000 - (now() - ledTestStarted));
      return copy(status);
    }
    if (path === '/api/wifi/scan') {
      const setupAccess = setupState(status, true).eligible;
      if (!setupAccess && (!paired || supplied !== token)) fail('Pair with the device first.', 401);
      if (method === 'POST') {
        scanState = 'scanning'; scanPolls = 0;
        return { state: scanState, networks: [] };
      }
      if (method === 'GET') {
        if (scanState === 'scanning' && scanPolls++ > 0) scanState = scenario === 'scan-error' ? 'failed' : 'ready';
        return { state: scanState, networks: scanState === 'ready' ? copy(scanNetworks) : [] };
      }
      fail('Unknown demo endpoint.', 404);
    }
    if (!paired || supplied !== token) fail('Pair with the device first.', 401);
    if (path.split('?')[0] === '/api/spools' && method === 'GET') return inventory();
    if (path === '/api/spoolman/discover') {
      if (!status.network.connected) fail('Connect to local Wi-Fi first.');
      if (method === 'POST') { discoveryStarted = true; discoveryPolls = 0; return { ok: true, state: 'searching', candidates: [], message: 'Searching the local network.' }; }
      const ready = discoveryStarted && ++discoveryPolls > 1;
      return { state: ready ? 'ready' : 'searching', candidates: ready && scenario !== 'discovery-empty' ? [{ url: 'http://192.168.1.50:7912', label: 'Spoolman' }] : [], message: ready ? scenario === 'discovery-empty' ? 'No server found. Enter its address manually.' : 'Server found. Choose it, then select Save server.' : 'Searching the local network.' };
    }
    if (path === '/api/config' && method === 'GET') return copy(config);
    if (method !== 'POST') fail('Unknown demo endpoint.', 404);
    if (path === '/api/diagnostics') {
      const component = body.component;
      if (!['nfc', 'leds', 'scale'].includes(component)) fail('Choose nfc, leds or scale.');
      if (component === 'leds') {
        if (!status.diagnostics.leds_supported) fail('This firmware profile has no addressable status LEDs.', 409);
        ledTestStarted = now(); status.diagnostics.led_test_remaining_ms = 12000;
        return {ok:true, component, state:'observe', duration_ms:12000, message:'Watch red, green, blue, each LED in white, then the 0–100% bar for 12 seconds. Confirm colors visually. No physical LEDs are connected in this demo.'};
      }
      ledTestStarted = null; status.diagnostics.led_test_remaining_ms = 0;
      if (component === 'nfc') {
        const available = scenario !== 'reader-missing', multiple = scenario === 'multiple-tags';
        const detected = available && status.nfc.present && !!status.nfc.uid && !multiple;
        return {ok:true, component, state:detected ? 'pass' : available && !multiple ? 'warning' : 'fail', reader_available:available, tag_detected:detected, targets:multiple ? 2 : detected ? 1 : 0, uid:detected ? status.nfc.uid : '', type:status.nfc.type,
          message:detected ? 'One tag ID was read. Reader communication and tag detection passed. No tag data or spool profile was changed.' : !available ? 'PN532 did not respond. Check power, HSU/UART mode and crossed TX/RX, then retry.' : multiple ? 'Multiple tags detected. Keep only one tag beside the antenna.' : 'PN532 is responding, but no single tag was read. Hold one tag beside the antenna and test again.'};
      }
      const scale = status.scale;
      return {ok:true, component, state:!scale.fresh ? 'fail' : !scale.calibrated || !scale.stable ? 'warning' : 'pass', fresh:scale.fresh, stable:scale.stable, calibrated:scale.calibrated, sample_count:scale.fresh ? 20 : 0, raw_mean:scale.fresh ? 123456 : null, gross_g:scale.fresh && scale.calibrated ? scale.gross_g : null,
        message:!scale.fresh ? 'No fresh HX711 samples. Check its power, DOUT and clock wiring, then retry.' : !scale.calibrated ? 'HX711 samples are arriving. Complete Calibration to convert them to grams.' : !scale.stable ? 'HX711 samples are arriving, but the weight is settling. Keep the cradle still and retry.' : 'Fresh, stable weight samples are arriving. Add and remove a known weight to check response. Use Calibration to verify accuracy.'};
    }
    if (path === '/api/spools/remove' || path === '/api/spools/tag') {
      if (body.confirm !== true) fail('Confirm this change first.');
      const selected = inventory().spools.find(item => item.id === body.id);
      const profile = profiles.find(item => item.id === body.id);
      const removal = path.endsWith('/remove');
      if (!profile) fail('This spool is no longer in the inventory.', 404);
      if (!removal && body.unlink === true) {
        if (profile.removal_pending || profile.recovery_required || profile.sync_pending) fail('Resolve the pending Spoolman operation first.', 409);
        if (!profile.tag_uid || profile.tag_uid !== body.expected_uid) fail('The saved RFID link changed. Review it again.', 409);
        profile.tag_uid = '';
        if (status.nfc.spool_id === profile.id) status.nfc.spool_id = null;
        return { ok: true, spool: copy(profile), message: 'Local RFID link removed in this demo. No tag or server record was changed.' };
      }
      const recovery = removal && profile?.removal_pending === true && Number.isInteger(profile.spoolman_id) && profile.spoolman_id > 0;
      const localDraft = removal && profile.draft && !profile.spoolman_id;
      if ((!selected || profile.archived) && !recovery && !localDraft) fail('This spool is no longer in the inventory.', 404);
      if (profile.removal_pending && !removal) fail('Check the pending archive before changing this spool.', 409);
      if (!profile.removal_pending && (profile.recovery_required || profile.sync_pending)) fail('Resolve the pending Spoolman operation first.', 409);
      if (status.spoolman.configured && inventory().inventory.stale && !(removal && (profile.removal_pending || localDraft))) fail('Refresh the Spoolman inventory before continuing.', 409);
      if (removal) {
        if (profile.spoolman_id && !status.spoolman.configured) fail('Reconnect the original Spoolman server to archive this linked spool.', 409);
        if (profile.spoolman_id && scenario === 'archive-error') {
          profile.removal_pending = true;
          const error = new Error('Spoolman did not confirm the archive. Check its status before retrying.');
          error.status = 502; error.data = { error: error.message, recovery_required: true, spool: copy(profile) }; throw error;
        }
        if (profile.spoolman_id) { profile.archived = true; profile.removal_pending = false; profile.tag_uid = ''; }
        else profiles.splice(profiles.indexOf(profile), 1);
        if (status.active_spool_id === profile.id) status.active_spool_id = null;
        if (status.nfc.spool_id === profile.id) status.nfc.spool_id = null;
        status.spoolman.pending = profiles.some(item => item.removal_pending || item.recovery_required || item.sync_pending);
        return { ok: true, id: profile.id, archived: !!profile.spoolman_id && !profile.remote_missing, ...(profile.remote_missing ? { removed_missing: true } : {}), message: profile.remote_missing ? 'Spoolman confirmed that this spool is already absent. The pending local record was removed. Deleted server history was not restored.' : profile.spoolman_id ? 'Spool archived in the simulated server. Its history is preserved.' : 'Spool removed from this demo session.' };
      }
      if (!body.expected_uid || !status.nfc.present || status.nfc.uid !== body.expected_uid) fail('The tag is missing or changed. Scan it again.', 409);
      if (profiles.some(item => item.id !== profile.id && item.tag_uid === body.expected_uid)) fail('This tag already identifies another spool.', 409);
      profile.tag_uid = body.expected_uid;
      status.nfc.spool_id = profile.id;
      status.active_spool_id = profile.id;
      return { ok: true, spool: copy(profile), message: 'RFID linked in this demo session. No tag was written.' };
    }
    if (path === '/api/spools') {
      const existing = profiles.find(s => s.id === body.id);
      if (existing?.removal_pending) fail('Check this spool\'s pending archive status before editing it.', 409);
      if (body.tag_uid !== undefined) {
        if (!existing) fail('Choose an existing spool.');
        if (!status.nfc.present || status.nfc.uid !== body.tag_uid) fail('Rescan the tag before linking its UID.');
        if (profiles.some(s => s.id !== body.id && s.tag_uid === body.tag_uid)) fail('This tag already identifies another spool.');
        existing.tag_uid = body.tag_uid;
        return { ok: true, id: existing.id };
      }
      if (status.spoolman.configured && existing?.spoolman_id) fail('Edit this profile in Spoolman.');
      if (existing?.recovery_required) {
        if (!Number.isInteger(body.spoolman_id) || body.spoolman_id < 1) fail('Check Spoolman and link the existing spool ID before retrying.');
        existing.spoolman_id = body.spoolman_id; existing.draft = false; existing.recovery_required = false;
        return { ok: true, spool: copy(existing), inventory_source: 'spoolman', message: 'Draft linked to the simulated server spool.' };
      }
      const profile = validateSpool(body);
      if (profile.id && !existing) fail('This spool no longer exists.');
      if (body.infer_empty_from_measurement) {
        const estimate = fullSpoolTare(status, true, profile.initial_weight_g, body.spool_condition);
        if (Math.abs(estimate.empty - profile.empty_weight_g) > 1) fail('The reading changed. Review the estimate and try again.');
      }
      if (status.spoolman.configured) {
        if (body.confirm !== true || !['full', 'used'].includes(body.spool_condition)) fail('Confirm the new spool and whether it is full or used.');
        if (body.spool_condition === 'used') {
          const measured = measurement(status, profile, true);
          if (!measured.canSync) fail(measured.message);
          profile.remaining_weight_g = measured.remaining;
        } else profile.remaining_weight_g = profile.initial_weight_g;
      }
      if (existing) Object.assign(existing, profile);
      else { profile.id = `demo-${nextId++}`; profiles.push(profile); }
      const stored = existing || profile;
      if (status.spoolman.configured && ['sync-error', 'remote-error'].includes(scenario)) {
        stored.draft = true; stored.recovery_required = true;
        const error = new Error('Creation was not confirmed. Check Spoolman and link its spool ID before retrying.');
        error.status = 502; error.data = { error: error.message, draft: true, recovery_required: true, spool: copy(stored) }; throw error;
      }
      if (status.spoolman.configured) { stored.spoolman_id = nextSpoolmanId++; stored.draft = false; }
      return { ok: true, id: stored.id, spool: copy(stored), inventory_source: status.spoolman.configured ? 'spoolman' : 'local', message: status.spoolman.configured ? 'Spool confirmed by the simulated server.' : 'Spool saved in this demo session.' };
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
      if (!Number.isFinite(body.reference_g) || body.reference_g < 100 || body.reference_g > 2000) fail('Reference mass must be 100 to 2,000 g.');
      const error = scaleCalibrationGuard(status, true, true);
      if (error) fail(error, 409);
      if (scenario === 'moving') fail('The simulated reference mass is moving. Keep it still before calibrating.', 409);
      status.scale.calibrated = true;
      status.scale.gross_g = body.reference_g;
      return { ok: true, message: 'Calibration updated in this preview. No physical calibration was performed.' };
    }
    if (path === '/api/nfc/read') {
      if (!status.nfc.present) fail(status.nfc.last_error || 'No tag is present.');
      const visible = inventory().spools;
      const linked = visible.find(s => s.tag_uid === status.nfc.uid);
      status.nfc.spool_id = linked?.id || null;
      status.active_spool_id = status.nfc.spool_id;
      return { ok: true, nfc: copy(status.nfc), spool_id: status.nfc.spool_id || null, message: status.nfc.last_message };
    }
    if (path === '/api/nfc/clone') {
      if (body.step === 'read') return { ok: true, step: 'read', source_uid: '04:7A:BC:12:34:56:78', label: 'Bambu Lab PLA Basic #FF6A00', message: 'Demo: source tag read. This preview does not touch hardware.' };
      return { ok: true, message: 'Demo: magic card clone verified. No hardware was used.' };
    }
    if (path === '/api/nfc/write') {
      if (!body.confirm || !body.expected_uid) fail('Confirm the expected tag UID before writing.');
      const error = tagWriteGuard(status, body.expected_uid, true, body.format || 'spoolancer');
      if (error) fail(error);
      const spool = profiles.find(s => s.id === body.spool_id);
      if (!spool) fail('The selected spool no longer exists.');
      spool.tag_uid = status.nfc.uid;
      status.nfc.spool_id = spool.id;
      status.active_spool_id = spool.id;
      status.nfc.last_message = 'Demo write and read-back verification completed. No tag was changed.';
      return { ok: true, message: status.nfc.last_message };
    }
    if (path === '/api/config') {
      if (body.wifi_open !== undefined) {
        let wifi;
        try { wifi = wifiCredentials(body, { allowSavedPassword: body.wifi_ssid === config.wifi_ssid && config.has_wifi_password && body.clear_wifi_password !== true }); }
        catch (error) { fail(error.message); }
        if (wifi.wifi_open) body = { ...body, clear_wifi_password: true };
        config.wifi_open = wifi.wifi_open;
      }
      const reconnect = body.wifi_ssid !== undefined && body.wifi_ssid !== config.wifi_ssid || !!body.wifi_password || !!body.clear_wifi_password;
      for (const key of ['device_name', 'wifi_ssid', 'spoolman_url']) if (body[key] !== undefined) config[key] = body[key];
      if (body.clear_wifi_password) config.has_wifi_password = false;
      else if (body.wifi_password) config.has_wifi_password = true;
      status.device.name = config.device_name;
      status.spoolman.configured = !!config.spoolman_url;
      return { ok: true, reconnect_required: reconnect, ap_ssid: 'Spoolancer-demo', ip: '192.168.4.1' };
    }
    if (path === '/api/spoolman/test') {
      if (!status.spoolman.configured) fail('Configure a Spoolman server first.');
      if (scenario === 'sync-error') fail('Demo server unavailable. Nothing was synchronized.', 503);
      return { ok: true, message: 'Demo connection test completed. No server was contacted.' };
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
        status.spoolman.last_message = 'Demo synchronization completed. No Spoolman server was changed.';
        status.spoolman.last_error = '';
        status.spoolman.pending = false;
      }
      spool.gross_g = m.gross;
      spool.remaining_weight_g = m.remaining;
      return { ok: true, gross_g: m.gross, remaining_weight_g: m.remaining, spoolman_id: spool.spoolman_id, message: path.endsWith('/sync') ? status.spoolman.last_message : 'Measurement saved in this demo session.' };
    }
    if (path === '/api/update' && method === 'GET') {
      return { current: status.device.firmware, variant: status.device.variant, latest: 'v0.6.0-demo',
        update_available: true, notes_url: 'https://github.com/bloodf/spoolancer/releases', has_filesystem: true };
    }
    if (path === '/api/update' && method === 'POST') {
      if (!body.confirm) fail('Explicit confirmation is required.');
      return { ok: true, latest: 'v0.6.0-demo', message: 'Demo update started. No firmware was downloaded or changed.' };
    }
    fail('Unknown demo endpoint.', 404);
  }
  function restart() {
    paired = false; scenario = 'ready';
    status.active_spool_id = null; status.nfc.spool_id = null;
    status.spoolman.pending = profiles.some(item => item.removal_pending || item.recovery_required || item.sync_pending);
  }
  return { request, setScenario, restart };
}
