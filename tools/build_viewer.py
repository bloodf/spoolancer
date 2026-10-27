#!/usr/bin/env python3
"""Build offline assembly-viewer assets from the actual exported CAD tessellations."""
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VARIANTS = ('headless-led', 'oled-led')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def build_variant(variant, metadata_bytes, scene_bytes):
    metadata = json.loads(metadata_bytes)
    scene = json.loads(scene_bytes)
    groups = metadata['parts']
    ids = [part['id'] for part in groups]
    if len(ids) != len(set(ids)):
        raise ValueError('Part IDs must be unique')
    expected = [mesh for part in groups for mesh in part['meshes']]
    if len(expected) != len(set(expected)):
        raise ValueError('A CAD mesh belongs to more than one part')
    unknown = set(scene) - set(expected)
    if unknown:
        raise ValueError(f'Unlabeled CAD meshes: {sorted(unknown)}')
    numbers = [part['number'] for part in groups]
    if len(numbers) != len(set(numbers)):
        raise ValueError('Part numbers must be unique')
    payload = bytearray()
    parts = []
    for group in groups:
        present = set(group['meshes']) & set(scene)
        if not present and group['id'] in metadata['optionalParts'] and variant == 'headless-led':
            continue
        if present != set(group['meshes']):
            raise ValueError(f"Missing meshes for {group['id']}: {set(group['meshes']) - present}")
        if len(group['explode']) != 3 or not all(math.isfinite(v) for v in group['explode']):
            raise ValueError('Explosion offset must be a finite 3-vector')
        part = {key: value for key, value in group.items() if key != 'meshes'}
        part['meshes'] = []
        vertices_for_bounds = []
        for mesh_id in group['meshes']:
            mesh = scene[mesh_id]
            vertices, faces = mesh['vertices'], mesh['faces']
            if not vertices or not faces:
                raise ValueError(f'Empty CAD mesh: {mesh_id}')
            if any(len(v) != 3 or not all(math.isfinite(n) for n in v) for v in vertices):
                raise ValueError(f'Invalid coordinates: {mesh_id}')
            if any(len(f) != 3 or any(not isinstance(i, int) or i < 0 or i >= len(vertices) for i in f) for f in faces):
                raise ValueError(f'Invalid triangles: {mesh_id}')
            position_offset = len(payload)
            payload.extend(struct.pack('<' + 'f' * (len(vertices) * 3), *(v for p in vertices for v in p)))
            index_offset = len(payload)
            payload.extend(struct.pack('<' + 'I' * (len(faces) * 3), *(i for face in faces for i in face)))
            part['meshes'].append(dict(id=mesh_id, color=mesh['color'], positionOffset=position_offset,
                                       vertexCount=len(vertices), indexOffset=index_offset, triangleCount=len(faces)))
            vertices_for_bounds.extend(vertices)
        minimum = [min(v[i] for v in vertices_for_bounds) for i in range(3)]
        maximum = [max(v[i] for v in vertices_for_bounds) for i in range(3)]
        part['bounds'] = [minimum, maximum]
        part['center'] = [(a + b) / 2 for a, b in zip(minimum, maximum)]
        parts.append(part)
    printed_count = sum(p['category'] == 'printed' for p in parts)
    cad_printed = {mesh_id for mesh_id in scene if not mesh_id.startswith('REF_')}
    labeled_printed = {mesh['id'] for part in parts if part['category'] == 'printed' for mesh in part['meshes']}
    if labeled_printed != cad_printed or printed_count != len(cad_printed):
        raise ValueError('Every CAD printed instance must have its own selectable printed part')
    data = dict(schemaVersion=1, variant=variant, units='mm', coordinates=metadata['coordinates'],
                source=f'cad/variants/{variant}/previews/scene.json',
                sourceSha256=digest(scene_bytes), metadataSha256=digest(metadata_bytes),
                meshFile=f'{variant}.bin', meshSha256=digest(payload), byteLength=len(payload),
                printedInstances=printed_count, parts=parts)
    return (json.dumps(data, ensure_ascii=False, separators=(',', ':')) + '\n').encode(), payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Fail when committed viewer data differs from CAD sources')
    args = parser.parse_args()
    metadata_bytes = (ROOT / 'cad/assembly-parts.json').read_bytes()
    output = ROOT / 'viewer/assets'
    if not args.check:
        output.mkdir(parents=True, exist_ok=True)
    for variant in VARIANTS:
        source = ROOT / 'cad/variants' / variant / 'previews/scene.json'
        description, mesh = build_variant(variant, metadata_bytes, source.read_bytes())
        for suffix, content in [('json', description), ('bin', mesh)]:
            path = output / f'{variant}.{suffix}'
            if args.check:
                if not path.exists() or path.read_bytes() != content:
                    raise SystemExit(f'Stale viewer asset: {path.relative_to(ROOT)}; run python3 tools/build_viewer.py')
            else:
                path.write_bytes(content)
        decoded = json.loads(description)
        triangles = sum(m['triangleCount'] for p in decoded['parts'] for m in p['meshes'])
        print(f'{variant}: {len(decoded["parts"])} labeled groups, {decoded["printedInstances"]} printed instances, {triangles:,} triangles, {len(mesh):,} bytes — {"verified" if args.check else "built"}')


if __name__ == '__main__':
    main()
