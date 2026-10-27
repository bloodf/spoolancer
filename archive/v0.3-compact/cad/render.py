"""Render the exported CAD tessellation, not an AI-generated concept image."""
import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import vtk
from vtk.util.numpy_support import numpy_to_vtk, vtk_to_numpy

HERE=Path(__file__).resolve().parent
S=json.loads((HERE/'previews/scene.json').read_text())
P=json.loads((HERE/'parameters.json').read_text())
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


def caps(ax,shift_z=0):
    for dy in (-P['button_pitch'],0,P['button_pitch']):
        geometry(ax,'button_cap',(0,dy,shift_z))


def header(fig,title,subtitle):
    fig.text(.055,.945,title,fontsize=25,fontweight='bold')
    fig.text(.055,.902,subtitle,fontsize=11,color=MUTED)


def assembled(ax,spool=True):
    for name in ['base','REF_screen_PCB','REF_screen_glass','top_cover','carrier','rim_rail','rim_rail_front','side_guide','nfc_guide','nfc_pod','nfc_pod_cover']:
        geometry(ax,name)
    caps(ax)
    if spool:
        geometry(ax,'REF_upright_spool');geometry(ax,'REF_filament')


def note(fig,y,title,body):
    fig.text(.755,y,title,fontsize=13,fontweight='bold')
    fig.text(.755,y-.04,body,fontsize=10.5,linespacing=1.5,color=MUTED,va='top')


fig=plt.figure(figsize=(12,9),facecolor=BG)
header(fig,'Spoolancer · compact upright scale','v0.3 · Actual CAD geometry with a 200 × 80 mm reference spool')
ax=setup(fig,[.005,.08,.75,.80],zmax=260)
ax._scale=175;ax._center=[0,-15,120];ax._view=np.array([340,-380,265])
assembled(ax)
note(fig,.77,'Read while seated','Reader moves with the guide\nRotate spool to align its tag')
note(fig,.59,'170 × 190 mm base','26% smaller footprint\n50–100 mm spool widths')
note(fig,.41,'Open rim supports','160–220 mm diameter target\nNo axle or bearings')
note(fig,.23,'Reader height adjusts','Center: 90–175 mm\n198 mm tall at full extension')
fig.text(.055,.045,'Fit prototype. Real fit, NFC range, wire-force bias, stability and weighing accuracy remain untested.',fontsize=9,color=MUTED)
finish(ax);fig.savefig(HERE/'previews/assembled.png',dpi=180,facecolor=BG);plt.close(fig)

fig=plt.figure(figsize=(12,9),facecolor=BG)
header(fig,'An open cradle and moving reader','All spool-contact parts and the NFC pod are carried by the load cell')
ax=setup(fig,[.005,.08,.75,.80],zmax=198)
ax._scale=157;ax._center=[0,-12,88];ax._view=np.array([-330,-360,270])
assembled(ax,spool=False)
note(fig,.77,'Two shallow rim rails','Hollow backs and an open frame\nReplace the full solid V block')
note(fig,.59,'Adjustable side guides','Set spacing to width + 4 mm\nGuide screws remain accessible')
note(fig,.41,'Thin antenna window','1.2 mm plastic facing the tag\nAbout 2 mm from the spool side')
note(fig,.23,'Flexible cable loop','Direct solder leads to the base\nMust not pull on the cradle')
fig.text(.055,.045,'Fixed mast is 184 mm high. Lowest reader position does not reduce mast height; pod and guide are removable.',fontsize=9,color=MUTED)
finish(ax);fig.savefig(HERE/'previews/cradle.png',dpi=180,facecolor=BG);plt.close(fig)

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
finish(ax);fig.savefig(HERE/'previews/nfc-detail.png',dpi=180,facecolor=BG);plt.close(fig)

fig=plt.figure(figsize=(12,11),facecolor=BG)
header(fig,'Nine main printed pieces','Base · cover · carrier · two rim rails · two guides · pod · pod cover')
ax=setup(fig,[.005,.07,.75,.81],zmax=400)
ax._center=[0,-15,180];ax._scale=245;ax._view=np.array([340,-380,220])
geometry(ax,'base')
for name in S:
    if name.startswith('REF_') and not any(k in name for k in ['screen','PN532','button','switch','spool','filament']): geometry(ax,name)
for name in ['REF_screen_PCB','REF_screen_glass']:
    geometry(ax,name,(0,0,50))
geometry(ax,'top_cover',(0,0,50));caps(ax,50)
geometry(ax,'carrier',(0,0,100))
geometry(ax,'rim_rail',(0,12,145));geometry(ax,'rim_rail_front',(0,-12,145))
geometry(ax,'side_guide',(-12,0,210));geometry(ax,'nfc_guide',(10,0,210))
geometry(ax,'nfc_pod',(45,0,210));geometry(ax,'nfc_pod_cover',(65,0,210))
note(fig,.78,'01  Guides + reader','Separate left and right guides\nReader pod and removable back')
note(fig,.62,'02  Rim rails × 2','One print, rotated for the front\nHollow back and vertical webs')
note(fig,.46,'03  Open carrier','Two M4 screws to load cell\nFour accessible overload stops')
note(fig,.30,'04  Fixed cover','Display and three buttons\nFour closure screws')
note(fig,.14,'05  Base','ESP32, HX711, fixed cell\nUSB opening and rubber feet')
fig.text(.055,.035,'Three optional button caps are additional. Simplified reference hardware; cables and most fasteners are omitted.',fontsize=9,color=MUTED)
finish(ax);fig.savefig(HERE/'previews/exploded.png',dpi=170,facecolor=BG);plt.close(fig)

fig=plt.figure(figsize=(12,8),facecolor=BG)
header(fig,'Inside the smaller base','The existing electronics and load cell remain in the fixed housing')
ax=setup(fig,[.005,.08,.75,.79])
ax._scale=130;ax._center=[0,-20,20];ax._view=np.array([-212,-340,446])
geometry(ax,'base')
for name in S:
    if name.startswith('REF_') and not any(k in name for k in ['screen','PN532','button','switch','spool','filament']): geometry(ax,name)
note(fig,.75,'Load path','M5 end fixed to the base\nM4 end carries the cradle\nCentral sensing section free')
note(fig,.53,'USB service opening','Both USB-C sockets accessible\nVerify the actual board and cable')
note(fig,.32,'Four adjustable stops','Set from measured deflection\n1 mm is an untuned initial gap')
fig.text(.055,.045,'Fit samples are included. Geometry checks do not establish physical fit, rigidity or calibrated scale accuracy.',fontsize=9,color=MUTED)
finish(ax);fig.savefig(HERE/'previews/interior.png',dpi=180,facecolor=BG);plt.close(fig)

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
fig.savefig(HERE/'previews/load-path.png',dpi=170,facecolor=BG);plt.close(fig)
print('Rendered compact assembly, empty cradle, reader, exploded view, interior and load path.')
