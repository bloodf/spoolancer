"""Fixed five-pixel / optional OLED pod. All hardware envelopes are provisional."""
import math


def make_indicators(p, app, part, box, cyl, union, cut, add):
    v=app.Vector
    width,height=p['indicator_panel_width'],p['indicator_panel_height']
    depth=p['indicator_depth'];wall=p['indicator_wall'];row=p['indicator_led_y']
    angle=p['indicator_angle_deg'];origin=p['indicator_origin']
    def pose(shape):
        result=shape.copy();result.rotate(v(0,0,0),v(1,0,0),angle);result.translate(v(*origin));return result
    pixel_x=[(i-(p['led_count']-1)/2)*p['led_pitch'] for i in range(p['led_count'])]
    screw_y=height/2-4
    screws=[(x,y) for x in (-47.5,47.5) for y in (-screw_y,screw_y)]
    face=box(width,height,2,0,0,0)
    shell=box(width,height,depth-2,0,0,-depth+2).cut(box(width-4,height-4,depth,0,0,-depth+1))
    bosses=[cyl(3,depth-2,x,y,-depth+2) for x,y in screws]
    fascia=union([face,shell]+bosses)
    face_cuts=[cyl(p['led_opening_diameter']/2,4,x,row,-1) for x in pixel_x]
    face_cuts += [cyl(1.05,depth-2,x,y,-depth+1.9) for x,y in screws]
    if p['oled_enabled']:
        ox,oy=p['oled_center'];bw,bh,bt=p['oled_board']
        face_cuts.append(box(*p['oled_window'],5,ox,oy,-1))
        # Edge pads bear only on PCB corners, outside the simplified glass.
        pads=[]
        for side in (-1,1):
            pads.append(box(bw+1,2,3,ox,oy+side*(bh/2-1),-3))
            pads.append(box(4,height/2-abs(oy+side*(bh/2-1))+1,2,
                            ox,side*(height/2+abs(oy+side*(bh/2-1)))/2,0))
        fascia=union([fascia]+pads)
    fascia=cut(fascia,face_cuts)
    # Two slim side arms and one bridge use the fixed front cover posts.
    # No contact is made with the floating carrier or spool rails.
    placed=pose(fascia)
    arm_front=origin[1]-1
    arm_rear=-85
    arms=[box(6,arm_rear-arm_front,4,x,(arm_front+arm_rear)/2,38) for x in (-53,53)]
    bridge=box(164,6,3,0,-84,38)
    tabs=[box(12,20,3,x,-79,38) for x in (-78,78)]
    placed=union([placed,bridge]+arms+tabs)
    placed=cut(placed,[cyl(1.7,6,x,-76,37) for x in (-78,78)])
    add('indicator_fascia',placed,(.25,.43,.42))

    back=box(width-4,height-4,2,0,0,-depth)
    # Adhesive strip attaches to a narrow bridge, supported from the rear cover.
    strip_support=box(p['led_strip_length']+3,p['led_strip_width']+1,1.6,0,row,-3.6)
    supports=[box(p['led_strip_length']+3,1.5,depth-5.6,0,row+y,-depth+2)
              for y in (-p['led_strip_width']/2+.75,p['led_strip_width']/2-.75)]
    back=union([back,strip_support]+supports)
    back_cuts=[cyl(1.4,5,x,y,-depth-1) for x,y in screws]
    # Pin and wire holes remain on the fixed pod, facing the fixed base.
    back_cuts += [box(10,6,depth,40,0,-depth-1)]
    if p['oled_enabled']:
        ox,oy=p['oled_center'];bw,bh,bt=p['oled_board']
        # Rear corner supports are 0.3 mm short for a light foam pad. No assumed
        # PCB screw-hole pattern or pressure on the OLED glass is needed.
        for side in (-1,1):
            back=union([back,box(bw+1,2,depth-2-4.9,ox,oy+side*(bh/2-1),-depth+2)])
        back_cuts.append(box(14,7,depth,ox,oy+bh/2-4.5,-depth-1))
    back=cut(back,back_cuts)
    add('indicator_back',pose(back),(.18,.29,.30))
    strip=box(p['led_strip_length'],p['led_strip_width'],p['led_strip_thickness'],0,row,-2)
    add('REF_LED_strip_provisional',pose(strip),(.14,.17,.18),True)
    for i,x in enumerate(pixel_x):
        add('REF_LED_pixel_'+str(i),pose(box(*p['led_package'],x,row,-1.5)),(.28,.80,.53),True)
    if p['oled_enabled']:
        ox,oy=p['oled_center'];bw,bh,bt=p['oled_board']
        add('REF_OLED_PCB_provisional',pose(box(bw,bh,bt,ox,oy,-3-bt)),(.11,.28,.53),True)
        add('REF_OLED_glass_provisional',pose(box(*p['oled_glass'],ox,oy,-3)),(.04,.07,.09),True)
        add('REF_OLED_active_area_provisional',pose(box(21.74,11.18,.02,ox,oy,.03)),(.29,.55,.64),True)
        add('REF_OLED_components_provisional',pose(box(22,19,p['oled_component_depth'],ox,oy,-4.6-p['oled_component_depth'])),(.24,.25,.27),True)
        add('REF_OLED_pin_allowance_provisional',pose(box(12,5,7,ox,oy+bh/2-4.5,-11.6)),(.62,.64,.65),True)
    coupons={
        'fit_led_face':fascia.common(box(90,13,3,0,row,0)).removeSplitter(),
        'fit_led_carrier':back.common(box(90,p['led_strip_width']+2,depth,0,row,-depth)).removeSplitter(),
    }
    if p['oled_enabled']:
        ox,oy=p['oled_center']
        coupons['fit_oled_face']=fascia.common(box(34,32,5,ox,oy,-3)).removeSplitter()
    return {'coupons':coupons,'notes':{
        'five_led_centers_mm':pixel_x,'led_pitch_mm':p['led_pitch'],
        'strip_length_mm_provisional':p['led_strip_length'],'strip_width_mm_provisional':p['led_strip_width'],
        'opening_diameter_mm':p['led_opening_diameter'],'face_angle_from_horizontal_deg':angle,
        'oled_enabled':p['oled_enabled'],'oled_fit':p['oled_fit_evidence'],
        'led_fit':p['led_fit_evidence'],'physical_validation':False,
        'attachment':'Fixed front cover posts; two M3x16 replace front closure screws, four M2.5x10 plastic-thread prototype back screws.',
        'oled_retention':'Edge pads and rear foam shims; no assumed PCB screw holes. Verify PCB/glass/headers before printing.',
        'wiring':'6 mm round fixed-base feedthrough at x=40,z=26; no wires to moving carrier.',
        'print_note':'Fascia face down; long mounting bridge is elevated in that orientation and requires local support. Back flat down; bridge ceilings require slicer inspection.'}}
