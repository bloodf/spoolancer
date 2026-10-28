"""Offline OrcaSlicer evidence for the simplified printed parts.

Keeps the exported STL orientations, enables automatic supports as a diagnostic,
and never connects to a printer. Bundled preset inheritance is resolved using
the same child-over-parent approach as compare_filament.py, without importing
that script's executable setup. Large slicer artifacts stay in ignored local/.
"""
import argparse
import hashlib
import json
import math
import re
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_EXE = Path('/Applications/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
DEFAULT_PROFILES = Path('/Applications/OrcaSlicer.app/Contents/Resources/profiles')
DEFAULT_OUTPUT = ROOT / 'analysis/printability-v0.8'
VARIANTS = ('headless-led', 'oled-led')
PRINTERS = {
    'p1s': ('BBL', 'Bambu Lab P1S 0.4 nozzle',
            '0.20mm Standard @BBL X1C', 'Generic PETG @BBL P1P'),
    'u1': ('Snapmaker', 'Snapmaker U1 (0.4 nozzle)',
           '0.20 Standard @Snapmaker U1 (0.4 nozzle)', 'Snapmaker PETG @U1'),
}
PROCESS_OVERRIDES = {
    'curr_bed_type': 'Textured PEI Plate',
    'layer_height': '0.2', 'initial_layer_print_height': '0.2',
    'wall_loops': '4', 'top_shell_layers': '5', 'bottom_shell_layers': '5',
    'sparse_infill_density': '20%', 'sparse_infill_pattern': 'gyroid',
    'enable_support': '1', 'support_type': 'normal(auto)',
    'support_style': 'default', 'support_threshold_angle': '30',
    'support_on_build_plate_only': '0', 'bridge_no_support': '0',
    'support_top_z_distance': '0.2', 'support_bottom_z_distance': '0.2',
    'support_interface_top_layers': '2', 'support_interface_bottom_layers': '2',
    'brim_type': 'no_brim', 'skirt_loops': '0', 'raft_layers': '0',
    'enable_arc_fitting': '0',
}
LIMITATIONS = [
    'Offline toolpaths and estimates only; no printer was contacted.',
    'Supports are enabled for diagnosis, with the bundled 30 degree threshold. '
    'Zero generated support is not proof of physical print quality or strength.',
    'No brim or raft is used, so support/brim cannot conceal a poor STL bed pose.',
    'STL bed contact is the area of coplanar triangles at the lowest Z, not an adhesion measurement.',
    'First-layer deposited-area estimate is positive model XY extrusion volume divided by 0.2 mm; '
    'it is not a polygon union and does not account for path overlap.',
    'Support volume sums positive XY extrusion assigned Support or Support interface by Orca. '
    'It excludes stationary unretraction and startup purge.',
    'Each part is sliced separately; summed times include repeated machine startup overhead. '
    'Actual multi-part plate arrangement may differ.',
    'The two printers use their bundled PETG presets, which differ in speed and density. '
    'Revision comparisons are valid within each printer profile, not as printer speed benchmarks.',
    'Warnings are those exported by the CLI and 3MF metadata. Their absence does not prove absence '
    'of every GUI diagnostic, floating feature, warping risk, or weak layer orientation.',
]


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + '\n')


def git_bytes(ref, path):
    return subprocess.check_output(['git', 'show', f'{ref}:{path}'], cwd=ROOT)


def resolve_presets(root, vendor, names):
    index = {}
    for path in (root / vendor).rglob('*.json'):
        data = json.loads(path.read_text())
        if data.get('name'):
            index[data['name']] = (path, data)
    provenance = {}

    def resolve(name, seen=()):
        if name in seen:
            raise ValueError(f'Preset inheritance cycle: {name}')
        path, data = index[name]
        provenance[str(path.relative_to(root))] = sha256(path.read_bytes())
        result = resolve(data['inherits'], seen + (name,)) if data.get('inherits') else {}
        result.update(data)
        result.pop('inherits', None)
        return result

    return [resolve(name) for name in names], provenance


def prepare_profiles(args, printer):
    vendor, machine_name, process_name, filament_name = PRINTERS[printer]
    (machine, process, filament), sources = resolve_presets(
        args.profiles, vendor, [machine_name, process_name, filament_name])
    process.update(PROCESS_OVERRIDES)
    process['compatible_printers'] = [machine_name]
    filament['compatible_printers'] = [machine_name]
    folder = args.output / 'local/profiles' / printer
    hashes = {}
    for name, data in [('machine', machine), ('process', process), ('filament', filament)]:
        path = folder / (name + '.json')
        write_json(path, data)
        hashes[name] = sha256(path.read_bytes())
    return folder, {
        'machine': machine_name, 'process_base': process_name, 'filament': filament_name,
        'process_overrides': PROCESS_OVERRIDES, 'resolved_sha256': hashes,
        'bundled_preset_sha256': sources,
        'filament_diameter_mm': float(filament['filament_diameter'][0]),
        'filament_density_g_cm3': float(filament['filament_density'][0]),
    }


def preserve_baseline(args, variant):
    commit = subprocess.check_output(
        ['git', 'rev-parse', f'{args.baseline_ref}^{{commit}}'], cwd=ROOT, text=True).strip()
    source = f'cad/variants/{variant}/exports'
    validation_bytes = git_bytes(commit, source + '/validation.json')
    validation = json.loads(validation_bytes)
    folder = args.output / 'local/baseline' / variant
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'validation.json').write_bytes(validation_bytes)
    parts = {}
    for name, part in validation['parts'].items():
        if name.startswith('fit_'):
            continue
        filename = part.get('files', {}).get('stl', name + '.stl')
        data = git_bytes(commit, source + '/' + filename)
        (folder / filename).write_bytes(data)
        parts[name] = {'sha256': sha256(data), 'quantity': part['quantity'],
                       'git_path': source + '/' + filename}
    manifest = {'requested_ref': args.baseline_ref, 'commit': commit, 'variant': variant,
                'validation_sha256': sha256(validation_bytes), 'parts': parts}
    write_json(args.output / f'baseline-{variant}.json', manifest)
    return folder, manifest


def stl_contact(path):
    raw = path.read_bytes()
    triangles = []
    count = struct.unpack_from('<I', raw, 80)[0] if len(raw) >= 84 else 0
    if 84 + 50 * count == len(raw):
        for offset in range(84, len(raw), 50):
            coords = struct.unpack_from('<9f', raw, offset + 12)
            triangles.append((coords[0:3], coords[3:6], coords[6:9]))
    else:
        vertices = [tuple(map(float, m)) for m in re.findall(
            r'vertex\s+([-+\deE.]+)\s+([-+\deE.]+)\s+([-+\deE.]+)', raw.decode())]
        if len(vertices) % 3:
            raise ValueError('Incomplete ASCII STL triangles')
        triangles = [vertices[i:i + 3] for i in range(0, len(vertices), 3)]
    if not triangles:
        raise ValueError('Empty STL')
    points = [v for triangle in triangles for v in triangle]
    low = [min(v[i] for v in points) for i in range(3)]
    high = [max(v[i] for v in points) for i in range(3)]
    contact = [t for t in triangles if all(abs(v[2] - low[2]) <= 1e-5 for v in t)]
    area = sum(abs((b[0] - a[0]) * (c[1] - a[1]) -
                   (b[1] - a[1]) * (c[0] - a[0])) / 2 for a, b, c in contact)
    cp = [v for t in contact for v in t]
    return {'stl_bounds_mm': [low, high],
            'stl_dimensions_mm': [round(high[i] - low[i], 5) for i in range(3)],
            'bed_contact_area_mm2': round(area, 3),
            'bed_contact_bounds_xy_mm': ([min(v[0] for v in cp), min(v[1] for v in cp),
                                          max(v[0] for v in cp), max(v[1] for v in cp)]
                                         if cp else None),
            'bed_contact_triangle_count': len(contact)}


def duration_seconds(value):
    return round(sum(float(n) * {'d': 86400, 'h': 3600, 'm': 60, 's': 1}[u]
                     for n, u in re.findall(r'([\d.]+)\s*([dhms])', value)))


def gcode_statistics(text, diameter):
    def total(unit):
        matches = re.findall(r'^; (?:total )?filament used \[' + re.escape(unit) +
                             r'\] = ([\d., ]+)', text, re.M)
        if not matches:
            raise ValueError(f'G-code is missing filament statistics: {unit}')
        return sum(float(v) for v in matches[-1].split(','))

    timing = re.search(r'^; model printing time: ([^;\n]+); total estimated time: ([^\n]+)', text, re.M)
    normal_timing = re.search(r'^; estimated printing time \(normal mode\) = ([^\n]+)', text, re.M)
    if not timing and not normal_timing:
        raise ValueError('G-code is missing estimated time')
    extrusion_area = math.pi * (diameter / 2) ** 2
    relative_e = False
    relative_xyz = False
    position = dict(X=0.0, Y=0.0, Z=0.0, E=0.0)
    feature = ''
    layer = 0
    width = 0.4
    support_e = 0.0
    first_e = 0.0
    bounds = [math.inf, math.inf, -math.inf, -math.inf]
    support_features = {}
    for line in text.splitlines():
        if line.startswith('; CHANGE_LAYER') or line.startswith(';LAYER_CHANGE'):
            layer += 1
        if line.startswith(('; FEATURE:', ';TYPE:')):
            feature = line.split(':', 1)[1].strip()
        if line.startswith(('; LINE_WIDTH:', ';WIDTH:')):
            width = float(line.split(':', 1)[1])
        code = line.split(';', 1)[0].strip()
        if not code:
            continue
        fields = code.split()
        cmd = fields[0]
        values = {k: float(v) for k, v in re.findall(r'([XYZE])([-+]?(?:\d*\.)?\d+)', code)}
        if cmd == 'M82':
            relative_e = False
        elif cmd == 'M83':
            relative_e = True
        elif cmd == 'G90':
            relative_xyz = False
        elif cmd == 'G91':
            relative_xyz = True
        elif cmd == 'G92':
            position.update(values)
        elif cmd in ('G0', 'G1', 'G2', 'G3'):
            end = position.copy()
            for axis in 'XYZ':
                if axis in values:
                    end[axis] = values[axis] + (position[axis] if relative_xyz else 0)
            delta_e = values.get('E', 0) if relative_e else values.get('E', position['E']) - position['E']
            if 'E' in values:
                end['E'] = position['E'] + delta_e
            moves_xy = abs(end['X'] - position['X']) + abs(end['Y'] - position['Y']) > 1e-7
            depositing = delta_e > 0 and moves_xy and layer > 0 and feature.lower() != 'custom'
            if depositing and cmd in ('G2', 'G3'):
                raise ValueError('Unexpected arc extrusion; diagnostic profile disables arc fitting')
            if depositing and 'support' in feature.lower():
                support_e += delta_e
                support_features[feature] = support_features.get(feature, 0) + delta_e
            elif depositing and layer == 1 and feature.lower() not in ('skirt', 'brim', 'wipe tower'):
                first_e += delta_e
                for p in (position, end):
                    bounds[0] = min(bounds[0], p['X'] - width / 2)
                    bounds[1] = min(bounds[1], p['Y'] - width / 2)
                    bounds[2] = max(bounds[2], p['X'] + width / 2)
                    bounds[3] = max(bounds[3], p['Y'] + width / 2)
            position = end
    return {
        'filament_g': round(total('g'), 3), 'filament_mm': round(total('mm'), 3),
        'model_time_s': duration_seconds(timing[1]) if timing else None,
        'total_estimated_time_s': duration_seconds(timing[2] if timing else normal_timing[1]),
        'support_filament_mm': round(support_e, 5),
        'support_extrusion_volume_mm3': round(support_e * extrusion_area, 5),
        'support_features_filament_mm': {k: round(v, 5) for k, v in support_features.items()},
        'first_layer_model_extrusion_volume_mm3': round(first_e * extrusion_area, 3),
        'first_layer_deposited_area_estimate_mm2': round(first_e * extrusion_area / 0.2, 3),
        'first_layer_toolpath_bounds_xy_mm': ([round(v, 4) for v in bounds] if first_e else None),
        'layer_count': layer,
    }


def read_project(path, diameter):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        gcodes = [n for n in names if n.endswith('.gcode')]
        if len(gcodes) != 1:
            raise ValueError(f'Expected one sliced plate, got {len(gcodes)}')
        result = gcode_statistics(archive.read(gcodes[0]).decode(errors='replace'), diameter)
        tree = ET.fromstring(archive.read('Metadata/slice_info.config'))
        result['metadata_warnings'] = [dict(node.attrib) for node in tree.findall('.//warning')]
        result['plate_metadata'] = {node.get('key'): node.get('value')
                                    for node in tree.findall('.//plate/metadata')}
        result['slicer_object_footprints'] = []
        for name in names:
            if re.fullmatch(r'Metadata/plate_\d+\.json', name):
                result['slicer_object_footprints'].extend(json.loads(archive.read(name)).get('bbox_objects', []))
        config = archive.read('Metadata/project_settings.config')
        result['exported_settings_sha256'] = sha256(config)
        settings = json.loads(config)
        result['verified_process_overrides'] = {}
        for key, expected in PROCESS_OVERRIDES.items():
            actual = settings.get(key)
            if actual != expected:
                raise ValueError(f'Slicer changed {key}: expected {expected!r}, got {actual!r}')
            result['verified_process_overrides'][key] = actual
        return result


def slice_part(args, printer, revision, variant, name, path, quantity, profile_folder, profile):
    folder = args.output / 'local/slices' / revision / variant / printer / name
    folder.mkdir(parents=True, exist_ok=True)
    report_path = args.output / 'parts' / revision / variant / printer / (name + '.json')
    fingerprint = {'stl_sha256': sha256(path.read_bytes()),
                   'script_sha256': sha256(Path(__file__).read_bytes()),
                   'profile_sha256': profile['resolved_sha256']}
    if report_path.exists() and not args.force:
        previous = json.loads(report_path.read_text())
        if previous.get('fingerprint') == fingerprint and previous.get('success'):
            print(f'{revision}/{variant}/{printer}/{name}: cached', flush=True)
            return previous
    result = {'name': name, 'quantity': quantity, 'fingerprint': fingerprint,
              'physical_validation': False, 'success': False, **stl_contact(path)}
    project = folder / 'diagnostic.3mf'
    settings_dir = args.output / 'local/isolated-settings' / printer
    settings_dir.mkdir(parents=True, exist_ok=True)
    command = [str(args.slicer), '--datadir', str(settings_dir),
               '--debug', '2', '--load-settings', str(profile_folder / 'machine.json') + ';' + str(profile_folder / 'process.json'),
               '--load-filaments', str(profile_folder / 'filament.json'),
               '--arrange', '1', '--orient', '0', '--ensure-on-bed', '--slice', '0',
               '--outputdir', str(folder), '--export-3mf', project.name, str(path)]
    try:
        completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, timeout=args.timeout)
        (folder / 'slicer.log').write_text(completed.stdout)
        result['exit_code'] = completed.returncode
        result['log_sha256'] = sha256(completed.stdout.encode())
        result['log_diagnostics'] = list(dict.fromkeys(line.strip() for line in completed.stdout.splitlines()
            if re.search(r'warning|error|floating|empty layer|conflict', line, re.I)))[:80]
        if completed.returncode:
            raise RuntimeError(f'Orca exited {completed.returncode}: {completed.stdout[-1200:]}')
        result.update(read_project(project, profile['filament_diameter_mm']))
        if sha256(path.read_bytes()) != fingerprint['stl_sha256']:
            raise RuntimeError('STL changed while slicing; rerun against frozen exports')
        result['project_sha256'] = sha256(project.read_bytes())
        result['success'] = True
    except (RuntimeError, ValueError, OSError, subprocess.TimeoutExpired, zipfile.BadZipFile) as error:
        result['error'] = str(error)
    write_json(report_path, result)
    print(f'{revision}/{variant}/{printer}/{name}: ' +
          (f"{result['filament_g']} g, support {result['support_extrusion_volume_mm3']} mm3, "
           f"bed contact {result['bed_contact_area_mm2']} mm2" if result['success'] else result['error']), flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant', choices=(*VARIANTS, 'both'), default='both')
    parser.add_argument('--printer', choices=(*PRINTERS, 'both'), default='both')
    parser.add_argument('--slicer', type=Path, default=DEFAULT_EXE)
    parser.add_argument('--profiles', type=Path, default=DEFAULT_PROFILES)
    parser.add_argument('--output', type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument('--baseline-ref', help='Local git tag or verified commit for the previous print set')
    parser.add_argument('--baseline-only', action='store_true')
    parser.add_argument('--prepare-only', action='store_true', help='Resolve settings and preserve baseline; do not slice')
    parser.add_argument('--only', help='Comma-separated part names for a targeted rerun')
    parser.add_argument('--force', action='store_true')
    parser.add_argument('--timeout', type=int, default=900)
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / '.gitignore').write_text('/local/\n')
    if args.baseline_only and not args.baseline_ref:
        parser.error('--baseline-only requires --baseline-ref')
    variants = VARIANTS if args.variant == 'both' else (args.variant,)
    printers = tuple(PRINTERS) if args.printer == 'both' else (args.printer,)
    profiles = {printer: prepare_profiles(args, printer) for printer in printers}
    baseline = {variant: preserve_baseline(args, variant) for variant in variants} if args.baseline_ref else {}
    if args.prepare_only:
        print('Prepared isolated settings and requested baseline. No slicing or printer connection.')
        return 0
    version = subprocess.check_output([str(args.slicer), '--help'], text=True).splitlines()[0]
    selected = set(args.only.split(',')) if args.only else None
    all_ok = True
    for variant in variants:
        revisions = [] if args.baseline_only else [('v0.8', ROOT / 'cad/variants' / variant / 'exports', None)]
        if variant in baseline:
            folder, manifest = baseline[variant]
            revisions.insert(0, ('v0.7', folder, manifest['commit']))
        for revision, folder, commit in revisions:
            validation = json.loads((folder / 'validation.json').read_text())
            if revision == 'v0.8' and not {'cradle', 'cell_spacer', 'nfc_tower', 'nfc_cassette', 'nfc_cover'} <= validation['parts'].keys():
                raise ValueError('Expected simplified v0.8 exports; old CAD is still present')
            for printer, (profile_folder, profile) in profiles.items():
                parts = {}
                for name, part in validation['parts'].items():
                    if name.startswith('fit_') or (selected and name not in selected):
                        continue
                    filename = part.get('files', {}).get('stl', name + '.stl')
                    parts[name] = slice_part(args, printer, revision, variant, name,
                        folder / filename, part['quantity'], profile_folder, profile)
                passed = bool(parts) and all(p['success'] for p in parts.values())
                all_ok &= passed
                summary = {'revision': revision, 'variant': variant, 'slicer': version,
                    'baseline_commit': commit, 'profile': profile,
                    'validation_sha256': sha256((folder / 'validation.json').read_bytes()),
                    'orientation_preserved': True, 'physical_validation': False,
                    'support_enabled': True, 'partial_part_selection': sorted(selected) if selected else None,
                    'all_slices_succeeded': passed, 'limitations': LIMITATIONS, 'parts': parts}
                if passed:
                    summary['totals_with_quantities'] = {key: (round(sum(
                        p[key] * p['quantity'] for p in parts.values()), 3)
                        if all(p[key] is not None for p in parts.values()) else None) for key in (
                        'filament_g', 'filament_mm', 'model_time_s', 'total_estimated_time_s',
                        'support_filament_mm', 'support_extrusion_volume_mm3')}
                    summary['printed_piece_count'] = sum(p['quantity'] for p in parts.values())
                suffix = '-' + '-'.join(sorted(selected)) if selected else ''
                write_json(args.output / f'{revision}-{variant}-{printer}{suffix}.json', summary)
    return 0 if all_ok else 1


if __name__ == '__main__':
    sys.exit(main())
