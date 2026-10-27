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
    ax._actors=[];ax._zmax=zmax
    return ax


def finish(ax):
    renderer=vtk.vtkRenderer();renderer.SetBackground(244/255,243/255,238/255)
    for actor in ax._actors:
        renderer.AddActor(actor)
    camera=renderer.GetActiveCamera();camera.ParallelProjectionOn()
    center=np.array([0,-24,ax._zmax*.5])
    camera.SetFocalPoint(*center);camera.SetPosition(*(center+np.array([-212,-340,446])))
    camera.SetViewUp(0,0,1);camera.SetParallelScale(153+(ax._zmax-65)*.25)
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


fig=plt.figure(figsize=(12,8),facecolor=BG)
header(fig,'Spoolancer','Mechanical prototype v0.1 · generated from the editable CAD')
ax=setup(fig,[.01,.08,.75,.79])
for name in ['base','REF_screen_PCB','REF_screen_glass','front_panel','tray']:
    geometry(ax,name)
caps(ax)
fig.text(.75,.76,'196 mm tray',fontsize=15,fontweight='bold')
fig.text(.75,.70,'Flat spool placement\nUp to 2 kg gross target',fontsize=11,linespacing=1.6,color=MUTED,va='top')
fig.text(.75,.58,'Separate front panel',fontsize=14,fontweight='bold')
fig.text(.75,.51,'2-inch color screen\nThree buttons · NFC scan face',fontsize=11,linespacing=1.6,color=MUTED,va='top')
fig.text(.75,.37,'200 × 248 mm',fontsize=19,fontweight='bold')
fig.text(.75,.32,'Footprint · about 52 mm high',fontsize=10,color=MUTED)
fig.text(.055,.045,'Fit prototype: board clearances, screw depths, NFC performance and weighing accuracy still require physical checks.',fontsize=9,color=MUTED)
finish(ax)
fig.savefig(HERE/'previews/assembled.png',dpi=180,facecolor=BG)
plt.close(fig)

fig=plt.figure(figsize=(11,10),facecolor=BG)
header(fig,'Three main printed pieces','Remove two screws to lift the tray; four screws release the front panel')
ax=setup(fig,[.00,.07,.78,.81],zmax=165)
geometry(ax,'base')
for name in S:
    if name.startswith('REF_') and not any(k in name for k in ['screen','PN532','button','switch']):
        geometry(ax,name)
for name in S:
    if name.startswith('REF_') and any(k in name for k in ['screen','PN532','button','switch']):
        geometry(ax,name,(0,0,55))
geometry(ax,'front_panel',(0,0,55));caps(ax,55)
geometry(ax,'tray',(0,0,100))
fig.text(.76,.77,'01  Floating tray',fontsize=13,fontweight='bold')
fig.text(.76,.70,'Ribs underneath\nTwo M4 cell screws\nFour stop-adjustment holes',fontsize=10,linespacing=1.6,color=MUTED,va='top')
fig.text(.76,.53,'02  Front panel',fontsize=13,fontweight='bold')
fig.text(.76,.46,'Screen and reader attach here\nOne small button board\nTwo nylon NFC fasteners',fontsize=10,linespacing=1.6,color=MUTED,va='top')
fig.text(.76,.29,'03  Fixed base',fontsize=13,fontweight='bold')
fig.text(.76,.22,'ESP32 + HX711 ledges\nWired end of cell fixed below\nFour adjustable stops',fontsize=10,linespacing=1.6,color=MUTED,va='top')
fig.text(.055,.035,'Hardware shown as simplified clearance envelopes. Cables, nuts and most screws are omitted for clarity.',fontsize=9,color=MUTED)
finish(ax)
fig.savefig(HERE/'previews/exploded.png',dpi=170,facecolor=BG)
plt.close(fig)

fig=plt.figure(figsize=(12,8),facecolor=BG)
header(fig,'Inside the base','Both electronics boards are fixed to the base; nothing rests against the weighing tray')
ax=setup(fig,[.01,.08,.73,.79])
geometry(ax,'base')
for name in S:
    if name.startswith('REF_') and not any(k in name for k in ['screen','PN532','button','switch']):
        geometry(ax,name)
fig.text(.76,.74,'Load cell',fontsize=15,fontweight='bold')
fig.text(.76,.67,'M5 end fixed to base\nM4 end carries tray\nCentral sensing section free',fontsize=11,linespacing=1.6,color=MUTED,va='top')
fig.text(.76,.52,'USB service notch',fontsize=14,fontweight='bold')
fig.text(.76,.46,'Both USB-C sockets accessible\nNo long bridge to print',fontsize=11,linespacing=1.6,color=MUTED,va='top')
fig.text(.76,.31,'Adjustable stops',fontsize=14,fontweight='bold')
fig.text(.76,.24,'Set after assembly and loading\n1 mm is an untuned starting gap',fontsize=11,linespacing=1.6,color=MUTED,va='top')
fig.text(.055,.045,'Printed fit samples are included for the load cell, ESP32 holder, display bezel, reader and buttons.',fontsize=10,color=MUTED)
finish(ax)
fig.savefig(HERE/'previews/interior.png',dpi=180,facecolor=BG)
plt.close(fig)

# Side section through the cell. This is an annotated engineering diagram;
# omitted sidewall allows the load path to remain visible.
from matplotlib.patches import Rectangle,FancyArrowPatch
fig,ax=plt.subplots(figsize=(12,5),facecolor=BG)
ax.set_facecolor(BG)
ax.add_patch(Rectangle((-82,0),108,3,facecolor='#34454a'))
ax.add_patch(Rectangle((-71.5,3),28,9,facecolor='#34454a'))
ax.add_patch(Rectangle((-70,12),80,12.7,facecolor='#adb5b7'))
ax.add_patch(Rectangle((-16,24.7),27,21.3,facecolor='#9badab'))
ax.add_patch(Rectangle((-82,46),108,4,facecolor='#9badab'))
for y in (-65,-50):
    ax.plot([y,y],[1.2,17.2],color='#293737',lw=3)
for y in (-10,5):
    ax.plot([y,y],[19.5,47.7],color='#293737',lw=3)
ax.annotate('Base end · two M5 screws',(-58,7),(-87,35),arrowprops={'arrowstyle':'-','color':MUTED},fontsize=11)
ax.annotate('Tray end · two M4 screws',(-2,34),(23,32),arrowprops={'arrowstyle':'-','color':MUTED},fontsize=11)
ax.annotate('Keep the central sensing region clear',(-30,18),(-65,-13),arrowprops={'arrowstyle':'-','color':MUTED},fontsize=11)
ax.annotate('Spool load',(-2,50),(-2,65),ha='center',arrowprops={'arrowstyle':'->','color':INK,'lw':2},fontsize=12,fontweight='bold')
ax.text(-87,76,'The only load path is through the cell',fontsize=20,fontweight='bold')
ax.text(-87,-24,'Diagram is not a drilling template. Follow the load-direction arrow on the actual sensor.',fontsize=10,color=MUTED)
ax.set_xlim(-92,92);ax.set_ylim(-28,82);ax.set_aspect('equal');ax.axis('off')
fig.savefig(HERE/'previews/load-path.png',dpi=160,facecolor=BG,bbox_inches='tight')
plt.close(fig)
print('Rendered assembled, exploded, interior, and load-path previews.')
