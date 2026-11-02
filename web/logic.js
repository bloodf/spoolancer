export function optionalNumber(value, label, { min = 0, max = 5000 } = {}) {
  if (value === null || value === undefined || String(value).trim() === '') return null;
  const n = Number(value);
  if (!Number.isFinite(n) || n < min || n > max) throw new Error(`${label} must be between ${min} and ${max}.`);
  return n;
}

export function validateSpool(input) {
  const name = String(input.name || '').trim();
  const material = String(input.material || '').trim();
  const brand = String(input.brand || '').trim();
  if (!name || name.length > 64) throw new Error('Enter a spool name, up to 64 characters.');
  if (!material || material.length > 30) throw new Error('Enter a material, up to 30 characters.');
  if (!brand || brand.length > 60) throw new Error('Enter a brand, up to 60 characters. Use Generic if unknown.');
  if (!/^#[0-9a-f]{6}$/i.test(input.color || '')) throw new Error('Choose a valid color.');
  const empty = optionalNumber(input.empty_weight_g, 'Empty spool weight', { max: 2000 });
  const initial = optionalNumber(input.initial_weight_g, 'Initial filament weight', { min: 1, max: 5000 });
  const density = optionalNumber(input.density_g_cm3, 'Density', { min: 0.1, max: 5 });
  const diameter = optionalNumber(input.diameter_mm, 'Diameter', { min: 0.5, max: 5 });
  const linked = optionalNumber(input.spoolman_id, 'Spoolman ID', { min: 1, max: 2147483647 });
  if (linked !== null && !Number.isInteger(linked)) throw new Error('Spoolman ID must be a whole number.');
  if (density === null || diameter === null) throw new Error('Enter filament density and diameter.');
  if (initial === null) throw new Error('Enter the initial filament weight, excluding the spool.');
  return {
    ...(input.id ? { id: String(input.id) } : {}), name, brand, material,
    color: input.color.toLowerCase(), empty_weight_g: empty, initial_weight_g: initial,
    density_g_cm3: density, diameter_mm: diameter, spoolman_id: linked,
  };
}

const finite = value => typeof value === 'number' && Number.isFinite(value);

export function measurement(status, spool, fresh) {
  const result = { state: 'offline', label: 'Device unavailable', message: 'Reconnect to the scale to see a live measurement.', gross: null, remaining: null, percent: null, canSync: false };
  if (!fresh || !status) return result;
  const scale = status.scale || {};
  if (scale.fresh !== true) return { ...result, state: 'waiting', label: 'Waiting for scale', message: 'The load-cell reading is stale. Check the scale connection.' };
  if (scale.calibrated !== true) return { ...result, state: 'uncalibrated', label: 'Calibration needed', message: 'Open Settings, then Calibration, to zero the empty cradle and calibrate with a known reference mass.' };
  if (!finite(scale.gross_g)) return { ...result, state: 'waiting', label: 'Waiting for weight', message: 'The device has not returned a valid measurement.' };
  result.gross = scale.gross_g;
  if (scale.gross_g < 0) return { ...result, state: 'invalid', label: 'Below zero', message: 'Remove the spool and check scale zero.' };
  if (scale.gross_g < 20) return { ...result, state: 'empty', label: 'Place a spool', message: 'At least 20 g gross weight is needed to save a measurement.' };
  if (scale.gross_g > 2000) return { ...result, state: 'invalid', label: 'Outside weighing range', message: 'This build targets up to 2,000 g gross spool weight. Remove the load.' };
  if (!spool) return scale.stable === true ? { ...result, state: 'unassigned', label: 'Stable gross weight', message: 'Select or add a spool to calculate remaining filament.' } : { ...result, state: 'unstable', label: 'Settling', message: 'Keep the spool still. Wait for a stable reading.' };
  if (!finite(spool.empty_weight_g) || spool.empty_weight_g < 0 || spool.empty_weight_g > 2000) return { ...result, state: 'unknown-tare', label: 'Empty spool weight needed', message: 'Edit this spool and enter its known empty weight. Gross weight cannot establish remaining filament.' };
  if (!finite(spool.initial_weight_g) || spool.initial_weight_g < 1 || spool.initial_weight_g > 5000) return { ...result, state: 'invalid', label: 'Initial weight needed', message: 'Edit this spool and enter its initial filament weight, from 1 to 5,000 g.' };
  const remaining = scale.gross_g - spool.empty_weight_g;
  if (remaining < 0) return { ...result, state: 'invalid', label: 'Check the spool profile', message: 'Gross weight is below the saved empty spool weight.' };
  if (finite(spool.initial_weight_g) && remaining > spool.initial_weight_g) return { ...result, state: 'invalid', label: 'Check the starting weight', message: 'Remaining filament exceeds the saved initial filament weight.' };
  const stable = scale.stable === true;
  return { ...result, state: stable ? 'ready' : 'unstable', label: stable ? 'Stable measurement' : 'Settling', message: stable ? 'Calculated from gross weight and this spool’s saved empty weight.' : 'Live estimate while settling. Keep the spool still before saving.', remaining, percent: remaining / spool.initial_weight_g * 100, canSync: stable };
}

// Printer formats mirror open-source writers accepted by stock firmware (see firmware/include/BrandTags.h).
export const TAG_FORMATS = [
  { id: 'spoolancer', label: 'Spoolancer inventory', chip: 'ntag', note: 'Full spool profile for this scale. Printers do not read it.' },
  { id: 'creality', label: 'Creality CFS (K2, K1, Hi)', chip: 'classic', note: 'MIFARE Classic 1K. Material ID, color and length; the printer supplies temperatures.' },
  { id: 'anycubic', label: 'Anycubic ACE (Kobra 3, S1)', chip: 'ntag', note: 'NTAG213/215/216. Material, color, temperatures, diameter and length.' },
  { id: 'elegoo', label: 'Elegoo Canvas (Centauri Carbon)', chip: 'ntag', note: 'NTAG213/215/216. Material, color, nozzle temperature and weight.' },
  { id: 'qidi', label: 'Qidi Box (Plus4, Q2, Max4)', chip: 'classic', note: 'MIFARE Classic 1K. Material code and the nearest of 24 Qidi colors.' },
  { id: 'openspool', label: 'OpenSpool (Snapmaker U1 Extended, OpenSpool readers)', chip: 'ntag', note: 'NTAG215/216 JSON record. Stock Snapmaker U1 firmware does not read it.' },
];

export function tagWriteGuard(status, expectedUid, fresh, format = 'spoolancer') {
  if (!fresh) return 'The device is disconnected. Scan the tag again after reconnecting.';
  const tag = status?.nfc;
  const target = TAG_FORMATS.find(item => item.id === format);
  if (!target) return 'Choose a supported tag format.';
  if (tag?.present !== true || !tag.uid) return 'No tag is present. Position a writable tag at the reader.';
  if (format !== 'spoolancer') {
    if (tag.chip !== target.chip) return target.chip === 'classic' ? `${target.label} needs a MIFARE Classic 1K tag with a 4-byte ID.` : `${target.label} needs an NTAG213, NTAG215 or NTAG216 tag.`;
    if (tag.raw_writable !== true) return 'This tag is locked, password-protected or holds several targets. Bambu Lab and stock Snapmaker tags are signed and cannot be written.';
  } else if (tag.writable !== true) return tag.raw_writable === true ? 'This tag holds other content. The Spoolancer inventory record needs a blank or Spoolancer tag; choose a printer format to overwrite it.' : 'This tag is read-only or unsupported. Original printer tags are never written.';
  if (expectedUid && tag.uid !== expectedUid) return 'The tag changed. Cancel and preview the new tag before writing.';
  return '';
}

export function tagLinkGuard(status, spool, expectedUid, spools, fresh) {
  if (!fresh) return 'Reconnect to the device before linking a tag.';
  if (!spool) return 'This spool is no longer in the inventory. Close this dialog and refresh.';
  if (typeof expectedUid !== 'string' || !expectedUid) return 'Scan the tag to preview its ID first.';
  const tag = status?.nfc;
  if (tag?.present !== true || !tag.uid) return 'No tag is present. Position one tag at the reader and scan again.';
  if (tag.uid !== expectedUid) return 'The tag changed. Scan again and review its ID.';
  const owner = spools.find(item => String(item.id) !== String(spool.id) && item.tag_uid === expectedUid);
  if (owner) return `This tag is already linked to ${owner.name}. Choose another tag.`;
  if (spool.tag_uid === expectedUid) return 'This tag is already linked to this spool.';
  return '';
}

export function scaleZeroGuard(status, fresh) {
  if (fresh !== true || !status) return 'Reconnect to the device before zeroing the scale.';
  if (status.scale?.fresh !== true) return 'Wait for fresh load-cell readings. Check the HX711 connection if they do not arrive.';
  if (status.scale.tare_ready !== true) return 'Keep the empty cradle still while enough stable readings are collected for zeroing.';
  return '';
}

export function scaleCalibrationGuard(status, fresh, zeroed = false) {
  if (fresh !== true || !status) return 'Reconnect to the device before calibrating the scale.';
  if (status.scale?.fresh !== true) return 'Wait for fresh load-cell readings. Check the HX711 connection if they do not arrive.';
  if (zeroed !== true || status.scale.zero_set !== true) return 'Zero the empty cradle in this calibration session before adding the reference mass.';
  if (status.scale.calibration_ready !== true) return 'Keep the reference still while enough readings are collected for calibration.';
  return '';
}

export function grams(value) {
  return finite(value) ? new Intl.NumberFormat('en', { maximumFractionDigits: 1 }).format(value) : '—';
}

export function validSpoolmanUrl(value) {
  if (value === '') return true;
  const match = /^http:\/\/(\d{1,3}(?:\.\d{1,3}){3})(?::(\d{1,5}))?(\/[^\s?#@\\]*)?$/.exec(value);
  if (!match) return false;
  const octets = match[1].split('.');
  if (octets.some(part => Number(part) > 255 || (part.length > 1 && part.startsWith('0')))) return false;
  if (Number(octets[0]) === 0 || Number(octets[0]) >= 224) return false;
  return match[2] === undefined || (Number(match[2]) >= 1 && Number(match[2]) <= 65535);
}

export const COMMON_MATERIALS = Object.freeze(['PLA', 'PLA+', 'PETG', 'ABS', 'ASA', 'TPU', 'PA', 'PC', 'PVA', 'HIPS']);
export const COLOR_SWATCHES = Object.freeze([
  ['Black', '#202523'], ['White', '#f3f0e5'], ['Gray', '#8c9593'],
  ['Red', '#c84e45'], ['Orange', '#dc8c36'], ['Yellow', '#e5c655'],
  ['Green', '#386c56'], ['Blue', '#3d75b9'], ['Purple', '#8b65b4'], ['Pink', '#df8fa3'],
]);

export function normalizeHex(value) {
  const hex = String(value ?? '').trim().replace(/^#/, '');
  if (!/^(?:[0-9a-f]{3}|[0-9a-f]{6})$/i.test(hex)) throw new Error('Enter a 3- or 6-digit hex color, such as #386C56.');
  return `#${(hex.length === 3 ? [...hex].map(c => c + c).join('') : hex).toLowerCase()}`;
}

export function materialSelection(value) {
  const material = String(value ?? '').trim();
  return { choice: !material ? '' : COMMON_MATERIALS.includes(material) ? material : 'custom', custom: COMMON_MATERIALS.includes(material) ? '' : material };
}

export function resolveMaterial(choice, custom) {
  const material = choice === 'custom' ? String(custom ?? '').trim() : COMMON_MATERIALS.includes(choice) ? choice : '';
  if (!material || material.length > 30 || /[\x00-\x1f]/.test(material)) throw new Error('Choose a material or enter a custom material, up to 30 characters.');
  return material;
}

export const THEME_STORAGE_KEY = 'spoolancer.theme';
export const normalizeTheme = preference => ['light', 'dark', 'system'].includes(preference) ? preference : 'system';
export const resolveTheme = (preference, systemDark) => normalizeTheme(preference) === 'system' ? (systemDark ? 'dark' : 'light') : preference;
export function readThemePreference(storage) {
  try { return normalizeTheme(storage.getItem(THEME_STORAGE_KEY)); } catch { return 'system'; }
}
export function saveThemePreference(storage, preference) {
  const theme = normalizeTheme(preference);
  try { storage.setItem(THEME_STORAGE_KEY, theme); } catch { /* Theme still applies when storage is unavailable. */ }
  return theme;
}

export const usesOpenSetup = status => status?.device?.headless === true || ['touch-8048', 'oled-led'].includes(status?.device?.variant);

export function setupState(status, fresh) {
  const setup = status?.setup || {};
  const required = setup.required === true;
  const eligible = required && setup.via_ap === true && setup.ap_active === true;
  return { required, eligible, canSubmit: eligible && fresh === true && setup.can_submit === true };
}

export function pairingState(status, fresh) {
  const mode = setupState(status, fresh);
  return { ...mode, canSubmit: !!status && fresh === true && !mode.required };
}

export const pinIsOptional = status => status?.auth?.pin_required === false && status.auth.invalid !== true && status?.setup?.complete === true && status.setup.required === false;

const byteLength = value => new TextEncoder().encode(value).length;
const validNetworkText = value => typeof value === 'string' && !/[\x00-\x1f\x7f]/.test(value) && ![...value].some(character => {
  const code = character.codePointAt(0);
  return code >= 0xd800 && code <= 0xdfff;
});

export function wifiCredentials(fields, { allowSavedPassword = false } = {}) {
  const ssid = fields.wifi_ssid;
  const password = fields.wifi_password ?? '';
  if (!validNetworkText(ssid) || !ssid.trim() || byteLength(ssid) > 32) throw new Error('Choose a Wi-Fi network or enter its exact name, from 1 to 32 UTF-8 bytes.');
  if (typeof fields.wifi_open !== 'boolean') throw new Error('Choose whether this Wi-Fi network requires a password.');
  if (!validNetworkText(password)) throw new Error('Enter a Wi-Fi password without control characters.');
  if (fields.wifi_open) {
    if (password !== '') throw new Error('An open Wi-Fi network must have an empty password.');
  } else if (!(allowSavedPassword && password === '') && !(byteLength(password) >= 8 && byteLength(password) <= 63) && !/^[0-9a-f]{64}$/i.test(password)) {
    throw new Error('Enter a Wi-Fi password of 8 to 63 UTF-8 bytes, or a 64-digit hexadecimal key.');
  }
  return { wifi_ssid: ssid, wifi_password: password, wifi_open: fields.wifi_open };
}

export function setupBody(status, fresh, fields) {
  const mode = setupState(status, fresh);
  if (fresh !== true || !status) throw new Error('Reconnect to the device before completing setup.');
  if (!mode.required) throw new Error('Initial setup is already complete. Use Settings to change Wi-Fi.');
  if (!mode.eligible) throw new Error('Join the temporary setup Wi-Fi and open http://192.168.4.1 to complete setup.');
  if (!mode.canSubmit) throw new Error('Setup is not available yet. Reconnect to the scale’s temporary Wi-Fi and try again.');
  if (typeof fields.pin_required !== 'boolean') throw new Error('Choose whether to protect device access with a PIN.');
  if (fields.pin_required && (typeof fields.pin !== 'string' || !/^[0-9]{8}$/.test(fields.pin))) throw new Error('Enter exactly 8 digits for the device PIN.');
  return { ...wifiCredentials(fields), pin_required: fields.pin_required, ...(fields.pin_required ? { pin: fields.pin } : {}) };
}

export function networkChoices(raw) {
  const strongest = new Map();
  for (const network of Array.isArray(raw) ? raw : []) {
    if (!network || !validNetworkText(network.ssid) || !network.ssid.trim() || byteLength(network.ssid) > 32 || !Number.isFinite(network.rssi)) continue;
    if (strongest.has(network.ssid) && strongest.get(network.ssid).rssi >= network.rssi) continue;
    strongest.set(network.ssid, {
      ssid: network.ssid, rssi: network.rssi, secure: network.secure === true,
      channel: Number.isInteger(network.channel) ? network.channel : null,
      supported: network.supported === true && typeof network.secure === 'boolean',
    });
  }
  return [...strongest.values()].sort((a, b) => b.rssi - a.rssi || a.ssid.localeCompare(b.ssid)).slice(0, 32);
}

export function sessionBody(status, fresh, pin) {
  const mode = pairingState(status, fresh);
  if (fresh !== true || !status) throw new Error('Reconnect to the device before pairing.');
  if (status.auth?.invalid === true) throw new Error('Device access settings are invalid. Hold BOOT for 8 seconds to reset Wi-Fi and setup.');
  if (status.setup?.required === true) throw new Error('Complete Wi-Fi setup first.');
  if (pinIsOptional(status)) return {};
  if (typeof pin !== 'string' || !/^[0-9]{8}$/.test(pin)) throw new Error('Enter exactly 8 digits for the device PIN.');
  return { pin };
}

// A full spool's nominal filament mass estimates tare; a used spool cannot.
export function fullSpoolTare(status, fresh, initialWeight, condition) {
  if (condition !== 'full') throw new Error('Choose New, full spool before estimating empty weight.');
  const initial = optionalNumber(initialWeight, 'Initial filament weight', { min: 1, max: 5000 });
  if (initial === null) throw new Error('Enter the full spool’s declared filament weight.');
  const reading = measurement(status, null, fresh);
  if (reading.state !== 'unassigned') throw new Error(reading.message);
  const empty = reading.gross - initial;
  if (empty < 0) throw new Error('Gross weight is below the declared filament weight. Check the label and the scale.');
  if (empty > 2000) throw new Error('Estimated empty-spool weight is outside the supported range.');
  return { empty: Math.round(empty * 10) / 10, gross: reading.gross, initial };
}

export function inventoryState(payload, configured) {
  const inventory = payload?.inventory || {};
  const source = configured || inventory.configured === true ? 'spoolman' : 'local';
  // A configured device never silently presents its local records as server data.
  if (source === 'spoolman' && inventory.source !== 'spoolman') {
    return { source, stale: true, cached: false, error: 'Server inventory could not be loaded. Refresh or check Spoolman settings.', spools: [] };
  }
  return { source, stale: inventory.stale === true, cached: inventory.cached === true,
    error: typeof inventory.error === 'string' ? inventory.error : '', limit: inventory.limit || 200,
    spools: Array.isArray(payload?.spools) ? payload.spools.filter(spool => source === 'local' || spool.source === 'spoolman') : [] };
}

export function cleanLocalDraft(spool) {
  return !!spool && (spool.spoolman_id == null || spool.spoolman_id === 0)
    && !spool.removal_pending && !spool.recovery_required && !spool.sync_pending && !spool.inactive_tag_link;
}

export function inventoryActionSpool(spools, drafts, id, operation = 'view') {
  const visible = spools.find(item => String(item.id) === String(id));
  const draft = drafts.find(item => String(item.id) === String(id));
  if (operation === 'discard') return draft && cleanLocalDraft(draft) ? draft : undefined;
  if (operation === 'unlink') return draft?.inactive_tag_link === true && !draft.removal_pending && !draft.sync_pending && !draft.recovery_required
    && Number.isInteger(draft.spoolman_id) && draft.spoolman_id > 0 && typeof draft.tag_uid === 'string' && draft.tag_uid ? draft : undefined;
  if (visible) return visible;
  return operation === 'archive' && draft?.removal_pending === true
    && Number.isInteger(draft.spoolman_id) && draft.spoolman_id > 0 ? draft : undefined;
}


// Completed 20% segments are cyan; a partial segment has four 5% color bands.
export function ledSegments(percent) {
  if (typeof percent !== 'number' || !Number.isFinite(percent) || percent < 0 || percent > 100) return Array(5).fill(null);
  return Array.from({length:5}, (_, i) => {
    const fraction = Math.min(1, Math.max(0, percent / 20 - i));
    return fraction <= 0 ? null : fraction >= 1 - 1e-9 ? '#00c8ff' : fraction <= .25 + 1e-9 ? '#ff2d2d' : fraction <= .5 + 1e-9 ? '#ff8c00' : fraction <= .75 + 1e-9 ? '#ffdc00' : '#78dc32';
  });
}
