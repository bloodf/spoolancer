import test from 'node:test';
import assert from 'node:assert/strict';
import { measurement, scaleCalibrationGuard, scaleZeroGuard, tagWriteGuard, validateSpool, validSpoolmanUrl, normalizeHex, materialSelection, resolveMaterial, normalizeTheme, resolveTheme, readThemePreference, saveThemePreference, THEME_STORAGE_KEY, pairingState, sessionBody } from '../logic.js';
import { createDemo } from '../demo.js';

const spool = { id: 'a', name: 'Green PLA', material: 'PLA', brand: 'Generic', color: '#306040', empty_weight_g: 250, initial_weight_g: 1000, density_g_cm3: 1.24, diameter_mm: 1.75, spoolman_id: null };
const status = { scale: { calibrated: true, zero_set: true, stable: true, fresh: true, gross_g: 842 }, nfc: { present: true, uid: '04:AA', writable: true } };

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
  assert.match(measurement({ scale: { ...status.scale, calibrated: false } }, spool, true).message, /Open Calibration/);
});

test('scale zero requires a connected device and fresh, stable sensor readings', () => {
  assert.equal(scaleZeroGuard(status, true), '');
  assert.equal(scaleZeroGuard({ scale: { ...status.scale, calibrated: false, zero_set: false } }, true), '');
  for (const value of [false, undefined, 'true']) assert.match(scaleZeroGuard(status, value), /Reconnect/);
  assert.match(scaleZeroGuard(null, true), /Reconnect/);
  for (const value of [false, undefined, 'true']) {
    assert.match(scaleZeroGuard({ scale: { ...status.scale, fresh: value } }, true), /fresh/);
    assert.match(scaleZeroGuard({ scale: { ...status.scale, stable: value } }, true), /still/);
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
  const replacement = { scale: { ...status.scale, stable: false, gross_g: null } };
  assert.equal(scaleCalibrationGuard(replacement, true, true), '');
  assert.equal(scaleCalibrationGuard({ scale: { ...replacement.scale, calibrated: false } }, true, true), '');
  assert.match(scaleZeroGuard(replacement, true), /still/);
});

test('unstable weight can show gross but cannot save remaining', () => {
  const m = measurement({ scale: { ...status.scale, stable: false } }, spool, true);
  assert.equal(m.gross, 842);
  assert.equal(m.remaining, null);
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
  await demo.post('/api/spools', { ...profile, tag_uid: '12:34:56:78' });
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

const setupStatus = { device: { headless: true }, setup: { ap_active: true, via_ap: true, enrollment_required: true, enrollment_armed: true, enrollment_seconds: 60 } };

test('chosen PIN enrollment requires current AP status and a physical arm window', () => {
  assert.deepEqual(sessionBody(setupStatus, true, '23456789'), { pin: '23456789', setup: true });
  assert.equal(pairingState(setupStatus, true).canSubmit, true);
  for (const patch of [{ ap_active: false }, { via_ap: false }, { enrollment_armed: false }, { enrollment_seconds: 0 }, { enrollment_armed: 'true' }, { enrollment_seconds: undefined }]) {
    const changed = { ...setupStatus, setup: { ...setupStatus.setup, ...patch } };
    assert.equal(pairingState(changed, true).canSubmit, false);
    assert.throws(() => sessionBody(changed, true, '23456789'));
  }
  assert.throws(() => sessionBody(setupStatus, false, '23456789'), /Reconnect/);
  assert.throws(() => sessionBody(setupStatus, true, '123'), /8 digits/);
  assert.deepEqual(sessionBody({ device: { headless: true }, setup: { enrollment_required: false } }, true, '23456789'), { pin: '23456789' });
  assert.deepEqual(sessionBody({ device: { headless: false } }, true, '12345678'), { pin: '12345678' });
});

test('demo enrollment consumes one arm attempt and retains chosen PIN only in the adapter', async () => {
  const demo = createDemo();
  demo.setScenario('headless-setup');
  const enroll = pin => demo.request('/api/session', { method: 'POST', body: { pin, setup: true } });
  await assert.rejects(enroll('23456789'), { status: 403 });
  demo.armEnrollment();
  await assert.rejects(enroll('bad'), /attempt was consumed/);
  await assert.rejects(enroll('23456789'), { status: 403 });
  demo.armEnrollment();
  const { token } = await enroll('23456789');
  const state = await demo.request('/api/status');
  assert.equal(state.setup.enrollment_required, false);
  assert.equal(state.setup.enrollment_armed, false);
  assert.equal(JSON.stringify(state).includes('23456789'), false);
  assert.equal(JSON.stringify(await demo.request('/api/config', { token })).includes('23456789'), false);
  await assert.rejects(demo.request('/api/session', { method: 'POST', body: { pin: '12345678' } }), { status: 401 });
  assert.ok((await demo.request('/api/session', { method: 'POST', body: { pin: '23456789' } })).token);
});

test('demo physical enrollment window expires without silently renewing', async () => {
  let time = 0;
  const demo = createDemo({ now: () => time });
  demo.setScenario('headless-setup');
  demo.armEnrollment();
  time = 60001;
  assert.equal((await demo.request('/api/status')).setup.enrollment_armed, false);
  await assert.rejects(demo.request('/api/session', { method: 'POST', body: { pin: '23456789', setup: true } }), { status: 403 });
  time = 600001;
  demo.armEnrollment();
  const state = await demo.request('/api/status');
  assert.equal(state.setup.ap_active, false);
  assert.equal(state.setup.enrollment_armed, false);
});

test('touchscreen and OLED builds require physical PIN enrollment despite having a display', () => {
  for (const variant of ['touch-8048', 'oled-led']) {
    const screen = { ...setupStatus, device: { headless: false, variant } };
    assert.equal(pairingState(screen, true).eligible, true);
    assert.deepEqual(sessionBody(screen, true, '23456789'), { pin: '23456789', setup: true });
    const unarmed = { ...screen, setup: { ...screen.setup, enrollment_armed: false } };
    assert.equal(pairingState(unarmed, true).canSubmit, false);
    assert.throws(() => sessionBody(unarmed, true, '23456789'), /physical BOOT button/);
    const outsideSetup = { ...screen, setup: { ...screen.setup, via_ap: false } };
    assert.throws(() => sessionBody(outsideSetup, true, '23456789'), /temporary setup Wi-Fi/);
  }
  const legacy = { ...setupStatus, device: { headless: false, variant: 'st7789' } };
  assert.deepEqual(sessionBody(legacy, true, '12345678'), { pin: '12345678' });
});
