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
  if (scale.calibrated !== true) return { ...result, state: 'uncalibrated', label: 'Calibration needed', message: 'Open Calibration to zero the empty cradle and calibrate with a known reference mass.' };
  if (!finite(scale.gross_g)) return { ...result, state: 'waiting', label: 'Waiting for weight', message: 'The device has not returned a valid measurement.' };
  result.gross = scale.gross_g;
  if (scale.stable !== true) return { ...result, state: 'unstable', label: 'Settling', message: 'Keep the spool still. Wait for a stable reading.' };
  if (scale.gross_g < 0) return { ...result, state: 'invalid', label: 'Below zero', message: 'Remove the spool and check scale zero.' };
  if (scale.gross_g < 20) return { ...result, state: 'empty', label: 'Place a spool', message: 'At least 20 g gross weight is needed to save a measurement.' };
  if (scale.gross_g > 2000) return { ...result, state: 'invalid', label: 'Outside weighing range', message: 'This build targets up to 2,000 g gross spool weight. Remove the load.' };
  if (!spool) return { ...result, state: 'unassigned', label: 'Stable gross weight', message: 'Select or add a spool to calculate remaining filament.' };
  if (!finite(spool.empty_weight_g) || spool.empty_weight_g < 0 || spool.empty_weight_g > 2000) return { ...result, state: 'unknown-tare', label: 'Empty spool weight needed', message: 'Edit this spool and enter its known empty weight. Gross weight cannot establish remaining filament.' };
  if (!finite(spool.initial_weight_g) || spool.initial_weight_g < 1 || spool.initial_weight_g > 5000) return { ...result, state: 'invalid', label: 'Initial weight needed', message: 'Edit this spool and enter its initial filament weight, from 1 to 5,000 g.' };
  const remaining = scale.gross_g - spool.empty_weight_g;
  if (remaining < 0) return { ...result, state: 'invalid', label: 'Check the spool profile', message: 'Gross weight is below the saved empty spool weight.' };
  if (finite(spool.initial_weight_g) && remaining > spool.initial_weight_g) return { ...result, state: 'invalid', label: 'Check the starting weight', message: 'Remaining filament exceeds the saved initial filament weight.' };
  return { ...result, state: 'ready', label: 'Stable measurement', message: 'Calculated from gross weight and this spool’s saved empty weight.', remaining, percent: finite(spool.initial_weight_g) && spool.initial_weight_g > 0 ? Math.min(100, remaining / spool.initial_weight_g * 100) : null, canSync: true };
}

export function tagWriteGuard(status, expectedUid, fresh) {
  if (!fresh) return 'The device is disconnected. Scan the tag again after reconnecting.';
  const tag = status?.nfc;
  if (tag?.present !== true || !tag.uid) return 'No tag is present. Position a writable inventory tag at the reader.';
  if (tag.writable !== true) return 'This tag is read-only or unsupported. Original printer tags are never written.';
  if (expectedUid && tag.uid !== expectedUid) return 'The tag changed. Cancel and preview the new tag before writing.';
  return '';
}

export function scaleZeroGuard(status, fresh) {
  if (fresh !== true || !status) return 'Reconnect to the device before zeroing the scale.';
  if (status.scale?.fresh !== true) return 'Wait for fresh load-cell readings. Check the HX711 connection if they do not arrive.';
  if (status.scale.stable !== true) return 'Keep the empty cradle still and wait for a stable reading before zeroing.';
  return '';
}

export function scaleCalibrationGuard(status, fresh, zeroed = false) {
  if (fresh !== true || !status) return 'Reconnect to the device before calibrating the scale.';
  if (status.scale?.fresh !== true) return 'Wait for fresh load-cell readings. Check the HX711 connection if they do not arrive.';
  if (zeroed !== true || status.scale.zero_set !== true) return 'Zero the empty cradle in this calibration session before adding the reference mass.';
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

export function pairingState(status, fresh) {
  const setup = status?.setup || {};
  const required = usesOpenSetup(status) && setup.enrollment_required === true;
  const eligible = required && setup.via_ap === true && setup.ap_active === true;
  const armed = eligible && fresh && setup.enrollment_armed === true && Number.isFinite(setup.enrollment_seconds) && setup.enrollment_seconds > 0;
  return { required, eligible, armed, canSubmit: !!fresh && (!required || armed) };
}

export function sessionBody(status, fresh, pin) {
  const mode = pairingState(status, fresh);
  if (!fresh) throw new Error('Reconnect to the device before pairing.');
  if (mode.required && !mode.eligible) throw new Error('Join the temporary setup Wi-Fi and open http://192.168.4.1 to choose a PIN.');
  if (mode.required && !mode.armed) throw new Error('Hold and release the physical BOOT button for 2 seconds while the scale is running to allow one PIN enrollment attempt.');
  if (!/^[0-9]{8}$/.test(pin)) throw new Error('Enter exactly 8 digits for the device PIN.');
  return { pin, ...(mode.required ? { setup: true } : {}) };
}
