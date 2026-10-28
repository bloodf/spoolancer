"""Six broad, support-conscious v0.8 pieces. All coordinates are millimetres.

Shapes are in assembly coordinates. Apply XYZ print rotations in order, then
translate the resulting bounding-box minimum Z to zero. No export side effects.
The checks below establish geometry only; they cannot establish printed strength.
"""

import math


def build(App, Part, parameters, variant):
    p = parameters
    V = App.Vector
    cell_holes = [-65.0, -50.0, -10.0, 5.0]
    reader_range = [85.0, 170.0]
    led_pitch = 16.6666666667
    assert reader_range[0] <= p["nfc_reference_height"] <= reader_range[1]
    assert len(p["stop_positions"]) == 4
    parts, refs, colors, poses = {}, {}, {}, {}

    def box(x0, x1, y0, y1, z0, z1):
        return Part.makeBox(x1-x0, y1-y0, z1-z0, V(x0, y0, z0))

    def polygon(points, direction):
        points = [V(*q) for q in points]
        return Part.Face(Part.makePolygon(points + [points[0]])).extrude(V(*direction))

    def plate(x0, x1, y0, y1, z0, z1, chamfer=2):
        c = min(chamfer, (x1-x0)/3, (y1-y0)/3)
        return polygon([(x0+c,y0,z0),(x1-c,y0,z0),(x1,y0+c,z0),
                        (x1,y1-c,z0),(x1-c,y1,z0),(x0+c,y1,z0),
                        (x0,y1-c,z0),(x0,y0+c,z0)], (0,0,z1-z0))

    def union(shapes):
        result = shapes[0]
        for shape in shapes[1:]:
            result = result.fuse(shape)
        return result.removeSplitter()

    def yz_plate(x0, x1, y0, y1, z0, z1, chamfer=2):
        c=min(chamfer,(y1-y0)/3,(z1-z0)/3)
        return polygon([(x0,y0+c,z0),(x0,y1-c,z0),(x0,y1,z0+c),
                        (x0,y1,z1-c),(x0,y1-c,z1),(x0,y0+c,z1),
                        (x0,y0,z1-c),(x0,y0,z0+c)],(x1-x0,0,0))

    def cut(shape, cutters):
        for cutter in cutters:
            shape = shape.cut(cutter)
        return shape.removeSplitter()

    def cylinder(radius, length, origin, axis=(0,0,1)):
        return Part.makeCylinder(radius, length, V(*origin), V(*axis))

    def hexagon(af, length, origin, axis="z"):
        r = af / math.sqrt(3)
        if axis == "z":
            points = [(origin[0]+r*math.cos(i*math.pi/3),
                       origin[1]+r*math.sin(i*math.pi/3),origin[2]) for i in range(6)]
            return polygon(points, (0,0,length))
        points = [(origin[0],origin[1]+r*math.cos(i*math.pi/3),
                   origin[2]+r*math.sin(i*math.pi/3)) for i in range(6)]
        return polygon(points, (length,0,0))

    def washer(ro, ri, thickness, origin, axis=(0,0,1)):
        return cylinder(ro, thickness, origin, axis).cut(cylinder(ri, thickness, origin, axis))

    def nut(af, height, origin, axis="z"):
        direction=(0,0,1) if axis == "z" else (1,0,0)
        return hexagon(af,height,origin,axis).cut(cylinder(1.5,height,origin,direction))

    def slot_x(y, low, high, radius, x0, x1):
        return union([box(x0,x1,y-radius,y+radius,low,high),
                      cylinder(radius,x1-x0,(x0,y,low),(1,0,0)),
                      cylinder(radius,x1-x0,(x0,y,high),(1,0,0))])

    def compound(shapes):
        return Part.makeCompound(shapes)

    def add_part(name, shape, color, rotation, bed_face):
        parts[name] = shape.removeSplitter()
        colors[name] = color
        poses[name] = {"rotation": rotation, "bed_face": bed_face,
                       "support_intent": "No supports intended; inspect sliced result and bridge quality."}

    def add_ref(name, shape, color):
        refs[name] = shape
        colors[name] = color

    def overlap(a, b):
        if not a.BoundBox.intersect(b.BoundBox):
            return 0.0
        return a.common(b).Volume

    # Flat-bottom open tub. The carrier covers its centre without a fragile lid.
    base = plate(-85,85,-85,85,0,32,6)
    base = base.cut(plate(-81.8,81.8,-81.8,81.8,4,33,3))
    base = base.fuse(plate(-14,14,-75,-40,4,12,3))
    base_cutters = []
    for y in (-65,-50):
        base_cutters += [cylinder(2.8,15,(0,y,-1)), cylinder(4.9,7.2,(0,y,-1))]
    for x,y in p["stop_positions"]:
        # Broad solid posts, no towers supported by thin ribs.
        base = base.fuse(plate(x-7,x+7,y-7,y+7,4,30,2))
        base_cutters += [cylinder(1.8,30,(x,y,4)), hexagon(5.8,4.4,(x,y,25.6))]
    # Solid electronics blocks with optional tie passage below the PCB.
    for cx,cy,w,d in [(-40,-21,18,8),(-40,21,18,8),(23,-32,8,14),(43,-32,8,14)]:
        base = base.fuse(plate(cx-w/2,cx+w/2,cy-d/2,cy+d/2,4,8,1))
    # The screen and strip fit behind the vertical front wall.
    led_center_x, led_z = 22.0, 21.0
    for i in range(5):
        x=led_center_x+(i-2)*led_pitch
        base_cutters.append(cylinder(3.7,5,(x,-86,led_z),(0,1,0)))
    if variant == "oled-led":
        base_cutters.append(box(-57,-33,-86,-80,11,25))
    base = cut(base,base_cutters)
    add_part("base",base,(.16,.32,.34),[0,0,0],"Large flat underside")

    # An eight-millimetre slab and solid sloping supports replace the old frame.
    cradle = plate(-70,85.4,-70,70,40.5,48.5,5)
    slope=math.tan(math.radians(p["ramp_angle_degrees"]))
    inner,outer=p["ramp_inner_y"],p["ramp_outer_y"]
    ramp_inner=48.5+p["ramp_inner_height"]
    ramp_outer=ramp_inner+(outer-inner)*slope
    for sign in (-1,1):
        points=[(-52,sign*inner,48.5),(-52,sign*outer,48.5),
                (-52,sign*outer,ramp_outer),(-52,sign*inner,ramp_inner)]
        cradle=cradle.fuse(polygon(points,(104,0,0)))
    # A broad fixed fence, continuous with the floor, no slotted width clamps.
    fence=plate(-58,-52,-58,58,48.5,98.5,2)
    cradle=cradle.fuse(fence)
    # Three-sided shallow glue nest keeps the tower datum out of the spool bay.
    nest=union([box(52,85.4,-53.4,-50.2,48.5,54.5),
                box(52,85.4,50.2,53.4,48.5,54.5),
                box(82.2,85.4,-50.2,50.2,48.5,54.5)])
    cradle=cradle.fuse(nest)
    cradle_cutters=[box(66,74,-6,6,39,55)]
    cradle_cutters += [cylinder(1.75,12,(x,y,39)) for x,y in p["stop_positions"]]
    boot_x,boot_y,boot_z=p["boot_center_estimate"]
    cradle_cutters.append(cylinder(p["boot_access_diameter"]/2,12,(boot_x,boot_y,39)))
    for y in (-10,5):
        cradle_cutters += [cylinder(2.3,12,(0,y,39)),cylinder(4.2,6,(0,y,43.7))]
    cradle=cut(cradle,cradle_cutters)
    add_part("cradle",cradle,(.19,.48,.47),[0,0,0],"Continuous eight-millimetre slab")
    spacer=plate(-14,14,-16,11,24.7,40.5,2)
    spacer=cut(spacer,[cylinder(2.3,18,(0,y,24)) for y in (-10,5)])
    add_part("cell_spacer",spacer,(.25,.57,.54),[0,0,0],"Broad flat bottom")

    # Tower is printed front-face-down. Its rear tray is entirely open upward.
    # Local slot bands allow flush nylon heads without thinning the RF panel.
    tower=box(52,55,-50,50,48.7,202)
    tower=union([tower,box(52,82,-50,50,48.7,54.7),
                 box(55,82,-50,-47,54.7,202),box(55,82,47,50,54.7,202),
                 box(55,82,-47,47,199,202)])
    for y in (-34,34):
        tower=tower.fuse(box(55,59,y-7,y+7,77,178))
    # Cover bosses are joined to the side wall, outside the moving cassette.
    cover_positions=[(-42,62),(42,194)]
    for y,z in cover_positions:
        sign=1 if y>0 else -1
        gusset=polygon([(55,sign*47,z-6),(67.5,sign*37,z-6),
                        (82,sign*37,z-6),(82,sign*49,z-6),
                        (55,sign*49,z-6)],(0,0,12))
        tower=tower.fuse(gusset)
    tower_cutters=[box(66,74,-6,6,47,56)]
    for y in (-34,34):
        tower_cutters += [slot_x(y,85,170,1.8,51,60),slot_x(y,85,170,3.7,51,55.5)]
    for y,z in cover_positions:
        tower_cutters += [cylinder(1.8,17,(67,y,z),(1,0,0)),
                          hexagon(5.8,2.8,(70.8,y,z),"x")]
        # Side-loaded nut cavity: accessible while the tray is open.
        tower_cutters.append(box(70.8,73.6,-51 if y<0 else y,
                                 y if y<0 else 51,z-3.35,z+3.35))
    tower=cut(tower,tower_cutters)
    tower=tower.common(yz_plate(52,82,-50,50,48.7,202,3)).removeSplitter()
    add_part("nfc_tower",tower,(.20,.43,.43),[0,-90,0],"Broad front panel; open tray points upward")

    def cassette_at(height):
        # Antenna faces the tower's RF panel. No fragile PCB retaining fingers.
        cup=yz_plate(55.8,67.2,-24.75,24.75,height-23.6,height+23.6,2)
        cavity=box(55.7,64.2,-21.75,21.75,height-20.6,height+20.6)
        cup=cup.cut(cavity)
        # Raised edge seats support the PCB without touching its components.
        for y0,y1 in [(-21.75,-19.75),(19.75,21.75)]:
            cup=cup.fuse(box(57.4,64.2,y0,y1,height-20.6,height+20.6))
        for y in (-34,34):
            cup=cup.fuse(yz_plate(59.2,67.2,min(y-7,-24.75) if y<0 else 24,
                                 -24 if y<0 else y+7,height-7,height+7,1))
            cup=cup.cut(cylinder(1.8,10,(58,y,height),(1,0,0)))
        # Cable enters at the back, leaving the front antenna opening unobstructed.
        cup=cup.cut(box(63,68,-6,6,height-18,height-11))
        pcb=box(55.8,57.4,-21.35,21.35,height-20.2,height+20.2)
        components=box(57.4,63.4,-18.8,18.8,height-17.5,height+17.5)
        return cup.removeSplitter(),pcb,components

    reader_z=p["nfc_reference_height"]
    cassette,pcb,components=cassette_at(reader_z)
    add_part("nfc_cassette",cassette,(.27,.58,.55),[0,90,0],"Flat closed back; antenna cup opens upward")
    add_ref("pn532_pcb",pcb,(.72,.13,.10))
    add_ref("pn532_components",components,(.14,.15,.16))
    # Flat plate, no hooks, tongues, snap clips, or moulded hinges.
    cover=yz_plate(82.2,85.2,-50,50,54.7,202,3)
    cover=cut(cover,[cylinder(1.8,5,(81.5,y,z),(1,0,0)) for y,z in cover_positions])
    add_part("nfc_cover",cover,(.16,.36,.37),[0,-90,0],"Entire flat front face")

    cell=box(-6.35,6.35,-70,10,12,24.7)
    cell=cut(cell,[cylinder(2.5 if y < -20 else 2.0,14,(0,y,11)) for y in cell_holes])
    add_ref("load_cell",cell,(.62,.65,.67))
    fixed=[]
    for y in (-65,-50):
        fixed += [cylinder(2.5,12,(0,y,5.2)),cylinder(4.25,5,(0,y,.2)),
                  washer(4.5,2.65,1,(0,y,5.2))]
    add_ref("fixed_cell_fasteners",compound(fixed),(.72,.73,.75))
    moving=[]
    for y in (-10,5):
        moving += [cylinder(2,25,(0,y,19.5)),cylinder(3.5,4,(0,y,44.5)),
                   washer(4,2.15,.8,(0,y,43.7))]
    add_ref("moving_cell_fasteners",compound(moving),(.72,.73,.75))
    stops=[]
    for x,y in p["stop_positions"]:
        stops += [cylinder(1.5,16,(x,y,20.5)),cylinder(2.75,3,(x,y,36.5)),
                  nut(5.5,4.2,(x,y,25.8))]
    add_ref("stop_fasteners",compound(stops),(.63,.65,.67))

    def height_fasteners_at(height):
        hardware=[]
        for y in (-34,34):
            hardware += [cylinder(2.75,3,(52,y,height),(1,0,0)),
                         cylinder(1.5,16,(55,y,height),(1,0,0)),
                         washer(3.5,1.6,.5,(55,y,height),(1,0,0)),
                         washer(3.5,1.6,.5,(67.2,y,height),(1,0,0)),
                         nut(5.5,2.4,(67.7,y,height),"x")]
        return compound(hardware)
    add_ref("nfc_height_fasteners",height_fasteners_at(reader_z),(.86,.86,.80))
    cover_hardware=[]
    for y,z in cover_positions:
        cover_hardware += [cylinder(2.75,3,(85.7,y,z),(1,0,0)),
                           cylinder(1.5,16,(69.7,y,z),(1,0,0)),
                           washer(3.5,1.6,.5,(85.2,y,z),(1,0,0)),
                           nut(5.5,2.4,(71,y,z),"x")]
    add_ref("nfc_cover_fasteners",compound(cover_hardware),(.86,.86,.80))

    # Purchased electronics are references, not print parts or guaranteed fit.
    add_ref("esp32_pcb",box(-53.8,-26.2,-31.65,31.65,8,9.6),(.12,.15,.16))
    add_ref("esp32_components",compound([box(-50,-30,2,29,9.6,19.6),
                                         box(-50,-30,-29,-13,9.6,15.6),
                                         box(boot_x-2,boot_x+2,boot_y-2,boot_y+2,9.6,boot_z),
                                         box(-46,-42,-8,-4,9.6,12.6)]),(.53,.55,.55))
    add_ref("esp32_header_wires",compound([box(-56.5,-54.1,-28,28,10,29),
                                            box(-25.9,-23.5,-28,28,10,29)]),(.28,.31,.28))
    add_ref("hx711_pcb",box(16,50,-42,-22,8,9.6),(.10,.40,.25))
    add_ref("hx711_components",box(19,47,-39,-25,9.6,16),(.15,.17,.16))
    add_ref("led_strip",box(-19.667,63.667,-81.8,-81.3,16,26),(.87,.85,.75))
    pixels=[box(22+(i-2)*led_pitch-2.5,22+(i-2)*led_pitch+2.5,
                -83.3,-81.8,18.5,23.5) for i in range(5)]
    add_ref("led_pixels",compound(pixels),(.83,.90,.64))
    if variant == "oled-led":
        add_ref("oled_glass",box(-58,-32,-81.8,-78.8,10.5,25.5),(.10,.18,.20))
        add_ref("oled_pcb",box(-58.65,-31.35,-78.8,-77.2,4.35,31.65),(.10,.27,.52))
        add_ref("oled_components",box(-55.65,-34.35,-77.2,-71.2,7.35,28.65),(.18,.19,.20))
    feet=[cylinder(6,2,(x,y,-2)) for x in (-73,73) for y in (-73,73)]
    add_ref("feet",compound(feet),(.17,.18,.17))

    # Cable is shown inside the broad tray as a representative loop. It is not
    # a simulated wire; final length/bend radius/strain must be checked physically.
    cable=union([cylinder(2,3.3,(66.7,0,reader_z-15),(1,0,0)),
                 cylinder(2,36,(70,0,reader_z-15),(0,1,0)),
                 cylinder(2,reader_z-75,(70,36,60),(0,0,1)),
                 cylinder(2,36,(70,0,60),(0,1,0)),
                 cylinder(2,20,(70,0,40),(0,0,1))])
    add_ref("nfc_wire_loop",cable,(.79,.49,.15))
    add_ref("nfc_wire_tail",compound([cylinder(2,27,(70,0,13)),
                                      cylinder(2,30,(70,0,13),(0,1,0)),
                                      cylinder(2,91,(70,30,13),(-1,0,0))]),(.74,.45,.14))

    apex=ramp_inner-inner*slope

    def spool_at(diameter,width):
        radius=diameter/2
        center_z=apex+radius/math.cos(math.radians(p["ramp_angle_degrees"]))
        right=p["spool_right_face"]
        left=right-width
        disk=lambda x,length,outer,inner: cylinder(outer,length,(x,0,center_z),(1,0,0)).cut(
            cylinder(inner,length,(x,0,center_z),(1,0,0)))
        return {"spool_left_flange":disk(left,3,radius,26),
                "spool_right_flange":disk(right-3,3,radius,26),
                "spool_hub":disk(left+3,width-6,34,26),
                "spool_filament":disk(left+3,width-6,radius-9,34)},center_z

    spool,spool_z=spool_at(p["reference_spool_diameter"],p["reference_spool_width"])
    for name,shape in spool.items():
        add_ref(name,shape,(.75,.50,.29) if name != "spool_filament" else (.74,.69,.54))

    spool_checks=[]
    for diameter in p["spool_diameters"]:
        for width in p["spool_widths"]:
            case,z=spool_at(diameter,width)
            shape=compound(list(case.values()))
            collisions={name:round(overlap(shape,other),6) for name,other in parts.items()
                        if overlap(shape,other)>.001}
            # The flush head bands must also remain outside the flange envelope.
            for height in reader_range+[reader_z]:
                hit=overlap(shape,height_fasteners_at(height))
                if hit>.001:
                    collisions["nfc_height_fasteners_z"+str(height)]=round(hit,6)
            spool_checks.append({"diameter_mm":diameter,"width_mm":width,
                                 "center_z_mm":round(z,6),"floor_gap_mm":round(z-diameter/2-48.5,6),
                                 "right_face_x_mm":p["spool_right_face"],
                                 "positive_volume_intersections":collisions})
    reader_checks=[]
    for height in [85,95,110,130,150,160,170]:
        cup,board,components=cassette_at(height)
        hardware=height_fasteners_at(height)
        collisions={}
        for name,other in parts.items():
            if name == "nfc_cassette":
                continue
            for label,shape in [("cassette",cup),("board",board),("components",components),("hardware",hardware)]:
                hit=overlap(shape,other)
                if hit>.001:
                    collisions[label+"/"+name]=round(hit,6)
        reader_checks.append({"height_mm":height,"positive_volume_intersections":collisions})
    pair_checks={}
    for i,(name,a) in enumerate(parts.items()):
        for other,b in list(parts.items())[i+1:]:
            value=overlap(a,b)
            if value>.001:
                pair_checks[name+"/"+other]=round(value,6)

    schedule={"total_screws":12,"dimensional_sizes":["M3x16","M4x25","M5x12"],
              "screws":[{"size":"M3x16","material":"metal","quantity":4,"use":"Adjustable stops"},
                        {"size":"M3x16","material":"nylon","quantity":4,"use":"Reader height and rear cover"},
                        {"size":"M4x25","material":"metal","quantity":2,"use":"Moving load-cell end"},
                        {"size":"M5x12","material":"metal","quantity":2,"use":"Fixed load-cell end"}],
              "nuts":{"M3_nylon_insert_locknut":4,"M3_all_nylon":4},
              "washers":{"M3_nylon":6,"M4_metal":2,"M5_metal":2},
              "physical_validation":False}
    info={"stem":"spoolancer-v0.8-"+variant,"variant":variant,"production_part_count":6,
          "revision":p["revision"],"fastener_schedule":schedule,"spool_axis":"X",
          "base_envelope_mm":[170,170,32],"assembly_footprint_mm":[173.7,170],
          "cradle_floor_mm":8,"minimum_general_wall_mm":3,
          "spool_bay_width_mm":104,"spool_right_datum_x_mm":52,"spool_reference_gap_mm":.5,
          "reader_height_range_mm":reader_range,"spool_center_z_mm":spool_z,
          "load_cell_flexure_y_range_mm":[-40,-16],"stop_gap_mm":1,
          "load_cell_engagement_mm":{"M4":5.2,"M5":5.2},
          "v_apex_z_mm":apex,"v_slope":slope,"v_angle_degrees":p["ramp_angle_degrees"],
          "cell_stack_mm":{"cell_bottom":12,"cell_top":24.7,"spacer_top":40.5,
                           "cradle_top":48.5,"moving_head_seat":43.7},
          "service":"Remove spool and two moving-end M4 screws to lift cradle, spacer and tower; fixed M5 screws remain installed. Disconnect the NFC lead before lifting fully.",
          "glue_joint":"Broad tower bottom and three-sided nest only. Dry-fit and verify clearance before permanent bonding.",
          "boot_access":"Remove spool; insert a straight nonconductive probe vertically through the 6 mm cradle hole at X=-33,Y=-6. Estimated button top Z=12.6; verify actual board position before final mounting.",
          "boot_center_estimate_mm":p["boot_center_estimate"],"boot_access_diameter_mm":p["boot_access_diameter"],
          "fit_notes":["Generic electronics, LED strip width and connector envelopes require actual-part checks.",
                       "Wire depiction is representative; route a gentle slack loop and check weighing zero throughout travel.",
                       "Tower face is 3 mm thick; verify NFC read distance with the actual tag before final assembly.",
                       "Screw thread form, cell tapped-hole depths and final stop setting must be checked on received load cell."],
          "physical_validation":False}
    checks={"physical_validation":False,"printed_part_intersections":pair_checks,
            "spool_cases":spool_checks,"reader_positions":reader_checks,
            "shapes":{name:{"valid":shape.isValid(),"solids":len(shape.Solids),
                              "volume_mm3":round(shape.Volume,3)} for name,shape in parts.items()}}
    boot_probe=cylinder(1.5,48,(boot_x,boot_y,boot_z))
    checks["boot_access"]={"probe_diameter_mm":3,"hole_diameter_mm":p["boot_access_diameter"],
                           "spool_removed":True,"button_position_provisional":True,
                           "positive_volume_intersections":{
                               name:round(overlap(boot_probe,shape),6)
                               for name,shape in {**parts,**{k:v for k,v in refs.items() if not k.startswith("spool_")}}.items()
                               if overlap(boot_probe,shape)>.001}}
    def moving_reader_at(height):
        cup,pcb,components=cassette_at(height)
        return {"nfc_cassette":cup,"pn532_pcb":pcb,"pn532_components":components,
                "nfc_height_fasteners":height_fasteners_at(height)}

    return {"parts":parts,"refs":refs,"colors":colors,"print_poses":poses,"info":info,"checks":checks,
            "cassette_at":moving_reader_at}
