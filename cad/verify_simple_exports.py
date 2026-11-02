"""Independently reopen the printable STEP/STL files and native assembly."""
import argparse
import hashlib
import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.append(os.environ.get('FREECAD_LIB', '/Applications/FreeCAD.app/Contents/Resources/lib'))
import FreeCAD as App
import Mesh
import Part


def verify(variant, output_root=None):
    target = Path(output_root) / variant if output_root else HERE / 'variants' / variant
    out = target / 'exports'
    v = json.loads((out / 'validation.json').read_text())
    assert v['revision'].startswith('v0.8') and v['physical_validation'] is False
    expected_parts = {'base', 'cradle', 'cell_spacer', 'nfc_tower', 'nfc_cassette', 'nfc_cover', 'base_top_cover', 'nfc_cable_gate'}
    assert set(v['parts']) == expected_parts
    assert not v['printed_part_intersections'] and not any(v['component_intersections'].values())
    assert not v['boot_access']['positive_volume_intersections']
    cover = v['top_cover_checks']
    assert cover['stationary'] and cover['physical_validation'] is False
    assert not cover['access_probe_intersections']
    assert cover['downward_travel_checks'] and all(
        row['cradle_gap_mm'] > 0 and not row['positive_volume_intersections']
        for row in cover['downward_travel_checks'])
    assert cover['spacer_minimum_side_clearance_mm'] > 0
    assert cover['cable_route_minimum_clearance_mm'] > 0
    assert cover['glue_rim_contact_area_mm2'] > 0
    entry = cover['side_entry']
    assert entry['connected_cable_end_threading_required'] is False
    assert entry['swept_cable_cover_intersection_mm3'] < .001
    assert entry['retrofit_path_checks'] and all(
        not row['positive_volume_intersections'] for row in entry['retrofit_path_checks'])
    gate = v['cable_gate_checks']
    assert gate['stationary'] and gate['physical_validation'] is False
    assert gate['glue_contact_area_mm2'] > 0 and gate['representative_cable_clearance_mm'] > 0
    assert gate['cradle_clearance_at_1mm_downward_travel_mm'] > 0
    assert gate['insertion_checks'] and all(
        not row['positive_volume_intersections'] for row in gate['insertion_checks'])
    for key in ('spool_envelope_checks', 'pod_adjustment_checks', 'service_lift_checks'):
        assert v[key] and not any(row['positive_volume_intersections'] for row in v[key])
    for relative, digest in v['source_sha256'].items():
        assert hashlib.sha256((HERE / relative).read_bytes()).hexdigest() == digest, ('stale CAD source', relative)
    r = {'revision': v['revision'], 'physical_validation': False,
         'freecad_version': '.'.join(App.Version()[:3]), 'stl_files': {}, 'individual_step_files': {},
         'source_sha256': v['source_sha256'], 'fastener_schedule': v['fastener_schedule'],
         'top_cover_checks': cover, 'cable_gate_checks': gate}
    for name, original in v['parts'].items():
        stl, step = out / (name + '.stl'), out / (name + '.step')
        mesh, shape = Mesh.Mesh(str(stl)), Part.read(str(step))
        assert mesh.isSolid() and not mesh.hasSelfIntersections(), name
        assert shape.isValid() and len(shape.Solids) == 1, name
        mb, sb = mesh.BoundBox, shape.optimalBoundingBox(False, False)
        mesh_bounds = [mb.XLength, mb.YLength, mb.ZLength]
        step_bounds = [sb.XLength, sb.YLength, sb.ZLength]
        assert abs(mb.ZMin) < .001 and abs(sb.ZMin) < .001, name
        assert max(abs(x-y) for x, y in zip(step_bounds, original['step_bounds_mm'])) < .001, name
        assert max(abs(x-y) for x, y in zip(step_bounds, mesh_bounds)) < .2, name
        assert abs(shape.Volume-original['volume_mm3']) < .02, name
        relative = abs(shape.Volume-abs(mesh.Volume))/shape.Volume
        assert relative < .005, (name, relative)
        r['stl_files'][name] = {'reopened_closed_mesh': True, 'self_intersections': False,
            'bed_z_mm': round(mb.ZMin, 6), 'print_bounds_mm': [round(n, 6) for n in mesh_bounds],
            'sha256': hashlib.sha256(stl.read_bytes()).hexdigest()}
        r['individual_step_files'][name] = {'reopened_valid_single_solid': True,
            'bed_z_mm': round(sb.ZMin, 6), 'volume_mm3': round(shape.Volume, 6),
            'stl_volume_difference_percent': round(relative*100, 5),
            'sha256': hashlib.sha256(step.read_bytes()).hexdigest()}
    assert {p.stem for p in out.glob('*.stl')} == set(v['parts']), 'Unlisted STL'
    assert {p.stem for p in out.glob('*.step') if not p.stem.endswith('-assembly')} == set(v['parts']), 'Unlisted STEP'
    doc = App.openDocument(str(out / (v['assembly_stem'] + '.FCStd')))
    printed = doc.getObject('PrintedParts').Group
    references = doc.getObject('ReferenceEnvelopes').Group
    assert {o.Name for o in printed} == expected_parts
    assert all(o.Shape.isValid() for o in printed + references)
    assembled = Part.read(str(out / (v['assembly_stem'] + '-assembly.step')))
    assert assembled.isValid()
    r['freecad'] = {'printed_instances': len(printed), 'reference_groups': len(references), 'reopened_valid': True}
    r['assembly_step'] = {'reopened_valid': True, 'solid_count': len(assembled.Solids)}
    r['checks'] = {'spool_cases': len(v['spool_envelope_checks']),
                   'reader_positions': len(v['pod_adjustment_checks']), 'service_lifts': len(v['service_lift_checks'])}
    App.closeDocument(doc.Name)
    (out / 'export-verification.json').write_text(json.dumps(r, indent=2) + '\n')
    print(json.dumps({'variant': variant, 'reopened_pairs': len(expected_parts), 'native_assembly': 'valid', 'physical_validation': False}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--variant', required=True, choices=['headless-led', 'oled-led'])
    parser.add_argument('--output-root')
    args = parser.parse_args()
    verify(args.variant, args.output_root)
