"""Spoolancer v0.3: original parametric FreeCAD geometry, millimetres.

Run with FreeCAD's Python environment; see cad/README.md.
Edit parameters.json and regenerate; hardware bodies are clearance references,
not manufacturer CAD. No third-party model is incorporated in the exports.
"""
import json
import math
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
if sys.platform == "darwin":
    sys.path.append("/Applications/FreeCAD.app/Contents/Resources/lib")
import FreeCAD as App
import Part
import MeshPart

P = json.loads((HERE / "parameters.json").read_text())
OUT = HERE / "exports"
OUT.mkdir(exist_ok=True)
V = App.Vector
PARTS = {}
REFS = {}
COLORS = {}


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
front_face=rounded_rectangle(BW,BD,P['base_corner_radius'],cy=CY)
inner_face=rounded_rectangle(BW-2*W,BD-2*W,P['base_corner_radius']-W,cy=CY)
base=extrusion(front_face,BT).cut(extrusion(inner_face,BT,P['floor']))
holes=[P['cell_front_y']+o for o in P['cell_hole_offsets']]
fixed_y=(holes[0]+holes[1])/2
moving_y=(holes[2]+holes[3])/2
base_add=[box(24,28,P['cell_bottom']-P['floor'],0,fixed_y,P['floor'])]
# Ribs spread load from the fixed pad into the floor, below the sensor.
base_add += [box(134,4,5,0,fixed_y,P['floor']),
             box(4,70,5,0,fixed_y-15,P['floor'])]
lid_posts=P['lid_posts']
stop_positions=P['stop_positions']
for x,y in lid_posts:
    base_add.append(cyl(6,BT-P['floor'],x,y,P['floor']))
for x in (27,53):
    base_add.append(cyl(3,BT-P['floor'],x,-111,P['floor']))
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
for x,y in lid_posts:
    base_cut += [cyl(1.7,BT,x,y,4),box(14,5.8,2.6,x-math.copysign(3,x),y,BT-5.6)]
for x,y in stop_positions:
    base_cut += [cyl(1.7,35,x,y,3),hex_pocket(x,y,P['stop_post_top']-P['stop_nut_height'],h=P['stop_nut_height'])]
# A broad side opening fits either USB-C port and typical cable overmoulds.
base_cut.append(box(12,36,BT-6,-BW/2,ey,7))
tie_locations=[(ex-18,ey,ew+9),(ex+17,ey,ew+9),(hx,hy,hw+9)]
for x,y,span in tie_locations:
    for side in (-1,1):
        base_cut.append(box(3.2,5,10,x,y+side*span/2,-1))
    base_cut.append(box(3.2,span+5,1.4,x,y,0))
feet=P['feet_positions']
for x,y in feet:
    base_cut.append(cyl(6.2,0.6,x,y,0))
base=cut(base,base_cut)
add('base',base,(0.15,0.20,0.23))

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
carrier=cut(carrier,carrier_cut)
add('carrier',carrier,(0.67,0.77,0.74))

slope=math.tan(math.radians(P['saddle_angle_deg']))
apex=P['saddle_apex_z'];half_depth=P['saddle_depth']/2
surface=lambda y:apex+abs(y)*slope
ri,ro=P['rim_inner_y'],P['rim_outer_y']
rail=yz_profile([(ri,50),(ro,50),(ro,surface(ro)),(ri,surface(ri))],-cw/2,cw)
rail_cut=[]
# Open bays between webs; roof and bottom flange remain continuous.
for x0,x1 in [(-58,-34),(-30,-2.5),(2.5,30),(34,58)]:
    rail_cut.append(yz_profile([(ri-1,53),(ro+1,53),
        (ro+1,surface(ro+1)-P['rim_roof']),(ri-1,surface(ri-1)-P['rim_roof'])],x0,x1-x0))
for x in (-64,64):
    rail_cut += [cyl(1.7,50,x,60,49),cyl(4,50,x,60,54)]
for x in (-61,61):
    rail_cut.append(cyl(1.75,50,x,52,49))
rail=cut(rail,rail_cut)
add('rim_rail',rail,(0.77,0.84,0.82))
front_rail=rail.copy();front_rail.rotate(V(0,0,0),V(0,0,1),180)


def x_slot(x,y,z,diameter,length,depth):
    delta=(length-diameter)/2
    return union([Part.makeCylinder(diameter/2,depth,V(x,y,z-delta),V(1,0,0)),
                  Part.makeCylinder(diameter/2,depth,V(x,y,z+delta),V(1,0,0)),
                  box(depth,diameter,2*delta,x+depth/2,y,z-delta)])


def guide_for_width(width,left=False,nfc=False):
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
    if left: guide.rotate(V(0,0,0),V(0,0,1),180)
    return guide

add('side_guide',guide_for_width(P['reference_spool_width'],True),(0.25,0.43,0.42))
add('nfc_guide',guide_for_width(P['reference_spool_width'],nfc=True),(0.25,0.43,0.42))


def x_cylinder(radius,length,x,y,z):
    return Part.makeCylinder(radius,length,V(x,y,z),V(1,0,0))


def pod_for_setting(width,height):
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
    cuts.append(box(10,12,5,inner+9,0,height-oz/2-1))
    pod=cut(pod,cuts)
    cover=box(P['nfc_back_thickness'],oy,oz,inner+depth+P['nfc_back_thickness']/2,0,height-oz/2)
    cover=cut(cover,[x_cylinder(1.35,4,inner+depth-1,y,height+dz) for y,dz in P['nfc_cover_screw_positions']])
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

# Full fixed cover, with a 2 mm plan gap around the floating carrier.
lid=extrusion(front_face,LT,BT).cut(extrusion(rounded_rectangle(
    cw+2*P['cover_carrier_gap'],cd+2*P['cover_carrier_gap'],6),LT+2,BT-1))
# The USB-side GPIO wires extend under the fixed cover, outside its aperture.
lid=lid.cut(box(20,36,P['esp_cover_relief_depth']+.1,-79,ey,BT-.1))
sx,sy=P['screen_center']; screen_holes=[]
for dx in (-P['screen_hole_pitch'][0]/2,P['screen_hole_pitch'][0]/2):
    for dy in (-P['screen_hole_pitch'][1]/2,P['screen_hole_pitch'][1]/2):
        screen_holes.append((sx+dx,sy+dy))
lid_add=[]
for x,y in screen_holes:
    lid_add.append(cyl(2.0,BT-P['screen_pcb_top'],x,y,P['screen_pcb_top']))
bx,by=P['button_center']
button_mounts=[(bx-5,by-19.0),(bx+5,by+19.0)]
for x,y in button_mounts:
    lid_add.append(cyl(2.6,BT-P['button_pcb_top'],x,y,P['button_pcb_top']))
lid=union([lid]+lid_add)
lid_cut=[]
for x,y in lid_posts:
    lid_cut.append(cyl(1.7,12,x,y,BT-5))
for x,y in screen_holes:
    lid_cut.append(cyl(1.25,20,x,y,25))
wx,wy=P['screen_window']; ox,oy=P['screen_window_offset']
lid_cut.append(box(wx,wy,BT-25+0.1,sx+ox,sy+oy,25))
# A 45-degree flare in the viewing direction exposes the recessed screen
# from above/front while retaining full webs around the side screw holes.
window_sections=[]
for z,depth in [(BT,wy),(BT+LT+0.1,wy+2*(LT+0.1))]:
    corners=[V(sx+ox+dx,sy+oy+dy,z) for dx,dy in [(-wx/2,-depth/2),(wx/2,-depth/2),(wx/2,depth/2),(-wx/2,depth/2)]]
    window_sections.append(Part.makePolygon(corners+[corners[0]]))
lid_cut.append(Part.makeLoft(window_sections,True,False))
for x,y in button_mounts:
    lid_cut.append(cyl(0.85,5.5,x,y,P['button_pcb_top']-0.1))
button_positions=[(bx,by+dy) for dy in (-P['button_pitch'],0,P['button_pitch'])]
for x,y in button_positions:
    lid_cut.append(box(7.4,7.4,15,x,y,28))
lid=cut(lid,lid_cut)
add('top_cover',lid,(0.25,0.43,0.42))

# Optional push-on cap, not a captive plunger. Print head down; fit depends
# on the actual actuator diameter and travel. Native buttons also work bare.
cap_rest=P['button_pcb_top']+P['button_height']
cap_head_bottom=BT+LT+P['button_cap_stop_gap']
cap=union([cyl(5,1.5,bx,by,cap_head_bottom),
           cyl(3.4,cap_head_bottom-(cap_rest-1.9),bx,by,cap_rest-1.9)])
cap=cap.cut(cyl(P['button_cap_bore']/2,2.0,bx,by,cap_rest-2.0))
add('button_cap',cap,(0.92,0.68,0.31))

# Reference component envelopes: communicate placement without pretending to
# represent manufacturer CAD, connectors, all components, or real wiring.
cell=box(P['cell_width'],P['cell_length'],P['cell_height'],0,P['cell_front_y']+P['cell_length']/2,P['cell_bottom'])
cell=cut(cell,[cyl(2.5 if i<2 else 2,20,0,y,P['cell_bottom']-1) for i,y in enumerate(holes)])
add('REF_load_cell',cell,(0.66,0.69,0.72),True)
screen=box(*P['screen_board'],sx,sy,P['screen_pcb_top']-P['screen_board'][2])
screen=cut(screen,[cyl(1.1,5,x,y,P['screen_pcb_top']-3) for x,y in screen_holes])
add('REF_screen_PCB',screen,(0.08,0.28,0.53),True)
add('REF_screen_glass',box(45,33,2,sx+ox,sy+oy,P['screen_pcb_top']), (0.025,0.06,0.10),True)
add('REF_screen_connector_allowance',box(7,22,8,sx-23,sy,P['screen_pcb_top']-P['screen_board'][2]-8),(0.9,0.87,0.73),True)
esp=box(*P['esp_envelope'],ex,ey,P['esp_pcb_bottom'])
add('REF_ESP32_PCB_estimate',esp,(0.08,0.10,0.11),True)
add('REF_ESP32_module_estimate',box(26,18,3.5,ex+14,ey,P['esp_pcb_bottom']+1.6),(0.60,0.64,0.66),True)
for dy in (-6,6):
    add('REF_USB_'+str(dy),box(8,9,3.3,ex-P['esp_envelope'][0]/2+3,ey+dy,P['esp_pcb_bottom']+1.6),(0.60,0.64,0.66),True)
add('REF_HX711_PCB',box(*P['hx_board'],hx,hy,P['hx_pcb_bottom']),(0.16,0.40,0.23),True)
buttonpcb=box(*P['button_board'],bx,by,P['button_pcb_top']-P['button_board'][2])
buttonpcb=cut(buttonpcb,[cyl(1.1,4,x,y,P['button_pcb_top']-3) for x,y in button_mounts])
add('REF_button_perfboard',buttonpcb,(0.61,0.39,0.18),True)
for i,(x,y) in enumerate(button_positions):
    add('REF_switch_'+str(i),union([box(6,6,3.5,x,y,P['button_pcb_top']),
        cyl(1.65,P['button_height']-3.5,x,y,P['button_pcb_top']+3.5)]),(0.19,0.20,0.20),True)
for i,(x,y) in enumerate(stop_positions):
    head_top=rib_bottom-P['stop_initial_gap']
    add('REF_adjustable_stop_'+str(i),union([cyl(2.75,3,x,y,head_top-3),
        cyl(1.5,20,x,y,head_top-23)]),(0.62,0.65,0.68),True)

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
 'fit_display':lid.common(box(68,44,20,sx,sy,25)).removeSplitter(),
 'fit_nfc':pod,
 'fit_buttons':lid.common(box(20,47,20,bx,by,25)).removeSplitter(),
 'fit_esp32':base.common(box(69,41,36,ex,ey,0)).removeSplitter(),
 'fit_cell':cut(box(24,80,3,0,-30,0),[
    cyl(P['fixed_clearance_diameter']/2 if i<2 else P['moving_clearance_diameter']/2,5,0,y,-1)
    for i,y in enumerate(holes)])
}


def print_orientation(name,shape):
    s=shape.copy()
    if name in ('carrier','top_cover','button_cap','fit_display','fit_buttons'):
        s.rotate(V(0,0,0),V(1,0,0),180)
    if name=='side_guide':
        s.rotate(V(0,0,0),V(0,1,0),90)
    if name in ('nfc_guide','nfc_pod','nfc_pod_cover','fit_nfc'):
        s.rotate(V(0,0,0),V(0,1,0),-90)
    if name=='rim_rail':
        s.rotate(V(0,0,0),V(1,0,0),-90)
    b=s.BoundBox
    s.translate(V(-(b.XMin+b.XMax)/2,-(b.YMin+b.YMax)/2,-b.ZMin))
    return s


doc=App.newDocument('Spoolancer_v0_3')
spread=doc.addObject('Spreadsheet::Sheet','Dimensions')
spread.set('A1','Spoolancer v0.3 — millimetres — physical fit unverified')
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
    (rg if name in REFS else g).addObject(ob)
for index,dy in enumerate((-P['button_pitch'],P['button_pitch'])):
    ob=doc.addObject('Part::Feature','ButtonCapCopy'+str(index+1))
    ob.Label='button cap '+str(index+2)+' (optional)'
    shape=PARTS['button_cap'].copy()
    shape.translate(V(0,dy,0))
    ob.Shape=shape
    g.addObject(ob)
ob=doc.addObject('Part::Feature','RimRailCopy')
ob.Label='front rim rail (same print, rotated)'
ob.Shape=front_rail
g.addObject(ob)
doc.recompute()
doc.saveAs(str(OUT/'spoolancer-v0.3.FCStd'))
Part.export([o for o in g.Group+rg.Group],str(OUT/'spoolancer-v0.3-assembly.step'))

report={'revision':P['revision'],'units':'mm','physical_validation':False,'parts':{},'geometry_checks':{}}
scene={}
for name,s in {**PARTS,**coupons}.items():
    oriented=print_orientation(name,s)
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
        'quantity':3 if name=='button_cap' else (2 if name=='rim_rail' else 1)}
for name,s in {**PARTS,**REFS,'rim_rail_front':front_rail}.items():
    verts,tris=s.tessellate(0.20)
    scene[name]={'vertices':[[v.x,v.y,v.z] for v in verts],'faces':tris,'color':COLORS.get(name,COLORS['rim_rail'])}
(HERE/'previews'/'scene.json').write_text(json.dumps(scene,separators=(',',':')))

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
 'assembled_printed_bounds_mm':[BW,BD,max(P['nfc_mast_top'],P['nfc_height_reference']+P['nfc_pod_outer'][1]/2)],
 'maximum_adjusted_assembly_bounds_mm':[BW,BD,P['nfc_height_max']+P['nfc_pod_outer'][1]/2],
 'nfc_face_to_spool_mm':P['guide_clearance_total']/2,
 'nfc_antenna_to_spool_nominal_mm':P['guide_clearance_total']/2+P['nfc_wall']+P['nfc_air_gap'],
 'nominal_unloaded_height_with_feet_mm':max(P['nfc_mast_top'],P['nfc_height_reference']+P['nfc_pod_outer'][1]/2)+P['foot_thickness']-0.6,
 'reference_loaded_height_with_feet_mm':round(spool.BoundBox.ZMax+P['foot_thickness']-0.6,2),
 'reference_loaded_depth_mm':round(max(CY+BD/2,P['reference_spool_diameter']/2)-(CY-BD/2),2),
}
report['printed_part_intersections']={}
names=list(assembly_parts)
for i,a in enumerate(names):
    for b in names[i+1:]:
        vol=overlap(assembly_parts[a],assembly_parts[b])
        if vol>0.01:
            report['printed_part_intersections'][a+' / '+b]=vol
report['component_intersections']={}
for name,shape in REFS.items():
    if 'adjustable_stop' in name:
        continue
    volumes={part:overlap(shape,s) for part,s in assembly_parts.items() if part!='button_cap'}
    report['component_intersections'][name]={k:v for k,v in volumes.items() if v>0.01}
wires=box(P['esp_envelope'][0],P['esp_envelope'][1],P['esp_header_wire_height'],ex,ey,P['esp_pcb_bottom']+P['esp_envelope'][2])
report['esp_wire_envelope_intersections']={name:overlap(wires,shape) for name,shape in assembly_parts.items() if overlap(wires,shape)>0.01}
report['spool_envelope_checks']=[]
report['pod_adjustment_checks']=[]
for diameter in [P['spool_diameter_min'],200,P['spool_diameter_max']]:
    for width in [P['spool_width_min'],80,P['spool_width_max']]:
        a,b=spool_envelopes(diameter,width)
        envelope=union([a,b])
        fixed_parts={k:v for k,v in assembly_parts.items() if k not in ('side_guide','nfc_guide','nfc_pod','nfc_pod_cover')}
        fixed_parts.update(side_guide=guide_for_width(width,True),nfc_guide=guide_for_width(width,nfc=True))
        for height in [P['nfc_height_min'],P['nfc_height_reference'],P['nfc_height_max']]:
            pod2,cover2,board2,components2,wires2=pod_for_setting(width,height)
            check_parts={**fixed_parts,'nfc_pod':pod2,'nfc_pod_cover':cover2}
            collisions={name:overlap(envelope,shape) for name,shape in check_parts.items()}
            report['spool_envelope_checks'].append({'diameter_mm':diameter,'width_mm':width,'pod_center_mm':height,
                'side_clearance_each_mm':P['guide_clearance_total']/2,
                'loaded_height_including_feet_mm':round(max(envelope.BoundBox.ZMax,height+P['nfc_pod_outer'][1]/2,P['nfc_mast_top'])+P['foot_thickness']-0.6,2),
                'loaded_depth_mm':max(CY+BD/2,diameter/2)-(CY-BD/2),
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
(OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
if any(not p['solid_valid'] or p['solid_count']!=1 or not p['closed_mesh'] or p['mesh_self_intersections'] for p in report['parts'].values()):
    raise RuntimeError('A printable shape failed geometry validation')
if any(v for v in report['component_intersections'].values()) or report['printed_part_intersections'] or report['esp_wire_envelope_intersections']:
    raise RuntimeError('Assembly intersects; inspect validation.json')
if any(v['positive_volume_intersections'] for v in report['spool_envelope_checks']+report['pod_adjustment_checks']):
    raise RuntimeError('A spool envelope intersects; inspect validation.json')
App.closeDocument(doc.Name)
