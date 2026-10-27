"""Rigid locating features and the v0.7 fastener schedule. Physical fit untested."""


def lid_hooks(box):
    hooks, sockets = [], []
    for x in (-78, 78):
        # Slide the lid 1.4 mm forward before lifting. The rear foot overlaps
        # a fixed roof by 1 mm; no cantilever must flex during assembly.
        hooks += [box(5, 1.6, 3.5, x, 61.2, 31.5),
                  box(5, 2.8, 1.6, x, 61.8, 31.5)]
        sockets += [box(5.4, 6.2, 8, x, 59.1, 31.3),
                    box(5.4, 6.4, 2.0, x, 60.2, 31.3)]
    return hooks, sockets


def rail_locators(app,part,box,finish,near):
    keys, pockets = [], []
    for x in (-55, 55):
        vertices=[app.Vector(x-3,y,z) for y,z in [(57.5,48.2),(60.7,48.2),(62.5,50),(57.5,50)]]
        key=part.Face(part.makePolygon(vertices+[vertices[0]])).extrude(app.Vector(6,0,0))
        key = finish('rail_locator_nose_' + str(x), key, 'chamfer', .3,
                     lambda b: near(b.ZMin, 48.2) and near(b.ZMax, 48.2))
        keys.append(key)
        for y in (-60, 60):
            pockets.append(box(6.4, 5.4, 2.1, x, y, 48.0))
    return keys, pockets


def schedule():
    screws = [
        {'size': 'M3x12', 'material': 'metal', 'quantity': 12,
         'locations': {'lid': 2, 'rails': 2, 'guides': 4, 'adjustable_stops': 4}},
        {'size': 'M3x12', 'material': 'nylon', 'quantity': 4,
         'locations': {'reader_height': 2, 'reader_cover': 1, 'cable_cover': 1}},
        {'size': 'M4x25', 'material': 'metal', 'quantity': 2,
         'locations': {'moving_load_cell_end': 2}},
        {'size': 'M5x12', 'material': 'metal', 'quantity': 2,
         'locations': {'fixed_load_cell_end': 2}},
    ]
    return {'screws': screws, 'total_screws': sum(s['quantity'] for s in screws),
            'dimensional_sizes': sorted({s['size'] for s in screws}),
            'purchasing_lines': len(screws),
            'nuts': {'M3_standard_metal': 8, 'M3_nylon_insert_locknut': 4, 'M3_nylon': 4},
            'washers': {'M3_metal': 8, 'M3_nylon': 4, 'M4_metal': 2, 'M5_metal': 2},
            'physical_validation': False}


def verify_service(p, app, part, box, union, parts, refs, guide, pod, cable, overlap):
    # A rectangular slab around each ring edge conservatively includes its
    # chamfers and corners. Expand each slab over the entire translation.
    # This avoids accepting clear endpoints while missing an intermediate hit.
    sections = [(-85, -72, -85, 75), (72, 85, -85, 75),
                (-72, 72, -85, -67), (-72, 72, 67, 75)]
    forward, vertical = [], []
    for x0, x1, y0, y1 in sections:
        forward.append(box(x1-x0, y1-y0+1.4, 3, (x0+x1)/2, (y0+y1)/2-.7, 35))
        vertical.append(box(x1-x0, y1-y0, 213, (x0+x1)/2, (y0+y1)/2-1.4, 35))
    for x in (-78, 78):
        forward.extend([box(5,3.0,3.5,x,60.5,31.5),box(5,4.2,1.6,x,61.1,31.5)])
        vertical.extend([box(5,1.6,213.5,x,59.8,31.5),box(5,2.8,211.6,x,60.4,31.5)])
    # Include the four chamfer-corner triangles omitted by rectangular slabs.
    # A shallow aperture bevel keeps these corners clear of the raised rails.
    for sx in (-1,1):
        for sy in (-1,1):
            pts=[app.Vector(sx*x,sy*y,35) for x,y in [(70,67),(72,67),(72,65)]]
            face=part.Face(part.makePolygon(pts+[pts[0]]))
            triangular_prism=face.extrude(app.Vector(0,0,3))
            moved=triangular_prism.copy();moved.translate(app.Vector(0,-1.4,0))
            edges=[part.Face(part.makePolygon([a,b,b+app.Vector(0,-1.4,0),a+app.Vector(0,-1.4,0),a]))
                   for a,b in zip(pts,pts[1:]+pts[:1]) if abs(a.x-b.x)>.001]
            forward.extend([triangular_prism,moved]+[e.extrude(app.Vector(0,0,3)) for e in edges])
            face.translate(app.Vector(0,-1.4,0))
            vertical.append(face.extrude(app.Vector(0,0,213)))
    width, height = 50, p['nfc_height_reference']
    body, back, pcb, components, wire = pod(width, height)
    targets = {k:s for k,s in parts.items()
               if k not in ('top_cover', 'side_guide', 'nfc_guide', 'nfc_pod', 'nfc_pod_cover', 'nfc_cable_cover')}
    targets.update(side_guide=guide(width, True), nfc_guide=guide(width, nfc=True),
                   nfc_pod=body, nfc_pod_cover=back, PN532_PCB=pcb, PN532_components=components)
    targets['nfc_cable_cover']=cable['channel'](width)[1]
    targets['NFC_cable']=cable['wire'](width,height)
    targets['NFC_base_tail']=cable['base_flex'](width)
    from nfc_mount import hardware_for_setting
    for name,hardware in hardware_for_setting(p,app,part,box,union,width,height).items():
        targets['NFC_'+name]=part.makeCompound(hardware)
    targets['NFC_cable_clamp']=part.makeCompound(cable['hardware'](width))
    for name,shape in refs.items():
        if name.startswith(('REF_fastener_rail','REF_adjustable_stop')):
            targets[name]=shape
        elif name.startswith('REF_fastener_guide'):
            shifted=shape.copy()
            direction=1 if shape.BoundBox.XMin>0 else -1
            shifted.translate(app.Vector(direction*(width-p['reference_spool_width'])/2,0,0))
            targets[name]=shifted
    targets.update({k:s for k,s in refs.items() if k.startswith(('REF_ESP32', 'REF_HX711', 'REF_OLED', 'REF_LED_', 'REF_load_cell'))})
    results = {}
    for name, sweeps in [('forward_1.4mm', forward), ('lift_210mm', vertical)]:
        results[name] = {str(i)+'/'+n:overlap(sweep, s) for i,sweep in enumerate(sweeps)
                         for n,s in targets.items() if overlap(sweep,s)>.01}
    return {'spool_removed': True, 'guide_width_mm': width, 'reader_height_mm': height,
            'motion': 'Remove two front screws; slide lid forward 1.4 mm, then lift vertically clear of the narrowed cradle.',
            'method': 'Conservative full-motion ring and hook slab envelopes; electronics remain installed.',
            'positive_volume_intersections': results, 'physical_validation': False}


def stop_hardware(p, part, cyl, hex_pocket, x, y, head_top):
    shaft=cyl(1.5,12,x,y,head_top-15)
    head=cyl(2.75,3,x,y,head_top-3)
    z=p['stop_post_top']-p['stop_nut_height']
    nut=hex_pocket(x,y,z,af=5.5,h=4.0).cut(cyl(1.5,5,x,y,z-.1))
    return part.makeCompound([shaft,head,nut])


def verify_structural_joints(p, part, box, cyl, hex_pocket, parts, refs, overlap):
    results={}
    for name in ('rim_rail','rim_rail_front'):
        rail=parts[name];b=rail.BoundBox;y=(b.YMin+b.YMax)/2
        # Material above the bearing plane can be conservatively swept as a
        # full rectangle; only the two locating keys extend below that plane.
        sweeps=[box(b.XLength,b.YLength,b.ZMax-50+60,0,y,50)]
        sweeps += [box(6,5,61.8,x,60 if y>0 else -60,48.2) for x in (-55,55)]
        removed={'side_guide','nfc_guide','nfc_pod','nfc_pod_cover','nfc_cable_cover',name}
        targets={k:s for k,s in parts.items() if k not in removed}
        targets.update({k:s for k,s in refs.items() if k.startswith(('REF_ESP32','REF_HX711','REF_load_cell'))})
        results[name+'_lift_60mm']={str(i)+'/'+k:overlap(sweep,s)
            for i,sweep in enumerate(sweeps) for k,s in targets.items() if overlap(sweep,s)>.01}
    stop_checks=[]
    for head_top in (36.2,37.0,38.0):
        for x,y in p['stop_positions']:
            hardware=stop_hardware(p,part,cyl,hex_pocket,x,y,head_top)
            hits={k:overlap(hardware,s) for k,s in parts.items() if overlap(hardware,s)>.01}
            hits.update({k:overlap(hardware,s) for k,s in refs.items()
                         if k.startswith(('REF_ESP32','REF_HX711','REF_load_cell')) and overlap(hardware,s)>.01})
            stop_checks.append({'head_top_mm':head_top,'position_mm':[x,y],
                                'positive_volume_intersections':hits})
    return {'rail_removal':results,'rail_service_precondition':'Spool and both guide assemblies removed; rail screw removed.',
            'rail_locator_print_lead_in_degrees':45,'rail_key_side_clearance_mm':.2,
            'rail_key_floor_clearance_mm':.2,'rail_lift_mm':60,
            'stop_checks':stop_checks,'stop_head_envelope_mm':[5.5,3.0],
            'stop_nut_envelope_mm':{'across_flats':5.5,'height':4.0},
            'stop_head_top_range_mm':[36.2,38.0],
            'stop_gap_note':'Geometric adjustment range only. Actual safe load-cell travel and stop setting must be measured.',
            'physical_validation':False}
