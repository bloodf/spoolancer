"""Spoolancer v0.1: original parametric FreeCAD geometry, millimetres.

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


def d_outline(radius, front, corner):
    # A semicircular rear and a rectangular front with rounded front corners.
    q = math.sqrt(0.5)
    a,b,c,d,e,f = [V(*p) for p in [
        (-radius,0,0), (-radius,front+corner,0),
        (-radius+corner,front,0),(radius-corner,front,0),
        (radius,front+corner,0),(radius,0,0)]]
    edges = [Part.makeLine(a,b),
        Part.Arc(b,V(-radius+corner-corner*q,front+corner-corner*q,0),c).toShape(),
        Part.makeLine(c,d),
        Part.Arc(d,V(radius-corner+corner*q,front+corner-corner*q,0),e).toShape(),
        Part.makeLine(e,f),Part.Arc(f,V(0,radius,0),a).toShape()]
    return Part.Face(Part.Wire(edges))


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


R,F,W=P['base_radius'],P['base_front_y'],P['wall']
BT,LT=P['body_top'],P['lid_thickness']
front_face=d_outline(R,F,P['front_corner_radius'])
inner_face=d_outline(R-W,F+W,P['front_corner_radius']-W)
base=extrusion(front_face,BT).cut(extrusion(inner_face,BT,P['floor']))
holes=[P['cell_front_y']+o for o in P['cell_hole_offsets']]
fixed_y=(holes[0]+holes[1])/2
moving_y=(holes[2]+holes[3])/2
base_add=[box(24,28,P['cell_bottom']-P['floor'],0,fixed_y,P['floor'])]
# Ribs spread load from the fixed pad into the floor, below the sensor.
base_add += [box(146,4,5,0,fixed_y,P['floor']),
             box(4,84,5,0,fixed_y-22,P['floor'])]
lid_posts=[(-91,F+9),(91,F+9),(-91,P['lid_rear_posts_y']),(91,P['lid_rear_posts_y'])]
stop_positions=[(x*P['stop_xy'],y*P['stop_xy']) for x in (-1,1) for y in (-1,1)]
for x,y in lid_posts:
    base_add.append(cyl(6,BT-P['floor'],x,y,P['floor']))
for x in (-14,14):
    base_add.append(cyl(3,BT-P['floor'],x,-138,P['floor']))
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
    base_cut += [cyl(1.7,BT,x,y,4),hex_pocket(x,y,BT-2.6)]
for x,y in stop_positions:
    base_cut += [cyl(1.7,35,x,y,3),hex_pocket(x,y,P['stop_post_top']-P['stop_nut_height'],h=P['stop_nut_height'])]
# A broad side opening fits either USB-C port and typical cable overmoulds.
base_cut.append(box(12,36,BT-6,-99,ey,7))
tie_locations=[(ex-18,ey,ew+9),(ex+17,ey,ew+9),(hx,hy,hw+9)]
for x,y,span in tie_locations:
    for side in (-1,1):
        base_cut.append(box(3.2,5,10,x,y+side*span/2,-1))
    base_cut.append(box(3.2,span+5,1.4,x,y,0))
feet=[(-70,-124),(70,-124),(-55,72),(55,72)]
for x,y in feet:
    base_cut.append(cyl(6.2,0.6,x,y,0))
base=cut(base,base_cut)
add('base',base,(0.15,0.20,0.23))

# Floating tray: one printed piece, top down on the bed.
tray_bottom=P['tray_top']-P['tray_plate']
rib_bottom=tray_bottom-P['tray_rib_height']
tray_pieces=[cyl(P['tray_diameter']/2,P['tray_plate'],z=tray_bottom),
             cyl(P['tray_diameter']/2,P['tray_rib_height'],z=rib_bottom).cut(
                 cyl(P['tray_diameter']/2-2.0,P['tray_rib_height']+2,z=rib_bottom-1)),
             box(24,27,tray_bottom-P['cell_bottom']-P['cell_height'],
                 0,moving_y,P['cell_bottom']+P['cell_height'])]
for angle in range(0,180,45):
    rib=box(P['tray_diameter']-4,P['tray_rib_width'],P['tray_rib_height'],0,0,rib_bottom)
    rib.rotate(V(0,0,0),V(0,0,1),angle)
    tray_pieces.append(rib)
for x,y in stop_positions:
    tray_pieces.append(cyl(6,P['tray_rib_height'],x,y,rib_bottom))
tray=union(tray_pieces)
tray_cut=[]
for y in holes[2:]:
    tray_cut += [cyl(P['moving_clearance_diameter']/2,60,0,y,0),
                 cyl(5.0,20,0,y,P['moving_head_seat_z'])]
for x,y in stop_positions:
    tray_cut.append(cyl(1.75,30,x,y,rib_bottom-1))
tray=cut(tray,tray_cut)
add('tray',tray,(0.77,0.84,0.82))

# Front panel; print exterior face down. All bosses grow upward when printed.
lid=extrusion(front_face,LT,BT).common(box(2*R+2,P['lid_back_y']-F,LT+2,0,(P['lid_back_y']+F)/2,BT-1))
lid=lid.cut(cyl(P['tray_diameter']/2+P['lid_tray_clearance'],LT+2,z=BT-1))
sx,sy=P['screen_center']; screen_holes=[]
for dx in (-P['screen_hole_pitch'][0]/2,P['screen_hole_pitch'][0]/2):
    for dy in (-P['screen_hole_pitch'][1]/2,P['screen_hole_pitch'][1]/2):
        screen_holes.append((sx+dx,sy+dy))
lid_add=[]
for x,y in screen_holes:
    lid_add.append(cyl(2.0,BT-P['screen_pcb_top'],x,y,P['screen_pcb_top']))
nx,ny=P['nfc_center']
nfc_face_z=BT+LT-P['nfc_wall']-P['nfc_air_gap']
# The scan patch is thinned from the underside. Four possible hole locations
# accept either diagonal; only the matching pair on the actual board is used.
nfc_holes=[(nx+dx,ny+dy) for dx in (-P['nfc_hole_pitch'][0]/2,P['nfc_hole_pitch'][0]/2)
                           for dy in (-P['nfc_hole_pitch'][1]/2,P['nfc_hole_pitch'][1]/2)]
lid=lid.cut(box(45,42.8,10,nx,ny,BT+LT-P['nfc_wall']-10))
for x,y in nfc_holes:
    lid_add.append(cyl(3.2,BT+LT-P['nfc_wall']-nfc_face_z,x,y,nfc_face_z))
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
for x,y in nfc_holes:
    lid_cut.append(slot(x,y,28,2.8,4.8,20,axis='x'))
for x,y in button_mounts:
    lid_cut.append(cyl(0.85,5.5,x,y,P['button_pcb_top']-0.1))
button_positions=[(bx,by+dy) for dy in (-P['button_pitch'],0,P['button_pitch'])]
for x,y in button_positions:
    lid_cut.append(box(7.4,7.4,15,x,y,28))
lid=cut(lid,lid_cut)
add('front_panel',lid,(0.25,0.43,0.42))

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
nfc=box(*P['nfc_board'],nx,ny,nfc_face_z-P['nfc_board'][2])
nfc=cut(nfc,[cyl(1.524,5,x,y,nfc_face_z-3) for x,y in [nfc_holes[0],nfc_holes[3]]])
add('REF_PN532_PCB',nfc,(0.71,0.16,0.13),True)
add('REF_PN532_components',box(28,22,2.4,nx,ny,nfc_face_z-P['nfc_board'][2]-2.4),(0.18,0.18,0.18),True)
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

# Fit samples reuse the same production geometry.
coupons={
 'fit_display':lid.common(box(68,44,20,sx,sy,25)).removeSplitter(),
 'fit_nfc':lid.common(box(53,47,20,nx,ny,25)).removeSplitter(),
 'fit_buttons':lid.common(box(20,47,20,bx,by,25)).removeSplitter(),
 'fit_esp32':base.common(box(69,41,36,-66.5,ey,0)).removeSplitter(),
 'fit_cell':cut(box(24,80,3,0,-30,0),[
    cyl(P['fixed_clearance_diameter']/2 if i<2 else P['moving_clearance_diameter']/2,5,0,y,-1)
    for i,y in enumerate(holes)])
}


def print_orientation(name,shape):
    s=shape.copy()
    if name in ('tray','front_panel','button_cap','fit_display','fit_nfc','fit_buttons'):
        s.rotate(V(0,0,0),V(1,0,0),180)
    b=s.BoundBox
    s.translate(V(-(b.XMin+b.XMax)/2,-(b.YMin+b.YMax)/2,-b.ZMin))
    return s


doc=App.newDocument('Spoolancer_v0_1')
spread=doc.addObject('Spreadsheet::Sheet','Dimensions')
spread.set('A1','Spoolancer v0.1 — millimetres — physical fit unverified')
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
doc.recompute()
doc.saveAs(str(OUT/'spoolancer-v0.1.FCStd'))
Part.export([o for o in g.Group+rg.Group],str(OUT/'spoolancer-v0.1-assembly.step'))

report={'revision':P['revision'],'units':'mm','physical_validation':False,'parts':{},'geometry_checks':{}}
scene={}
for name,s in {**PARTS,**coupons}.items():
    oriented=print_orientation(name,s)
    mesh=MeshPart.meshFromShape(Shape=oriented,LinearDeflection=0.08,AngularDeflection=0.15,Relative=False)
    mesh.write(str(OUT/(name+'.stl')))
    b=oriented.BoundBox
    report['parts'][name]={'solid_valid':s.isValid(),'solid_count':len(s.Solids),
        'closed_mesh':mesh.isSolid(),'mesh_self_intersections':mesh.hasSelfIntersections(),
        'triangle_count':mesh.CountFacets,'volume_mm3':round(s.Volume,2),
        'print_bounds_mm':[round(b.XLength,2),round(b.YLength,2),round(b.ZLength,2)],
        'quantity':3 if name=='button_cap' else 1}
for name,s in {**PARTS,**REFS}.items():
    verts,tris=s.tessellate(0.20)
    scene[name]={'vertices':[[v.x,v.y,v.z] for v in verts],'faces':tris,'color':COLORS[name]}
(HERE/'previews'/'scene.json').write_text(json.dumps(scene,separators=(',',':')))

def overlap(a,b):
    return round(a.common(b).Volume,6)

report['geometry_checks']={
 'base_tray_overlap_mm3':overlap(base,tray),
 'base_front_panel_overlap_mm3':overlap(base,lid),
 'front_panel_tray_overlap_mm3':overlap(lid,tray),
 'base_load_cell_overlap_mm3':overlap(base,cell),
 'tray_load_cell_overlap_mm3':overlap(tray,cell),
 'tray_body_vertical_gap_mm':round(rib_bottom-BT,2),
 'tray_front_panel_plan_gap_mm':P['lid_tray_clearance'],
 'esp_wire_allowance_to_tray_ribs_mm':round(rib_bottom-(P['esp_pcb_bottom']+P['esp_envelope'][2]+P['esp_header_wire_height']),2),
 'nfc_patch_thickness_mm':P['nfc_wall'],
 'initial_stop_gap_mm_unvalidated':P['stop_initial_gap'],
 'assembled_printed_bounds_mm':[2*R,R-F,P['tray_top']],
 'nominal_total_height_with_feet_mm':P['tray_top']+P['foot_thickness']-0.6,
}
report['component_intersections']={}
for name,shape in REFS.items():
    # Stop shanks intentionally occupy nut/post holes. Other positive volumes
    # flag a dimension error in our simplified reference envelopes.
    if 'adjustable_stop' in name:
        continue
    volumes={part:overlap(shape,s) for part,s in PARTS.items() if part!='button_cap'}
    report['component_intersections'][name]={k:v for k,v in volumes.items() if v>0.01}
(OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
if any(not p['solid_valid'] or p['solid_count']!=1 or not p['closed_mesh'] or p['mesh_self_intersections'] for p in report['parts'].values()):
    raise RuntimeError('A printable shape failed geometry validation')
if any(v for v in report['component_intersections'].values()):
    raise RuntimeError('Reference component intersects a printed part; inspect validation.json')
if any(value>0.01 for key,value in report['geometry_checks'].items() if key.endswith('_overlap_mm3')):
    raise RuntimeError('Printed parts or the load cell overlap; inspect validation.json')
App.closeDocument(doc.Name)
