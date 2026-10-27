import * as THREE from 'three';
import { OrbitControls } from './vendor/OrbitControls.js';
import { clampExplosion, partPosition, partVisible } from './model.mjs';

const $ = id => document.getElementById(id);
const viewport = $('viewport');
const canvas = $('canvas');
const loading = $('loading');
const state = { amount: 0, selected: null, isolated: null, hidden: new Set(), visibility: { spool: true, electronics: true, clearance: false }, xray: false, labels: true };
const groups = new Map();
const raycaster = new THREE.Raycaster();
const pointer = new THREE.Vector2();
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(36, 1, 0.1, 6000);
camera.up.set(0, 0, 1);
let renderer;
try {
  renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false });
} catch {
  loading.textContent = 'This browser cannot start WebGL2. Open the STEP or FreeCAD assembly from the repository instead.';
  throw new Error('WebGL2 is required for the assembly viewer.');
}
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFSoftShadowMap;
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.25;
const controls = new OrbitControls(camera, canvas);
controls.enableDamping = true;
controls.dampingFactor = 0.09;
controls.minDistance = 25;
controls.maxDistance = 2200;
controls.listenToKeyEvents(canvas);
controls.target.set(0, -15, 110);
camera.position.set(335, -430, 320);
controls.update();
const hemi = new THREE.HemisphereLight(0xffffff, 0x71816a, 2.5);
scene.add(hemi);
const key = new THREE.DirectionalLight(0xffffff, 3.1);
key.position.set(-250, -350, 600);
key.castShadow = true;
key.shadow.mapSize.set(2048, 2048);
key.shadow.camera.left = key.shadow.camera.bottom = -600;
key.shadow.camera.right = key.shadow.camera.top = 600;
key.shadow.camera.far = 1600;
key.shadow.bias = -0.0002;
key.shadow.normalBias = 0.5;
scene.add(key);
const fill = new THREE.DirectionalLight(0xdcebf1, 1.4);
fill.position.set(400, 200, 300);
scene.add(fill);
const floor = new THREE.Mesh(new THREE.PlaneGeometry(1800, 1800), new THREE.ShadowMaterial({ opacity: 0.12 }));
floor.position.z = -0.5;
floor.receiveShadow = true;
scene.add(floor);
let dirty = true;
let loadNumber = 0;
let data = null;
let lastPointer = null;
const initialSelection = $('selection').innerHTML;
const categoryNames = { printed: 'Printed pieces', electronics: 'Hardware references', spool: 'Spool reference', clearance: 'Routing & clearances' };
const housingIds = new Set(['base', 'top_cover', 'nfc_pod', 'nfc_pod_cover', 'nfc_cable_cover']);

function theme(value) {
  document.documentElement.dataset.theme = value;
  const dark = value === 'dark';
  scene.background = new THREE.Color(dark ? '#202e27' : '#e9eee5');
  $('theme').setAttribute('aria-label', `Switch to ${dark ? 'light' : 'dark'} theme`);
  try { localStorage.setItem('spoolancer-viewer-theme', value); } catch { /* Storage is optional. */ }
  dirty = true;
}
let savedTheme;
try { savedTheme = localStorage.getItem('spoolancer-viewer-theme'); } catch { /* Storage is optional. */ }
theme(savedTheme === 'dark' ? 'dark' : 'light');
$('theme').addEventListener('click', () => theme(document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark'));

function clearScene() {
  for (const { group, label } of groups.values()) {
    group.traverse(object => {
      object.geometry?.dispose();
      if (Array.isArray(object.material)) object.material.forEach(material => material.dispose());
      else object.material?.dispose();
    });
    scene.remove(group);
    label.remove();
  }
  groups.clear();
}

function makePart(part, buffer) {
  const group = new THREE.Group();
  group.userData.part = part;
  for (const mesh of part.meshes) {
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(new Float32Array(buffer, mesh.positionOffset, mesh.vertexCount * 3), 3));
    geometry.setIndex(new THREE.BufferAttribute(new Uint32Array(buffer, mesh.indexOffset, mesh.triangleCount * 3), 1));
    geometry.computeVertexNormals();
    geometry.computeBoundingSphere();
    const color = new THREE.Color().setRGB(...mesh.color, THREE.SRGBColorSpace);
    const material = new THREE.MeshStandardMaterial({ color, roughness: 0.68, metalness: part.id === 'load_cell' || part.id === 'stops' ? 0.4 : 0.05, flatShading: true });
    const object = new THREE.Mesh(geometry, material);
    object.userData.part = part;
    object.castShadow = part.category !== 'clearance';
    object.receiveShadow = true;
    if (part.category === 'clearance') {
      material.transparent = true;
      material.opacity = part.role === 'cable_route' ? 0.82 : 0.17;
      material.depthWrite = false;
      object.castShadow = false;
    }
    group.add(object);
    const edges = new THREE.LineSegments(new THREE.EdgesGeometry(geometry, 32), new THREE.LineBasicMaterial({ color: part.category === 'clearance' ? 0x9b8155 : 0x1f5b4a, transparent: true, opacity: part.category === 'clearance' ? 0.7 : 0.75 }));
    edges.visible = part.category === 'clearance';
    edges.userData.isOutline = true;
    group.add(edges);
  }
  const label = document.createElement('button');
  label.className = 'part-label';
  label.textContent = part.number;
  label.dataset.kind = part.category;
  label.title = `${part.number} · ${part.label}`;
  label.setAttribute('aria-label', `${part.number}: ${part.label}`);
  label.addEventListener('click', () => selectPart(part.id));
  $('labels').append(label);
  groups.set(part.id, { group, part, label });
  scene.add(group);
}

async function loadVariant(variant) {
  const ticket = ++loadNumber;
  loading.hidden = false;
  loading.textContent = 'Loading actual CAD geometry…';
  try {
    const response = await fetch(`./assets/${variant}.json`);
    if (!response.ok) throw new Error(`Model description: HTTP ${response.status}`);
    const nextData = await response.json();
    const binaryResponse = await fetch(`./assets/${nextData.meshFile}`);
    if (!binaryResponse.ok) throw new Error(`Model geometry: HTTP ${binaryResponse.status}`);
    const buffer = await binaryResponse.arrayBuffer();
    if (buffer.byteLength !== nextData.byteLength) throw new Error('Incomplete geometry download');
    if (ticket !== loadNumber) return;
    clearScene();
    data = nextData;
    for (const part of data.parts) makePart(part, buffer);
    resetState();
    $('group-count').textContent = data.parts.length;
    $('part-count').textContent = `${data.printedInstances} printed pieces`;
    $('build-info').textContent = `CAD source SHA-256 ${data.sourceSha256.slice(0, 12)} · Three.js r180`;
    $('build-info').title = `CAD: ${data.source}\nSource SHA-256: ${data.sourceSha256}\nMesh SHA-256: ${data.meshSha256}`;
    loading.hidden = true;
    const url = new URL(location.href);
    url.searchParams.set('model', variant);
    history.replaceState({}, '', url);
    dirty = true;
  } catch (error) {
    if (ticket !== loadNumber) return;
    loading.textContent = `The model could not load. ${error.message}. Serve the viewer folder over HTTP; see README.md.`;
    console.error(error);
  }
}

function setSelectedDetails(part) {
  const element = $('selection');
  if (!part) { element.innerHTML = initialSelection; return; }
  element.replaceChildren();
  const eyebrow = document.createElement('p');
  eyebrow.className = 'eyebrow';
  eyebrow.textContent = `${part.number} / ${categoryNames[part.category]}`;
  const heading = document.createElement('h3');
  heading.textContent = part.label;
  const description = document.createElement('p');
  description.textContent = part.description;
  const note = document.createElement('p');
  note.className = 'selection-note';
  note.textContent = part.pose === 'min' || part.pose === 'max'
    ? state.isolated === part.id
      ? 'Travel reference shown alone. Return to the assembly to see the nominal cable route.'
      : 'Alternative route hidden in the assembly. Isolate it to inspect this height position.'
    : part.provisional ? 'Reference geometry · verify the real component.' : 'Printable component · nominal CAD fit, not physically validated.';
  const actions = document.createElement('div');
  actions.className = 'selection-actions';
  for (const [text, action] of [
    [state.isolated === part.id ? 'Show assembly' : 'Isolate part', () => { state.isolated = state.isolated === part.id ? null : part.id; update(); setSelectedDetails(part); fitVisible(); }],
    ['Focus', () => fitVisible(part.id)],
    [state.hidden.has(part.id) ? 'Show part' : 'Hide part', () => { if (state.hidden.has(part.id)) state.hidden.delete(part.id); else state.hidden.add(part.id); update(); setSelectedDetails(part); }],
  ]) {
    const button = document.createElement('button');
    button.textContent = text;
    button.addEventListener('click', action);
    actions.append(button);
  }
  element.append(eyebrow, heading, description, note, actions);
}

function selectPart(id) {
  state.selected = id;
  const part = groups.get(id)?.part;
  if (part) {
    state.hidden.delete(id);
    if (state.isolated && state.isolated !== id) state.isolated = null;
    if (part.pose === 'min' || part.pose === 'max') state.isolated = id;
    if (part.category !== 'printed') {
      state.visibility[part.category] = true;
      $(`show-${part.category === 'electronics' ? 'electronics' : part.category}`).checked = true;
    }
  }
  update();
  setSelectedDetails(part);
}

function renderList() {
  const focusedId = document.activeElement?.dataset.part;
  $('parts').replaceChildren();
  for (const category of ['printed', 'electronics', 'spool', 'clearance']) {
    const entries = [...groups.values()].filter(item => item.part.category === category);
    if (!entries.length) continue;
    const title = document.createElement('div');
    title.className = 'group-label';
    title.textContent = categoryNames[category].toUpperCase();
    $('parts').append(title);
    for (const { part, group } of entries) {
      const button = document.createElement('button');
      button.className = 'part-row';
      button.dataset.part = part.id;
      button.setAttribute('aria-pressed', String(state.selected === part.id));
      const number = document.createElement('span');
      number.className = 'part-number';
      number.textContent = part.number;
      const text = document.createElement('span');
      text.textContent = part.label;
      const marker = document.createElement('span');
      marker.className = group.visible ? 'part-dot' : 'part-hidden';
      if (group.visible) marker.style.background = `rgb(${part.meshes[0].color.map(value => Math.round(value * 255)).join(',')})`;
      else marker.textContent = 'hidden';
      button.append(number, text, marker);
      button.addEventListener('click', () => selectPart(part.id));
      $('parts').append(button);
      if (focusedId === part.id) button.focus({ preventScroll: true });
    }
  }
}

function update() {
  const scroll = $('parts').scrollTop;
  for (const { group, part, label } of groups.values()) {
    group.position.fromArray(partPosition(part, state.amount));
    group.visible = partVisible(part, state.visibility, state.isolated, state.hidden);
    const selected = state.selected === part.id;
    label.dataset.selected = String(selected);
    group.traverse(object => {
      if (object.isMesh) {
        const xray = state.xray && housingIds.has(part.id) && !state.isolated;
        object.material.opacity = part.category === 'clearance' ? (part.role === 'cable_route' ? 0.82 : 0.17) : xray ? 0.18 : 1;
        object.material.transparent = object.material.opacity < 1;
        object.material.depthWrite = object.material.opacity === 1;
        object.material.emissive.set(selected ? 0x2c6d57 : 0x000000);
        object.material.emissiveIntensity = selected ? 0.35 : 0;
        object.castShadow = object.material.opacity === 1;
      }
      if (object.userData.isOutline) object.visible = selected || part.category === 'clearance';
    });
  }
  floor.position.z = state.amount > 0 ? -92 : -0.5;
  $('explode').value = Math.round(state.amount * 100);
  $('explode-value').value = `${Math.round(state.amount * 100)}%`;
  $('mode-label').textContent = state.isolated ? 'Isolated part' : state.amount === 0 ? 'Assembled' : state.amount === 1 ? 'Exploded' : 'Opening assembly';
  $('show-all').hidden = !state.isolated && state.hidden.size === 0;
  renderList();
  $('parts').scrollTop = scroll;
  dirty = true;
}

function visibleBounds(id) {
  const box = new THREE.Box3();
  for (const { group, part } of groups.values()) {
    if (!group.visible || (id && part.id !== id)) continue;
    if (part.category === 'clearance' && !id) continue;
    const local = new THREE.Box3(new THREE.Vector3().fromArray(part.bounds[0]), new THREE.Vector3().fromArray(part.bounds[1]));
    local.translate(group.position);
    box.union(local);
  }
  return box;
}

function fitVisible(id, direction) {
  const box = visibleBounds(id);
  if (box.isEmpty()) return;
  const center = box.getCenter(new THREE.Vector3());
  const size = box.getSize(new THREE.Vector3());
  const aspect = Math.max(camera.aspect, 0.3);
  const verticalFov = THREE.MathUtils.degToRad(camera.fov);
  const horizontalFov = 2 * Math.atan(Math.tan(verticalFov / 2) * aspect);
  const radius = size.length() / 2;
  const distance = Math.max(40, radius / Math.sin(Math.min(verticalFov, horizontalFov) / 2) * 1.1);
  const vector = direction ? new THREE.Vector3(...direction).normalize() : camera.position.clone().sub(controls.target).normalize();
  controls.target.copy(center);
  camera.position.copy(center).addScaledVector(vector, distance);
  camera.near = Math.max(0.1, distance / 1000);
  camera.far = Math.max(3000, distance * 6);
  camera.updateProjectionMatrix();
  controls.update();
  dirty = true;
}

function resetState() {
  state.amount = 0;
  state.selected = state.isolated = null;
  state.hidden.clear();
  state.visibility = { spool: true, electronics: true, clearance: false };
  state.xray = false;
  state.labels = true;
  for (const key of ['spool', 'electronics', 'clearance']) $(`show-${key}`).checked = state.visibility[key];
  $('xray').checked = false;
  $('show-labels').checked = true;
  update();
  setSelectedDetails(null);
  setCamera('iso');
}

function setCamera(name) {
  const directions = { iso: [1, -1.35, 0.8], front: [0, -1, 0.03], side: [1, 0, 0.03], top: [0, -0.0001, 1] };
  document.querySelectorAll('[data-view]').forEach(button => button.classList.toggle('active', button.dataset.view === name));
  fitVisible(null, directions[name]);
}

function layoutLabels() {
  const rect = viewport.getBoundingClientRect();
  const occupied = [];
  const visibleMeshes = [];
  for (const { group } of groups.values()) if (group.visible) group.traverse(object => { if (object.isMesh && object.material.opacity > 0.5) visibleMeshes.push(object); });
  const entries = [...groups.values()].sort((a, b) => (b.part.id === state.selected) - (a.part.id === state.selected));
  for (const { group, part, label } of entries) {
    label.hidden = true;
    if (!state.labels || !group.visible) continue;
    const world = new THREE.Vector3().fromArray(part.center).add(group.position);
    const projected = world.clone().project(camera);
    if (projected.z < -1 || projected.z > 1) continue;
    const x = (projected.x + 1) * rect.width / 2;
    const y = (1 - projected.y) * rect.height / 2;
    if (x < 16 || x > rect.width - 16 || y < 112 || y > rect.height - 16) continue;
    const vector = world.clone().sub(camera.position);
    raycaster.set(camera.position, vector.clone().normalize());
    const hit = raycaster.intersectObjects(visibleMeshes, false)[0];
    if (hit && hit.object.userData.part.id !== part.id && hit.distance < vector.length() - 1) continue;
    if (occupied.some(point => Math.abs(point.x - x) < 30 && Math.abs(point.y - y) < 27)) continue;
    occupied.push({ x, y });
    label.style.left = `${x}px`;
    label.style.top = `${y}px`;
    label.hidden = false;
  }
}

canvas.addEventListener('pointerdown', event => { lastPointer = { x: event.clientX, y: event.clientY }; });
canvas.addEventListener('pointerup', event => {
  if (!lastPointer || event.button !== 0 || Math.hypot(event.clientX - lastPointer.x, event.clientY - lastPointer.y) > 5) return;
  const rect = canvas.getBoundingClientRect();
  pointer.set((event.clientX - rect.left) / rect.width * 2 - 1, -(event.clientY - rect.top) / rect.height * 2 + 1);
  raycaster.setFromCamera(pointer, camera);
  const candidates = [];
  for (const { group, part } of groups.values()) {
    if (!group.visible || (state.xray && housingIds.has(part.id) && !state.isolated)) continue;
    group.traverse(object => { if (object.isMesh) candidates.push(object); });
  }
  const hit = raycaster.intersectObjects(candidates, false)[0];
  if (hit) selectPart(hit.object.userData.part.id);
});
$('variant').addEventListener('change', event => loadVariant(event.target.value));
$('explode').addEventListener('input', event => { state.amount = clampExplosion(event.target.value / 100); update(); fitVisible(); });
$('assemble').addEventListener('click', () => { state.amount = 0; update(); fitVisible(); });
$('explode-all').addEventListener('click', () => { state.amount = 1; update(); fitVisible(); });
$('reset').addEventListener('click', resetState);
$('fit').addEventListener('click', () => fitVisible());
document.querySelectorAll('[data-view]').forEach(button => button.addEventListener('click', () => setCamera(button.dataset.view)));
for (const key of ['spool', 'electronics', 'clearance']) $(`show-${key}`).addEventListener('change', event => { state.visibility[key] = event.target.checked; update(); });
$('show-labels').addEventListener('change', event => { state.labels = event.target.checked; dirty = true; });
$('xray').addEventListener('change', event => { state.xray = event.target.checked; update(); });
$('show-all').addEventListener('click', () => { state.isolated = null; state.hidden.clear(); update(); setSelectedDetails(groups.get(state.selected)?.part); fitVisible(); });
document.addEventListener('keydown', event => { if (event.key === 'Escape') { state.isolated = null; state.selected = null; update(); setSelectedDetails(null); } });
controls.addEventListener('change', () => { dirty = true; });
new ResizeObserver(() => {
  const width = viewport.clientWidth;
  const height = viewport.clientHeight;
  if (!width || !height) return;
  renderer.setSize(width, height, false);
  camera.aspect = width / height;
  camera.updateProjectionMatrix();
  if (groups.size) fitVisible();
  dirty = true;
}).observe(viewport);
function frame() {
  requestAnimationFrame(frame);
  controls.update();
  if (!dirty) return;
  scene.updateMatrixWorld(true);
  renderer.render(scene, camera);
  layoutLabels();
  dirty = false;
}
frame();
const requested = new URL(location.href).searchParams.get('model');
$('variant').value = requested === 'headless-led' ? 'headless-led' : 'oled-led';
loadVariant($('variant').value);
