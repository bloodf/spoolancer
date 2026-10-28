"""Generate the six-piece v0.8 prototype; all tests are geometric, not physical."""
import argparse
import hashlib
import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.append(os.environ.get('FREECAD_LIB', '/Applications/FreeCAD.app/Contents/Resources/lib'))
import FreeCAD as App
import MeshPart
import Part
from simple_geometry import build


def bounds(shape):
    b = shape.optimalBoundingBox(False, False)
    return [round(n, 6) for n in (b.XLength, b.YLength, b.ZLength)]


def overlap(a, b):
    return a.common(b).Volume if a.BoundBox.intersect(b.BoundBox) else 0


def scene_mesh(shape, color):
    vertices, faces = shape.tessellate(.15)
    return {'vertices': [[v.x, v.y, v.z] for v in vertices], 'faces': faces, 'color': color}


def generate(variant, output_root=None):
    source_target = HERE / 'variants' / variant
    target = Path(output_root) / variant if output_root else source_target
    out, previews = target / 'exports', target / 'previews'
    out.mkdir(parents=True, exist_ok=True)
    previews.mkdir(parents=True, exist_ok=True)
    p = json.loads((HERE / 'simple-parameters.json').read_text())
    p.update(json.loads((source_target / 'parameters.json').read_text()))
    sources = (HERE / 'build_simple.py', HERE / 'simple_geometry.py', HERE / 'simple-parameters.json', source_target / 'parameters.json')
    source_hashes = {str(path.relative_to(HERE)): hashlib.sha256(path.read_bytes()).hexdigest() for path in sources}
    model = build(App, Part, p, variant)
    parts, refs, info = model['parts'], model['refs'], model['info']
    checks = model['checks']
    assert len(parts) == 6
    assert all(s.isValid() and len(s.Solids) == 1 for s in parts.values())
    assert not checks['printed_part_intersections'], checks['printed_part_intersections']
    assert all(not c['positive_volume_intersections'] for c in checks['spool_cases']), checks['spool_cases']
    assert all(not c['positive_volume_intersections'] for c in checks['reader_positions']), checks['reader_positions']
    assert not checks['boot_access']['positive_volume_intersections'], checks['boot_access']
    component_hits = {}
    for name, shape in refs.items():
        hits = {other: round(overlap(shape, solid), 6) for other, solid in parts.items()
                if overlap(shape, solid) > .001}
        component_hits[name] = hits
    assert not any(component_hits.values()), {k: v for k, v in component_hits.items() if v}

    # A complete cylinder is stricter than the detailed reference spool's disks.
    spool_checks = []
    for diameter in p['spool_diameters']:
        for width in p['spool_widths']:
            import math
            z = info['v_apex_z_mm'] + diameter / 2 / math.cos(math.radians(p['ramp_angle_degrees']))
            envelope = Part.makeCylinder(diameter / 2, width,
                App.Vector(p['spool_right_face'] - width, 0, z), App.Vector(1, 0, 0))
            hits = {name: round(overlap(envelope, shape), 6) for name, shape in parts.items()
                    if overlap(envelope, shape) > .001}
            assert not hits, (diameter, width, hits)
            spool_checks.append({'diameter_mm': diameter, 'width_mm': width,
                'floor_gap_mm': round(z - diameter / 2 - info['cell_stack_mm']['cradle_top'], 6),
                'positive_volume_intersections': hits})

    # Raise the moving assembly through an open top, with its cable disconnected.
    service = []
    for dz in (1, 5, 15, 35, 60):
        hits = {}
        for name, shape in parts.items():
            if name == 'base':
                continue
            lifted = shape.copy()
            lifted.translate(App.Vector(0, 0, dz))
            for fixed_name in ('base', 'load_cell', 'esp32_pcb', 'esp32_components', 'hx711_pcb'):
                fixed = parts.get(fixed_name, refs.get(fixed_name))
                volume = overlap(lifted, fixed)
                if volume > .001:
                    hits[name + '/' + fixed_name] = round(volume, 6)
        assert not hits, (dz, hits)
        service.append({'lift_mm': dz, 'positive_volume_intersections': hits})

    report = {'revision': p['revision'], 'units': 'mm', 'variant': variant,
        'assembly_stem': info['stem'], 'physical_validation': False, 'parts': {},
        'geometry_checks': {**info, 'assembled_printed_bounds_mm': bounds(Part.makeCompound(list(parts.values())))},
        'fastener_schedule': info['fastener_schedule'], 'print_orientations': {},
        'printed_part_intersections': checks['printed_part_intersections'],
        'component_intersections': component_hits, 'spool_envelope_checks': spool_checks,
        'pod_adjustment_checks': checks['reader_positions'], 'service_lift_checks': service,
        'boot_access': checks['boot_access'], 'source_sha256': source_hashes}
    doc = App.newDocument('Spoolancer_v0_8_' + variant.replace('-', '_'))
    sheet = doc.addObject('Spreadsheet::Sheet', 'Dimensions')
    sheet.set('A1', 'Prototype dimensions in mm. Edit JSON/generator and regenerate; spreadsheet is a record.')
    for row, (key, value) in enumerate(p.items(), 3):
        sheet.set('A' + str(row), key)
        sheet.set('B' + str(row), json.dumps(value))
    group = doc.addObject('App::DocumentObjectGroup', 'PrintedParts')
    ref_group = doc.addObject('App::DocumentObjectGroup', 'ReferenceEnvelopes')
    scene, print_scene = {}, {}
    for name, shape in {**parts, **refs}.items():
        key = name if name in parts or name.startswith('REF_') else 'REF_' + name
        obj = doc.addObject('Part::Feature', key)
        obj.Shape = shape
        obj.Label = name.replace('_', ' ')
        obj.addProperty('App::PropertyString', 'Evidence', 'Design').Evidence = 'Geometric prototype; actual fit and strength unverified'
        color = model['colors'][name]
        if getattr(obj, 'ViewObject', None):
            obj.ViewObject.ShapeColor = color
        (group if name in parts else ref_group).addObject(obj)
        scene[key] = scene_mesh(shape, color)
    doc.recompute()
    doc.saveAs(str(out / (info['stem'] + '.FCStd')))
    Part.export(group.Group + ref_group.Group, str(out / (info['stem'] + '-assembly.step')))
    for name, shape in parts.items():
        oriented = shape.copy()
        pose = model['print_poses'][name]
        for axis, angle in zip(((1, 0, 0), (0, 1, 0), (0, 0, 1)), pose['rotation']):
            if angle:
                oriented.rotate(App.Vector(), App.Vector(*axis), angle)
        b = oriented.optimalBoundingBox(False, False)
        oriented.translate(App.Vector(-(b.XMin+b.XMax)/2, -(b.YMin+b.YMax)/2, -b.ZMin))
        oriented.exportStep(str(out / (name + '.step')))
        mesh = MeshPart.meshFromShape(Shape=oriented, LinearDeflection=.08, AngularDeflection=.15, Relative=False)
        matrix = App.Matrix()
        matrix.move(App.Vector(0, 0, -mesh.BoundBox.ZMin))
        mesh.transform(matrix)
        assert mesh.isSolid() and not mesh.hasSelfIntersections(), name
        mesh.write(str(out / (name + '.stl')))
        b = mesh.BoundBox
        contact = sum(face.Area for face in oriented.Faces
                      if abs(face.BoundBox.ZMax) < .0001 and abs(face.BoundBox.ZMin) < .0001)
        assert contact > 100, (name, 'insufficient flat bed contact', contact)
        report['parts'][name] = {'solid_valid': True, 'solid_count': 1, 'closed_mesh': True,
            'mesh_self_intersections': False, 'triangle_count': mesh.CountFacets,
            'print_z_min_mm': round(b.ZMin, 6), 'volume_mm3': round(shape.Volume, 6),
            'print_bounds_mm': [round(n, 6) for n in (b.XLength, b.YLength, b.ZLength)],
            'step_bounds_mm': bounds(oriented), 'step_z_min_mm': 0,
            'files': {'stl': name + '.stl', 'step': name + '.step'}, 'quantity': 1}
        report['print_orientations'][name] = {'rotation_degrees_xyz': pose['rotation'],
            'bed_face': pose['bed_face'], 'flat_bed_contact_mm2': round(contact, 3),
            'support_intent': pose['support_intent']}
        print_scene[name] = scene_mesh(oriented, model['colors'][name])
    for path in sources:
        assert hashlib.sha256(path.read_bytes()).hexdigest() == source_hashes[str(path.relative_to(HERE))], ('Source changed during export', path)
    for filename, content in (('scene.json', scene), ('scene-print.json', print_scene)):
        (previews / filename).write_text(json.dumps(content, separators=(',', ':')))
    (out / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')
    App.closeDocument(doc.Name)
    print(json.dumps({'variant': variant, 'parts': len(parts), 'spool_cases': len(spool_checks),
                      'physical_validation': False, 'output': str(target)}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--variant', required=True, choices=['headless-led', 'oled-led'])
    parser.add_argument('--output-root')
    args = parser.parse_args()
    generate(args.variant, args.output_root)
