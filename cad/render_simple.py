"""Publication previews for the six-piece cradle, using exported CAD meshes."""
from pathlib import Path
import hashlib
import json

import exploded_render as drawing


HERE = Path(__file__).resolve().parent
PRINTED = ('base', 'cradle', 'cell_spacer', 'nfc_tower', 'nfc_cassette', 'nfc_cover')
EVIDENCE = 'Actual CAD geometry · Physical fit, strength, wiring and calibration remain unverified'


def render_variant(previews, scene_data, parameters, validation, variant):
    previews = Path(previews)
    metadata_path = HERE / 'assembly-parts.json'
    metadata_bytes = metadata_path.read_bytes()
    metadata = json.loads(metadata_bytes)
    scene_bytes = (previews / 'scene.json').read_bytes()
    model = drawing.Scene(scene_data, metadata)
    printed = [part for part in model.parts if part['category'] == 'printed']
    if {part['id'] for part in printed} != set(PRINTED):
        raise ValueError('The simple cradle must contain its six named printed pieces.')
    hardware = [part for part in model.parts if part['category'] == 'electronics']
    main = printed + hardware
    nominal_routes = [part for part in model.parts if part.get('role') == 'cable_route'
                      and part.get('pose', 'nominal') == 'nominal']
    drawing.MOVING.update({'cradle', 'cell_spacer', 'nfc_tower', 'nfc_cassette', 'nfc_cover'})
    oled = bool(parameters.get('oled_enabled', variant == 'oled-led'))
    name = 'OLED + five LEDs' if oled else 'Five LEDs + web controls'
    revision = parameters.get('revision', 'v0.8 prototype')
    outputs = []

    def present(ids):
        return [model.by_id[key] for key in ids if key in model.by_id]

    def output(fig, name):
        target = previews / (name + '.png')
        drawing.save(fig, target)
        outputs.append(target)

    def notes(fig, entries):
        for y, (title, text) in zip([.76, .59, .42, .25], entries):
            fig.text(.75, y, title, fontsize=12, fontweight='bold')
            fig.text(.75, y - .034, text, fontsize=10, color=drawing.MUTED,
                     linespacing=1.6, va='top')

    for with_spool, filename in [(True, 'assembled'), (False, 'cradle')]:
        fig = drawing.figure('Spoolancer / ' + name, EVIDENCE)
        ax = drawing.axes(fig, [.01, .10, .72, .76])
        drawing_parts = main + (present(['spool']) if with_spool else [])
        model.draw(ax, drawing_parts, view=(350, -490, 320))
        notes(fig, [
            ('6 printed pieces', 'One rigid cradle with integral ramps.\nNo separate rails or sliding side guides.'),
            ('Upright spools', 'Fixed bay for spools 50–100 mm wide.\nPlace the reader-side flange at the tower.'),
            ('One reader adjustment', 'Move the reader vertically.\nRotate the seated spool to find its tag.'),
            ('Service from above', 'Remove the cradle to reach the boards.\nThe front controls stay on the base.'),
        ])
        drawing.footer(fig, f'{revision} · Six production parts · 12 V input requires a regulated 5 V converter.')
        output(fig, filename)

    fig = drawing.figure('Six-piece exploded assembly',
                         f'{name} · Numbers match the 3D viewer · Separation is illustrative, not assembly motion', (20, 15))
    drawing.legend(fig)
    ax = drawing.axes(fig, [.195, .095, .61, .73])
    positions = model.draw(ax, main, exploded=True, view=(420, -760, 550))
    drawing.callouts(fig, ax, main, positions, left=.023, right=.81, ymin=.14, ymax=.795,
                     subtitles={'cradle': 'One rigid part with integral ramps',
                                'cell_spacer': 'Between cradle and load-cell end',
                                'nfc_tower': 'Broad keyed glue joint',
                                'nfc_cassette': 'Vertical reader adjustment',
                                'nfc_cover': 'Removable flat rear panel',
                                'esp32': 'Approximate board envelope',
                                'pn532': 'Inside the sliding cassette',
                                'load_cell': 'Nominal 80 mm bar cell'})
    drawing.footer(fig, '6 printed pieces. Hardware groups are references, not prints. Spool and cable envelopes omitted. Physical fit and bond strength untested.')
    output(fig, 'exploded')

    fig = drawing.figure('Where each component lives',
                         f'{name} · Transparent printed parts reveal the assembled electronics', (20, 14))
    drawing.legend(fig)
    ax = drawing.axes(fig, [.185, .10, .63, .72])
    clearances = [part for part in model.parts if part['category'] == 'clearance'
                  and part.get('role') != 'cable_route']
    opacity = {part['id']: .12 for part in printed}
    opacity.update({part['id']: .15 for part in clearances})
    positions = model.draw(ax, main + clearances + nominal_routes, view=(350, -520, 540), opacity=opacity)
    focus = hardware + clearances + nominal_routes
    drawing.callouts(fig, ax, focus, positions, left=.025, right=.815, ymin=.16, ymax=.79)
    drawing.footer(fig, 'PCB dimensions and connector clearances are provisional. Converter, jack, adhesive and electrical terminations are not modeled.')
    output(fig, 'component-locations')

    fig = drawing.figure('Service through the top',
                         'Cradle, spacer and reader assembly omitted · Fixed components remain in their assembled positions')
    ax = drawing.axes(fig, [.01, .13, .72, .73])
    shown = present(['base', 'esp32', 'hx711', 'load_cell', 'stops', 'leds', 'oled',
                     'usb_space', 'boot_space', 'cell_fasteners_fixed'])
    model.draw(ax, shown, view=(-170, -330, 620), opacity={'base': .65, 'usb_space': .18, 'boot_space': .18})
    notes(fig, [
        ('Remove the spool first', 'Power off before opening.\nFree the reader cable before lifting.'),
        ('Two cradle screws', 'Remove the moving-end cell screws.\nSupport the loose spacer while lifting.'),
        ('Boards inside the base', 'The ESP32 and HX711 remain fixed.\nKeep wiring clear of the floating cradle.'),
        ('Owner-installed power', 'Glue the converter to a suitable wall.\nDrill the DC-jack hole after checking fit.'),
    ])
    drawing.footer(fig, 'Service view omits removed parts for visibility. It does not certify the removal path or actual connector access.')
    output(fig, 'service-interior')

    fig = drawing.figure('Vertical front controls',
                         'OLED and LED openings are part of the base · Actual module dimensions remain provisional')
    shown = present(['base', 'leds', 'oled'])
    ax = drawing.axes(fig, [.015, .17, .48, .60])
    model.draw(ax, shown, view=(80, -650, 190))
    ax2 = drawing.axes(fig, [.51, .17, .475, .60])
    model.draw(ax2, shown, view=(-100, 400, 390), opacity={'base': .22})
    fig.text(.055, .81, 'Front of the fixed base', fontsize=13, fontweight='bold')
    fig.text(.55, .81, 'Inside the fixed base', fontsize=13, fontweight='bold')
    drawing.footer(fig, 'Five LED centres at 16.67 mm pitch. Dry-fit your actual strip and OLED before bonding them into the enclosure.')
    output(fig, 'indicator-detail')

    fig = drawing.figure('Chamfered, rigid printed parts',
                         'Actual printable solids · Broad walls and joined ramps replace the separate thin frames')
    ax = drawing.axes(fig, [.02, .17, .46, .60])
    model.draw(ax, present(['base']), view=(360, -460, 310))
    ax2 = drawing.axes(fig, [.52, .17, .46, .60])
    model.draw(ax2, present(['cradle']), view=(360, -460, 310))
    fig.text(.055, .81, 'Low base enclosure', fontsize=13, fontweight='bold')
    fig.text(.55, .81, 'One-piece weighing cradle', fontsize=13, fontweight='bold')
    drawing.footer(fig, 'CAD appearance does not predict printed surface quality or strength. Use the exported print poses and inspect your slicer preview.')
    output(fig, 'edge-detail')

    fig = drawing.figure('Reader and hidden wiring',
                         'Rear cover removed · Nominal cable space is a dimensional reference, not a flexible-wire simulation')
    ax = drawing.axes(fig, [.015, .16, .58, .65])
    shown = present(['base', 'cradle', 'nfc_tower', 'nfc_cassette', 'pn532']) + nominal_routes
    model.draw(ax, shown, view=(680, -230, 290), opacity={'base': .12, 'cradle': .22, 'nfc_tower': .3, 'nfc_cassette': .45})
    for y, title, text in [
        (.74, '04  Fixed reader tower', 'The tower is glued to the moving cradle.\nIts rear panel conceals the cable cavity.'),
        (.53, '05  Sliding cassette', 'Two nylon screws clamp the reader height.\nKeep enough loose wire for its full travel.'),
        (.32, '06  Removable rear panel', 'Remove two screws for cable access.\nKeep the free cable loop clear of the base.'),
    ]:
        fig.text(.63, y, title, fontsize=12, fontweight='bold')
        fig.text(.63, y - .04, text, fontsize=10, color=drawing.MUTED, va='top', linespacing=1.6)
    drawing.footer(fig, 'Verify actual cable slack, bend radius and zero shift through the full reader-height travel. No cable may bridge the weighing clearance rigidly.')
    output(fig, 'cable-routing')

    print_path = previews / 'scene-print.json'
    if not print_path.exists():
        raise ValueError('Export scene-print.json with the six actual print-oriented meshes before rendering.')
    print_bytes = print_path.read_bytes()
    print_scene = json.loads(print_bytes)
    print_metadata = {**metadata, 'parts': printed}
    print_model = drawing.Scene(print_scene, print_metadata)
    fig = drawing.figure('Six parts / intended print orientations',
                         'Actual exported print poses · Each panel is independently scaled · Bed is Z = 0', (20, 14))
    orientations = validation.get('print_orientations', {})
    for index, part in enumerate(printed):
        column, row = index % 3, index // 3
        x, y = .025 + column * .325, .50 - row * .385
        ax = drawing.axes(fig, [x, y, .31, .29])
        print_model.draw(ax, [part], view=(340, -460, 410))
        fig.text(x + .015, y + .305, f"{part['number']}  {part['label']}", fontsize=13, fontweight='bold')
        description = orientations.get(part['id'], {})
        if isinstance(description, dict):
            description = description.get('bed_face', description.get('description', 'Use the exported STL orientation.'))
        fig.text(x + .015, y - .025, str(description), fontsize=9, color=drawing.MUTED)
    drawing.footer(fig, 'These views show geometry, not sliced toolpaths. Slicer support assessment and physical printing remain separate checks.')
    output(fig, 'print-orientations')

    if metadata_path.read_bytes() != metadata_bytes or (previews / 'scene.json').read_bytes() != scene_bytes:
        raise RuntimeError('Scene or part map changed during rendering; regenerate the previews.')
    if print_path.read_bytes() != print_bytes:
        raise RuntimeError('Print scene changed during rendering; regenerate the previews.')
    report = {'variant': variant, 'revision': revision, 'source': 'actual CAD tessellation',
              'scene_sha256': hashlib.sha256(scene_bytes).hexdigest(),
              'print_scene_sha256': hashlib.sha256(print_bytes).hexdigest(),
              'part_map_sha256': hashlib.sha256(metadata_bytes).hexdigest(),
              'renderer_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'drawing_helpers_sha256': hashlib.sha256(Path(drawing.__file__).read_bytes()).hexdigest(),
              'numbered_printed_instances': len(printed),
              'numbered_hardware_groups': len(hardware), 'physical_validation': False,
              'images': {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in outputs}}
    (previews / 'render-manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'variant': variant, 'images': [path.name for path in outputs], 'printed_instances': len(printed)}))
