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
    for name in ['base','REF_screen_PCB','REF_screen_glass','top_cover','carrier','saddle','side_guide','side_guide_left']:
        geometry(ax,name)
    caps(ax)
    if spool:
        geometry(ax,'REF_upright_spool');geometry(ax,'REF_filament')


def note(fig,y,title,body):
    fig.text(.755,y,title,fontsize=14,fontweight='bold')
    fig.text(.755,y-.045,body,fontsize=11,linespacing=1.6,color=MUTED,va='top')


fig=plt.figure(figsize=(12,9),facecolor=BG)
header(fig,'Spoolancer · upright cradle','v0.2 · Actual CAD geometry with a 200 × 80 mm reference spool')
ax=setup(fig,[.005,.08,.75,.80],zmax=275)
ax._scale=187;ax._center=[0,-20,125]
assembled(ax)
note(fig,.76,'Spool stands upright','Rim rests in a V saddle\nPlace it here to weigh it')
note(fig,.59,'Adjustable side guides','50–100 mm spool widths\n160–220 mm diameter target')
note(fig,.42,'190 × 230 mm base','Screen · three buttons · NFC\nUSB power at the side')
note(fig,.25,'About 272 mm high','With this 200 mm spool\nAbout 121 mm without it')
fig.text(.055,.045,'Fit prototype. Printed geometry checked; real fit, stability, NFC performance and weighing accuracy remain untested.',fontsize=9,color=MUTED)
finish(ax);fig.savefig(HERE/'previews/assembled.png',dpi=180,facecolor=BG);plt.close(fig)

fig=plt.figure(figsize=(12,8),facecolor=BG)
header(fig,'The upright cradle','The complete saddle and both guides move with the load cell')
ax=setup(fig,[.005,.08,.75,.79],zmax=120)
ax._scale=160;ax._center=[0,-25,52];ax._view=np.array([-250,-360,410])
assembled(ax,spool=False)
note(fig,.76,'V-shaped saddle','Two rim contacts center it\nNo axle or rollers to assemble')
note(fig,.58,'Sliding side guides','Set gap to spool width + 4 mm\nFour screws lock the guides')
note(fig,.40,'Separate front controls','Tap the tag before placement\nThen weigh and confirm')
note(fig,.22,'Simple service access','Four cover screws\nReplaceable component mounts')
fig.text(.055,.045,'Guides shown at the 80 mm setting. All spool supports attach to the floating carrier; the fixed housing stays clear.',fontsize=9,color=MUTED)
finish(ax);fig.savefig(HERE/'previews/cradle.png',dpi=180,facecolor=BG);plt.close(fig)

fig=plt.figure(figsize=(12,11),facecolor=BG)
header(fig,'Six main printed pieces','One base · one cover · one carrier · one saddle · two identical guides')
ax=setup(fig,[.005,.07,.75,.81],zmax=310)
ax._center=[0,-24,145];ax._scale=218;ax._view=np.array([-340,-380,250])
geometry(ax,'base')
for name in S:
    if name.startswith('REF_') and not any(k in name for k in ['screen','PN532','button','switch','spool','filament']):
        geometry(ax,name)
for name in S:
    if name.startswith('REF_') and any(k in name for k in ['screen','PN532','button','switch']):
        geometry(ax,name,(0,0,55))
geometry(ax,'top_cover',(0,0,55));caps(ax,55)
geometry(ax,'carrier',(0,0,105))
geometry(ax,'saddle',(0,0,150))
geometry(ax,'side_guide',(15,0,195));geometry(ax,'side_guide_left',(-15,0,195))
note(fig,.78,'01  Side guides × 2','Same print, rotated\nFour sliding M3 fasteners')
note(fig,.63,'02  V saddle','Four M3 screws\nCaptive guide nuts underneath')
note(fig,.48,'03  Moving carrier','Two M4 screws to load cell\nFour stop-access holes')
note(fig,.33,'04  Fixed cover','Holds display, reader, buttons\nFour M3 closure screws')
note(fig,.18,'05  Base','ESP32, HX711 and fixed cell\nAdjustable stops + rubber feet')
fig.text(.055,.035,'Reference hardware is simplified. Most screws, nuts, cables and washers are omitted from this view.',fontsize=9,color=MUTED)
finish(ax);fig.savefig(HERE/'previews/exploded.png',dpi=170,facecolor=BG);plt.close(fig)

fig=plt.figure(figsize=(12,8),facecolor=BG)
header(fig,'Inside the base','Fixed electronics and a clear load path through the sensor')
ax=setup(fig,[.005,.08,.75,.79])
ax._scale=155;ax._center=[0,-24,20];ax._view=np.array([-212,-340,446])
geometry(ax,'base')
for name in S:
    if name.startswith('REF_') and not any(k in name for k in ['screen','PN532','button','switch','spool','filament']):
        geometry(ax,name)
note(fig,.75,'Load cell','M5 end fixed to the base\nM4 end carries the cradle\nCentral sensing section free')
note(fig,.53,'USB service notch','Both USB-C sockets accessible\nVerify with the actual board')
note(fig,.35,'Four adjustable stops','Set from measured deflection\n1 mm is an untuned initial gap')
fig.text(.055,.045,'Fit samples are included for the load cell, ESP32 holder, display bezel, reader and buttons.',fontsize=10,color=MUTED)
finish(ax);fig.savefig(HERE/'previews/interior.png',dpi=180,facecolor=BG);plt.close(fig)

# Schematic sections: exact nominal positions, omitted fixed cover for clarity.
from matplotlib.patches import Rectangle,Polygon,Circle
fig,axes=plt.subplots(1,2,figsize=(13,6),facecolor=BG,gridspec_kw={'width_ratios':[1.05,1]})
fig.suptitle('Upright support · one load path',x=.055,y=.96,ha='left',fontsize=23,fontweight='bold')
a=axes[0];a.set_facecolor(BG)
slope=np.tan(np.radians(P['saddle_angle_deg']));apex=P['saddle_apex_z'];r=100;zc=apex+r/np.cos(np.radians(30))
a.add_patch(Polygon([(-65,50),(65,50),(65,apex+65*slope),(0,apex),(-65,apex+65*slope)],facecolor='#a9c0b7'))
a.add_patch(Circle((0,zc),r,facecolor='#d7dad6',edgecolor='#34454a',lw=2))
a.add_patch(Circle((0,zc),26,facecolor=BG,edgecolor='#34454a',lw=1.5))
for y in (-50,50):
    a.plot(y,apex+abs(y)*slope,'o',color='#cf832e',ms=7)
a.annotate('Two rim contacts',(-50,apex+50*slope),(-136,22),arrowprops={'arrowstyle':'-','color':MUTED},fontsize=11)
a.text(-136,294,'Side view: spool stands vertically',fontsize=12,fontweight='bold')
a.set_xlim(-145,145);a.set_ylim(0,310);a.set_aspect('equal');a.axis('off')
b=axes[1];b.set_facecolor(BG)
b.add_patch(Rectangle((-82,0),108,3,facecolor='#34454a'))
b.add_patch(Rectangle((-71.5,3),28,9,facecolor='#34454a'))
b.add_patch(Rectangle((-70,12),80,12.7,facecolor='#adb5b7'))
b.add_patch(Rectangle((-16,24.7),27,21.3,facecolor='#9badab'))
b.add_patch(Rectangle((-82,46),108,4,facecolor='#9badab'))
for y in (-65,-50): b.plot([y,y],[1.2,17.2],color='#293737',lw=3)
for y in (-10,5): b.plot([y,y],[19.5,47.7],color='#293737',lw=3)
b.annotate('Base · M5',(-58,7),(-87,34),arrowprops={'arrowstyle':'-','color':MUTED},fontsize=11)
b.annotate('Carrier · M4',(-2,34),(23,32),arrowprops={'arrowstyle':'-','color':MUTED},fontsize=11)
b.annotate('Central sensing region stays free',(-30,18),(-85,-14),arrowprops={'arrowstyle':'-','color':MUTED},fontsize=10)
b.annotate('Saddle + guides + spool',(-2,50),(-2,66),ha='center',arrowprops={'arrowstyle':'->','color':INK,'lw':2},fontsize=11,fontweight='bold')
b.set_xlim(-92,92);b.set_ylim(-25,85);b.set_aspect('equal');b.axis('off')
fig.text(.055,.045,'Schematic only. Follow the actual sensor load arrow. No support, wire or stop may bypass the cell during weighing.',fontsize=10,color=MUTED)
fig.savefig(HERE/'previews/load-path.png',dpi=170,facecolor=BG);plt.close(fig)
print('Rendered upright assembly, empty cradle, exploded view, interior and load path.')
