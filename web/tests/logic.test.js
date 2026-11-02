import test from 'node:test';
import assert from 'node:assert/strict';
import { TAG_FORMATS, fullSpoolTare, inventoryState, inventoryActionSpool, cleanLocalDraft, ledSegments, measurement, networkChoices, pinIsOptional, scaleCalibrationGuard, scaleZeroGuard, setupBody, setupState, tagWriteGuard, tagLinkGuard, validateSpool, validSpoolmanUrl, wifiCredentials, normalizeHex, materialSelection, resolveMaterial, normalizeTheme, resolveTheme, readThemePreference, saveThemePreference, THEME_STORAGE_KEY, pairingState, sessionBody } from '../logic.js';
import { createDemo } from '../demo.js';

const spool = { id: 'a', name: 'Green PLA', material: 'PLA', brand: 'Generic', color: '#306040', empty_weight_g: 250, initial_weight_g: 1000, density_g_cm3: 1.24, diameter_mm: 1.75, spoolman_id: null };
const status = { scale: { calibrated: true, zero_set: true, stable: true, fresh: true, tare_ready: true, calibration_ready: true, gross_g: 842 }, nfc: { present: true, uid: '04:AA', writable: true } };

test('remaining weight uses measured gross minus known tare', () => {
  const m = measurement(status, spool, true);
  assert.equal(m.remaining, 592);
  assert.ok(Math.abs(m.percent - 59.2) < 1e-9);
  assert.equal(m.canSync, true);
});

test('unknown tare is different from a measured zero tare', () => {
  assert.equal(measurement(status, { ...spool, empty_weight_g: null }, true).canSync, false);
  assert.equal(measurement(status, { ...spool, empty_weight_g: 0 }, true).remaining, 842);
  assert.equal(validateSpool({ ...spool, empty_weight_g: '' }).empty_weight_g, null);
  assert.equal(validateSpool({ ...spool, empty_weight_g: '0' }).empty_weight_g, 0);
});

test('cached or uncalibrated weight is never shown as a live measurement', () => {
  for (const m of [measurement(status, spool, false), measurement({ scale: { ...status.scale, calibrated: false } }, spool, true), measurement({ scale: { ...status.scale, fresh: false } }, spool, true)]) {
    assert.equal(m.gross, null);
    assert.equal(m.remaining, null);
    assert.equal(m.canSync, false);
  }
  assert.match(measurement({ scale: { ...status.scale, calibrated: false } }, spool, true).message, /Settings, then Calibration/);
});

test('scale zero requires a connected device and fresh, stable sensor readings', () => {
  assert.equal(scaleZeroGuard(status, true), '');
  assert.equal(scaleZeroGuard({ scale: { ...status.scale, calibrated: false, zero_set: false } }, true), '');
  for (const value of [false, undefined, 'true']) assert.match(scaleZeroGuard(status, value), /Reconnect/);
  assert.match(scaleZeroGuard(null, true), /Reconnect/);
  for (const value of [false, undefined, 'true']) {
    assert.match(scaleZeroGuard({ scale: { ...status.scale, fresh: value } }, true), /fresh/);
    assert.match(scaleZeroGuard({ scale: { ...status.scale, tare_ready: value } }, true), /still/);
  }
});

test('calibration requires a live zero completed in this browser flow', () => {
  assert.equal(scaleCalibrationGuard(status, true, true), '');
  for (const value of [false, undefined, 'true']) assert.match(scaleCalibrationGuard(status, value, true), /Reconnect/);
  assert.match(scaleCalibrationGuard(null, true, true), /Reconnect/);
  for (const value of [false, undefined, 'true']) {
    assert.match(scaleCalibrationGuard({ scale: { ...status.scale, fresh: value } }, true, true), /fresh/);
    assert.match(scaleCalibrationGuard({ scale: { ...status.scale, zero_set: value } }, true, true), /Zero the empty cradle/);
    assert.match(scaleCalibrationGuard(status, true, value), /Zero the empty cradle/);
  }
  assert.match(scaleCalibrationGuard(status, true), /Zero the empty cradle/);
});

test('replacement-cell calibration leaves reference stability assessment to the device', () => {
  const replacement = { scale: { ...status.scale, stable: false, tare_ready: false, gross_g: null } };
  assert.equal(scaleCalibrationGuard(replacement, true, true), '');
  assert.equal(scaleCalibrationGuard({ scale: { ...replacement.scale, calibrated: false } }, true, true), '');
  assert.match(scaleZeroGuard(replacement, true), /still/);
});

test('settling keeps the same remaining-weight display basis but cannot save', () => {
  const m = measurement({ scale: { ...status.scale, stable: false } }, spool, true);
  assert.equal(m.gross, 842);
  assert.equal(m.remaining, 592);
  assert.equal(m.state, 'unstable');
  assert.equal(m.canSync, false);
});

test('save/sync gate agrees with the physical 20–2000 g range', () => {
  const zeroTare = { ...spool, empty_weight_g: 0, initial_weight_g: 5000 };
  for (const gross of [0, 19.9, -1, 2000.1, Infinity, NaN]) assert.equal(measurement({ scale: { ...status.scale, gross_g: gross } }, zeroTare, true).canSync, false, String(gross));
  for (const gross of [20, 2000]) assert.equal(measurement({ scale: { ...status.scale, gross_g: gross } }, zeroTare, true).canSync, true, String(gross));
});

test('inconsistent tare or initial weight never produces valid remaining', () => {
  for (const profile of [{ ...spool, empty_weight_g: 900 }, { ...spool, initial_weight_g: 100 }, { ...spool, initial_weight_g: null }, { ...spool, initial_weight_g: 0 }, { ...spool, empty_weight_g: '250' }, { ...spool, empty_weight_g: 2001 }]) {
    const m = measurement(status, profile, true);
    assert.equal(m.canSync, false);
    assert.equal(m.remaining, null);
  }
});

test('missing freshness and non-boolean sensor flags fail closed', () => {
  for (const patch of [{ fresh: undefined }, { fresh: 'true' }, { stable: 'true' }, { calibrated: 'true' }]) assert.equal(measurement({ scale: { ...status.scale, ...patch } }, spool, true).canSync, false);
});

test('no spool identity means gross only', () => {
  const m = measurement(status, null, true);
  assert.equal(m.gross, 842);
  assert.equal(m.remaining, null);
  assert.equal(m.canSync, false);
});

test('inventory normalization preserves unknown values and validates server bounds', () => {
  assert.equal(validateSpool({ ...spool, initial_weight_g: 5000, empty_weight_g: 2000 }).initial_weight_g, 5000);
  for (const patch of [{ initial_weight_g: '' }, { initial_weight_g: 0 }, { initial_weight_g: 5001 }, { empty_weight_g: -1 }, { empty_weight_g: 2001 }, { empty_weight_g: 'x' }, { name: 'a'.repeat(65) }, { color: 'red' }, { diameter_mm: 0 }, { density_g_cm3: Infinity }, { spoolman_id: 1.5 }, { spoolman_id: 0 }]) assert.throws(() => validateSpool({ ...spool, ...patch }));
});

test('printer tag formats require the matching chip and an unlocked tag', () => {
  const ntag = { nfc: { present: true, uid: '04:AA', writable: false, raw_writable: true, chip: 'ntag' } };
  const classic = { nfc: { present: true, uid: 'DEADBEEF', writable: false, raw_writable: true, chip: 'classic' } };
  for (const id of ['anycubic', 'elegoo', 'openspool']) {
    assert.equal(tagWriteGuard(ntag, '04:AA', true, id), '');
    assert.match(tagWriteGuard(classic, 'DEADBEEF', true, id), /NTAG213/);
  }
  for (const id of ['creality', 'qidi']) {
    assert.equal(tagWriteGuard(classic, 'DEADBEEF', true, id), '');
    assert.match(tagWriteGuard(ntag, '04:AA', true, id), /Classic 1K/);
  }
  assert.match(tagWriteGuard({ nfc: { ...classic.nfc, raw_writable: false } }, 'DEADBEEF', true, 'creality'), /signed/);
  assert.match(tagWriteGuard(ntag, '04:AA', true, 'spoolancer'), /other content/);
  assert.match(tagWriteGuard({ nfc: { ...ntag.nfc, raw_writable: false } }, '04:AA', true, 'spoolancer'), /read-only/);
  assert.match(tagWriteGuard(ntag, '04:AA', true, 'bambu'), /supported tag format/);
  assert.match(tagWriteGuard(ntag, '04:BB', true, 'anycubic'), /changed/);
  assert.deepEqual(TAG_FORMATS.map(item => item.id), ['spoolancer', 'creality', 'anycubic', 'elegoo', 'qidi', 'openspool']);
});

test('tag write requires the same present, writable UID and a live device', () => {
  assert.equal(tagWriteGuard(status, '04:AA', true), '');
  assert.match(tagWriteGuard(status, '04:BB', true), /changed/);
  assert.match(tagWriteGuard(status, '04:AA', false), /disconnected/);
  assert.match(tagWriteGuard({ nfc: { ...status.nfc, writable: false } }, '04:AA', true), /read-only/);
  assert.match(tagWriteGuard({ nfc: { ...status.nfc, present: false } }, '04:AA', true), /No tag/);
});

async function pairedDemo() {
  const demo = createDemo();
  const { token } = await demo.request('/api/session', { method: 'POST', body: { pin: '12345678' } });
  const post = (path, body) => demo.request(path, { method: 'POST', body, token });
  return { ...demo, token, post };
}

test('demo is explicitly marked and private endpoints require pairing', async () => {
  const demo = createDemo();
  assert.equal((await demo.request('/api/status')).mode, 'demo');
  await assert.rejects(demo.request('/api/spools'), { status: 401 });
  await assert.rejects(demo.request('/api/config'), { status: 401 });
  await assert.rejects(demo.request('/api/session', { method: 'POST', body: { pin: 'bad' } }), { status: 401 });
});

test('local save works without a Spoolman connection', async () => {
  const demo = await pairedDemo();
  await demo.post('/api/config', { device_name: 'Demo', wifi_ssid: 'Workshop', spoolman_url: '' });
  const result = await demo.post('/api/measurements', { spool_id: 'demo-1', confirm: true });
  assert.equal(result.remaining_weight_g, 592);
  const { spools } = await demo.request('/api/spools', { token: demo.token });
  assert.equal(spools[0].remaining_weight_g, 592);
  await assert.rejects(demo.post('/api/spoolman/sync', { spool_id: 'demo-1', confirm: true }), /Configure/);
});

test('demo errors never mutate the saved reading or claim synchronization', async () => {
  const demo = await pairedDemo();
  demo.setScenario('sync-error');
  await assert.rejects(demo.post('/api/spoolman/sync', { spool_id: 'demo-1', confirm: true }), /Nothing was synchronized/);
  assert.equal((await demo.request('/api/spools', { token: demo.token })).spools[0].remaining_weight_g, 618);
  assert.equal((await demo.request('/api/status')).spoolman.pending, true);
  demo.setScenario('moving');
  await assert.rejects(demo.post('/api/measurements', { spool_id: 'demo-1', confirm: true }), /still/);
});

test('demo read-only and swapped tags cannot be written', async () => {
  const demo = await pairedDemo();
  await assert.rejects(demo.post('/api/nfc/write', { spool_id: 'demo-1', expected_uid: 'wrong', confirm: true }), /changed/);
  demo.setScenario('readonly');
  await assert.rejects(demo.post('/api/nfc/write', { spool_id: 'demo-1', expected_uid: '12:34:56:78', confirm: true }), /read-only/);
});

test('demo does not return or retain a Wi-Fi password in config', async () => {
  const demo = await pairedDemo();
  await demo.post('/api/config', { device_name: 'Demo', wifi_ssid: 'Test', wifi_password: 'example-only', spoolman_url: '' });
  const config = await demo.request('/api/config', { token: demo.token });
  assert.equal(config.has_wifi_password, true);
  assert.equal('wifi_password' in config, false);
  assert.equal(JSON.stringify(config).includes('example-only'), false);
});

test('original tag can identify a local profile without being written', async () => {
  const demo = await pairedDemo();
  demo.setScenario('readonly');
  await demo.post('/api/nfc/read', {});
  assert.equal((await demo.request('/api/status')).active_spool_id, null);
  const profile = (await demo.request('/api/spools', { token: demo.token })).spools[0];
  await demo.post('/api/spools/tag', { id: profile.id, expected_uid: '12:34:56:78', confirm: true });
  await demo.post('/api/nfc/read', {});
  const final = await demo.request('/api/status');
  assert.equal(final.active_spool_id, profile.id);
  assert.equal(final.nfc.writable, false);
});

test('demo calibration accepts only finite numeric references from 100 to 2000 g', async () => {
  const demo = await pairedDemo();
  for (const reference_g of [99, 2000.1, 0, -100, null, undefined, '1000', Infinity, NaN]) {
    await assert.rejects(demo.post('/api/scale/calibrate', { reference_g }), /100/);
  }
  for (const reference_g of [100, 1000, 2000]) assert.equal((await demo.post('/api/scale/calibrate', { reference_g })).ok, true);
});

test('demo first calibration zeroes an empty cradle before accepting a reference', async () => {
  const demo = await pairedDemo();
  demo.setScenario('uncalibrated');
  let state = await demo.request('/api/status');
  assert.equal(state.scale.zero_set, false);
  assert.equal(state.scale.gross_g, null);
  await assert.rejects(demo.post('/api/scale/calibrate', { reference_g: 1000 }), /Zero the empty cradle/);
  assert.equal((await demo.post('/api/scale/tare', {})).ok, true);
  state = await demo.request('/api/status');
  assert.equal(state.scale.zero_set, true);
  assert.equal(state.scale.calibrated, false);
  assert.equal(state.scale.gross_g, null);
  assert.equal(state.active_spool_id, null);
  const result = await demo.post('/api/scale/calibrate', { reference_g: 1000 });
  assert.equal(result.ok, true);
  assert.match(result.message, /No physical calibration/);
  state = await demo.request('/api/status');
  assert.equal(state.scale.calibrated, true);
  assert.equal(state.scale.gross_g, 1000);
  assert.equal(state.active_spool_id, null);
  await demo.post('/api/scale/tare', {});
  assert.equal((await demo.request('/api/status')).scale.gross_g, 0);
});

test('demo rejects zero and calibration during simulated disconnection, stale data or motion', async () => {
  const demo = await pairedDemo();
  for (const [scenario, message] of [['offline', /disconnected/], ['stale', /fresh/], ['moving', /still/]]) {
    demo.setScenario(scenario);
    await assert.rejects(demo.post('/api/scale/tare', {}), message);
    await assert.rejects(demo.post('/api/scale/calibrate', { reference_g: 1000 }), message);
    if (scenario !== 'offline') {
      const state = await demo.request('/api/status');
      assert.equal(state.scale.gross_g, 842);
      assert.equal(state.active_spool_id, 'demo-1');
    }
  }
});

test('Spoolman address matches bounded numeric-IPv4 HTTP support', () => {
  for (const url of ['', 'http://192.168.1.50:7912', 'http://10.0.0.20', 'http://192.168.0.12:80/spoolman']) assert.equal(validSpoolmanUrl(url), true, url);
  for (const url of ['https://192.168.1.50', 'http://spoolman.local:7912', 'http://user:pass@192.168.1.2', 'http://192.168.1.2?x=1', 'http://192.168.1.2#hash', 'http://0.0.0.0', 'http://224.1.1.1', 'http://256.1.1.1', 'http://192.168.001.1', 'http://192.168.1.2:0', 'http://192.168.1.2:65536', 'http://192.168.1.2/path\\other']) assert.equal(validSpoolmanUrl(url), false, url);
});

test('color editor expands short hex without changing arbitrary custom colors', () => {
  assert.equal(normalizeHex(' #AbC '), '#aabbcc');
  assert.equal(normalizeHex('00FF7A'), '#00ff7a');
  assert.equal(normalizeHex('#123456'), '#123456');
  for (const value of ['', '#12', '#1234', '#12345678', 'red', 'rgb(0,0,0)']) assert.throws(() => normalizeHex(value));
});

test('material selector round-trips common and custom saved profiles', () => {
  for (const value of ['PLA', 'PETG', 'PA-CF', 'PETG translucent', 'pla']) {
    const selection = materialSelection(value);
    assert.equal(resolveMaterial(selection.choice, selection.custom), value);
  }
  assert.deepEqual(materialSelection(''), { choice: '', custom: '' });
  assert.equal(resolveMaterial('custom', '  PA6-GF  '), 'PA6-GF');
  for (const [choice, custom] of [['', ''], ['injected', 'PLA'], ['custom', ''], ['custom', 'a'.repeat(31)], ['custom', 'PLA\nPETG']]) assert.throws(() => resolveMaterial(choice, custom));
});

test('theme preference resolves system changes without overriding explicit choices', () => {
  assert.equal(resolveTheme('system', true), 'dark');
  assert.equal(resolveTheme('system', false), 'light');
  assert.equal(resolveTheme('light', true), 'light');
  assert.equal(resolveTheme('dark', false), 'dark');
  assert.equal(normalizeTheme('unexpected'), 'system');
  assert.equal(resolveTheme('unexpected', true), 'dark');
});

test('theme persistence writes only the theme key and survives blocked storage', () => {
  const values = new Map();
  const storage = { getItem: key => values.get(key), setItem: (key, value) => values.set(key, value) };
  assert.equal(readThemePreference(storage), 'system');
  assert.equal(saveThemePreference(storage, 'dark'), 'dark');
  assert.equal(readThemePreference(storage), 'dark');
  assert.deepEqual([...values], [[THEME_STORAGE_KEY, 'dark']]);
  assert.equal(saveThemePreference(storage, 'not-a-theme'), 'system');
  const blocked = { getItem() { throw new Error('Blocked'); }, setItem() { throw new Error('Blocked'); } };
  assert.equal(readThemePreference(blocked), 'system');
  assert.equal(saveThemePreference(blocked, 'light'), 'light');
});

const setupStatus = { device: { headless: true }, setup: { required: true, complete: false, ap_active: true, via_ap: true, can_submit: true, physical_confirmation_required: false } };

test('PIN pairing cannot bypass the first-setup Wi-Fi flow', () => {
  assert.throws(() => sessionBody(setupStatus, true, '23456789'), /Complete Wi-Fi setup/);
  assert.equal(pairingState(setupStatus, true).canSubmit, false);
  assert.throws(() => sessionBody(setupStatus, false, '23456789'), /Reconnect/);
  const complete = { setup: { required: false, complete: true }, auth: { pin_required: true } };
  assert.deepEqual(sessionBody(complete, true, '23456789'), { pin: '23456789' });
  assert.throws(() => sessionBody(complete, true, '123'), /8 digits/);
});

const freshSetup = { device: { headless: true }, auth: { pin_required: true }, setup: { ...setupStatus.setup, required: true, complete: false } };
const setupFields = { wifi_ssid: 'Workshop', wifi_password: 'test-password', wifi_open: false, pin_required: true, pin: '23456789' };

test('first setup requires a live eligible setup network, without physical button access', () => {
  assert.deepEqual(setupBody(freshSetup, true, setupFields), setupFields);
  assert.equal(setupState(freshSetup, true).canSubmit, true);
  for (const pin_required of [false, true]) {
    const fields = { ...setupFields, pin_required };
    assert.throws(() => setupBody(freshSetup, false, fields), /Reconnect/);
    for (const patch of [{ ap_active: false }, { via_ap: false }, { can_submit: false }, { can_submit: undefined }, { can_submit: 'true' }]) {
      const changed = { ...freshSetup, setup: { ...freshSetup.setup, ...patch } };
      assert.equal(setupState(changed, true).canSubmit, false);
      assert.throws(() => setupBody(changed, true, fields));
    }
  }
  assert.throws(() => setupBody(freshSetup, true, { ...setupFields, pin_required: undefined }), /Choose whether/);
  for (const pin of ['123', '123456789', 12345678, 'abcdefgh']) assert.throws(() => setupBody(freshSetup, true, { ...setupFields, pin }), /8 digits/);
  const withoutPin = setupBody(freshSetup, true, { ...setupFields, pin_required: false });
  assert.equal('pin' in withoutPin, false);
  assert.throws(() => sessionBody(freshSetup, true, '23456789'), /Complete Wi-Fi setup/);
  const complete = { ...freshSetup, setup: { ...freshSetup.setup, required: false, complete: true } };
  assert.equal(setupState(complete, true).canSubmit, false);
  assert.throws(() => setupBody(complete, true, setupFields), /already complete/);
});

test('Wi-Fi credentials preserve SSIDs and enforce byte limits and explicit open networks', () => {
  const secured = { wifi_ssid: ' Workshop ', wifi_password: 'password', wifi_open: false };
  assert.deepEqual(wifiCredentials(secured), secured);
  assert.equal(wifiCredentials({ ...secured, wifi_ssid: '🛠'.repeat(8) }).wifi_ssid, '🛠'.repeat(8));
  for (const wifi_ssid of ['', '   ', 'a'.repeat(33), '🛠'.repeat(9), '\ud800', 'Workshop\n', null]) assert.throws(() => wifiCredentials({ ...secured, wifi_ssid }), /network|name/);
  for (const wifi_password of ['', '1234567', 'g'.repeat(64), 'a'.repeat(65), '🛠'.repeat(16), 'abcd\n1234', '\ud800'.repeat(8)]) assert.throws(() => wifiCredentials({ ...secured, wifi_password }), /password/);
  for (const wifi_password of ['a'.repeat(63), 'A0'.repeat(32), '🛠🛠']) assert.equal(wifiCredentials({ ...secured, wifi_password }).wifi_password, wifi_password);
  assert.deepEqual(wifiCredentials({ ...secured, wifi_open: true, wifi_password: '' }), { ...secured, wifi_open: true, wifi_password: '' });
  assert.throws(() => wifiCredentials({ ...secured, wifi_open: true }), /empty password/);
  assert.throws(() => wifiCredentials({ ...secured, wifi_open: undefined }), /requires a password/);
  assert.equal(wifiCredentials({ ...secured, wifi_password: '' }, { allowSavedPassword: true }).wifi_password, '');
});

test('automatic no-PIN sessions require explicit completed setup rather than missing authentication data', () => {
  const open = { auth: { pin_required: false }, setup: { required: false, complete: true } };
  assert.equal(pinIsOptional(open), true);
  assert.deepEqual(sessionBody(open, true), {});
  for (const state of [{}, { auth: { pin_required: false } }, { auth: { pin_required: false }, setup: { required: false } }, { ...open, auth: { pin_required: true } }, { ...open, auth: { pin_required: false, invalid: true } }, { ...open, setup: { required: true, complete: true } }, { ...open, setup: { required: false, complete: false } }]) {
    assert.equal(pinIsOptional(state), false);
    assert.throws(() => sessionBody(state, true));
  }
  assert.throws(() => sessionBody(open, false), /Reconnect/);
  assert.deepEqual(sessionBody({ device: { headless: true }, setup: { enrollment_required: false } }, true, '12345678'), { pin: '12345678' });
  assert.throws(() => sessionBody({ device: { headless: true }, setup: { enrollment_required: false } }, true), /8 digits/);
});

test('network choices keep the strongest visible SSID and label unsupported security', () => {
  const networks = [
    { ssid: 'Workshop', rssi: -75, secure: true, channel: 1, supported: true },
    { ssid: 'Workshop', rssi: -32, secure: true, channel: 6, supported: true },
    { ssid: '', rssi: -10, secure: true, supported: true },
    { ssid: '   ', rssi: -11, secure: false, supported: true },
    { ssid: 'Enterprise', rssi: -40, secure: true, channel: 11, supported: false },
    { ssid: 'Open', rssi: -50, secure: false, channel: 1, supported: true },
    { ssid: 'Bad signal', rssi: 'strong', secure: false, supported: true },
    { ssid: 'a'.repeat(33), rssi: -20, secure: false, supported: true },
  ];
  assert.deepEqual(networkChoices(networks), [networks[1], networks[4], networks[5]]);
  assert.deepEqual(networkChoices(null), []);
  assert.equal(networkChoices([{ ssid: 'Unknown security', rssi: -50, supported: true }])[0].supported, false);
  const many = Array.from({ length: 40 }, (_, index) => ({ ssid: `Network ${index}`, rssi: -index, secure: true, supported: true }));
  assert.equal(networkChoices(many).length, 32);
  assert.equal(networkChoices(many)[31].ssid, 'Network 31');
});

test('demo setup accepts corrected input without a physical button and keeps secrets out of responses', async () => {
  const demo = createDemo();
  demo.setScenario('headless-setup');
  const enroll = pin => demo.request('/api/setup', { method: 'POST', body: { ...setupFields, pin } });
  await assert.rejects(demo.request('/api/session', { method: 'POST', body: { pin: '23456789', setup: true } }), { status: 403 });
  await assert.rejects(enroll('bad'), /8 digits/);
  const { token } = await enroll('23456789');
  const state = await demo.request('/api/status');
  assert.equal(state.setup.required, false);
  assert.equal(state.setup.complete, true);
  assert.equal(state.auth.pin_required, true);
  assert.equal(state.setup.can_submit, false);
  assert.equal(state.setup.physical_confirmation_required, false);
  assert.equal(JSON.stringify(state).includes('23456789'), false);
  const saved = JSON.stringify(await demo.request('/api/config', { token }));
  assert.equal(saved.includes('23456789'), false);
  assert.equal(saved.includes('test-password'), false);
  await assert.rejects(demo.request('/api/session', { method: 'POST', body: { pin: '12345678' } }), { status: 401 });
  assert.ok((await demo.request('/api/session', { method: 'POST', body: { pin: '23456789' } })).token);
  await assert.rejects(enroll('34567890'), { status: 403 });
});

test('demo no-PIN setup issues sessions after setup while private actions still require bearer authentication', async () => {
  const demo = createDemo();
  demo.setScenario('headless-setup');
  await assert.rejects(demo.request('/api/session', { method: 'POST' }), { status: 403 });
  const body = { ...setupFields, wifi_ssid: 'Guest Wi-Fi', wifi_open: true, wifi_password: '', pin_required: false };
  const result = await demo.request('/api/setup', { method: 'POST', body });
  assert.equal(result.reconnect_required, true);
  const state = await demo.request('/api/status');
  assert.equal(pinIsOptional(state), true);
  await assert.rejects(demo.request('/api/spools'), { status: 401 });
  const { token } = await demo.request('/api/session', { method: 'POST', body: sessionBody(state, true) });
  assert.ok((await demo.request('/api/spools', { token })).spools.length);
  const config = await demo.request('/api/config', { token });
  assert.equal(config.has_wifi_password, false);
  assert.equal(config.wifi_open, true);
  assert.equal(config.wifi_ssid, 'Guest Wi-Fi');
  demo.setScenario('ready');
  assert.equal(pinIsOptional(await demo.request('/api/status')), true);
});

test('demo setup simulates delayed LAN connection and a bounded AP grace period', async () => {
  let time = 1000;
  const demo = createDemo({ now: () => time });
  demo.setScenario('headless-setup');
  await demo.request('/api/setup', { method: 'POST', body: setupFields });
  let state = await demo.request('/api/status');
  assert.equal(state.mode, 'demo');
  assert.equal(state.network.connected, false);
  assert.equal(state.setup.ap_active, true);
  time = 2499;
  assert.equal((await demo.request('/api/status')).network.connected, false);
  time = 2500;
  state = await demo.request('/api/status');
  assert.equal(state.network.connected, true);
  assert.equal(state.network.ip, '192.168.1.42');
  assert.equal(state.setup.ap_seconds, 60);
  assert.equal(state.setup.ap_active, true);
  time = 62499;
  assert.equal((await demo.request('/api/status')).setup.ap_active, true);
  time = 62500;
  state = await demo.request('/api/status');
  assert.equal(state.network.connected, true);
  assert.equal(state.setup.ap_active, false);
  assert.equal(state.setup.via_ap, false);
});

test('completed no-PIN demo fixture obtains local access without a PIN after UI session loss', async () => {
  const demo = createDemo();
  demo.setScenario('headless-open');
  const state = await demo.request('/api/status');
  assert.equal(state.mode, 'demo');
  assert.equal(state.device.headless, true);
  assert.equal(state.setup.required, false);
  assert.equal(state.setup.complete, true);
  assert.equal(state.network.connected, true);
  assert.equal(pinIsOptional(state), true);
  await assert.rejects(demo.request('/api/spools'), { status: 401 });
  const { token } = await demo.request('/api/session', { method: 'POST', body: sessionBody(state, true) });
  assert.ok((await demo.request('/api/spools', { token })).spools.length);
  const renewed = await demo.request('/api/session', { method: 'POST', body: {} });
  assert.ok((await demo.request('/api/config', { token: renewed.token })).wifi_ssid);
});

test('demo Wi-Fi scans allow setup AP discovery and require a session after setup', async () => {
  const demo = createDemo();
  await assert.rejects(demo.request('/api/wifi/scan'), { status: 401 });
  demo.setScenario('headless-setup');
  assert.equal((await demo.request('/api/wifi/scan', { method: 'POST' })).state, 'scanning');
  assert.equal((await demo.request('/api/wifi/scan')).state, 'scanning');
  const scan = await demo.request('/api/wifi/scan');
  assert.equal(scan.state, 'ready');
  const choices = networkChoices(scan.networks);
  assert.equal(choices[0].ssid, 'Workshop');
  assert.equal(choices.filter(network => network.ssid === 'Workshop').length, 1);
  assert.equal(choices.some(network => network.ssid === ''), false);
  assert.equal(choices.find(network => network.ssid === 'Office Enterprise').supported, false);
  assert.equal(choices.find(network => network.ssid === 'Guest Wi-Fi').secure, false);
  const { token } = await demo.request('/api/setup', { method: 'POST', body: setupFields });
  await assert.rejects(demo.request('/api/wifi/scan'), { status: 401 });
  assert.equal((await demo.request('/api/wifi/scan', { token })).state, 'ready');
  demo.setScenario('scan-error');
  await demo.request('/api/wifi/scan', { method: 'POST', token });
  await demo.request('/api/wifi/scan', { token });
  assert.equal((await demo.request('/api/wifi/scan', { token })).state, 'failed');
});

test('the temporary setup network expires without silently renewing', async () => {
  let time = 0;
  const demo = createDemo({ now: () => time });
  demo.setScenario('headless-setup');
  time = 600001;
  const state = await demo.request('/api/status');
  assert.equal(state.setup.ap_active, false);
  assert.equal(state.setup.can_submit, false);
  await assert.rejects(demo.request('/api/setup', { method: 'POST', body: setupFields }), { status: 403 });
});

test('every screen variant uses web setup without a BOOT enrollment window', () => {
  for (const variant of ['touch-8048', 'oled-led', 'headless-led']) {
    const screen = { ...freshSetup, device: { headless: false, variant } };
    assert.equal(setupState(screen, true).canSubmit, true);
    assert.deepEqual(setupBody(screen, true, setupFields), setupFields);
    const outsideSetup = { ...screen, setup: { ...screen.setup, via_ap: false } };
    assert.throws(() => setupBody(outsideSetup, true, setupFields), /temporary setup Wi-Fi/);
  }
});

test('full spool tare estimation needs an explicit full assertion and a stable measured gross', () => {
  const full = { scale: { ...status.scale, gross_g: 1248.2 } };
  assert.deepEqual(fullSpoolTare(full, true, '1000', 'full'), { empty: 248.2, gross: 1248.2, initial: 1000 });
  assert.throws(() => fullSpoolTare(full, true, 1000, 'used'), /Choose New/);
  assert.throws(() => fullSpoolTare(full, false, 1000, 'full'), /Reconnect/);
  assert.throws(() => fullSpoolTare(full, true, '', 'full'), /declared/);
  assert.throws(() => fullSpoolTare(status, true, 1000, 'full'), /below/);
  for (const patch of [{ stable: false }, { calibrated: false }, { fresh: false }, { gross_g: 2001 }, { gross_g: null }]) {
    assert.throws(() => fullSpoolTare({ scale: { ...full.scale, ...patch } }, true, 1000, 'full'));
  }
});

test('calibration and tare respect their specific readiness flags rather than ordinary stable weight', () => {
  for (const value of [false, undefined, 'true']) {
    assert.match(scaleZeroGuard({ scale: { ...status.scale, tare_ready: value } }, true), /still/);
    assert.match(scaleCalibrationGuard({ scale: { ...status.scale, calibration_ready: value } }, true, true), /still/);
  }
  assert.equal(scaleCalibrationGuard({ scale: { ...status.scale, stable: false, calibration_ready: true } }, true, true), '');
});

test('configured inventory never mixes local records into authoritative server results', () => {
  const local = { ...spool, source: 'local' };
  const remote = { ...spool, id: 'remote', source: 'spoolman', metadata_readonly: true };
  const rejected = inventoryState({ spools: [local], inventory: { source: 'local' } }, true);
  assert.equal(rejected.spools.length, 0);
  assert.equal(rejected.stale, true);
  assert.match(rejected.error, /could not be loaded/);
  const selected = inventoryState({ spools: [local, remote], inventory: { source: 'spoolman', stale: true, cached: true, error: 'Unavailable' } }, true);
  assert.deepEqual(selected.spools, [remote]);
  assert.equal(selected.cached, true);
  assert.equal(selected.error, 'Unavailable');
  assert.deepEqual(inventoryState({ spools: [local], inventory: { source: 'local' } }, false).spools, [local]);
});

test('server discovery is explicit, returns selectable candidates, and never saves a URL automatically', async () => {
  const demo = await pairedDemo();
  const original = await demo.request('/api/config', { token: demo.token });
  assert.equal((await demo.post('/api/spoolman/discover', {})).state, 'searching');
  await demo.request('/api/spoolman/discover', { token: demo.token });
  const found = await demo.request('/api/spoolman/discover', { token: demo.token });
  assert.equal(found.state, 'ready');
  assert.equal(found.candidates.length, 1);
  assert.deepEqual(await demo.request('/api/config', { token: demo.token }), original);
  demo.setScenario('discovery-empty');
  await demo.post('/api/spoolman/discover', {});
  await demo.request('/api/spoolman/discover', { token: demo.token });
  assert.equal((await demo.request('/api/spoolman/discover', { token: demo.token })).candidates.length, 0);
});

test('server offline, empty and cached inventory are distinct demo states', async () => {
  const demo = await pairedDemo();
  for (const [scenario, count, stale, cached] of [['ready', 1, false, false], ['remote-empty', 0, false, false], ['remote-error', 0, true, false], ['sync-error', 1, true, true]]) {
    demo.setScenario(scenario);
    const result = await demo.request('/api/spools?refresh=1', { token: demo.token });
    assert.equal(result.spools.length, count);
    assert.equal(result.inventory.stale, stale);
    assert.equal(result.inventory.cached, cached);
    assert.equal(result.inventory.source, 'spoolman');
  }
});

test('used-spool registration sends measured remaining rather than nominal initial filament mass', async () => {
  const demo = await pairedDemo();
  const result = await demo.post('/api/spools', { ...spool, id: undefined, confirm: true, spool_condition: 'used' });
  assert.equal(result.spool.remaining_weight_g, 592);
  assert.equal(result.spool.initial_weight_g, 1000);
  assert.equal(result.inventory_source, 'spoolman');
  await assert.rejects(demo.post('/api/spools', { ...spool, id: undefined, empty_weight_g: null, confirm: true, spool_condition: 'used' }), /empty weight/);
});

test('full-spool registration rechecks the inference before writing and preserves an uncertain draft', async () => {
  const demo = await pairedDemo();
  demo.setScenario('full-spool');
  const fields = { ...spool, id: undefined, empty_weight_g: 248, confirm: true, spool_condition: 'full', infer_empty_from_measurement: true };
  await assert.rejects(demo.post('/api/spools', { ...fields, empty_weight_g: 240 }), /reading changed/);
  const saved = await demo.post('/api/spools', fields);
  assert.equal(saved.spool.remaining_weight_g, 1000);
  demo.setScenario('sync-error');
  let error;
  try { await demo.post('/api/spools', { ...spool, id: undefined, confirm: true, spool_condition: 'used' }); } catch (caught) { error = caught; }
  assert.equal(error.data.draft, true);
  assert.ok(error.data.spool.id);
  assert.equal(error.data.recovery_required, true);
  const inventory = await demo.request('/api/spools', { token: demo.token });
  assert.equal(inventory.drafts.length, 1);
  assert.equal(inventory.spools.some(item => item.id === error.data.spool.id), false);
});


test('saving only the server address preserves Wi-Fi and device settings', async () => {
  const demo = await pairedDemo();
  const before = await demo.request('/api/config', { token: demo.token });
  const result = await demo.post('/api/config', { spoolman_url: 'http://192.168.1.80:7912' });
  assert.equal(result.reconnect_required, false);
  const after = await demo.request('/api/config', { token: demo.token });
  assert.equal(after.wifi_ssid, before.wifi_ssid);
  assert.equal(after.device_name, before.device_name);
  assert.equal(after.has_wifi_password, before.has_wifi_password);
  assert.equal(after.spoolman_url, 'http://192.168.1.80:7912');
});

test('an uncertain registration keeps the same draft and requires server reconciliation', async () => {
  const demo = await pairedDemo(); demo.setScenario('sync-error');
  let draft;
  try { await demo.post('/api/spools', { ...spool, id: undefined, confirm: true, spool_condition: 'used' }); } catch (error) { draft = error.data.spool; }
  demo.setScenario('ready');
  await assert.rejects(demo.post('/api/spools', { ...draft, confirm: true, spool_condition: 'used' }), /link the existing/);
  const pending = await demo.request('/api/spools', { token: demo.token });
  assert.equal(pending.drafts.length, 1);
  const linked = await demo.post('/api/spools', { ...draft, spoolman_id: 73, confirm: true, spool_condition: 'used' });
  assert.equal(linked.spool.id, draft.id);
  assert.equal(linked.spool.spoolman_id, 73);
  assert.equal((await demo.request('/api/spools', { token: demo.token })).drafts.length, 0);
});

test('RFID linking accepts a read-only UID and requires a preview of the same present tag', () => {
  const tag = { nfc: { present: true, uid: '04:AA', writable: false, last_error: 'Contents unreadable' } };
  assert.equal(tagLinkGuard(tag, spool, '04:AA', [spool], true), '');
  assert.match(tagLinkGuard(tag, spool, '', [spool], true), /Scan the tag/);
  assert.match(tagLinkGuard(tag, spool, '04:BB', [spool], true), /changed/);
  assert.match(tagLinkGuard(tag, spool, '04:AA', [spool], false), /Reconnect/);
  assert.match(tagLinkGuard({ nfc: { present: false } }, spool, '04:AA', [spool], true), /No tag/);
  assert.match(tagLinkGuard(tag, null, '04:AA', [spool], true), /no longer/);
  assert.match(tagLinkGuard(tag, spool, '04:AA', [spool, { id: 'b', name: 'Blue PETG', tag_uid: '04:AA' }], true), /Blue PETG/);
  assert.match(tagLinkGuard(tag, { ...spool, tag_uid: '04:AA' }, '04:AA', [spool], true), /already linked to this spool/);
});

test('a registered non-active local spool can gain and change RFID without writing tag memory', async () => {
  const demo = await pairedDemo(); demo.setScenario('local');
  const original = (await demo.request('/api/spools', { token: demo.token })).spools.find(item => item.id === 'demo-2');
  const initialTag = (await demo.post('/api/nfc/read', {})).nfc;
  assert.notEqual((await demo.request('/api/status')).active_spool_id, original.id);
  const result = await demo.post('/api/spools/tag', { id: original.id, expected_uid: initialTag.uid, confirm: true });
  assert.equal(result.spool.tag_uid, initialTag.uid);
  assert.equal(result.spool.name, original.name);
  assert.equal(result.spool.remaining_weight_g, original.remaining_weight_g);
  assert.equal((await demo.request('/api/status')).nfc.last_message, initialTag.last_message);
  demo.setScenario('unreadable');
  const unreadable = (await demo.post('/api/nfc/read', {})).nfc;
  assert.equal(unreadable.writable, false);
  assert.ok(unreadable.last_error);
  await demo.post('/api/config', { spoolman_url: '' });
  await demo.post('/api/spools/tag', { id: original.id, expected_uid: unreadable.uid, confirm: true });
  const linked = (await demo.request('/api/spools', { token: demo.token })).spools.find(item => item.id === original.id);
  assert.equal(linked.tag_uid, unreadable.uid);
  assert.notEqual(linked.tag_uid, initialTag.uid);
  assert.equal((await demo.post('/api/nfc/read', {})).spool_id, original.id);
});

test('RFID link rejects missing, swapped, multiple, duplicate and unconfirmed tags without altering profiles', async () => {
  const demo = await pairedDemo(); demo.setScenario('local');
  const uid = (await demo.post('/api/nfc/read', {})).nfc.uid;
  await assert.rejects(demo.post('/api/spools/tag', { id: 'demo-2', expected_uid: uid }), /Confirm/);
  await demo.post('/api/spools/tag', { id: 'demo-2', expected_uid: uid, confirm: true });
  await assert.rejects(demo.post('/api/spools/tag', { id: 'demo-3', expected_uid: uid, confirm: true }), /another spool/);
  for (const scenario of ['removed', 'readonly', 'multiple-tags']) {
    demo.setScenario(scenario);
    await demo.post('/api/config', { spoolman_url: '' });
    await assert.rejects(demo.post('/api/spools/tag', { id: 'demo-2', expected_uid: uid, confirm: true }), /missing or changed/);
    assert.equal((await demo.request('/api/spools', { token: demo.token })).spools.find(item => item.id === 'demo-2').tag_uid, uid);
  }
});

test('local removal clears selection and cannot remove a remote link with the server disabled', async () => {
  const demo = await pairedDemo(); demo.setScenario('local');
  await demo.post('/api/active-spool', { id: 'demo-2' });
  await assert.rejects(demo.post('/api/spools/remove', { id: 'demo-2' }), /Confirm/);
  const result = await demo.post('/api/spools/remove', { id: 'demo-2', confirm: true });
  assert.equal(result.archived, false);
  assert.equal((await demo.request('/api/status')).active_spool_id, null);
  const inventory = await demo.request('/api/spools', { token: demo.token });
  assert.equal(inventory.spools.some(item => item.id === 'demo-2'), false);
  assert.equal(inventory.spools.length, 2);
  await assert.rejects(demo.post('/api/spools/remove', { id: 'demo-1', confirm: true }), /Reconnect the original/);
});

test('Spoolman removal archives a record and failures remain visible until checked', async () => {
  const demo = await pairedDemo(); demo.setScenario('archive-error');
  let failure;
  try { await demo.post('/api/spools/remove', { id: 'demo-1', confirm: true }); } catch (error) { failure = error; }
  assert.equal(failure.data.spool.removal_pending, true);
  assert.equal(failure.data.recovery_required, true);
  assert.equal((await demo.request('/api/spools', { token: demo.token })).spools.length, 1);
  await assert.rejects(demo.post('/api/spools/tag', { id: 'demo-1', expected_uid: '04:A1:B2:C3:D4:E5:F6', confirm: true }), /pending archive/);
  demo.setScenario('ready');
  const result = await demo.post('/api/spools/remove', { id: 'demo-1', confirm: true });
  assert.equal(result.archived, true);
  assert.match(result.message, /history is preserved/);
  assert.equal((await demo.request('/api/spools', { token: demo.token })).spools.length, 0);
  assert.equal((await demo.request('/api/status')).active_spool_id, null);
});

test('stale remote inventory blocks new removal and tag binding rather than changing local records', async () => {
  const demo = await pairedDemo(); demo.setScenario('sync-error');
  await assert.rejects(demo.post('/api/spools/remove', { id: 'demo-1', confirm: true }), /Refresh/);
  await assert.rejects(demo.post('/api/spools/tag', { id: 'demo-1', expected_uid: '04:A1:B2:C3:D4:E5:F6', confirm: true }), /Refresh/);
  const inventory = await demo.request('/api/spools', { token: demo.token });
  assert.equal(inventory.spools.length, 1);
  assert.equal(inventory.spools[0].tag_uid, undefined);
});

test('inventory removal and RFID binding require a paired session', async () => {
  const demo = createDemo();
  await assert.rejects(demo.request('/api/spools/remove', { method: 'POST', body: { id: 'demo-1', confirm: true } }), { status: 401 });
  await assert.rejects(demo.request('/api/spools/tag', { method: 'POST', body: { id: 'demo-1', expected_uid: '04:A1:B2:C3:D4:E5:F6', confirm: true } }), { status: 401 });
});

test('detected NFC-A, NFC-B, FeliCa and Jewel IDs bind unchanged without enabling memory writes', async () => {
  for (const [scenario, id] of [
    ['nfc-a4', '11223344'], ['nfc-a7', '04112233445566'], ['nfc-a10', '04112233445566778899'],
    ['nfc-b', 'B:11223344'], ['felica', 'F:0123456789ABCDEF'], ['jewel', 'J:11223344'],
  ]) {
    const demo = await pairedDemo(); demo.setScenario(scenario);
    await demo.post('/api/config', { spoolman_url: '' });
    const before = (await demo.post('/api/nfc/read', {})).nfc;
    assert.equal(before.uid, id);
    assert.equal(before.writable, false);
    assert.equal(tagLinkGuard({ nfc: before }, spool, id, [spool], true), '');
    const linked = await demo.post('/api/spools/tag', { id: 'demo-2', expected_uid: id, confirm: true });
    assert.equal(linked.spool.tag_uid, id);
    const scanned = await demo.post('/api/nfc/read', {});
    assert.equal(scanned.spool_id, 'demo-2');
    assert.equal(scanned.nfc.uid, id);
    assert.equal(scanned.nfc.last_message, before.last_message);
    await assert.rejects(demo.post('/api/nfc/write', { spool_id: 'demo-2', expected_uid: id, confirm: true }), /read-only/);
  }
});

test('protocol prefixes prevent NFC-A, NFC-B and Jewel ID collisions and cannot be dropped when confirming', async () => {
  const demo = await pairedDemo();
  for (const [scenario, id, profileId] of [['nfc-a4', '11223344', 'demo-1'], ['nfc-b', 'B:11223344', 'demo-2'], ['jewel', 'J:11223344', 'demo-3']]) {
    demo.setScenario(scenario);
    await demo.post('/api/config', { spoolman_url: '' });
    if (id.includes(':')) await assert.rejects(demo.post('/api/spools/tag', { id: profileId, expected_uid: '11223344', confirm: true }), /changed/);
    await demo.post('/api/spools/tag', { id: profileId, expected_uid: id, confirm: true });
  }
  const profiles = (await demo.request('/api/spools', { token: demo.token })).spools;
  assert.deepEqual(profiles.map(item => item.tag_uid), ['11223344', 'B:11223344', 'J:11223344']);
  await assert.rejects(demo.post('/api/spools/tag', { id: 'demo-2', expected_uid: 'J:11223344', confirm: true }), /another spool/);
});

test('only explicit archive recovery can resolve a hidden pending record with a valid server ID', () => {
  const archived = { ...spool, id: 'hidden', spoolman_id: 12, removal_pending: true };
  assert.equal(inventoryActionSpool([], [archived], 'hidden'), undefined);
  assert.equal(inventoryActionSpool([], [archived], 'hidden', 'archive'), archived);
  assert.equal(inventoryActionSpool([spool], [archived], spool.id), spool);
  for (const invalid of [null, 0, -1, 1.5, '12']) {
    assert.equal(inventoryActionSpool([], [{ ...archived, spoolman_id: invalid }], 'hidden', 'archive'), undefined);
  }
  assert.equal(inventoryActionSpool([], [{ ...archived, removal_pending: false }], 'hidden', 'archive'), undefined);
});

test('archive recovery remains possible after server refresh hides the spool and a simulated device restart', async () => {
  const demo = await pairedDemo(); demo.setScenario('archive-error');
  await assert.rejects(demo.post('/api/spools/remove', { id: 'demo-1', confirm: true }), /did not confirm/);
  demo.setScenario('archive-hidden');
  let snapshot = await demo.request('/api/spools?refresh=1', { token: demo.token });
  assert.equal(snapshot.spools.length, 0);
  assert.equal(snapshot.drafts.length, 1);
  assert.equal(snapshot.drafts[0].removal_pending, true);
  assert.equal(inventoryActionSpool(snapshot.spools, snapshot.drafts, 'demo-1'), undefined);
  assert.equal(inventoryActionSpool(snapshot.spools, snapshot.drafts, 'demo-1', 'archive').spoolman_id, 12);
  await assert.rejects(demo.post('/api/spools', { ...snapshot.drafts[0], name: 'Changed' }), /archive status before editing/);
  await assert.rejects(demo.post('/api/spools/tag', { id: 'demo-1', expected_uid: '04:A1:B2:C3:D4:E5:F6', confirm: true }), /no longer/);
  demo.restart();
  await assert.rejects(demo.request('/api/spools', { token: demo.token }), { status: 401 });
  const session = await demo.request('/api/session', { method: 'POST', body: { pin: '12345678' } });
  snapshot = await demo.request('/api/spools?refresh=1', { token: session.token });
  assert.equal(snapshot.drafts[0].id, 'demo-1');
  assert.equal(snapshot.drafts[0].removal_pending, true);
  const recovered = await demo.request('/api/spools/remove', { method: 'POST', token: session.token, body: { id: 'demo-1', confirm: true } });
  assert.equal(recovered.archived, true);
  const finished = await demo.request('/api/spools?refresh=1', { token: session.token });
  assert.equal(finished.spools.length, 0);
  assert.equal(finished.drafts.length, 0);
  assert.equal((await demo.request('/api/status')).spoolman.pending, false);
});

test('hidden draft lookup permits only the matching unlink, archive or clean discard operation', () => {
  const inactive = { ...spool, id: 'inactive', spoolman_id: 12, inactive_tag_link: true, tag_uid: '11223344' };
  const clean = { ...spool, id: 'clean', spoolman_id: null };
  const pending = { ...clean, id: 'pending', sync_pending: true };
  const archive = { ...inactive, id: 'archive', inactive_tag_link: false, removal_pending: true };
  const drafts = [inactive, clean, pending, archive];
  assert.equal(inventoryActionSpool([], drafts, inactive.id, 'unlink'), inactive);
  assert.equal(inventoryActionSpool([], drafts, clean.id, 'discard'), clean);
  assert.equal(inventoryActionSpool([], drafts, archive.id, 'archive'), archive);
  for (const [record, allowed] of [[inactive, 'unlink'], [clean, 'discard'], [pending, null], [archive, 'archive']]) {
    for (const operation of ['view', 'unlink', 'discard', 'archive']) {
      if (operation !== allowed) assert.equal(inventoryActionSpool([], drafts, record.id, operation), undefined);
    }
  }
  for (const invalid of [{ sync_pending: true }, { recovery_required: true }, { removal_pending: true }, { tag_uid: '' }, { spoolman_id: 0 }]) {
    assert.equal(inventoryActionSpool([], [{ ...inactive, ...invalid }], inactive.id, 'unlink'), undefined);
  }
  for (const changed of [{ sync_pending: true }, { recovery_required: true }, { removal_pending: true }, { spoolman_id: 12 }, { spoolman_id: '12' }]) assert.equal(cleanLocalDraft({ ...clean, ...changed }), false);
});

test('an externally archived spool retains its RFID until explicit unlink, then the ID can be reused', async () => {
  const demo = await pairedDemo(); demo.setScenario('inactive-tag');
  const before = await demo.request('/api/spools?refresh=1', { token: demo.token });
  const hidden = before.drafts.find(item => item.inactive_tag_link);
  assert.equal(before.spools.length, 0);
  assert.equal(hidden.tag_uid, '04:A1:B2:C3:D4:E5:F6');
  const created = await demo.post('/api/spools', { ...spool, id: undefined, name: 'Replacement spool', confirm: true, spool_condition: 'used' });
  await assert.rejects(demo.post('/api/spools/tag', { id: created.id, expected_uid: hidden.tag_uid, confirm: true }), /another spool/);
  await assert.rejects(demo.post('/api/spools/tag', { id: hidden.id, expected_uid: 'WRONG', unlink: true, confirm: true }), /saved RFID link changed/);
  assert.equal((await demo.request('/api/spools', { token: demo.token })).drafts[0].tag_uid, hidden.tag_uid);
  demo.setScenario('removed');
  assert.equal((await demo.request('/api/status')).nfc.present, false);
  const result = await demo.post('/api/spools/tag', { id: hidden.id, expected_uid: hidden.tag_uid, unlink: true, confirm: true });
  assert.equal(result.spool.archived, true);
  assert.equal(result.spool.spoolman_id, hidden.spoolman_id);
  assert.equal(result.spool.tag_uid, '');
  const cleared = await demo.request('/api/spools', { token: demo.token });
  assert.equal(cleared.drafts.some(item => item.id === hidden.id), false);
  assert.equal(cleared.spools.find(item => item.id === created.id).name, 'Replacement spool');
  demo.setScenario('ready');
  await demo.post('/api/spools/tag', { id: created.id, expected_uid: hidden.tag_uid, confirm: true });
  assert.equal((await demo.post('/api/nfc/read', {})).spool_id, created.id);
});

test('clean local drafts can be discarded even while the server is unavailable', async () => {
  const demo = await pairedDemo(); demo.setScenario('clean-draft');
  const draft = (await demo.request('/api/spools', { token: demo.token })).drafts[0];
  assert.equal(cleanLocalDraft(draft), true);
  demo.setScenario('remote-error');
  assert.equal((await demo.request('/api/spools', { token: demo.token })).inventory.stale, true);
  const discarded = await demo.post('/api/spools/remove', { id: draft.id, confirm: true });
  assert.equal(discarded.archived, false);
  assert.equal((await demo.request('/api/spools', { token: demo.token })).drafts.length, 0);
});

test('pending creation journals cannot be discarded or unlinked', async () => {
  const demo = await pairedDemo(); demo.setScenario('pending-draft');
  const before = (await demo.request('/api/spools', { token: demo.token })).drafts;
  assert.equal(cleanLocalDraft(before[0]), false);
  await assert.rejects(demo.post('/api/spools/remove', { id: before[0].id, confirm: true }), /pending Spoolman operation/);
  await assert.rejects(demo.post('/api/spools/tag', { id: before[0].id, expected_uid: '11223344', unlink: true, confirm: true }), /pending Spoolman operation/);
  assert.deepEqual((await demo.request('/api/spools', { token: demo.token })).drafts, before);
});

test('missing-server-spool recovery reports removal without claiming preserved archive history', async () => {
  const demo = await pairedDemo(); demo.setScenario('archive-missing');
  assert.equal((await demo.request('/api/spools', { token: demo.token })).drafts[0].removal_pending, true);
  const result = await demo.post('/api/spools/remove', { id: 'demo-1', confirm: true });
  assert.equal(result.archived, false);
  assert.equal(result.removed_missing, true);
  assert.match(result.message, /already absent/);
  assert.match(result.message, /history was not restored/);
  assert.equal((await demo.request('/api/spools', { token: demo.token })).drafts.length, 0);
});

test('tag metadata alone does not restore a removed local UID association', async () => {
  const demo = await pairedDemo();
  const initial = await demo.request('/api/status');
  assert.equal(initial.nfc.spool_id, 'demo-1');
  const read = await demo.post('/api/nfc/read', {});
  assert.equal(read.spool_id, null);
  assert.equal((await demo.request('/api/status')).active_spool_id, null);
});

test('a verified inventory-tag write also saves its explicit local binding', async () => {
  const demo = await pairedDemo();
  const uid = (await demo.request('/api/status')).nfc.uid;
  await demo.post('/api/nfc/write', { spool_id: 'demo-1', expected_uid: uid, confirm: true });
  const linked = (await demo.request('/api/spools', { token: demo.token })).spools[0];
  assert.equal(linked.tag_uid, uid);
  assert.equal((await demo.post('/api/nfc/read', {})).spool_id, 'demo-1');
});


test('five LEDs show 20% completed segments and distinct 5% partial colors', () => {
  assert.deepEqual(ledSegments(85), ['#00c8ff','#00c8ff','#00c8ff','#00c8ff','#ff2d2d']);
  assert.equal(ledSegments(90)[4], '#ff8c00');
  assert.equal(ledSegments(95)[4], '#ffdc00');
  assert.equal(ledSegments(99)[4], '#78dc32');
  assert.deepEqual(ledSegments(100), Array(5).fill('#00c8ff'));
  assert.deepEqual(ledSegments(60), ['#00c8ff','#00c8ff','#00c8ff',null,null]);
  assert.deepEqual(ledSegments(5), ['#ff2d2d',null,null,null,null]);
  assert.deepEqual(ledSegments(0), Array(5).fill(null));
  for (const invalid of [null, undefined, NaN, Infinity, -1, 101, '85']) assert.deepEqual(ledSegments(invalid), Array(5).fill(null));
});

test('diagnostics require a session and reject unknown components', async () => {
  const unpaired = createDemo();
  await assert.rejects(unpaired.request('/api/diagnostics', {method:'POST', body:{component:'leds'}}), {status:401});
  const demo = await pairedDemo();
  await assert.rejects(demo.post('/api/diagnostics', {component:'erase'}), /Choose nfc, leds or scale/);
  await assert.rejects(demo.request('/api/diagnostics', {token:demo.token}), {status:404});
});

test('NFC and scale diagnostics preserve active spool, profiles and stored configuration', async () => {
  const demo = await pairedDemo();
  const beforeStatus = await demo.request('/api/status');
  const beforeInventory = await demo.request('/api/spools', {token:demo.token});
  const beforeConfig = await demo.request('/api/config', {token:demo.token});
  const nfc = await demo.post('/api/diagnostics', {component:'nfc'});
  assert.equal(nfc.state, 'pass'); assert.equal(nfc.tag_detected, true); assert.equal(nfc.uid, beforeStatus.nfc.uid);
  const scale = await demo.post('/api/diagnostics', {component:'scale'});
  assert.equal(scale.state, 'pass'); assert.equal(scale.gross_g, 842); assert.equal(scale.fresh, true);
  const afterStatus = await demo.request('/api/status');
  assert.deepEqual(afterStatus.scale, beforeStatus.scale);
  assert.equal(afterStatus.active_spool_id, beforeStatus.active_spool_id);
  assert.deepEqual(await demo.request('/api/spools', {token:demo.token}), beforeInventory);
  assert.deepEqual(await demo.request('/api/config', {token:demo.token}), beforeConfig);
});

test('NFC diagnostics distinguish no tag, missing reader and multiple tags', async () => {
  const demo = await pairedDemo();
  demo.setScenario('removed');
  let result = await demo.post('/api/diagnostics', {component:'nfc'});
  assert.equal(result.state, 'warning'); assert.equal(result.reader_available, true); assert.equal(result.tag_detected, false);
  demo.setScenario('reader-missing');
  result = await demo.post('/api/diagnostics', {component:'nfc'});
  assert.equal(result.state, 'fail'); assert.equal(result.reader_available, false);
  demo.setScenario('multiple-tags');
  result = await demo.post('/api/diagnostics', {component:'nfc'});
  assert.equal(result.state, 'fail'); assert.equal(result.targets, 2); assert.equal(result.tag_detected, false);
});

test('scale diagnostics separate calibration, movement and missing samples', async () => {
  const demo = await pairedDemo();
  for (const [scenario, expected] of [['uncalibrated','warning'],['moving','warning'],['stale','fail']]) {
    demo.setScenario(scenario);
    assert.equal((await demo.post('/api/diagnostics', {component:'scale'})).state, expected);
  }
});

test('LED diagnostic runs for twelve seconds without blocking status or claiming visual success', async () => {
  let clock = 1000;
  const demo = createDemo({now:()=>clock});
  const {token} = await demo.request('/api/session', {method:'POST', body:{pin:'12345678'}});
  const result = await demo.request('/api/diagnostics', {method:'POST',token,body:{component:'leds'}});
  assert.equal(result.state, 'observe'); assert.equal(result.duration_ms, 12000);
  assert.equal((await demo.request('/api/status')).diagnostics.led_test_remaining_ms, 12000);
  clock += 6000;
  assert.equal((await demo.request('/api/status')).diagnostics.led_test_remaining_ms, 6000);
  clock += 6000;
  assert.equal((await demo.request('/api/status')).diagnostics.led_test_remaining_ms, 0);
});
