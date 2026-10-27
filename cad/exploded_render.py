"""Publication previews from the same tessellation and part map as the 3D viewer."""
from pathlib import Path
import hashlib
import json
from itertools import product

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.patches import Circle
import numpy as np
import vtk
from vtk.util.numpy_support import numpy_to_vtk, vtk_to_numpy

HERE = Path(__file__).resolve().parent
BG = '#f5f4ef'
INK = '#193735'
MUTED = '#64746f'
COLORS = {'fixed': '#263f47', 'moving': '#35847a', 'hardware': '#a66e29', 'clearance': '#8c69a8'}
MOVING = {'carrier', 'rim_rail', 'rim_rail_front', 'side_guide', 'nfc_guide', 'nfc_pod', 'nfc_pod_cover'}
plt.rcParams.update({'font.family': 'DejaVu Sans', 'text.color': INK})


def tone(part):
    if part['category'] == 'clearance':
        return COLORS['clearance']
    if part['category'] != 'printed':
        return COLORS['hardware']
    return COLORS['moving'] if part['id'] in MOVING or part['number'] == '29' else COLORS['fixed']


class Scene:
    def __init__(self, data, metadata):
        self.data = data
        self.parts = [p for p in metadata['parts'] if any(m in data for m in p['meshes'])]
        self.by_id = {p['id']: p for p in self.parts}
        mapped = [m for p in self.parts for m in p['meshes']]
        if len(mapped) != len(set(mapped)) or set(mapped) != set(data):
            raise ValueError('Part map must cover every scene mesh exactly once; rebuild matching CAD and metadata.')
        self.polydata = {}
        for name, mesh in data.items():
            vertices = np.asarray(mesh['vertices'], dtype=float)
            points = vtk.vtkPoints()
            points.SetData(numpy_to_vtk(vertices, deep=True))
            faces = np.asarray(mesh['faces'], dtype=np.int64)
            cells = vtk.vtkCellArray()
            offsets = np.arange(0, 3 * len(faces) + 1, 3, dtype=np.int64)
            cells.SetData(numpy_to_vtk(offsets, deep=True, array_type=vtk.VTK_ID_TYPE),
                          numpy_to_vtk(faces.ravel(), deep=True, array_type=vtk.VTK_ID_TYPE))
            poly = vtk.vtkPolyData()
            poly.SetPoints(points)
            poly.SetPolys(cells)
            normals = vtk.vtkPolyDataNormals()
            normals.SetInputData(poly)
            normals.ConsistencyOn()
            normals.AutoOrientNormalsOn()
            normals.SplittingOn()
            normals.SetFeatureAngle(38)
            normals.Update()
            self.polydata[name] = normals.GetOutput()

    def part_bounds(self, part, exploded=False):
        points = np.concatenate([np.asarray(self.data[m]['vertices'], dtype=float)
                                 for m in part['meshes'] if m in self.data])
        if exploded:
            points += np.asarray(part['explode'])
        return points.min(axis=0), points.max(axis=0)

    def draw(self, ax, parts, *, exploded=False, view=(350, -470, 320), opacity=None,
             center=None, scale=None, anchors=None, mesh_filter=None, shifts=None):
        """Render actual scene meshes and return projected callout anchors."""
        opacity = opacity or {}
        shifts = shifts or {}
        def part_shift(part):
            return np.asarray(part['explode'] if exploded else shifts.get(part['id'], (0, 0, 0)))
        renderer = vtk.vtkRenderer()
        renderer.SetBackground(245 / 255, 244 / 255, 239 / 255)
        renderer.SetUseDepthPeeling(True)
        renderer.SetMaximumNumberOfPeels(80)
        renderer.SetOcclusionRatio(0.1)
        bounds = []
        for part in parts:
            alpha = opacity.get(part['id'], 1)
            shift = part_shift(part)
            for name in part['meshes']:
                if name not in self.data or (mesh_filter and not mesh_filter(name)):
                    continue
                mapper = vtk.vtkPolyDataMapper()
                mapper.SetInputData(self.polydata[name])
                actor = vtk.vtkActor()
                actor.SetMapper(mapper)
                actor.SetPosition(*shift)
                prop = actor.GetProperty()
                prop.SetColor(*self.data[name]['color'])
                prop.SetOpacity(alpha)
                prop.SetAmbient(.22)
                prop.SetDiffuse(.78)
                prop.SetSpecular(.24)
                prop.SetSpecularPower(45)
                renderer.AddActor(actor)
                points = np.asarray(self.data[name]['vertices']) + shift
                lo, hi = points.min(axis=0), points.max(axis=0)
                bounds.extend(product(*zip(lo, hi)))
        bounds = np.asarray(bounds)
        lo, hi = bounds.min(axis=0), bounds.max(axis=0)
        center = np.asarray(center if center is not None else (lo + hi) / 2)
        direction = np.asarray(view, dtype=float)
        direction /= np.linalg.norm(direction)
        right = np.cross([0, 0, 1], direction)
        right /= np.linalg.norm(right)
        up = np.cross(direction, right)
        width = int(ax.get_position().width * ax.figure.get_figwidth() * 180)
        height = int(ax.get_position().height * ax.figure.get_figheight() * 180)
        projected = np.asarray([(b - center) for b in bounds])
        if scale is None:
            scale = max(np.max(np.abs(projected @ up)),
                        np.max(np.abs(projected @ right)) * height / width) * 1.13
        camera = renderer.GetActiveCamera()
        camera.ParallelProjectionOn()
        camera.SetFocalPoint(*center)
        camera.SetPosition(*(center + direction * max(1000, np.linalg.norm(hi - lo) * 3)))
        camera.SetViewUp(0, 0, 1)
        camera.SetParallelScale(float(scale))
        renderer.AutomaticLightCreationOff()
        for pos, intensity in [((-300, -450, 650), .8), ((450, -50, 250), .42), ((100, 450, 500), .38)]:
            light = vtk.vtkLight()
            light.SetLightTypeToSceneLight()
            light.SetPosition(*(center + np.asarray(pos)))
            light.SetFocalPoint(*center)
            light.SetIntensity(intensity)
            renderer.AddLight(light)
        renderer.ResetCameraClippingRange()
        window = vtk.vtkRenderWindow()
        window.SetOffScreenRendering(1)
        window.SetAlphaBitPlanes(1)
        window.SetMultiSamples(0)
        window.SetSize(width, height)
        window.AddRenderer(renderer)
        window.Render()
        capture = vtk.vtkWindowToImageFilter()
        capture.SetInput(window)
        capture.ReadFrontBufferOff()
        capture.Update()
        output = capture.GetOutput()
        pixels = vtk_to_numpy(output.GetPointData().GetScalars()).reshape(height, width, -1)[::-1]
        positions = {}
        for part in parts:
            if anchors and part['id'] in anchors:
                point = np.asarray(anchors[part['id']], dtype=float)
                point += part_shift(part)
            else:
                # Snap leaders to a real triangle, not the empty centre of a frame.
                name = next(m for m in part['meshes'] if m in self.data)
                mesh = self.data[name]
                vertices = np.asarray(mesh['vertices'], dtype=float)
                triangles = vertices[np.asarray(mesh['faces'], dtype=int)].mean(axis=1)
                a, b = vertices.min(axis=0), vertices.max(axis=0)
                hints = {'base': (60, -84, 20), 'top_cover': (0, -80, 36.5),
                         'nfc_guide': (50, -30, 165), 'side_guide': (-50, -20, 97)}
                hint = np.asarray(hints.get(part['id'], (a + b) / 2))
                point = triangles[np.argmin(np.sum((triangles - hint) ** 2, axis=1))]
                point += part_shift(part)
            renderer.SetWorldPoint(*point, 1)
            renderer.WorldToDisplay()
            x, y, _ = renderer.GetDisplayPoint()
            positions[part['id']] = (x / width, y / height)
        ax.imshow(pixels, extent=(0, 1, 0, 1), aspect='auto')
        ax.set_xlim(0, 1)
        ax.set_ylim(0, 1)
        ax.set_axis_off()
        window.Finalize()
        return positions


def figure(title, subtitle, size=(16, 11)):
    fig = plt.figure(figsize=size, facecolor=BG)
    fig.text(.04, .946, title, fontsize=27, fontweight='bold')
    fig.text(.04, .908, subtitle, fontsize=11, color=MUTED)
    return fig


def footer(fig, text):
    fig.text(.04, .035, text, fontsize=9, color=MUTED)


def axes(fig, rect):
    ax = fig.add_axes(rect)
    ax.set_axis_off()
    return ax


def legend(fig, y=.865):
    fig.text(.04, y + .024, 'CALLOUT COLOURS', fontsize=8, color=MUTED, fontweight='bold')
    entries = [('fixed', 'Fixed printed parts'), ('moving', 'Moving weighing assembly'),
               ('hardware', 'Hardware references')]
    for x, (key, label) in zip([.04, .29, .60], entries):
        fig.add_artist(Line2D([x, x + .015], [y, y], transform=fig.transFigure,
                             color=COLORS[key], lw=5, solid_capstyle='round'))
        fig.text(x + .023, y - .006, label, fontsize=10, color=MUTED)


def callouts(fig, ax, parts, positions, *, left=.035, right=.82, ymin=.14, ymax=.81,
             subtitles=None):
    """Pack side labels without overlap; leader endpoints use real CAD coordinates."""
    subtitles = subtitles or {}
    projected = []
    for p in parts:
        x, y = positions[p['id']]
        px, py = fig.transFigure.inverted().transform(ax.transAxes.transform((x, y)))
        projected.append((p, px, py))
    ranked = sorted(projected, key=lambda item: item[1])
    half = (len(ranked) + 1) // 2
    for side, group in [('left', ranked[:half]), ('right', ranked[half:])]:
        group.sort(key=lambda item: item[2], reverse=True)
        if not group:
            continue
        ys = np.linspace(ymax, ymin, len(group)) if len(group) > 1 else [(ymax + ymin) / 2]
        for (part, px, py), y in zip(group, ys):
            label_x = left if side == 'left' else right
            end_x = left + .165 if side == 'left' else right - .014
            elbow_x = end_x + .013 if side == 'left' else end_x - .013
            color = tone(part)
            fig.add_artist(Line2D([px, elbow_x, end_x], [py, y, y],
                                 transform=fig.transFigure, color=color, alpha=.55, lw=.85))
            fig.add_artist(Circle((px, py), radius=.0022, transform=fig.transFigure,
                                  facecolor=color, edgecolor=BG, lw=.6))
            fig.text(label_x, y + .007, part['number'], color=color, fontsize=12, fontweight='bold')
            label = part['label'].replace('Five addressable LEDs', '5 addressable LEDs')
            fig.text(label_x + .026, y + .007, label, fontsize=10.3, fontweight='bold')
            desc = subtitles.get(part['id'])
            if desc:
                fig.text(label_x + .026, y - .011, desc, fontsize=8.3, color=MUTED, va='top')


def save(fig, path):
    fig.canvas.draw()
    renderer = fig.canvas.get_renderer()
    canvas = fig.bbox
    for label in fig.texts:
        box = label.get_window_extent(renderer)
        if box.x0 < 0 or box.y0 < 0 or box.x1 > canvas.width or box.y1 > canvas.height:
            raise ValueError(f'Clipped label in {path.name}: {label.get_text()}')
    fig.savefig(path, dpi=180, facecolor=BG)
    plt.close(fig)


def render_variant(previews, scene_data, parameters, validation, variant):
    metadata_path = HERE / 'assembly-parts.json'
    metadata_bytes = metadata_path.read_bytes()
    metadata = json.loads(metadata_bytes)
    scene_digest = hashlib.sha256((Path(previews) / 'scene.json').read_bytes()).hexdigest()
    model = Scene(scene_data, metadata)
    previews = Path(previews)
    printed = [p for p in model.parts if p['category'] == 'printed']
    hardware = [p for p in model.parts if p['category'] in ('electronics', 'hardware')]
    unique_printed_files = sum(not name.startswith('fit_') for name in validation['parts'])
    main = printed + hardware
    routes = [p for p in model.parts if p.get('role') == 'cable_route']
    nominal_routes = [p for p in routes if p.get('pose', 'nominal') == 'nominal']
    base_routes = [p for p in nominal_routes if 'REF_NFC_base_flex_nominal' in p['meshes']]
    routing_cover = next((p for p in printed if p['number'] == '29'), None)
    integrated_front = 'indicator_fascia' not in model.by_id
    simplified_fasteners = 'nfc_fasteners' in model.by_id
    fastener_schedule = validation.get('fastener_schedule')
    if simplified_fasteners and not fastener_schedule:
        raise ValueError('The simplified assembly preview requires its final fastener schedule.')
    finishing = validation.get('edge_finishing', {}).get('operations', {})
    chamfer_count = sum(op['operation'] == 'chamfer' for op in finishing.values())
    if integrated_front and (not finishing or chamfer_count != len(finishing)):
        raise ValueError('The integrated-front preview requires the final chamfer-only geometry report.')
    def finish_caption(key, label):
        op = finishing.get(key)
        return f"{label}: {op['radius_or_chamfer_mm']:g} mm {op['operation']}" if op else label + ': see geometry report'
    oled = bool(parameters['oled_enabled'])
    name = 'OLED + five LEDs' if oled else 'Five LEDs + web controls'
    revision = parameters['revision']
    bounds = validation['geometry_checks']['assembled_printed_bounds_mm']
    evidence = 'Actual CAD geometry · Prototype fit, wiring and calibration remain unverified'
    outputs = []
    def output(fig, filename):
        target = previews / (filename + '.png')
        save(fig, target)
        outputs.append(target)
    for spool, filename in [(True, 'assembled'), (False, 'cradle')]:
        fig = figure('Spoolancer / ' + name, evidence)
        ax = axes(fig, [.01, .10, .72, .76])
        shown = main + ([model.by_id['spool']] if spool else [])
        model.draw(ax, shown, view=(350, -470, 290),
                   opacity={'load_cell': 1, 'esp32': 1, 'hx711': 1})
        notes = [
            ('Upright, adjustable cradle', 'Spools up to 100 mm wide.\nThe reader stays beside the flange.'),
            ('Chamfered exterior edges', 'Exterior bevels continue across\nthe housing, reader and guides.'),
            (f'{len(printed)} printed pieces', 'Keyed rails and a hooked main lid.\nReader board captured at its edges.' if simplified_fasteners else ('Two identical cradle rails.\nControls integrated into the front wall.' if integrated_front else 'Two identical cradle rails.\nRemovable lid, reader and indicator pod.')),
            ('DC-powered enclosure', 'Closed USB wall and BOOT access.\nDrill your jack hole after checking fit.'),
        ]
        for y, (title, desc) in zip([.76, .59, .42, .25], notes):
            fig.text(.75, y, title, fontsize=12, fontweight='bold')
            fig.text(.75, y - .034, desc, fontsize=10, color=MUTED, linespacing=1.6, va='top')
        footer(fig, f'{bounds[0]:.0f} × {bounds[1]:.2f} mm printed footprint  /  {revision}  /  12 V input requires a regulated 5 V converter.')
        output(fig, filename)

    fig = figure('Exploded assembly', f'{name} · Numbers match the interactive 3D viewer · Illustrative separation, not assembly motion', (20, 15))
    legend(fig)
    ax = axes(fig, [.195, .095, .61, .73])
    positions = model.draw(ax, main, exploded=True, view=(420, -760, 550))
    callouts(fig, ax, main, positions, left=.023, right=.81, ymin=.14, ymax=.795,
             subtitles={'carrier': 'Attached to the load cell', 'rim_rail': 'Same print as 05 · keyed seat' if simplified_fasteners else 'Same print as 05',
                        'rim_rail_front': 'Same print as 04 · keyed seat' if simplified_fasteners else 'Same print as 04',
                        'top_cover': 'Two rear hooks; two front screws' if simplified_fasteners else '',
                        'pn532': 'Captured at PCB edges' if simplified_fasteners else 'Inside the side reader pod',
                        'esp32': 'Approximate board envelope', 'oled': 'Module dimensions provisional',
                        'stops': 'M3×12 + nylon-insert locknuts' if simplified_fasteners else 'Hardware references; not printed',
                        'cover_fasteners': 'Metal M3×12 · washer + captive nut' if simplified_fasteners else '',
                        'rail_fasteners': 'One metal M3×12 set per keyed rail' if simplified_fasteners else '',
                        'guide_fasteners': 'Metal M3×12 · width adjustment' if simplified_fasteners else '',
                        'nfc_fasteners': 'Nylon M3×12 · washer + nut',
                        'indicator_fascia': 'Fixed to the main enclosure',
                        'leds': '60 LEDs/m · 16.67 mm pitch', 'load_cell': 'Nominal 80 mm bar cell'})
    if fastener_schedule:
        screw_lines = ' · '.join(f"{item['quantity']} {item['material']} {item['size'].replace('x', '×')}" for item in fastener_schedule['screws'])
        fig.text(.04, .061, f"Purchase list: {fastener_schedule['total_screws']} screws — {screw_lines}. Four load-cell mounting screws are omitted from the model.", fontsize=9, color=MUTED)
    footer(fig, f'{len(printed)} printed instances ({unique_printed_files} unique files). Numbers match the 3D viewer. Spool, cables and clearance volumes omitted; converter and jack are not modeled.')
    output(fig, 'exploded')

    fig = figure('Where each component lives', f'{name} · Housing and cradle shown transparent; all parts remain in their assembled positions', (20, 14))
    legend(fig)
    ax = axes(fig, [.185, .10, .63, .72])
    clearances = [model.by_id[k] for k in ['power_space', 'usb_space', 'boot_space'] if k in model.by_id] + nominal_routes
    opacity = {'base': .18, 'top_cover': .07, 'carrier': .10, 'rim_rail': .16, 'rim_rail_front': .16,
               'side_guide': .28, 'nfc_guide': .32, 'nfc_pod': .15, 'nfc_pod_cover': .12,
               'indicator_fascia': .14, 'indicator_back': .12, 'power_space': .2, 'usb_space': .18, 'boot_space': .15}
    if routing_cover is not None:
        opacity[routing_cover['id']] = .12
    positions = model.draw(ax, main + clearances, view=(350, -520, 540), opacity=opacity,
                           anchors={'stops': [61, -52, 32], 'pn532': [45, 0, 155]})
    focus = [p for p in hardware if int(p['number']) <= 18] + clearances
    callouts(fig, ax, focus, positions, left=.025, right=.815, ymin=.16, ymax=.79,
             subtitles={'power_space': 'Unassigned space, not a converter', 'usb_space': 'Internal plug allowance only',
                        'boot_space': 'Onboard button; no extra switch', 'load_cell': 'Fixed end → floating carrier',
                        'pn532': 'Moves with the weighing assembly', 'leds': 'Fixed front wall' if integrated_front else 'Fixed front pod',
                        'oled': 'Fixed left of the LEDs' if integrated_front else 'Fixed above the LEDs',
                        'stops': 'Set a clearance below the carrier'})
    footer(fig, 'Clearance volumes are provisional references. The converter is owner-selected and glued to a suitable fixed wall after checking actual fit and wire clearance.')
    output(fig, 'component-locations')

    fig = figure('Inside the fixed base', 'Lid and weighing assembly omitted for service visibility · Hardware and clearance references retain their real locations', (16, 11))
    ax = axes(fig, [.01, .13, .72, .73])
    interior_ids = ['base', 'esp32', 'hx711', 'load_cell', 'stops', 'power_space', 'usb_space', 'boot_space']
    shown = [model.by_id[k] for k in interior_ids if k in model.by_id] + base_routes
    positions = model.draw(ax, shown, view=(-170, -330, 620),
                           opacity={'base': .55, 'power_space': .2, 'usb_space': .16, 'boot_space': .2})
    for y, (title, desc) in zip([.75, .57, .39, .21], [
        ('12 / Load cell', 'Fixed M5 end bolts to the base.\nM4 end supports the moving carrier.'),
        ('13 + 14 / Control boards', 'ESP32-S3 on the left.\nHX711 amplifier on the right.'),
        ('Power conversion', 'Converter and jack are owner-selected.\nNo mount or jack hole is imposed.'),
        ('21 + 22 / Service access', 'Internal USB power plug allowance.\nBOOT reached through its tool bore.')]):
        fig.text(.75, y, title, fontsize=12, fontweight='bold')
        fig.text(.75, y - .037, desc, fontsize=10, color=MUTED, va='top', linespacing=1.6)
    footer(fig, 'Disconnect converter power before computer USB. Jack location is user-drilled; board, plug, wiring and BOOT access require a physical fit check.')
    output(fig, 'service-interior')

    front_ids = ['base', 'top_cover'] if integrated_front else ['indicator_fascia', 'indicator_back']
    shown = [model.by_id[k] for k in front_ids + ['leds'] + (['oled'] if oled else [])]
    fig = figure('Vertical front controls' if integrated_front else 'Indicator pod / inside and outside',
                 'Actual enclosure geometry · LED strip and OLED module dimensions remain provisional', (16, 11))
    ax = axes(fig, [.015, .17, .48, .60])
    model.draw(ax, shown, view=(100, -650, 215))
    ax2 = axes(fig, [.51, .17, .475, .60])
    inside = [p for p in shown if p['id'] != 'top_cover']
    model.draw(ax2, inside, exploded=not integrated_front, view=(-120, 400, 390),
               opacity={'base': .22} if integrated_front else {})
    fig.text(.055, .81, ('OLED and LEDs in the front wall' if oled else 'Five LEDs in the front wall') if integrated_front else 'Assembled front', fontsize=13, fontweight='bold')
    fig.text(.55, .81, 'Service through the main lid' if integrated_front else 'Separated rear', fontsize=13, fontweight='bold')
    fig.text(.055, .12, '01  One base; no separate front pod' if integrated_front else '10  Front panel with five light openings', fontsize=11, color=MUTED)
    fig.text(.55, .12, '16  LED strip' + ('     17  OLED module' if oled else '') + ('     Lid omitted; base transparent' if integrated_front else '     11  Rear cover'), fontsize=11, color=MUTED)
    footer(fig, 'LED centres: 16.67 mm. Strip width, coating, OLED board and pin dimensions need measuring. No screw-hole pitch is assumed for the OLED.')
    output(fig, 'indicator-detail')

    fig = figure('Exterior chamfers, up close', 'Rendered from the printable solids · Functional seats, screw holes and the load path retain their clearance geometry', (16, 11))
    ax = axes(fig, [.02, .17, .46, .60])
    model.draw(ax, [model.by_id[k] for k in ['base', 'top_cover']], view=(360, -460, 310),
               center=[57, -54, 27], scale=42)
    ax2 = axes(fig, [.52, .17, .46, .60])
    model.draw(ax2, [model.by_id[k] for k in front_ids], view=(120, -650, 215))
    fig.text(.055, .81, 'Housing rim and outer corner', fontsize=13, fontweight='bold')
    fig.text(.55, .81, 'Vertical front openings' if integrated_front else 'Indicator panel perimeter', fontsize=13, fontweight='bold')
    fig.text(.055, .12, finish_caption('base_top_outer_perimeter', 'Base rim'), fontsize=11, color=MUTED)
    fig.text(.55, .12, f'{chamfer_count} chamfer operations recorded in the CAD report.', fontsize=11, color=MUTED)
    footer(fig, 'These are CAD surface details, not simulated print finish. Verify the exported parts in your slicer and print the fit samples first.')
    output(fig, 'edge-detail')

    if routing_cover is not None:
        min_h = parameters['nfc_height_min']
        max_h = parameters['nfc_height_max']
        ref_h = parameters['nfc_height_reference']
        fig = figure('Hidden NFC wiring / full reader travel',
                     'Cover removed to reveal the actual channel · Cable volumes are nominal routing envelopes, not a physical flex simulation', (20, 13))
        fig.text(.04, .825, f'Channel open · reader centre {ref_h:g} mm', fontsize=14, fontweight='bold')
        ax = axes(fig, [.015, .18, .49, .60])
        route_base_ids = ['base', 'carrier', 'nfc_guide', 'nfc_pod', 'pn532']
        route_parts = [model.by_id[k] for k in route_base_ids] + nominal_routes
        model.draw(ax, route_parts, view=(680, -250, 280),
                   opacity={'base': .15, 'carrier': .25, 'nfc_pod': .20})
        fig.text(.04, .135, f"07  Guide and channel     {routing_cover['number']}  Removable cover omitted", fontsize=11, color=MUTED)
        fig.text(.04, .108, 'Transparent base reveals the internal cable tail.', fontsize=10, color=MUTED)
        case_base = [model.by_id[k] for k in ['base', 'carrier', 'nfc_guide', 'nfc_pod', 'nfc_pod_cover', 'pn532']]
        actual_extreme_routes = all(any(p.get('pose') == pose for p in routes) for pose in ['min', 'max'])
        for x, height, pose, title in [(.51, min_h, 'min', 'Lowest reader position'), (.755, max_h, 'max', 'Highest reader position')]:
            fig.text(x + .01, .825, title, fontsize=13, fontweight='bold')
            ax = axes(fig, [x, .18, .235, .60])
            shift = (0, 0, height - ref_h)
            moved = {k: shift for k in ['nfc_pod', 'nfc_pod_cover', 'pn532']}
            actual_route = [p for p in routes if p.get('pose') == pose]
            model.draw(ax, case_base + actual_route + base_routes, view=(680, 0, 150),
                       opacity={'base': .14, 'carrier': .20, 'nfc_pod': .20, 'nfc_pod_cover': .18},
                       shifts=moved, center=[30, -5, 100], scale=145)
            fig.text(x + .01, .135, f'{height:g} mm reader centre', fontsize=11, color=MUTED)
        route_note = 'Height panels show the exported reader-loop envelopes with the nominal base tail.' if actual_extreme_routes else 'Height panels show rigid travel geometry; only the left view shows the nominal cable envelope.'
        fig.text(.52, .108, f'{max_h - min_h:g} mm vertical travel. Cover removed in all three views.', fontsize=10, color=MUTED)
        footer(fig, route_note + ' Cable bend radius, slack, wear and effect on scale zero require a physical test.')
        output(fig, 'cable-routing')

    if metadata_path.read_bytes() != metadata_bytes or hashlib.sha256((previews / 'scene.json').read_bytes()).hexdigest() != scene_digest:
        raise RuntimeError('Scene or part map changed during rendering; regenerate the previews.')
    report = {'variant': variant, 'revision': revision, 'source': 'actual CAD tessellation',
              'scene_sha256': scene_digest,
              'part_map_sha256': hashlib.sha256(metadata_bytes).hexdigest(),
              'renderer_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'numbered_printed_instances': len(printed),
              'numbered_hardware_groups': len(hardware),
              'physical_validation': False,
              'images': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in outputs}}
    (previews / 'render-manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'variant': variant, 'images': [p.name for p in outputs], 'printed_instances': len(printed)}))
