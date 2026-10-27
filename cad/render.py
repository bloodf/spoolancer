"""Render the exported CAD tessellation, not an AI-generated concept image."""
import argparse
import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import vtk
from vtk.util.numpy_support import numpy_to_vtk, vtk_to_numpy

HERE=Path(__file__).resolve().parent
parser=argparse.ArgumentParser()
parser.add_argument('--variant',choices=['headless','touch-8048','headless-led','oled-led'])
ARGS=parser.parse_args()
VARIANT=ARGS.variant or 'headless'
TARGET=HERE/'variants'/VARIANT if ARGS.variant else HERE
PREV=TARGET/'previews'
S=json.loads((PREV/'scene.json').read_text())
P=json.loads((HERE/'parameters.json').read_text())
if VARIANT!='headless': P.update(json.loads((TARGET/'parameters.json').read_text()))
BG='#f4f3ee'; INK='#213b3b'; MUTED='#5e706d'
plt.rcParams.update({'font.family':'DejaVu Sans','text.color':INK})


def geometry(ax,name,shift=(0,0,0),alpha=1,flip=False):
    data=S[name]; vertices=np.array(data['vertices'],dtype=float)
    if flip:
        vertices[:,1:3]*=-1
    vertices+=np.array(shift)
    points=vtk.vtkPoints();points.SetData(numpy_to_vtk(vertices,deep=True))
    cells=vtk.vtkCellArray()
    for face in data['faces']:
        cells.InsertNextCell(3)
        for index in face:
            cells.InsertCellPoint(int(index))
    poly=vtk.vtkPolyData();poly.SetPoints(points);poly.SetPolys(cells)
    normals=vtk.vtkPolyDataNormals();normals.SetInputData(poly)
    normals.ConsistencyOn();normals.AutoOrientNormalsOn();normals.SplittingOn();normals.SetFeatureAngle(45)
    mapper=vtk.vtkPolyDataMapper();mapper.SetInputConnection(normals.GetOutputPort())
    actor=vtk.vtkActor();actor.SetMapper(mapper)
    material=actor.GetProperty();material.SetColor(*data['color']);material.SetOpacity(alpha)
    material.SetAmbient(.30);material.SetDiffuse(.70);material.SetSpecular(.18);material.SetSpecularPower(32)
    ax._actors.append(actor)


def setup(fig,rect,zmax=65):
    ax=fig.add_axes(rect)
    ax.set_facecolor(BG);ax.set_axis_off()
    ax._actors=[];ax._zmax=zmax;ax._view=np.array([-340,-330,300]);ax._scale=None;ax._center=None
    return ax


def finish(ax):
    renderer=vtk.vtkRenderer();renderer.SetBackground(244/255,243/255,238/255)
    for actor in ax._actors:
        renderer.AddActor(actor)
    camera=renderer.GetActiveCamera();camera.ParallelProjectionOn()
    center=np.array([0,-24,ax._zmax*.5]) if ax._center is None else np.array(ax._center)
    camera.SetFocalPoint(*center);camera.SetPosition(*(center+ax._view))
    camera.SetViewUp(0,0,1);camera.SetParallelScale(ax._scale or 153+(ax._zmax-65)*.25)
    renderer.ResetCameraClippingRange()
    window=vtk.vtkRenderWindow();window.SetOffScreenRendering(1);window.SetMultiSamples(8)
    window.SetSize(1500,1500);window.AddRenderer(renderer);window.Render()
    capture=vtk.vtkWindowToImageFilter();capture.SetInput(window);capture.ReadFrontBufferOff();capture.Update()
    output=capture.GetOutput();w,h,_=output.GetDimensions()
    pixels=vtk_to_numpy(output.GetPointData().GetScalars()).reshape(h,w,-1)[::-1]
    ax.imshow(pixels);ax.set_axis_off();window.Finalize()


def header(fig,title,subtitle):
    fig.text(.055,.945,title,fontsize=25,fontweight='bold')
    fig.text(.055,.902,subtitle,fontsize=11,color=MUTED)


def assembled(ax,spool=True):
    for name in ['base','top_cover','carrier','rim_rail','rim_rail_front','side_guide','nfc_guide','nfc_pod','nfc_pod_cover']:
        geometry(ax,name)
    for name in ['touch_bezel','touch_stand','touch_bridge','REF_touch_PCB','REF_touch_glass','REF_touch_active_area']:
        if name in S: geometry(ax,name)
    for name in S:
        if name in ('indicator_fascia','indicator_back') or name.startswith(('REF_LED_pixel_', 'REF_OLED_glass', 'REF_OLED_active')): geometry(ax,name)
    if spool:
        geometry(ax,'REF_upright_spool');geometry(ax,'REF_filament')


def note(fig,y,title,body):
    fig.text(.755,y,title,fontsize=13,fontweight='bold')
    fig.text(.755,y-.04,body,fontsize=10.5,linespacing=1.5,color=MUTED,va='top')


if VARIANT in ('headless-led','oled-led'):
    label='OLED + five LEDs' if P['oled_enabled'] else 'Five LEDs + web controls'
    validation=json.loads((TARGET/'exports/validation.json').read_text())
    footprint=validation['geometry_checks']['assembled_printed_bounds_mm']
    for filename,spool in [('assembled',True),('cradle',False)]:
        fig=plt.figure(figsize=(12,9),facecolor=BG)
        header(fig,'Spoolancer · '+label,'v0.5 DC input fit prototype · Actual CAD geometry · Hardware fit unverified')
        ax=setup(fig,[.005,.08,.75,.80],zmax=260 if spool else 198)
        ax._scale=180 if spool else 153;ax._center=[0,-25,115 if spool else 88];ax._view=np.array([340,-420,275])
        assembled(ax,spool=spool)
        note(fig,.77,'Five separate light openings','16.67 mm center spacing\nFixed front pod; 60 LEDs/metre')
        note(fig,.59,'Compact shared base',f'{footprint[0]:.0f} × {footprint[1]:.2f} mm with pod\n170 × 160 mm main housing')
        note(fig,.41,'DC power enclosure','Closed USB wall; BOOT tool bore\nDrill jack hole after checking fit')
        note(fig,.23,'Check hardware first','Strip width and OLED board\nremain provisional; print samples')
        fig.text(.055,.045,'12 V input requires a separate 5 V converter. Geometry checks do not prove fit, power safety or scale accuracy.',fontsize=9,color=MUTED)
        finish(ax);fig.savefig(PREV/(filename+'.png'),dpi=180,facecolor=BG);plt.close(fig)
    fig=plt.figure(figsize=(12,9),facecolor=BG)
    header(fig,'Fixed indicator pod','Five separate light openings; rear cover supports the strip and optional OLED')
    ax=setup(fig,[.005,.08,.75,.80],zmax=75)
    ax._scale=95;ax._center=[0,-108,32];ax._view=np.array([120,-270,180])
    for name in S:
        if name.startswith(('indicator_', 'REF_LED_', 'REF_OLED_')): geometry(ax,name)
    note(fig,.77,'OLED edge pads' if P['oled_enabled'] else 'Five status lights', 'PCB edges retained with foam\nNo guessed screw-hole pattern' if P['oled_enabled'] else 'One 60 LEDs/metre strip\nSeparate round face openings')
    note(fig,.59,'Strip support','83.33 × 10 mm nominal strip\n5 × 5050 package envelopes')
    note(fig,.41,'Assembly','Two longer front cover screws\nFour rear-cover plastic screws')
    note(fig,.23,'Service access','Removable rear; fixed wire route\nOpen main lid for USB flashing')
    fig.text(.055,.045,'All strip and OLED dimensions except LED pitch are fit assumptions. Screen remains optional.',fontsize=9,color=MUTED)
    finish(ax);fig.savefig(PREV/'indicator-detail.png',dpi=180,facecolor=BG);plt.close(fig)
    fig=plt.figure(figsize=(12,9),facecolor=BG)
    header(fig,'Closed USB wall and BOOT access','New DC variants only · Internal USB plug and converter spaces are provisional references')
    ax=setup(fig,[.005,.08,.75,.80],zmax=60)
    ax._scale=115;ax._center=[0,-5,25];ax._view=np.array([-280,-320,300])
    geometry(ax,'base',alpha=.40)
    for name in S:
        if name.startswith(('REF_ESP32','REF_USB','REF_BOOT','REF_HX711','REF_load_cell')): geometry(ax,name)
    geometry(ax,'REF_power_conversion_space_unassigned',alpha=.25)
    note(fig,.77,'BOOT tool bore','4 mm hole for 2.5 mm tool\nButton position is photo-derived')
    note(fig,.59,'Removable USB power','10 × 22 × 12 mm plug allowance\nOpen lid to connect computer')
    note(fig,.41,'User-drilled DC jack','Check jack body and wires first\nNo converter mount is imposed')
    note(fig,.23,'Clearance reference','Purple volume is unassigned\nNo selected converter is modeled')
    fig.text(.055,.045,'Disconnect converter power before computer USB. Real board, plug, wiring and BOOT actuation remain unverified.',fontsize=9,color=MUTED)
    finish(ax);fig.savefig(PREV/'service-interior.png',dpi=180,facecolor=BG);plt.close(fig)
    raise SystemExit(0)


if VARIANT=='touch-8048':
    for filename,title,spool in [('assembled','Spoolancer · 4.3-inch touch option',True),('cradle','The fixed display and floating cradle',False)]:
        fig=plt.figure(figsize=(12,9),facecolor=BG)
        header(fig,title,'v0.4 touch · Actual CAD geometry · ESP32-8048S043C-I family-outline fit prototype')
        ax=setup(fig,[.005,.08,.75,.80],zmax=260 if spool else 198)
        ax._scale=180 if spool else 153;ax._center=[0,-28,115 if spool else 88];ax._view=np.array([340,-420,275])
        assembled(ax,spool=spool)
        note(fig,.77,'Integrated controller','800 × 480 capacitive display\nReplaces the standalone ESP32')
        note(fig,.59,'170 × 160 mm base','170 × 205.73 mm with holder\n60° face, open back for access')
        note(fig,.41,'Removable holder','Bezel, stand and flat bridge\nFixed to front cover posts')
        note(fig,.23,'Same moving NFC reader','50–100 mm spool widths\n160–220 mm diameter target')
        fig.text(.055,.045,'Fit prototype. Received-board dimensions, capacitive edges, wiring, stiffness and scale accuracy remain untested.',fontsize=9,color=MUTED)
        finish(ax);fig.savefig(PREV/(filename+'.png'),dpi=180,facecolor=BG);plt.close(fig)
    fig=plt.figure(figsize=(12,9),facecolor=BG)
    header(fig,'Open-back touch holder','The screen mounts to the fixed enclosure; it never touches the weighing cradle')
    ax=setup(fig,[.005,.08,.75,.80],zmax=120)
    ax._scale=115;ax._center=[0,-102,50];ax._view=np.array([-280,350,190])
    for name in ['touch_bezel','touch_stand','touch_bridge','REF_touch_PCB','REF_touch_glass','REF_touch_components_allowance']:
        geometry(ax,name)
    note(fig,.77,'Four PCB clamps','116 × 67.1 mm nominal pitch\n4.4 mm vertical clearance slots')
    note(fig,.59,'Direct service access','Open left, right and rear\nBOOT, USB and lower connectors')
    note(fig,.41,'Base attachment','Two M3 × 16 at front posts\nTwo M3 × 12 bridge clamps')
    note(fig,.23,'Start with the fit sample','Family drawing has a 0.2 mm\nvertical-pitch inconsistency')
    fig.text(.055,.045,'Board body and components are approximate clearance references, not manufacturer CAD. Clamp lightly; verify board/glass stack.',fontsize=9,color=MUTED)
    finish(ax);fig.savefig(PREV/'touch-back.png',dpi=180,facecolor=BG);plt.close(fig)
    raise SystemExit(0)

fig=plt.figure(figsize=(12,9),facecolor=BG)
header(fig,'Spoolancer · headless upright scale','v0.4.1 headless · Actual CAD geometry with a 200 × 80 mm reference spool')
ax=setup(fig,[.005,.08,.75,.80],zmax=260)
ax._scale=175;ax._center=[0,-15,120];ax._view=np.array([340,-380,265])
assembled(ax)
note(fig,.77,'Read while seated','Reader moves with the guide\nRotate spool to align its tag')
note(fig,.59,'170 × 160 mm base','Web controls from your phone\nBuilt-in BOOT resets Wi-Fi')
note(fig,.41,'Open rim supports','160–220 mm diameter target\nNo axle or bearings')
note(fig,.23,'Shared upright cradle','50–100 mm spool widths\nReader center: 90–175 mm')
fig.text(.055,.045,'Fit prototype. Real fit, NFC range, wire-force bias, stability and weighing accuracy remain untested.',fontsize=9,color=MUTED)
finish(ax);fig.savefig(PREV/'assembled.png',dpi=180,facecolor=BG);plt.close(fig)

fig=plt.figure(figsize=(12,9),facecolor=BG)
header(fig,'The smaller headless enclosure','All spool-contact parts and the NFC pod are carried by the load cell')
ax=setup(fig,[.005,.08,.75,.80],zmax=198)
ax._scale=148;ax._center=[0,-5,88];ax._view=np.array([-330,-360,270])
assembled(ax,spool=False)
note(fig,.77,'Same floating cradle','Rails, carrier, guides and pod\nUnchanged from v0.3')
note(fig,.59,'One physical control','Use the built-in BOOT button\nNo extra switch or perfboard')
note(fig,.41,'Thin antenna window','1.2 mm plastic facing the tag\nAbout 2 mm from the spool side')
note(fig,.23,'Flexible cable loop','Direct solder leads to the base\nMust not pull on the cradle')
fig.text(.055,.045,'Fixed mast is 184 mm high. Lowest reader position does not reduce mast height; pod and guide are removable.',fontsize=9,color=MUTED)
finish(ax);fig.savefig(PREV/'cradle.png',dpi=180,facecolor=BG);plt.close(fig)

fig=plt.figure(figsize=(12,9),facecolor=BG)
header(fig,'The reader stays with the spool','A removable PN532 pod slides vertically on the moving right-hand guide')
ax=setup(fig,[.005,.08,.75,.80],zmax=200)
ax._scale=105;ax._center=[36,0,119];ax._view=np.array([-330,-220,170])
for name in ['nfc_guide','nfc_pod','nfc_pod_cover','carrier','rim_rail','rim_rail_front']:
    geometry(ax,name)
note(fig,.78,'Independent adjustment','Width: 50–100 mm spools\nReader center: 90–175 mm')
note(fig,.59,'No front scan patch','Antenna faces the spool side\nTurn the tag toward the reader')
note(fig,.40,'Nylon near the antenna','Two sliding M3 fasteners\nNylon PCB fasteners')
note(fig,.21,'Cable exits underneath','Leave a generous silicone loop\nTest its effect on zero and weight')
fig.text(.055,.045,'Nominal antenna-to-spool separation is 4 mm including the window and air gap. Actual read/write range is unverified.',fontsize=9,color=MUTED)
finish(ax);fig.savefig(PREV/'nfc-detail.png',dpi=180,facecolor=BG);plt.close(fig)

fig=plt.figure(figsize=(12,11),facecolor=BG)
header(fig,'Nine main printed pieces','Base · headless cover · carrier · two rails · two guides · pod · pod cover')
ax=setup(fig,[.005,.07,.75,.81],zmax=400)
ax._center=[0,-15,180];ax._scale=245;ax._view=np.array([340,-380,220])
geometry(ax,'base')
for name in S:
    if name.startswith('REF_') and not any(k in name for k in ['screen','PN532','button','switch','spool','filament']): geometry(ax,name)
geometry(ax,'top_cover',(0,0,50))
geometry(ax,'carrier',(0,0,100))
geometry(ax,'rim_rail',(0,12,145));geometry(ax,'rim_rail_front',(0,-12,145))
geometry(ax,'side_guide',(-12,0,210));geometry(ax,'nfc_guide',(10,0,210))
geometry(ax,'nfc_pod',(45,0,210));geometry(ax,'nfc_pod_cover',(65,0,210))
note(fig,.78,'01  Guides + reader','Separate left and right guides\nReader pod and removable back')
note(fig,.62,'02  Rim rails × 2','One print, rotated for the front\nHollow back and vertical webs')
note(fig,.46,'03  Open carrier','Two M4 screws to load cell\nFour accessible overload stops')
note(fig,.30,'04  Headless cover','Blank cover, four closure screws\nBOOT access through USB opening')
note(fig,.14,'05  Base','ESP32, HX711, fixed cell\nUSB opening and rubber feet')
fig.text(.055,.035,'Nine printed pieces total. Simplified reference hardware; cables and most fasteners are omitted.',fontsize=9,color=MUTED)
finish(ax);fig.savefig(PREV/'exploded.png',dpi=170,facecolor=BG);plt.close(fig)

fig=plt.figure(figsize=(12,8),facecolor=BG)
header(fig,'Inside the smaller base','The ESP32-S3, HX711 and load cell remain in the fixed housing')
ax=setup(fig,[.005,.08,.75,.79])
ax._scale=120;ax._center=[0,-5,20];ax._view=np.array([-212,-340,446])
geometry(ax,'base')
for name in S:
    if name.startswith('REF_') and not any(k in name for k in ['screen','PN532','button','switch','spool','filament']): geometry(ax,name)
note(fig,.75,'Load path','M5 end fixed to the base\nM4 end carries the cradle\nCentral sensing section free')
note(fig,.53,'USB + BOOT access','Existing opening, no added switch\nUse a thin blunt plastic tool')
note(fig,.32,'Clearance retained','13 mm ahead of the cell\nModeled lead allowance: 12 mm')
fig.text(.055,.045,'Board and BOOT locations are photo estimates. Check actual board, cable and plastic-tool access before printing.',fontsize=9,color=MUTED)
finish(ax);fig.savefig(PREV/'interior.png',dpi=180,facecolor=BG);plt.close(fig)


fig=plt.figure(figsize=(12,8),facecolor=BG)
header(fig,'Use the ESP32’s own BOOT button','The existing USB service opening provides an angled approach above the connectors')
ax=setup(fig,[.005,.08,.75,.79],zmax=50)
ax._scale=68;ax._center=[-50,-20,22];ax._view=np.array([-250,-180,195])
geometry(ax,'base',alpha=.22);geometry(ax,'top_cover',alpha=.10);geometry(ax,'carrier',alpha=.10)
for name in S:
    if name.startswith(('REF_ESP32','REF_USB','REF_BOOT')): geometry(ax,name)
note(fig,.75,'No extra electronics','No separate button or perfboard\nNo printed plunger or cap')
note(fig,.53,'Blunt plastic tool','Nominal 2.5 mm tool shown\nKeep headers and wiring clear')
note(fig,.31,'Fit check required','BOOT location is photo-derived\nVerify access with USB plugged in')
fig.text(.055,.045,'Housing is translucent to show the approach. This is a geometric clearance check, not a physical button or cable test.',fontsize=9,color=MUTED)
finish(ax);fig.savefig(PREV/'boot-access.png',dpi=180,facecolor=BG);plt.close(fig)

from matplotlib.patches import Rectangle,Polygon,Circle
fig,axes=plt.subplots(1,2,figsize=(13,6),facecolor=BG)
fig.suptitle('Open rim supports · one load path',x=.055,y=.96,ha='left',fontsize=23,fontweight='bold')
a=axes[0];a.set_facecolor(BG)
slope=np.tan(np.radians(P['saddle_angle_deg']));apex=P['saddle_apex_z'];r=100;zc=apex+r/np.cos(np.radians(30))
for side in (-1,1):
    a.add_patch(Polygon([(side*34,50),(side*65,50),(side*65,apex+65*slope),(side*34,apex+34*slope)],facecolor='#a9c0b7'))
a.add_patch(Circle((0,zc),r,facecolor='#d7dad6',edgecolor='#34454a',lw=2))
a.add_patch(Circle((0,zc),26,facecolor=BG,edgecolor='#34454a',lw=1.5))
for y in (-50,50): a.plot(y,apex+abs(y)*slope,'o',color='#cf832e',ms=7)
a.annotate('Two rim contacts',(-50,apex+50*slope),(-136,22),arrowprops={'arrowstyle':'-','color':MUTED},fontsize=11)
a.text(-136,275,'Side view: no axle or rollers',fontsize=12,fontweight='bold')
a.set_xlim(-145,145);a.set_ylim(0,295);a.set_aspect('equal');a.axis('off')
b=axes[1];b.set_facecolor(BG)
b.add_patch(Rectangle((-82,0),108,2.6,facecolor='#34454a'))
b.add_patch(Rectangle((-71.5,2.6),28,9.4,facecolor='#34454a'))
b.add_patch(Rectangle((-70,12),80,12.7,facecolor='#adb5b7'))
b.add_patch(Rectangle((-16,24.7),27,21.3,facecolor='#9badab'))
b.add_patch(Rectangle((-65,38),130,12,facecolor='#9badab'))
for y in (-65,-50): b.plot([y,y],[1.2,17.2],color='#293737',lw=3)
for y in (-10,5): b.plot([y,y],[19.5,47.7],color='#293737',lw=3)
b.annotate('Base · M5',(-58,7),(-87,33),arrowprops={'arrowstyle':'-','color':MUTED},fontsize=11)
b.annotate('Carrier · M4',(-2,34),(23,25),arrowprops={'arrowstyle':'-','color':MUTED},fontsize=11)
b.annotate('Central sensing region stays free',(-30,18),(-85,-14),arrowprops={'arrowstyle':'-','color':MUTED},fontsize=10)
b.annotate('Spool + rails + guides + reader',(-2,50),(-2,66),ha='center',arrowprops={'arrowstyle':'->','color':INK,'lw':2},fontsize=11,fontweight='bold')
b.set_xlim(-92,92);b.set_ylim(-25,85);b.set_aspect('equal');b.axis('off')
fig.text(.055,.045,'Schematic only. Follow the actual sensor load arrow. The reader cable must not create a second load path.',fontsize=10,color=MUTED)
fig.savefig(PREV/'load-path.png',dpi=170,facecolor=BG);plt.close(fig)
print('Rendered headless assembly, empty cradle, reader, exploded view, interior and load path.')
