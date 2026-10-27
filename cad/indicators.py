"""Vertical front-wall controls. Hardware and adhesive allowances are provisional."""


def make_indicators(p, app, part, box, cyl, union, cut, add, finish, near):
    v = app.Vector
    wall = p['wall']
    outer_y = p['base_center_y'] - p['base_depth'] / 2
    origin = (0, outer_y + wall, 0)
    led_x, row = p.get('front_led_center', [22.0, 29.0])
    ox, oy = p.get('front_oled_center', [-52.0, 19.0])
    pixel_x = [led_x + (i - (p['led_count'] - 1) / 2) * p['led_pitch'] for i in range(p['led_count'])]

    def pose(shape):
        result = shape.copy()
        result.rotate(v(0, 0, 0), v(1, 0, 0), 90)
        result.translate(v(*origin))
        return result

    def unpose(shape):
        result = shape.copy()
        result.translate(v(*[-n for n in origin]))
        result.rotate(v(0, 0, 0), v(1, 0, 0), -90)
        return result

    length, sw, st = p['led_strip_length'], p['led_strip_width'], p['led_strip_thickness']
    wall_gap, adhesive = .2, .2
    strip_back = -wall_gap - p['led_package'][2] - st
    plate_front = strip_back - adhesive
    plate_back = plate_front - 1.6
    # Open-top gutter supports the back of the strip; its bottom is outside the
    # load-cell lead envelope. Actual adhesive thickness and retention need a fit test.
    supports = [box(length + 4, sw, 1.6, led_x, row, plate_back),
                box(length + 4, 1, -plate_back + .2, led_x, row-sw/2-.5, plate_back)]
    for side in (-1, 1):
        supports.append(box(1.7, sw, -plate_back+.2,
                            led_x+side*(length/2+.3+.85), row, plate_back))
    cutters = [cyl(p['led_opening_diameter']/2, wall+2, x, row, -1) for x in pixel_x]
    add('REF_LED_strip_provisional', pose(box(length, sw, st, led_x, row, strip_back)), (.14, .17, .18), True)
    for i, x in enumerate(pixel_x):
        add('REF_LED_pixel_'+str(i), pose(box(*p['led_package'], x, row, strip_back+st)), (.28, .80, .53), True)

    if p['oled_enabled']:
        bw, bh, bt = p['oled_board']
        glass_depth = p['oled_glass'][2]
        pcb_front = -glass_depth - .2
        pcb_back = pcb_front - bt
        rear_gap, rail_thickness = .4, 1.4
        rear_face = pcb_back-rear_gap
        rear_back = rear_face-rail_thickness
        lower, upper = p['floor'], oy+bh/2+.3
        # Open-top side guides retain only PCB margins, outside components and pins.
        # Rear rails stand on the floor. A compressible edge shim takes up rear slack.
        for side in (-1, 1):
            supports.append(box(1.4, upper-lower, -rear_back+.2,
                                ox+side*(bw/2+1), (upper+lower)/2, rear_back))
            supports.append(box(2, upper-lower, rail_thickness,
                                ox+side*(bw/2-.3), (upper+lower)/2, rear_back))
            if side < 0:
                supports.append(box(bw+1, 2, -pcb_front+.2,
                                    ox, oy+side*(bh/2-1), pcb_front))
        supports.append(box(bw+3.4, oy-bh/2-lower, -rear_back+.2,
                            ox, (oy-bh/2+lower)/2, rear_back))
        cutters.append(box(*p['oled_window'], wall+2, ox, oy, -1))
        add('REF_OLED_PCB_provisional', pose(box(bw,bh,bt,ox,oy,pcb_back)), (.11,.28,.53), True)
        add('REF_OLED_glass_provisional', pose(box(*p['oled_glass'],ox,oy,pcb_front)), (.04,.07,.09), True)
        add('REF_OLED_active_area_provisional', pose(box(21.74,11.18,.02,ox,oy,-.19)), (.29,.55,.64), True)
        add('REF_OLED_components_provisional', pose(box(22,19,p['oled_component_depth'],ox,oy,pcb_back-p['oled_component_depth'])), (.24,.25,.27), True)
        add('REF_OLED_pin_allowance_provisional', pose(box(12,5,7,ox,oy+bh/2-4.5,pcb_back-7)), (.62,.64,.65), True)

    insertion_sweeps={}
    if p['oled_enabled']:
        lift=40.0
        # All selected component references are rectangular envelopes. Extending
        # their local Y dimension exactly represents continuous vertical insertion.
        insertion_sweeps={
            'OLED_PCB':pose(box(bw,bh+lift,bt,ox,oy+lift/2,pcb_back)),
            'OLED_glass':pose(box(p['oled_glass'][0],p['oled_glass'][1]+lift,p['oled_glass'][2],ox,oy+lift/2,pcb_front)),
            'OLED_components':pose(box(22,19+lift,p['oled_component_depth'],ox,oy+lift/2,pcb_back-p['oled_component_depth'])),
            'OLED_headers':pose(box(12,5+lift,7,ox,oy+bh/2-4.5+lift/2,pcb_back-7)),
        }

    def finish_base(base):
        def opening_edge(b):
            if not (near(b.YMin, outer_y) and near(b.YMax, outer_y)):
                return False
            if near((b.ZMin+b.ZMax)/2, row) and any(near((b.XMin+b.XMax)/2,x) for x in pixel_x):
                return True
            if p['oled_enabled']:
                ww, wh = p['oled_window']
                return (b.XMin>=ox-ww/2-.001 and b.XMax<=ox+ww/2+.001
                        and b.ZMin>=oy-wh/2-.001 and b.ZMax<=oy+wh/2+.001)
            return False
        return finish('front_control_opening_bevels', base, 'chamfer',
                      p['finish_indicator_opening_chamfer'], opening_edge)

    def coupons_from_base(base):
        local = unpose(base)
        result = {
            'fit_led_face': local.common(box(length+6,sw+3,wall+1,led_x,row,0)).removeSplitter(),
            'fit_led_carrier': local.common(box(length+6,sw+3,wall-plate_back+1,led_x,row,plate_back)).removeSplitter(),
        }
        if p['oled_enabled']:
            bw,bh,_ = p['oled_board']
            result['fit_oled_face'] = local.common(box(bw+5,upper-lower,wall-rear_back+1,ox,(upper+lower)/2,rear_back)).removeSplitter()
        for name, shape in result.items():
            if not shape.isValid() or len(shape.Solids)!=1:
                raise RuntimeError('Invalid integrated front coupon: '+name)
        return result

    return {
        'insertion_sweeps':insertion_sweeps,
        'base_additions': [pose(s) for s in supports],
        'base_cutters': [pose(s) for s in cutters],
        'finish_base': finish_base,
        'coupons_from_base': coupons_from_base,
        'notes': {
            'five_led_centers_mm': pixel_x, 'led_pitch_mm': p['led_pitch'],
            'led_center_height_mm': row, 'front_plane_y_mm': outer_y,
            'strip_length_mm_provisional': length, 'strip_width_mm_provisional': sw,
            'opening_diameter_mm': p['led_opening_diameter'],
            'face_angle_from_horizontal_deg': 90, 'oled_enabled': p['oled_enabled'],
            'oled_center_xz_mm': [ox,oy] if p['oled_enabled'] else None,
            'oled_fit': p['oled_fit_evidence'], 'led_fit': p['led_fit_evidence'],
            'physical_validation': False,
            'led_wall_gap_mm_provisional': wall_gap, 'led_adhesive_gap_mm_provisional': adhesive,
            'attachment': 'Supports integrated into fixed base front wall; service through main lid. No separate fascia or rear cover.',
            'oled_retention': 'Open-top PCB side guides, lower front edge pad, bottom stop and rear edge rails with 0.4 mm nominal foam allowance. No PCB hole pattern assumed; do not clamp glass.',
            'wiring': 'Front controls wire inside the fixed base. Keep clear of load-cell leads and moving carrier.',
            'print_note': 'Base floor down. Inspect support needs beneath internal LED gutter and PCB edge pads; all are accessible through the main lid. Fit coupons include the actual retained support geometry.',
        },
    }
