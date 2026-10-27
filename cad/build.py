"""Spoolancer v0.4.1 headless: original parametric FreeCAD geometry, millimetres.

Run with FreeCAD's Python environment; see cad/README.md.
Edit parameters.json and regenerate; hardware bodies are clearance references,
not manufacturer CAD. No third-party model is incorporated in the exports.
"""
import argparse
import json
import math
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
freecad_lib = os.environ.get('FREECAD_LIB')
if freecad_lib:
    sys.path.insert(0, freecad_lib)
elif sys.platform == 'darwin':
    bundled_lib = Path('/Applications/FreeCAD.app/Contents/Resources/lib')
    if bundled_lib.is_dir():
        sys.path.append(str(bundled_lib))
import FreeCAD as App
import Part
import MeshPart

parser=argparse.ArgumentParser()
parser.add_argument('--variant',choices=['headless','touch-8048','headless-led','oled-led'])
ARGS=parser.parse_args()
VARIANT=ARGS.variant or 'headless'
TARGET=HERE/'variants'/VARIANT if ARGS.variant else HERE
P=json.loads((HERE/'parameters.json').read_text())
if VARIANT!='headless':
    P.update(json.loads((TARGET/'parameters.json').read_text()))
OUT=TARGET/'exports';PREV=TARGET/'previews'
OUT.mkdir(parents=True,exist_ok=True);PREV.mkdir(parents=True,exist_ok=True)
TOUCH=VARIANT=='touch-8048'
INDICATORS=VARIANT in ('headless-led','oled-led')
STEM='spoolancer-v0.4-touch-8048' if TOUCH else 'spoolancer-v0.4.1-headless'
if INDICATORS: STEM='spoolancer-v0.7-'+VARIANT
V = App.Vector
PARTS = {}
REFS = {}
COLORS = {}
FINISHES = {}


def near(a,b):
    return abs(a-b)<.001


def finish(name,shape,operation,radius,predicate):
    """Apply one deliberate edge treatment; missing/invalid geometry is an error."""
    selected=[(i,e) for i,e in enumerate(shape.Edges,1) if predicate(e.BoundBox)]
    if not selected:
        raise RuntimeError('No edges selected for '+name)
    edges=[e for _,e in selected]
    result=(shape.makeFillet(radius,edges) if operation=='fillet' else shape.makeChamfer(radius,edges)).removeSplitter()
    if not result.isValid() or len(result.Solids)!=1:
        raise RuntimeError('Invalid finish for '+name)
    added=result.cut(shape).Volume
    removed=shape.Volume-result.Volume
    if added>.001 or removed<=.001:
        raise RuntimeError('Finish must remove material only: '+name)
    if name not in FINISHES:
        FINISHES[name]={'operation':operation,'radius_or_chamfer_mm':radius,'selected_edge_count':len(edges),
            'selected_edges':[{'index':i,'bounds_mm':[round(n,5) for n in (e.BoundBox.XMin,e.BoundBox.YMin,e.BoundBox.ZMin,e.BoundBox.XMax,e.BoundBox.YMax,e.BoundBox.ZMax)]} for i,e in selected],
            'before_volume_mm3':round(shape.Volume,6),'after_volume_mm3':round(result.Volume,6),
            'removed_volume_mm3':round(removed,6),'added_volume_mm3':round(added,6),
            'valid_single_solid':True,'selection_evidence':'Explicit geometric bounds; no fallback or silent edge skipping'}
    return result


def box(w, d, h, x=0, y=0, z=0):
    return Part.makeBox(w, d, h, V(x-w/2, y-d/2, z))


def cyl(r, h, x=0, y=0, z=0):
    return Part.makeCylinder(r, h, V(x, y, z))


def union(items):
    items = list(items)
    return items[0].multiFuse(items[1:]).removeSplitter() if len(items)>1 else items[0]


def cut(shape, cutters):
    return shape.cut(union(cutters)).removeSplitter()


def rounded_rectangle(width,depth,radius,cx=0,cy=0):
    x0,x1=cx-width/2,cx+width/2
    y0,y1=cy-depth/2,cy+depth/2
    r=radius;q=math.sqrt(.5)
    pts=[V(x0+r,y0,0),V(x1-r,y0,0),V(x1,y0+r,0),V(x1,y1-r,0),
         V(x1-r,y1,0),V(x0+r,y1,0),V(x0,y1-r,0),V(x0,y0+r,0)]
    mids=[V(x1-r+r*q,y0+r-r*q,0),V(x1-r+r*q,y1-r+r*q,0),
          V(x0+r-r*q,y1-r+r*q,0),V(x0+r-r*q,y0+r-r*q,0)]
    edges=[]
    for i in range(4):
        edges.append(Part.makeLine(pts[2*i],pts[2*i+1]))
        edges.append(Part.Arc(pts[2*i+1],mids[i],pts[(2*i+2)%8]).toShape())
    return Part.Face(Part.Wire(edges))


def yz_profile(points,x,width):
    vertices=[V(x,y,z) for y,z in points]
    return Part.Face(Part.makePolygon(vertices+[vertices[0]])).extrude(V(width,0,0))


def extrusion(face, height, z=0):
    s = face.extrude(V(0,0,height))
    s.translate(V(0,0,z))
    return s


def hex_pocket(x,y,z,af=5.7,h=2.6):
    r=af/math.sqrt(3)
    pts=[V(x+r*math.cos(math.radians(30+60*i)),
           y+r*math.sin(math.radians(30+60*i)),z) for i in range(6)]
    return Part.Face(Part.makePolygon(pts+[pts[0]])).extrude(V(0,0,h))


def slot(x,y,z,diameter,length,h,axis="x"):
    delta=max(0,(length-diameter)/2)
    dx,dy=(delta,0) if axis=="x" else (0,delta)
    return union([cyl(diameter/2,h,x-dx,y-dy,z),
                  cyl(diameter/2,h,x+dx,y+dy,z),
                  box(max(0.01,2*dx),diameter,h,x,y,z) if axis=="x" else
                  box(diameter,max(0.01,2*dy),h,x,y,z)])


def add(name,shape,color,reference=False):
    (REFS if reference else PARTS)[name]=shape.removeSplitter()
    COLORS[name]=color


def rail_pair(cx,cy,length,width,z):
    # Narrow shelves bear on PCB outer margins, clear of underside components.
    # Light cable ties provide retention; width is intentionally editable.
    pieces=[]
    for side in (-1,1):
        pieces += [box(length,1.0,z-P['floor'],cx,cy+side*(width/2-0.2),P['floor']),
                   box(length,1.1,z+2.0-P['floor'],cx,cy+side*(width/2+0.85),P['floor'])]
    return pieces


BW,BD,CY,W=P['base_width'],P['base_depth'],P['base_center_y'],P['wall']
BT,LT=P['body_top'],P['lid_thickness']
def beveled_rectangle(width,depth,bevel,cx=0,cy=0):
    x0,x1=cx-width/2,cx+width/2;y0,y1=cy-depth/2,cy+depth/2;c=bevel
    pts=[V(x0+c,y0,0),V(x1-c,y0,0),V(x1,y0+c,0),V(x1,y1-c,0),V(x1-c,y1,0),V(x0+c,y1,0),V(x0,y1-c,0),V(x0,y0+c,0)]
    return Part.Face(Part.makePolygon(pts+[pts[0]]))

outline=beveled_rectangle if INDICATORS else rounded_rectangle
front_face=outline(BW,BD,P['base_corner_radius'],cy=CY)
inner_face=outline(BW-2*W,BD-2*W,P['base_corner_radius']-W,cy=CY)
base=extrusion(front_face,BT).cut(extrusion(inner_face,BT,P['floor']))
holes=[P['cell_front_y']+o for o in P['cell_hole_offsets']]
fixed_y=(holes[0]+holes[1])/2
moving_y=(holes[2]+holes[3])/2
base_add=[box(24,28,P['cell_bottom']-P['floor'],0,fixed_y,P['floor'])]
# Ribs spread load from the fixed pad into the floor, below the sensor.
base_add += [box(134,4,5,0,fixed_y,P['floor']),
             box(4,P['fixed_spine_length'],5,0,P['fixed_spine_center_y'],P['floor'])]
lid_posts=P['lid_posts']
closure_posts=[(x,y) for x,y in lid_posts if not INDICATORS or y<0]
if INDICATORS:
    from simple_fasteners import lid_hooks,rail_locators,schedule,verify_service,stop_hardware,verify_structural_joints
    lid_hook_shapes,lid_hook_sockets=lid_hooks(box)
    rail_keys,rail_key_pockets=rail_locators(App,Part,box,finish,near)
stop_positions=P['stop_positions']
for x,y in lid_posts:
    base_add.append(cyl(6,BT-P['floor'],x,y,P['floor']))
for x,y in P['cover_support_posts']:
    base_add.append(cyl(2.5,BT-P['floor'],x,y,P['floor']))
for x,y in stop_positions:
    base_add.append(cyl(6,P['stop_post_top']-P['floor'],x,y,P['floor']))
ex,ey=P['esp_center']; ew=P['esp_envelope'][1]
hx,hy=P['hx_center']; hw=P['hx_board'][1]
base_add += rail_pair(ex,ey,P['esp_rail_length'],ew,P['esp_pcb_bottom'])
base_add += rail_pair(hx,hy,29,hw,P['hx_pcb_bottom'])
base=union([base]+base_add)
base_cut=[]
for y in holes[:2]:
    base_cut += [cyl(P['fixed_clearance_diameter']/2,30,0,y,-1),
                 cyl(5.4,P['fixed_head_seat_z']+1,0,y,-1)]
if INDICATORS:base_cut += lid_hook_sockets
for x,y in closure_posts:
    base_cut += [cyl(1.7,BT,x,y,4),box(14,5.8,2.6,x-math.copysign(3,x),y,BT-5.6)]
for x,y in stop_positions:
    base_cut += [cyl(1.7,35,x,y,3),hex_pocket(x,y,P['stop_post_top']-P['stop_nut_height'],h=P['stop_nut_height'])]
# Older variants retain their USB service opening. DC variants expose only BOOT.
if not INDICATORS:
    base_cut.append(box(12,36,BT-6,-BW/2,ey,7))
else:
    bx,by=P['boot_center_estimate']
    boot_top=P['esp_pcb_bottom']+P['esp_envelope'][2]+P['boot_body_height_estimate']+P['boot_actuator_height_estimate']
    entry=V(*P['boot_tool_entry']);direction=V(bx,by,boot_top+1.5)-entry
    base_cut.append(Part.makeCylinder(P['boot_access_hole_diameter']/2,direction.Length,entry,direction))

tie_locations=[(ex-18,ey,ew+9),(ex+17,ey,ew+9),(hx,hy,hw+9)]
for x,y,span in tie_locations:
    for side in (-1,1):
        base_cut.append(box(3.2,5,10,x,y+side*span/2,-1))
    base_cut.append(box(3.2,span+5,1.4,x,y,0))
feet=P['feet_positions']
for x,y in feet:
    base_cut.append(cyl(6.2,0.6,x,y,0))
if INDICATORS:
    from indicators import make_indicators
    indicator_data=make_indicators(P,App,Part,box,cyl,union,cut,add,finish,near)
    base=union([base]+indicator_data['base_additions'])
    base_cut += indicator_data['base_cutters']
    # Fixed cable tie saddle, wholly below the carrier's open central side bay.
    base=union([base,box(10,8,5,24,12,P['floor'])])
    base_cut += [box(2,12,2,24,12,4.2)]
base=cut(base,base_cut)
if INDICATORS:
    base=indicator_data['finish_base'](base)
    def base_outline(b,z):
        return near(b.ZMin,z) and near(b.ZMax,z) and (near(b.XMin,-BW/2) or near(b.XMax,BW/2) or near(b.YMin,CY-BD/2) or near(b.YMax,CY+BD/2))
    base=finish('base_bottom_perimeter',base,'chamfer',P['finish_base_bottom_chamfer'],lambda b:base_outline(b,0))
    base=finish('base_top_outer_perimeter',base,'chamfer',P['finish_base_top_chamfer'],lambda b:base_outline(b,BT))
add('base',base,(0.15,0.20,0.23))
if INDICATORS:
    indicator_data['coupons']=indicator_data['coupons_from_base'](base)
    from cable_route import cable_system
    CABLE=cable_system(P,App,Part,box,union,cut,finish,near)

# All contact parts and the reader move with the cell. The carrier has open bays.
cw,cd=P['carrier_size']
carrier_bottom=P['carrier_top']-P['carrier_plate']
rib_bottom=carrier_bottom-P['carrier_rib_height']
carrier_pieces=[box(20,cd,12,0,0,rib_bottom),
    box(24,27,carrier_bottom-P['cell_bottom']-P['cell_height'],0,moving_y,P['cell_bottom']+P['cell_height'])]
for y in (-58,-15,15,58):
    carrier_pieces.append(box(cw,14,12,0,y,rib_bottom))
for x in (-68,68):
    carrier_pieces.append(box(4,cd,12,x,0,rib_bottom))
for x,y in stop_positions:
    carrier_pieces.append(cyl(6,12,x,y,rib_bottom))
carrier=union(carrier_pieces)
carrier_cut=[]
for y in holes[2:]:
    carrier_cut += [cyl(P['moving_clearance_diameter']/2,60,0,y,0),cyl(5,20,0,y,P['moving_head_seat_z'])]
for x,y in stop_positions:
    carrier_cut.append(cyl(1.75,30,x,y,rib_bottom-1))
for x,y in P['saddle_bolt_positions']:
    carrier_cut += [cyl(1.7,30,x,y,rib_bottom-1),box(5.8,11,2.6,x,math.copysign(61.5,y),44.4)]
# Side-inserted nuts bear against a 3 mm retaining roof, so tightening
# clamps the guide feet to the carrier rather than only to loose nuts.
for x in (-49.5,49.5):
    for y in (-15,15):
        carrier_cut += [slot(x,y,37,3.4,28.4,15),box(38,5.8,2.6,math.copysign(52,x),y,44.4)]
if INDICATORS:carrier_cut += rail_key_pockets
carrier=cut(carrier,carrier_cut)
if INDICATORS:
    carrier=finish('carrier_lower_outer_edges',carrier,'chamfer',P['finish_carrier_chamfer'],
        lambda b:near(b.ZMin,rib_bottom) and near(b.ZMax,rib_bottom) and (near(b.XMin,-cw/2) or near(b.XMax,cw/2) or near(b.YMin,-cd/2) or near(b.YMax,cd/2)))
add('carrier',carrier,(0.67,0.77,0.74))

slope=math.tan(math.radians(P['saddle_angle_deg']))
apex=P['saddle_apex_z'];half_depth=P['saddle_depth']/2
surface=lambda y:apex+abs(y)*slope
ri,ro=P['rim_inner_y'],P['rim_outer_y']
rail=yz_profile([(ri,50),(ro,50),(ro,surface(ro)),(ri,surface(ri))],-cw/2,cw)
rail_cut=[]
# Open bays between webs; roof and bottom flange remain continuous.
for x0,x1 in ([(-58,-34),(-30,-6),(6,30),(34,58)] if INDICATORS else [(-58,-34),(-30,-2.5),(2.5,30),(34,58)]):
    rail_cut.append(yz_profile([(ri-1,53),(ro+1,53),
        (ro+1,surface(ro+1)-P['rim_roof']),(ri-1,surface(ri-1)-P['rim_roof'])],x0,x1-x0))
for x in ([0] if INDICATORS else [-64,64]):
    rail_cut += [cyl(1.7,50,x,60,49),cyl(4,50,x,60,54)]
for x in (-61,61):
    rail_cut.append(cyl(1.75,50,x,52,49))
rail=cut(rail,rail_cut)
if INDICATORS:rail=union([rail]+rail_keys)
if INDICATORS:
    rail=finish('rim_rail_external_end_perimeters',rail,'chamfer',P['finish_rail_end_chamfer'],lambda b:near(abs(b.XMin),cw/2) and near(b.XLength,0))
add('rim_rail',rail,(0.77,0.84,0.82))
front_rail=rail.copy();front_rail.rotate(V(0,0,0),V(0,0,1),180)


def x_slot(x,y,z,diameter,length,depth):
    delta=(length-diameter)/2
    return union([Part.makeCylinder(diameter/2,depth,V(x,y,z-delta),V(1,0,0)),
                  Part.makeCylinder(diameter/2,depth,V(x,y,z+delta),V(1,0,0)),
                  box(depth,diameter,2*delta,x+depth/2,y,z-delta)])


GUIDE_CACHE={}

def guide_for_width(width,left=False,nfc=False):
    key=(width,left,nfc)
    if key in GUIDE_CACHE:return GUIDE_CACHE[key].copy()
    inner=(width+P['guide_clearance_total'])/2
    top=P['guide_top'];h=half_depth
    bottom=[(-h,surface(h)+1),(-34,surface(34)+1),(-33,50),(33,50),(34,surface(34)+1),(h,surface(h)+1)]
    wall=yz_profile(bottom+[(h,top-4),(h-4,top),(-h+4,top),(-h,top-4)],inner,4)
    wall=wall.cut(box(8,51,125 if nfc else 30,inner+2,0,66))
    feet=[box(16,14,5,inner+8,y,50) for y in (-15,15)]
    pieces=[wall]+feet
    if nfc:
        for y in (-P['nfc_mast_y'],P['nfc_mast_y']):
            pieces.append(box(6,8,P['nfc_mast_top']-80,inner+3,y,80))
    guide=union(pieces)
    cutters=[cyl(1.7,20,inner+10,y,45) for y in (-15,15)]
    if nfc:
        for y in (-P['nfc_mast_y'],P['nfc_mast_y']):
            cutters += [x_slot(inner-.1,y,(P['nfc_height_min']+P['nfc_height_max'])/2,3.4,P['nfc_height_max']-P['nfc_height_min']+3.4,6.2),
                        box(2.6,5.8,P['nfc_mast_top']-P['nfc_height_min']+7,inner+3.3,y,P['nfc_height_min']-6)]
    guide=cut(guide,cutters)
    if INDICATORS:
        if nfc:
            guide=finish('nfc_mast_external_vertical_corners',guide,'chamfer',P['finish_mast_corner_chamfer'],
                lambda b:near(b.XMin,inner+6) and near(b.XMax,inner+6) and b.ZLength>50 and near(b.YLength,0) and (near(abs(b.YMin),P['nfc_mast_y']+4) or near(abs(b.YMin),P['nfc_mast_y']-4)))
            guide=finish('nfc_mast_top_outer_edges',guide,'chamfer',P['finish_mast_top_chamfer'],
                lambda b:near(b.ZMin,P['nfc_mast_top']) and near(b.ZMax,P['nfc_mast_top']) and (near(b.XMin,inner) or near(b.XMax,inner+6) or near(abs(b.YMin),P['nfc_mast_y']+4) or near(abs(b.YMax),P['nfc_mast_y']+4) or near(abs(b.YMin),P['nfc_mast_y']-4) or near(abs(b.YMax),P['nfc_mast_y']-4)))
        else:
            guide=finish('side_guide_top_outer_edge',guide,'chamfer',P['finish_guide_top_chamfer'],
                lambda b:near(b.ZMin,top) and near(b.ZMax,top) and near(b.XMin,inner+4) and near(b.XMax,inner+4))
    if INDICATORS and nfc:guide=union([guide,CABLE['channel'](width)[0]])
    if left: guide.rotate(V(0,0,0),V(0,0,1),180)
    GUIDE_CACHE[key]=guide.copy()
    return guide

add('side_guide',guide_for_width(P['reference_spool_width'],True),(0.25,0.43,0.42))
add('nfc_guide',guide_for_width(P['reference_spool_width'],nfc=True),(0.25,0.43,0.42))
if INDICATORS:add('nfc_cable_cover',CABLE['channel'](P['reference_spool_width'])[1],(.18,.29,.30))


def x_cylinder(radius,length,x,y,z):
    return Part.makeCylinder(radius,length,V(x,y,z),V(1,0,0))


def pod_for_setting(width,height):
    if INDICATORS:
        from nfc_mount import make_pod
        return make_pod(P,App,Part,box,cyl,union,cut,finish,near,CABLE,width,height)
    inner=(width+P['guide_clearance_total'])/2
    oy,oz=P['nfc_pod_outer'];depth=P['nfc_pod_depth'];wall=1.6
    pod=box(depth,oy,oz,inner+depth/2,0,height-oz/2)
    pod=pod.cut(box(depth+1,oy-2*wall,oz-2*wall,inner+P['nfc_wall']+(depth+1)/2,0,height-oz/2+wall))
    additions=[];cuts=[]
    for y in (-P['nfc_mast_y'],P['nfc_mast_y']):
        additions.append(box(2,12,10,inner+7,y,height-5))
        cuts.append(x_cylinder(1.7,5,inner+5,y,height))
    # Both possible PCB diagonals are provided. Use only the received board's pair.
    for y in (-P['nfc_hole_pitch'][0]/2,P['nfc_hole_pitch'][0]/2):
        for dz in (-P['nfc_hole_pitch'][1]/2,P['nfc_hole_pitch'][1]/2):
            additions.append(x_cylinder(3.0,P['nfc_air_gap'],inner+P['nfc_wall'],y,height+dz))
            cuts += [x_cylinder(1.4,4,inner-.1,y,height+dz),
                     Part.makeCone(2.7,1.4,1.3,V(inner-.01,y,height+dz),V(1,0,0))]
    for y,dz in P['nfc_cover_screw_positions']:
        additions.append(x_cylinder(3.4,depth-7,inner+7,y,height+dz))
        cuts.append(x_cylinder(1.05,9,inner+5,y,height+dz))
    pod=union([pod]+additions)
    # Bottom slot permits direct solder wires with a silicone service loop.
    if not INDICATORS:cuts.append(box(10,12,5,inner+9,0,height-oz/2-1))
    pod=cut(pod,cuts)
    cover=box(P['nfc_back_thickness'],oy,oz,inner+depth+P['nfc_back_thickness']/2,0,height-oz/2)
    cover=cut(cover,[x_cylinder(1.35,4,inner+depth-1,y,height+dz) for y,dz in P['nfc_cover_screw_positions']])
    if INDICATORS:
        def corner(b):return near(b.YLength,0) and near(b.ZLength,0) and near(abs(b.YMin),oy/2) and (near(b.ZMin,height-oz/2) or near(b.ZMin,height+oz/2))
        pod=finish('nfc_pod_external_corners',pod,'chamfer',P['finish_nfc_corner_chamfer'],corner)
        cover=finish('nfc_back_cover_corners',cover,'chamfer',P['finish_nfc_corner_chamfer'],corner)
        cover=finish('nfc_back_cover_outer_rim',cover,'chamfer',P['finish_nfc_back_edge_chamfer'],
            lambda b:near(b.XMin,inner+depth+P['nfc_back_thickness']) and near(b.XLength,0) and (near(b.YMin,-oy/2) or near(b.YMax,oy/2) or near(b.ZMin,height-oz/2) or near(b.ZMax,height+oz/2)))
    if INDICATORS:
        port=CABLE['port'](width,height)
        pod=pod.cut(port).removeSplitter()
        cover=union([cover,CABLE['hood'](width,height)]).cut(port).removeSplitter()
    pcb=box(P['nfc_board'][2],P['nfc_board'][0],P['nfc_board'][1],inner+2+P['nfc_board'][2]/2,0,height-P['nfc_board'][1]/2)
    pcb=cut(pcb,[x_cylinder(1.524,4,inner+1,y,height+dz) for y,dz in [(-13.9,-12.6),(13.9,12.6)]])
    components=box(2.4,28,22,inner+4.8,0,height-11)
    wires=box(7,12,8,inner+7.1,0,height-20.2)
    return pod,cover,pcb,components,wires

pod,pod_cover,pnboard,pncomponents,pnwires=pod_for_setting(P['reference_spool_width'],P['nfc_height_reference'])
add('nfc_pod',pod,(0.88,0.58,0.25))
add('nfc_pod_cover',pod_cover,(0.78,0.42,0.16))
add('REF_PN532_PCB',pnboard,(0.71,0.16,0.13),True)
add('REF_PN532_components',pncomponents,(0.18,0.18,0.18),True)
add('REF_PN532_wire_allowance',pnwires,(0.34,0.23,0.15),True)
if INDICATORS:
    for label,height in [('nominal',P['nfc_height_reference']),('low',P['nfc_height_min']),('high',P['nfc_height_max'])]:
        add('REF_NFC_cable_'+label,CABLE['wire'](P['reference_spool_width'],height),(.90,.48,.13),True)
    add('REF_NFC_base_flex_nominal',CABLE['base_flex'](P['reference_spool_width']),(.90,.48,.13),True)

# Full fixed cover, with a 2 mm plan gap around the floating carrier.
lid=extrusion(front_face,LT,BT).cut(extrusion(outline(
    cw+2*P['cover_carrier_gap'],cd+2*P['cover_carrier_gap'],2 if INDICATORS else 6),LT+2,BT-1))
# The USB-side GPIO wires extend under the fixed cover, outside its aperture.
lid=lid.cut(box(20,36,P['esp_cover_relief_depth']+.1,-79,ey,BT-.1))
# Recovery uses the controller's onboard BOOT switch through the USB service
# opening. Both variants have a blank fixed cover without an added switch mount.
lid=cut(lid,[cyl(1.7,12,x,y,BT-5) for x,y in closure_posts])
if INDICATORS:lid=union([lid]+lid_hook_shapes)
if INDICATORS:
    lid=finish('cover_top_outer_perimeter',lid,'chamfer',P['finish_cover_chamfer'],lambda b:base_outline(b,BT+LT))
    lid=finish('cover_top_carrier_aperture',lid,'chamfer',P['finish_cover_aperture_chamfer'],
        lambda b:near(b.ZMin,BT+LT) and near(b.ZMax,BT+LT) and b.XMin>=-cw/2-P['cover_carrier_gap']-.001 and b.XMax<=cw/2+P['cover_carrier_gap']+.001 and b.YMin>=-cd/2-P['cover_carrier_gap']-.001 and b.YMax<=cd/2+P['cover_carrier_gap']+.001)
add('top_cover',lid,(0.25,0.43,0.42))

# Reference component envelopes: communicate placement without pretending to
# represent manufacturer CAD, connectors, all components, or real wiring.
cell=box(P['cell_width'],P['cell_length'],P['cell_height'],0,P['cell_front_y']+P['cell_length']/2,P['cell_bottom'])
cell=cut(cell,[cyl(2.5 if i<2 else 2,20,0,y,P['cell_bottom']-1) for i,y in enumerate(holes)])
add('REF_load_cell',cell,(0.66,0.69,0.72),True)
add('REF_load_cell_wire_allowance',box(*P['cell_wire_allowance'],*P['cell_wire_allowance_center']),(0.42,0.29,0.18),True)
if not TOUCH:
    esp=box(*P['esp_envelope'],ex,ey,P['esp_pcb_bottom'])
    add('REF_ESP32_PCB_estimate',esp,(0.08,0.10,0.11),True)
    add('REF_ESP32_module_estimate',box(26,18,3.5,ex+14,ey,P['esp_pcb_bottom']+1.6),(0.60,0.64,0.66),True)
    for dy in (-6,6):
        add('REF_USB_'+str(dy),box(8,9,3.3,ex-P['esp_envelope'][0]/2+3,ey+dy,P['esp_pcb_bottom']+1.6),(0.60,0.64,0.66),True)
add('REF_HX711_PCB',box(*P['hx_board'],hx,hy,P['hx_pcb_bottom']),(0.16,0.40,0.23),True)
if INDICATORS:
    add('REF_USB_internal_plug_allowance',box(*P['usb_plug_space'],*P['usb_plug_space_center']),(.45,.48,.5),True)
if not TOUCH:
    bx,by=P['boot_center_estimate']
    boot_bottom=P['esp_pcb_bottom']+P['esp_envelope'][2]
    boot_top=boot_bottom+P['boot_body_height_estimate']+P['boot_actuator_height_estimate']
    add('REF_BOOT_body_estimate',box(6,4,P['boot_body_height_estimate'],bx,by,boot_bottom),(.4,.42,.43),True)
    add('REF_BOOT_actuator_estimate',cyl(1.5,P['boot_actuator_height_estimate'],bx,by,boot_top-P['boot_actuator_height_estimate']),(.92,.68,.31),True)
    # A thin blunt plastic tool approaches diagonally above the USB sockets.
    # This is an estimated free approach path, not measured button actuation.
    boot_entry=V(*P['boot_tool_entry'])
    boot_tip=V(bx,by,boot_top+1.5)
    boot_direction=boot_tip-boot_entry
    boot_tool=Part.makeCylinder(P['boot_tool_diameter']/2,boot_direction.Length,boot_entry,boot_direction)
    add('REF_BOOT_tool_approach_estimate',boot_tool,(.92,.68,.31),True)
for i,(x,y) in enumerate(stop_positions):
    head_top=rib_bottom-P['stop_initial_gap']
    stop=stop_hardware(P,Part,cyl,hex_pocket,x,y,head_top) if INDICATORS else union([
        cyl(2.75,3,x,y,head_top-3),cyl(1.5,20,x,y,head_top-23)])
    add('REF_adjustable_stop_'+str(i),stop,(0.62,0.65,0.68),True)

# The alternate controller sits entirely on the fixed base. This is an open-back
# holder based on the manufacturer's family outline, not received-board CAD.
def touch_pose(shape):
    s=shape.copy();s.rotate(V(0,0,0),V(1,0,0),P['touch_angle_deg'])
    s.translate(V(*P['touch_origin']));return s

if TOUCH:
    tx,ty=P['touch_hole_pitch'][0]/2,P['touch_hole_pitch'][1]/2
    mounts=[(x,y) for x in (-tx,tx) for y in (-ty,ty)]
    bezel=box(*P['touch_bezel'],0,0,0)
    bezel=bezel.cut(box(*P['touch_window'],5,0,0,-1))
    bezel=union([bezel]+[cyl(4.8,3.8,x,y,-3.8) for x,y in mounts])
    bezel=cut(bezel,[s for x,y in mounts for s in (
        slot(x,y,-4,3.4,4.4,7,axis='y'),box(5.8,7.6,2.8,x,y,-.6))])
    add('touch_bezel',touch_pose(bezel),(0.18,0.31,0.31))
    profile=[(-132,0),(-87,0),(-87,40),(-78,75),(-92,83),(-132,14)]
    legs=union([yz_profile(profile,x-2,4) for x in (-tx,tx)])
    # Keep the legs behind the PCB, then add the four rear clamp pads.
    legs=legs.cut(touch_pose(box(300,300,120,0,0,-7.4)))
    supports=[touch_pose(cyl(4.8,2,x,y,-7.4)) for x,y in mounts]
    feet=[box(124,14,3,0,-120,0),box(124,8,3,0,-91,0)]
    tabs=[box(26,22,3,x,-81,38) for x in (-69,69)]
    bosses=[cyl(4.5,9,x,-90,29) for x in (-65,65)]+[box(10,6,9,x,-90,29) for x in (-61.5,61.5)]
    for sign in (-1,1):
        pts=[V(sign*x,-94.5,z) for x,z in [(58,18),(70,30),(58,30)]]
        bosses.append(Part.Face(Part.makePolygon(pts+[pts[0]])).extrude(V(0,9,0)))
    stand=union([legs]+supports+feet+bosses)
    cuts=[touch_pose(slot(x,y,-15,3.4,4.4,30,axis='y')) for x,y in mounts]
    cuts += [touch_pose(cyl(3.5,32.6,x,y,-40)) for x,y in mounts]
    # Reserve rear-side space for the edge controls, USB and connectors.
    cuts += [touch_pose(box(22,54,9,x,0,-14.4)) for x in (-tx,tx)]
    cuts += [s for x in (-65,65) for s in (cyl(1.7,12,x,-90,28),box(12,5.8,2.6,x+math.copysign(2,x),-90,30.8))]
    cuts += [cyl(6.2,.6,x,-120,0) for x in (-48,48)]
    stand=cut(stand,cuts)
    # A stepped seat keeps the removable flat bridge clear of the leg bodies.
    stand=cut(stand,[box(26.4,22.4,3.2,x,-81,38) for x in (-69,69)]+[box(148.4,6.4,3.2,0,-90,38)])
    add('touch_stand',stand,(0.25,0.43,0.42))
    bridge=union(tabs+[box(148,6,3,0,-90,38)])
    bridge=cut(bridge,[cyl(1.7,8,x,y,36) for x,y in [(-78,-76),(78,-76),(-65,-90),(65,-90)]])
    add('touch_bridge',bridge,(0.25,0.43,0.42))
    pcb=cut(box(*P['touch_board'],0,0,-5.4),[cyl(1.6,3,x,y,-6) for x,y in mounts])
    add('REF_touch_PCB',touch_pose(pcb),(0.10,0.37,0.24),True)
    add('REF_touch_glass',touch_pose(box(*P['touch_glass'],0,0,-3.8)),(0.055,0.075,0.085),True)
    add('REF_touch_active_area',touch_pose(box(95.04,53.86,.1,0,0,-.79)),(0.12,0.24,0.27),True)
    add('REF_touch_components_allowance',touch_pose(box(104,58,8,0,0,-13.4)),(.25,.27,.29),True)
    add('REF_touch_USB_allowance',touch_pose(box(26,16,8,65,4,-13.4)),(.48,.50,.52),True)
    add('REF_touch_left_controls_allowance',touch_pose(box(12,40,6,-57,0,-11.4)),(.48,.50,.52),True)

if INDICATORS:
    def clamp_envelope(x,y,seat,length,nut_z,nut_angle=30):
        shaft=cyl(1.5,length,x,y,seat+.5-length)
        head=cyl(2.75,3,x,y,seat+.5)
        washer=cyl(3.5,.5,x,y,seat).cut(cyl(1.6,1,x,y,seat-.1))
        nut=hex_pocket(x,y,nut_z,af=5.5,h=2.4).cut(cyl(1.5,3,x,y,nut_z-.1))
        nut.rotate(V(x,y,0),V(0,0,1),nut_angle)
        return Part.makeCompound([union([shaft,head]),washer,nut])

    # Nominal hardware envelopes from the previously verified clamp stacks.
    # Threads, screw drives and bought-part tolerances are intentionally absent.
    for x,y in closure_posts:
        front=y<0
        add('REF_fastener_cover_'+str(x)+'_'+str(y),
            clamp_envelope(x,y,38,12,29.4),(.55,.58,.60),True)
    for x,y in P['saddle_bolt_positions']:
        add('REF_fastener_rail_'+str(x)+'_'+str(y),clamp_envelope(x,y,54,12,44.4,0),(.55,.58,.60),True)
    guide_x=(P['reference_spool_width']+P['guide_clearance_total'])/2+P['guide_bolt_offset']
    for x in (-guide_x,guide_x):
        for y in (-15,15):
            add('REF_fastener_guide_'+str(x)+'_'+str(y),clamp_envelope(x,y,55,12,44.4),(.55,.58,.60),True)
    from nfc_mount import hardware_for_setting,verify_mounts
    for name,hardware in hardware_for_setting(P,App,Part,box,union,P['reference_spool_width'],P['nfc_height_reference']).items():
        add('REF_fastener_nfc_'+name,Part.makeCompound(hardware),(.83,.81,.71),True)
    add('REF_fastener_nfc_cable_cover',Part.makeCompound(CABLE['hardware'](P['reference_spool_width'])),(.83,.81,.71),True)

# Upright spool: flange disks, core and filament are separate references only.
def spool_envelopes(diameter,width):
    r=diameter/2
    z=apex+r/math.cos(math.radians(P['saddle_angle_deg']))
    def tube(outer,inner,length,x):
        return Part.makeCylinder(outer,length,V(x,0,z),V(1,0,0)).cut(
            Part.makeCylinder(inner,length+2,V(x-1,0,z),V(1,0,0)))
    flanges=union([tube(r,26,4,-width/2),tube(r,26,4,width/2-4)])
    core=tube(40,26,width-8,-width/2+4)
    filament=tube(r-10,40,width-10,-width/2+5)
    return union([flanges,core]),filament

spool,filament=spool_envelopes(P['reference_spool_diameter'],P['reference_spool_width'])
add('REF_upright_spool',spool,(0.29,0.31,0.32),True)
add('REF_filament',filament,(0.89,0.57,0.23),True)

# Fit samples reuse the same production geometry.
coupons={
 'fit_nfc':pod,
 'fit_esp32':base.common(box(69,41,36,ex,ey,0)).removeSplitter(),
 'fit_cell':cut(box(24,80,3,0,-30,0),[
    cyl(P['fixed_clearance_diameter']/2 if i<2 else P['moving_clearance_diameter']/2,5,0,y,-1)
    for i,y in enumerate(holes)])
}
if TOUCH:
    coupons.pop('fit_esp32')
    coupons['fit_touch_mount']=bezel.common(box(20,24,10,tx,ty,-5)).removeSplitter()
if INDICATORS:
    coupons.update(indicator_data['coupons'])


def print_orientation(name,shape):
    s=shape.copy()
    if name in ('indicator_fascia','indicator_back'):
        s.translate(V(*[-n for n in P['indicator_origin']]))
        s.rotate(V(0,0,0),V(1,0,0),-P['indicator_angle_deg'])
        if name=='indicator_fascia': s.rotate(V(0,0,0),V(1,0,0),180)
    if name in ('fit_led_face','fit_oled_face'):
        s.rotate(V(0,0,0),V(1,0,0),180)
    if name=='touch_bezel':
        s.translate(V(*[-n for n in P['touch_origin']]))
        s.rotate(V(0,0,0),V(1,0,0),180-P['touch_angle_deg'])
    if name=='fit_touch_mount':
        s.rotate(V(0,0,0),V(1,0,0),180)
    if name in ('carrier','top_cover'):
        s.rotate(V(0,0,0),V(1,0,0),180)
    if name=='side_guide':
        s.rotate(V(0,0,0),V(0,1,0),90)
    if INDICATORS and name=='nfc_pod_cover':
        s.rotate(V(0,0,0),V(1,0,0),-90)
    elif name in ('nfc_guide','nfc_pod','nfc_pod_cover','nfc_cable_cover','fit_nfc'):
        s.rotate(V(0,0,0),V(0,1,0),90 if INDICATORS and name=='nfc_cable_cover' else -90)
    if name=='rim_rail':
        s.rotate(V(0,0,0),V(1,0,0),-90)
    b=s.BoundBox
    s.translate(V(-(b.XMin+b.XMax)/2,-(b.YMin+b.YMax)/2,-b.ZMin))
    return s


doc=App.newDocument('Spoolancer_'+('v0_7_' if INDICATORS else 'v0_4_' if TOUCH else 'v0_4_1_')+VARIANT.replace('-','_'))
spread=doc.addObject('Spreadsheet::Sheet','Dimensions')
spread.set('A1','Spoolancer '+('v0.7 ' if INDICATORS else 'v0.4 ' if TOUCH else 'v0.4.1 ')+VARIANT+' — millimetres — physical fit unverified')
spread.set('A2','Edit parameters.json and rerun cad/build.py to regenerate all outputs')
for row,(key,value) in enumerate(P.items(),4):
    spread.set('A'+str(row),key)
    spread.set('B'+str(row),json.dumps(value))
spread.setColumnWidth('A',270);spread.setColumnWidth('B',270)
g=doc.addObject('App::DocumentObjectGroup','PrintedParts')
rg=doc.addObject('App::DocumentObjectGroup','ReferenceEnvelopes')
for name,s in {**PARTS,**REFS}.items():
    ob=doc.addObject('Part::Feature',name)
    ob.Label=name.replace('_',' ')
    ob.Shape=s
    ob.addProperty('App::PropertyString','Evidence','Design').Evidence='Reference envelope / verify received hardware' if name in REFS else 'Original generated prototype; edit parameters.json and regenerate'
    if getattr(ob,'ViewObject',None):
        ob.ViewObject.ShapeColor=COLORS[name]
        if name in ('REF_NFC_cable_low','REF_NFC_cable_high'):ob.ViewObject.Visibility=False
    (rg if name in REFS else g).addObject(ob)
ob=doc.addObject('Part::Feature','RimRailCopy')
ob.Label='front rim rail (same print, rotated)'
ob.Shape=front_rail
g.addObject(ob)
doc.recompute()
doc.saveAs(str(OUT/(STEM+'.FCStd')))
Part.export([o for o in g.Group+rg.Group],str(OUT/(STEM+'-assembly.step')))

report={'revision':P['revision'],'units':'mm','physical_validation':False,'parts':{},'geometry_checks':{}}
if INDICATORS:
    report['edge_finishing']={'operations':FINISHES,'all_operations_remove_material_only':True,
        'functional_exclusions':['Load-cell bearing pads and bolt seats','Rail slopes within the 100 mm spool width','Guide sliding feet and inner spool-facing planes','Nut pockets and fastener bores','Central NFC antenna face and 1.2 mm window','PCB support pads and OLED glass seat','Indicator opening throats; only front edges are beveled'],
        'coordinate_note':'Selections are recorded in each construction frame before later mirror/rotation; repeated adjustment cases reuse the same predicates.'}
    report['fastener_references']={'included':'Eight nominal M3 cover/rail/guide clamp sets, four stop screws and NFC mounting hardware',
        'not_complete':'Load-cell screws, threads, screw drives, cable ties and adhesive are not modeled',
        'physical_fit_verified':False}
scene={}
for name,s in {**PARTS,**coupons}.items():
    oriented=print_orientation(name,s)
    # Trimmed curved faces can inflate OCC's conservative BoundBox (touch stand).
    # Use tight analytic bounds to put the actual STEP solid on the print bed.
    step_solid=oriented.copy()
    step_solid.translate(V(0,0,-step_solid.optimalBoundingBox(False,False).ZMin))
    step_solid.exportStep(str(OUT/(name+'.step')))
    step_bounds=step_solid.optimalBoundingBox(False,False)
    mesh=MeshPart.meshFromShape(Shape=oriented,LinearDeflection=0.08,AngularDeflection=0.15,Relative=False)
    # OCC's analytic bounds can exceed the tessellated surface by a tolerance.
    # Place the exported mesh itself on Z=0, so slicers receive the intended pose.
    placement=App.Matrix();placement.move(V(0,0,-mesh.BoundBox.ZMin))
    mesh.transform(placement)
    mesh.write(str(OUT/(name+'.stl')))
    b=mesh.BoundBox
    report['parts'][name]={'solid_valid':s.isValid(),'solid_count':len(s.Solids),
        'closed_mesh':mesh.isSolid(),'mesh_self_intersections':mesh.hasSelfIntersections(),
        'triangle_count':mesh.CountFacets,'print_z_min_mm':round(b.ZMin,6),'volume_mm3':round(s.Volume,2),
        'print_bounds_mm':[round(b.XLength,2),round(b.YLength,2),round(b.ZLength,2)],
        'step_bounds_mm':[round(step_bounds.XLength,6),round(step_bounds.YLength,6),round(step_bounds.ZLength,6)],
        'step_z_min_mm':round(step_bounds.ZMin,6),
        'files':{'stl':name+'.stl','step':name+'.step'},
        'quantity':2 if name=='rim_rail' else 1}
for name,s in {**PARTS,**REFS,'rim_rail_front':front_rail}.items():
    verts,tris=s.tessellate(0.20)
    scene[name]={'vertices':[[v.x,v.y,v.z] for v in verts],'faces':tris,'color':COLORS.get(name,COLORS['rim_rail'])}
(PREV/'scene.json').write_text(json.dumps(scene,separators=(',',':')))

def overlap(a,b):
    return round(a.common(b).Volume,6)

assembly_parts={**PARTS,'rim_rail_front':front_rail}
report['geometry_checks']={
 'carrier_body_vertical_gap_mm':round(rib_bottom-BT,2),
 'carrier_cover_plan_gap_mm':P['cover_carrier_gap'],
 'esp_wire_allowance_to_carrier_ribs_mm':round(rib_bottom-(P['esp_pcb_bottom']+P['esp_envelope'][2]+P['esp_header_wire_height']),2),
 'esp_wire_allowance_to_relief_roof_mm':round(BT+P['esp_cover_relief_depth']-(P['esp_pcb_bottom']+P['esp_envelope'][2]+P['esp_header_wire_height']),2),
 'esp_relief_roof_thickness_mm':LT-P['esp_cover_relief_depth'],
 'nfc_patch_thickness_mm':P['nfc_wall'],
 'initial_stop_gap_mm_unvalidated':P['stop_initial_gap'],
 'cell_wire_allowance_front_wall_gap_mm':round(P['cell_wire_allowance_center'][1]-P['cell_wire_allowance'][1]/2-(CY-BD/2+W),2),
 'boot_gpio':P['boot_gpio'],
 'assembled_printed_bounds_mm':[BW,BD,max(P['nfc_mast_top'],P['nfc_height_reference']+P['nfc_pod_outer'][1]/2)],
 'maximum_adjusted_assembly_bounds_mm':[BW,BD,P['nfc_height_max']+P['nfc_pod_outer'][1]/2],
 'nfc_face_to_spool_mm':P['guide_clearance_total']/2,
 'nfc_antenna_to_spool_nominal_mm':P['guide_clearance_total']/2+P['nfc_wall']+P['nfc_air_gap'],
 'nominal_unloaded_height_with_feet_mm':max(P['nfc_mast_top'],P['nfc_height_reference']+P['nfc_pod_outer'][1]/2)+P['foot_thickness']-0.6,
 'reference_loaded_height_with_feet_mm':round(spool.BoundBox.ZMax+P['foot_thickness']-0.6,2),
 'reference_loaded_depth_mm':round(max(CY+BD/2,P['reference_spool_diameter']/2)-min(CY-BD/2,-P['reference_spool_diameter']/2),2),
}
if not TOUCH:
    report['boot_access']={
      'opening':'Existing left-side USB service opening; no extra hole or plunger',
      'opening_width_height_mm':[36,29],
      'tool_diameter_mm':P['boot_tool_diameter'],
      'entry_xyz_mm':list(P['boot_tool_entry']),
      'tip_xyz_mm':[round(boot_tip.x,2),round(boot_tip.y,2),round(boot_tip.z,2)],
      'button_center_xy_mm_estimate':P['boot_center_estimate'],
      'button_position_evidence':'Photo estimate only; verify actual board, cable and button before printing',
      'spool_removal_required_by_nominal_path':False,
      'tool_component_intersections':{name:overlap(boot_tool,shape) for name,shape in REFS.items()
          if name!='REF_BOOT_tool_approach_estimate' and overlap(boot_tool,shape)>.01},
      'physical_access_and_operation_verified':False}
    if INDICATORS:
        report['boot_access']['opening']='Dedicated angled BOOT bore in the closed USB wall; relocated board estimate'
        report['boot_access'].pop('opening_width_height_mm')
        report['boot_access']['bore_diameter_mm']=P['boot_access_hole_diameter']
report['printed_part_intersections']={}
names=list(assembly_parts)
for i,a in enumerate(names):
    for b in names[i+1:]:
        vol=overlap(assembly_parts[a],assembly_parts[b])
        if vol>0.01:
            report['printed_part_intersections'][a+' / '+b]=vol
report['component_intersections']={}
for name,shape in REFS.items():
    if ('adjustable_stop' in name and not INDICATORS) or name in ('REF_NFC_cable_low','REF_NFC_cable_high'):
        continue
    volumes={part:overlap(shape,s) for part,s in assembly_parts.items()}
    report['component_intersections'][name]={k:v for k,v in volumes.items() if v>0.01}
wires=box(P.get('esp_header_wire_length',P['esp_envelope'][0]),P['esp_envelope'][1],P['esp_header_wire_height'],ex+P.get('esp_header_wire_x_offset',0),ey,P['esp_pcb_bottom']+P['esp_envelope'][2])
report['esp_wire_envelope_intersections']={} if TOUCH else {name:overlap(wires,shape) for name,shape in assembly_parts.items() if overlap(wires,shape)>0.01}
report['spool_envelope_checks']=[]
report['pod_adjustment_checks']=[]
for diameter in [P['spool_diameter_min'],200,P['spool_diameter_max']]:
    for width in [P['spool_width_min'],80,P['spool_width_max']]:
        a,b=spool_envelopes(diameter,width)
        envelope=union([a,b])
        fixed_parts={k:v for k,v in assembly_parts.items() if k not in ('side_guide','nfc_guide','nfc_pod','nfc_pod_cover','nfc_cable_cover')}
        fixed_parts.update(side_guide=guide_for_width(width,True),nfc_guide=guide_for_width(width,nfc=True))
        if INDICATORS:fixed_parts['nfc_cable_cover']=CABLE['channel'](width)[1]
        for height in [P['nfc_height_min'],P['nfc_height_reference'],P['nfc_height_max']]:
            pod2,cover2,board2,components2,wires2=pod_for_setting(width,height)
            check_parts={**fixed_parts,'nfc_pod':pod2,'nfc_pod_cover':cover2}
            collisions={name:overlap(envelope,shape) for name,shape in check_parts.items()}
            if not TOUCH: collisions['boot_tool_approach_estimate']=overlap(envelope,boot_tool)
            report['spool_envelope_checks'].append({'diameter_mm':diameter,'width_mm':width,'pod_center_mm':height,
                'side_clearance_each_mm':P['guide_clearance_total']/2,
                'loaded_height_including_feet_mm':round(max(envelope.BoundBox.ZMax,height+P['nfc_pod_outer'][1]/2,P['nfc_mast_top'])+P['foot_thickness']-0.6,2),
                'loaded_depth_mm':max(CY+BD/2,diameter/2)-min(CY-BD/2,-diameter/2),
                'positive_volume_intersections':{k:v for k,v in collisions.items() if v>0.01}})
            if diameter==P['spool_diameter_min']:
                moving_checks={}
                for n,sh in [('pod',pod2),('cover',cover2),('pcb',board2),('components',components2),('wires',wires2)]:
                    for gn,gs in fixed_parts.items():
                        val=overlap(sh,gs)
                        if val>.01: moving_checks[n+' / '+gn]=val
                for n,sh in [('pcb',board2),('components',components2),('wires',wires2)]:
                    for pn,ps in [('pod',pod2),('cover',cover2)]:
                        val=overlap(sh,ps)
                        if val>.01: moving_checks[n+' / '+pn]=val
                report['pod_adjustment_checks'].append({'width_mm':width,'height_mm':height,'positive_volume_intersections':moving_checks})
if TOUCH or INDICATORS:
    bounds=Part.makeCompound(list(assembly_parts.values())).BoundBox
    report['geometry_checks']['assembled_printed_bounds_mm']=[round(bounds.XLength,2),round(bounds.YLength,2),round(bounds.ZLength,2)]
    report['geometry_checks']['nominal_unloaded_height_with_feet_mm']=round(bounds.ZMax+P['foot_thickness']-.6,2)
    report['geometry_checks']['maximum_adjusted_assembly_bounds_mm']=[round(bounds.XLength,2),round(bounds.YLength,2),198.4]
    if TOUCH:
        report['geometry_checks']['touch_mount_evidence']='Manufacturer family outline; exact received-board fit unverified'
        report['geometry_checks']['touch_angle_from_horizontal_deg']=P['touch_angle_deg']
    report['geometry_checks']['reference_loaded_depth_mm']=round(max(bounds.YMax,100)-min(bounds.YMin,-100),2)
    for item in report['spool_envelope_checks']:
        r=item['diameter_mm']/2
        item['loaded_depth_mm']=round(max(bounds.YMax,r)-min(bounds.YMin,-r),2)
if INDICATORS:
    report['fastener_schedule']=schedule()
    report['nfc_mounts']=verify_mounts(P,App,Part,box,union,pod_for_setting,guide_for_width,CABLE,overlap,assembly_parts,REFS)
    if not report['nfc_mounts']['digital_clearance_pass']:
        raise RuntimeError('NFC mounting/service failed: '+str(report['nfc_mounts']))
    report['lid_service']=verify_service(P,App,Part,box,union,assembly_parts,REFS,guide_for_width,pod_for_setting,CABLE,overlap)
    if any(report['lid_service']['positive_volume_intersections'].values()):
        raise RuntimeError('Lid service blocked: '+str(report['lid_service']))
    report['structural_joints']=verify_structural_joints(P,Part,box,cyl,hex_pocket,assembly_parts,REFS,overlap)
    joints=report['structural_joints']
    if any(joints['rail_removal'].values()) or any(t['positive_volume_intersections'] for t in joints['stop_checks']):
        raise RuntimeError('Structural joint clearance failed: '+str(joints))
    report['indicator_fit']=indicator_data['notes']
    insertion_checks={}
    for name,sweep in indicator_data['insertion_sweeps'].items():
        # Main lid is removed for service. Carrier, scale hardware and the
        # remaining enclosure stay in place; compare the entire 40 mm sweep.
        targets={k:s for k,s in assembly_parts.items() if k!='top_cover'}
        targets.update({k:s for k,s in REFS.items() if k in ('REF_load_cell','REF_ESP32_PCB_estimate','REF_ESP32_module_estimate','REF_HX711_PCB','REF_load_cell_wire_allowance')})
        insertion_checks[name]={k:overlap(sweep,s) for k,s in targets.items() if overlap(sweep,s)>.01}
    report['oled_insertion']={'applicable':P['oled_enabled'],'main_lid_removed':True,'continuous_vertical_lift_mm':40,'reference_envelopes_checked':list(indicator_data['insertion_sweeps']),'positive_volume_intersections':insertion_checks,'physical_insertion_verified':False}
    if any(insertion_checks.values()):raise RuntimeError('OLED vertical insertion blocked: '+str(insertion_checks))
    report['nfc_cable_route']=CABLE['verify'](P,assembly_parts,REFS,guide_for_width,pod_for_setting,overlap)
    report['power_fit']={'external_usb_openings':False,'jack_hole':'User drilled; none modeled',
        'converter_placement':'User chooses and verifies wall attachment; no reserved module volume','actual_converter_fit_verified':False,
        'internal_usb_plug_envelope_mm':P['usb_plug_space'],'actual_usb_plug_fit_verified':False,
        'pcb_edge_to_closed_inner_wall_mm':round(ex-P['esp_envelope'][0]/2-(-BW/2+W),2)}
report['cad_solid_volume_mm3_with_quantities']=round(sum(p['volume_mm3']*p['quantity'] for n,p in report['parts'].items() if not n.startswith('fit_')),2)
old_path=HERE.parent/'archive/v0.2-upright/cad/exports/validation.json'
if old_path.exists():
    old=json.loads(old_path.read_text())
    old_volume=sum(p['volume_mm3']*p['quantity'] for n,p in old['parts'].items() if not n.startswith('fit_'))
    report['comparison_to_v0_2']={'old_base_footprint_mm':[190,230],'new_base_footprint_mm':[BW,BD],
      'footprint_area_reduction_percent':round(100*(1-BW*BD/(190*230)),2),
      'old_cad_solid_volume_mm3_with_quantities':round(old_volume,2),
      'new_cad_solid_volume_mm3_with_quantities':report['cad_solid_volume_mm3_with_quantities'],
      'cad_solid_volume_reduction_percent':round(100*(1-report['cad_solid_volume_mm3_with_quantities']/old_volume),2),
      'meaning':'Analytic CAD solid volume, including print quantities; not mass or sliced filament.'}
old3_path=HERE.parent/'archive/v0.3-compact/cad/exports/validation.json'
if old3_path.exists():
    old3=json.loads(old3_path.read_text())
    old3_volume=sum(p['volume_mm3']*p['quantity'] for n,p in old3['parts'].items() if not n.startswith('fit_'))
    report['comparison_to_v0_3']={'old_base_footprint_mm':[170,190],'new_base_footprint_mm':[BW,BD],
      'footprint_area_reduction_percent':round(100*(1-BW*BD/(170*190)),2),
      'old_cad_solid_volume_mm3_with_quantities':round(old3_volume,2),
      'new_cad_solid_volume_mm3_with_quantities':report['cad_solid_volume_mm3_with_quantities'],
      'cad_solid_volume_reduction_percent':round(100*(1-report['cad_solid_volume_mm3_with_quantities']/old3_volume),2),
      'meaning':'Analytic CAD solid volume including quantities; not grams or sliced filament.'}
    report['shared_parts_vs_v0_3']={n:{'quantity':report['parts'][n]['quantity'],
       'analytic_volume_unchanged':abs(report['parts'][n]['volume_mm3']-old3['parts'][n]['volume_mm3'])<.01,
       'print_bounds_unchanged':report['parts'][n]['print_bounds_mm']==old3['parts'][n]['print_bounds_mm']}
       for n in ['carrier','rim_rail','side_guide','nfc_guide','nfc_pod','nfc_pod_cover']}
(OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
if any(not p['solid_valid'] or p['solid_count']!=1 or not p['closed_mesh'] or p['mesh_self_intersections'] for p in report['parts'].values()):
    raise RuntimeError('A printable shape failed geometry validation')
if any(v for v in report['component_intersections'].values()) or report['printed_part_intersections'] or report['esp_wire_envelope_intersections']:
    raise RuntimeError('Assembly intersects; inspect validation.json')
if any(v['positive_volume_intersections'] for v in report['spool_envelope_checks']+report['pod_adjustment_checks']):
    raise RuntimeError('A spool envelope intersects; inspect validation.json')
if INDICATORS and not report['nfc_cable_route']['digital_clearance_pass']:
    raise RuntimeError('NFC cable route clearance failed; inspect validation.json')
if not TOUCH:
    if report['boot_access']['tool_component_intersections']:
        raise RuntimeError('BOOT approach intersects a modeled component; inspect validation.json')
    # Remove only explicitly superseded outputs owned by this generator.
    for name in (['indicator_fascia.stl','indicator_fascia.step','indicator_back.stl','indicator_back.step','spoolancer-v0.6-'+VARIANT+'.FCStd','spoolancer-v0.6-'+VARIANT+'-assembly.step'] if INDICATORS else [])+['button_cap.stl','fit_button.stl','button_cap.step','fit_button.step','spoolancer-v0.4-headless.FCStd','spoolancer-v0.4-headless-assembly.step']:
        (OUT/name).unlink(missing_ok=True)
App.closeDocument(doc.Name)
