#!/usr/bin/env python3
"""Check that the current CAD files match their verified sources and images."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_PARTS = {'base', 'cradle', 'cell_spacer', 'nfc_tower', 'nfc_cassette', 'nfc_cover', 'base_top_cover', 'nfc_cable_gate'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


for variant in ('headless-led', 'oled-led'):
    folder = ROOT / 'cad/variants' / variant
    exports, previews = folder / 'exports', folder / 'previews'
    validation = json.loads((exports / 'validation.json').read_text())
    verification = json.loads((exports / 'export-verification.json').read_text())
    rendered = json.loads((previews / 'render-manifest.json').read_text())
    assert validation['physical_validation'] is verification['physical_validation'] is rendered['physical_validation'] is False
    assert validation['source_sha256'] == verification['source_sha256']
    assert validation['top_cover_checks'] == verification['top_cover_checks']
    assert validation['cable_gate_checks'] == verification['cable_gate_checks']
    for path, digest in validation['source_sha256'].items():
        assert sha(ROOT / 'cad' / path) == digest, ('stale geometry', path)
    assert set(validation['parts']) == EXPECTED_PARTS
    assert len(validation['parts']) == verification['freecad']['printed_instances'] == rendered['numbered_printed_instances']
    assert {p.stem for p in exports.glob('*.stl')} == set(validation['parts']), 'stale printable files'
    for name in validation['parts']:
        assert sha(exports / (name + '.stl')) == verification['stl_files'][name]['sha256']
        assert sha(exports / (name + '.step')) == verification['individual_step_files'][name]['sha256']
        assert validation['print_orientations'][name]['flat_bed_contact_mm2'] > 100, ('edge-down print', name)
    assert sha(previews / 'scene.json') == rendered['scene_sha256']
    assert sha(previews / 'scene-print.json') == rendered['print_scene_sha256']
    assert sha(ROOT / 'cad/assembly-parts.json') == rendered['part_map_sha256']
    assert sha(ROOT / 'cad/render_simple.py') == rendered['renderer_sha256']
    assert sha(ROOT / 'cad/exploded_render.py') == rendered['drawing_helpers_sha256']
    for path, digest in rendered['images'].items():
        assert sha(previews / path) == digest, ('stale render', path)
    for printer in ('p1s', 'u1'):
        sliced = json.loads((ROOT / 'analysis/printability-v0.8' / f'v0.8-{variant}-{printer}.json').read_text())
        assert sliced['validation_sha256'] == sha(exports / 'validation.json'), 'stale slice provenance'
        assert sliced['all_slices_succeeded'] and sliced['printed_piece_count'] == len(EXPECTED_PARTS)
        assert sliced['partial_part_selection'] is None
        assert set(sliced['parts']) == set(validation['parts'])
        for name, record in sliced['parts'].items():
            assert record['fingerprint']['stl_sha256'] == sha(exports / (name + '.stl')), ('stale toolpath', name)
            assert record['fingerprint']['script_sha256'] == sha(ROOT / 'tools/slice_simple.py')
    print(f'{variant}: {len(EXPECTED_PARTS)} verified STEP/STL pairs, broad print faces, source and image hashes match')
