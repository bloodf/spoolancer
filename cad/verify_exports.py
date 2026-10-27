"""Reopen exported solids/meshes and check touch fastener clearance envelopes."""
import argparse,json,hashlib,math,os,sys
from pathlib import Path
freecad_lib = os.environ.get('FREECAD_LIB')
if freecad_lib:
    sys.path.insert(0, freecad_lib)
elif sys.platform == 'darwin':
    bundled_lib = Path('/Applications/FreeCAD.app/Contents/Resources/lib')
    if bundled_lib.is_dir():
        sys.path.append(str(bundled_lib))
import FreeCAD as App,Mesh,Part
parser=argparse.ArgumentParser();parser.add_argument('--variant',choices=['headless','touch-8048','headless-led','oled-led'])
args=parser.parse_args();variant=args.variant or 'headless'
root=Path(__file__).resolve().parent;target=root/'variants'/variant if args.variant else root;out=target/'exports'
p=json.loads((root/'parameters.json').read_text())
if variant!='headless':p.update(json.loads((target/'parameters.json').read_text()))
v=json.loads((out/'validation.json').read_text())
assert not v['printed_part_intersections'] and not v['esp_wire_envelope_intersections']
assert not any(v['component_intersections'].values())
assert not any(x['positive_volume_intersections'] for x in v['spool_envelope_checks']+v['pod_adjustment_checks'])
if variant!='touch-8048':
    assert not v['boot_access']['tool_component_intersections']
    assert not (out/'button_cap.stl').exists() and not (out/'fit_button.stl').exists()
r={'revision':v['revision'],'physical_validation':False,'stl_files':{},'individual_step_files':{},'source_sha256':{},
   'freecad_version':'.'.join(App.Version()[:3]),
   'step_bounds_method':'optimalBoundingBox(useTriangulation=False, useShapeTolerance=False)',
   'comparison_tolerances':{'step_vs_design_bounds_mm':0.001,'step_vs_stl_bounds_mm':0.2,'step_vs_design_volume_mm3':0.02,'step_vs_stl_relative_volume':0.005}}
for name,original in v['parts'].items():
    mesh=Mesh.Mesh(str(out/(name+'.stl')));b=mesh.BoundBox
    assert mesh.isSolid() and not mesh.hasSelfIntersections(),name
    bounds=[round(b.XLength,2),round(b.YLength,2),round(b.ZLength,2)]
    assert all(abs(x-y)<.02 for x,y in zip(bounds,original['print_bounds_mm'])) and abs(b.ZMin)<.001,name
    r['stl_files'][name]={'reopened_closed_mesh':True,'self_intersections':False,'print_bounds_mm':bounds,'bed_z_mm':round(b.ZMin,4)}
    step_file=out/(name+'.step')
    solid=Part.read(str(step_file));sb=solid.optimalBoundingBox(False,False)
    assert solid.isValid() and len(solid.Solids)==1,(name,'STEP single solid')
    step_bounds=[sb.XLength,sb.YLength,sb.ZLength]
    mesh_bounds=[b.XLength,b.YLength,b.ZLength]
    design_deltas=[abs(x-y) for x,y in zip(step_bounds,original['step_bounds_mm'])]
    mesh_deltas=[abs(x-y) for x,y in zip(step_bounds,mesh_bounds)]
    assert max(design_deltas)<.001 and abs(sb.ZMin)<.001,(name,'STEP pose/design bounds')
    assert max(mesh_deltas)<.2,(name,'STEP/STL bounds')
    assert abs(solid.Volume-original['volume_mm3'])<.02,(name,'STEP/design volume')
    mesh_volume=abs(mesh.Volume)
    relative_volume_error=abs(solid.Volume-mesh_volume)/solid.Volume
    assert relative_volume_error<.005,(name,'STEP/STL volume',relative_volume_error)
    r['individual_step_files'][name]={
        'reopened_valid_single_solid':True,'print_bounds_mm':[round(x,6) for x in step_bounds],
        'bed_z_mm':round(sb.ZMin,6),'volume_mm3':round(solid.Volume,6),
        'stl_volume_mm3':round(mesh_volume,6),'stl_volume_difference_percent':round(100*relative_volume_error,6),
        'max_design_bounds_difference_mm':round(max(design_deltas),6),
        'max_stl_bounds_difference_mm':round(max(mesh_deltas),6),
        'sha256':hashlib.sha256(step_file.read_bytes()).hexdigest()}
assert {p.stem for p in out.glob('*.stl')}==set(v['parts']),'Unlisted STL export'
assert {p.stem for p in out.glob('*.step') if not p.stem.endswith('-assembly')}==set(v['parts']),'Missing or unlisted individual STEP'

stem='spoolancer-v0.4-touch-8048' if variant=='touch-8048' else 'spoolancer-v0.4.1-headless'
if variant in ('headless-led','oled-led'): stem='spoolancer-v0.7-'+variant
doc=App.openDocument(str(out/(stem+'.FCStd')))
shapes=[o for o in doc.Objects if hasattr(o,'Shape')]
assert all(o.Shape.isValid() for o in shapes)
r['freecad']={'reopened_valid_shapes':len(shapes),'printed_instances':len(doc.getObject('PrintedParts').Group)}
s=Part.read(str(out/(stem+'-assembly.step')));assert s.isValid()
r['step']={'reopened_valid':True,'solids':len(s.Solids)}
if variant=='touch-8048':
    V=App.Vector
    def cylinder(rad,height,x,y,z):return Part.makeCylinder(rad,height,V(x,y,z))
    def nut(x,y,z):
        a=5.5/math.sqrt(3);vs=[V(x+a*math.cos(math.radians(30+60*i)),y+a*math.sin(math.radians(30+60*i)),z) for i in range(6)]
        return Part.Face(Part.makePolygon(vs+[vs[0]])).extrude(V(0,0,2.4)).cut(cylinder(1.5,3,x,y,z-.1))
    def hardware(x,y,seat,length,nutz):
        shaft=cylinder(1.5,length,x,y,seat+.5-length)
        head=cylinder(2.75,3,x,y,seat+.5)
        washer=cylinder(3.2,.5,x,y,seat).cut(cylinder(1.6,1,x,y,seat-.1))
        n=nut(x,y,nutz);n.rotate(V(x,y,0),V(0,0,1),30)
        return Part.makeCompound([shaft,head,washer,n])
    def pose(s):
        s=s.copy();s.rotate(V(0,0,0),V(1,0,0),p['touch_angle_deg']);s.translate(V(*p['touch_origin']));return s
    samples={}
    # Display screws enter from the open rear: shaft runs towards the bezel.
    for x in (-58,58):
        for y in (-33.55,33.55):
            shaft=cylinder(1.5,10,x,y,-7.9)
            head=cylinder(2.75,3,x,y,-10.9)
            washer=cylinder(3.2,.5,x,y,-7.9).cut(cylinder(1.6,1,x,y,-8))
            samples[f'display_M3x10_{x}_{y}']=pose(Part.makeCompound([shaft,head,washer,nut(x,y,-.6)]))
    for x in (-65,65):samples[f'bridge_M3x12_{x}']=hardware(x,-90,41,12,30.8)
    for x in (-78,78):samples[f'front_closure_M3x16_{x}']=hardware(x,-76,41,16,29.4)
    checked=list(doc.getObject('PrintedParts').Group)+[doc.getObject(n) for n in ['REF_touch_PCB','REF_touch_glass','REF_touch_components_allowance']]
    results=[]
    for name,shape in samples.items():
        overlaps={o.Name:round(shape.common(o.Shape).Volume,5) for o in checked if shape.common(o.Shape).Volume>.01}
        results.append({'sample':name,'positive_volume_intersections':overlaps})
        assert not overlaps,(name,overlaps)
    r['touch_fastener_checks']=results
if variant in ('headless-led','oled-led'):
    V=App.Vector
    def cyl(rad,height,x,y,z):return Part.makeCylinder(rad,height,V(x,y,z))
    checks=[]
    printed=list(doc.getObject('PrintedParts').Group)
    for x in (-78,78):
        shaft=cyl(1.5,12,x,-76,26.5)
        head=cyl(2.75,3,x,-76,38.5)
        washer=cyl(3.2,.5,x,-76,38).cut(cyl(1.6,1,x,-76,37.9))
        radius=5.5/math.sqrt(3)
        vertices=[V(x+radius*math.cos(math.radians(60*i)),-76+radius*math.sin(math.radians(60*i)),29.4) for i in range(6)]
        nut=Part.Face(Part.makePolygon(vertices+[vertices[0]])).extrude(V(0,0,2.4)).cut(cyl(1.5,3,x,-76,29.3))
        hardware=Part.makeCompound([shaft,head,washer,nut])
        collisions={o.Name:round(hardware.common(o.Shape).Volume,5) for o in printed if hardware.common(o.Shape).Volume>.01}
        assert not collisions,(x,collisions)
        checks.append({'fastener':'front cover M3x12','x_mm':x,'positive_volume_intersections':collisions})
    r['indicator_front_clamp_checks']=checks
    r['nfc_cable_cover_fastener_note']='One M3x12 nylon screw, washer and captive nut plus rigid tongue. PCB uses printed edge capture. Physical retention remains untested.'
    r['fastener_schedule']=v['fastener_schedule']
    assert r['fastener_schedule']['total_screws']==20
    assert r['fastener_schedule']['dimensional_sizes']==['M3x12','M4x25','M5x12']
    assert not any(v['lid_service']['positive_volume_intersections'].values())
    assert not any(v['structural_joints']['rail_removal'].values())
    assert not any(t['positive_volume_intersections'] for t in v['structural_joints']['stop_checks'])
    r['lid_service']=v['lid_service']
    r['structural_joints']=v['structural_joints']
    r['nfc_mounts']=v['nfc_mounts']
    assert v['nfc_mounts']['digital_clearance_pass']
    assert v['nfc_cable_route']['digital_clearance_pass']
    assert not any(v['oled_insertion']['positive_volume_intersections'].values())
    assert all(x['operation']=='chamfer' for x in v['edge_finishing']['operations'].values())
    assert r['freecad']['printed_instances']==10
    assert v['power_fit']['external_usb_openings'] is False
    assert not v['boot_access']['tool_component_intersections']
    assert v['edge_finishing']['all_operations_remove_material_only']
    assert len(v['edge_finishing']['operations'])>=15
    assert all(t['valid_single_solid'] and t['added_volume_mm3']<.001 and t['removed_volume_mm3']>.001 for t in v['edge_finishing']['operations'].values())
    r['edge_finishing']={'verified_operation_count':len(v['edge_finishing']['operations']),'removal_only':True,'physical_validation':False}
    r['indicator_geometry_checks']={'spool_cases':len(v['spool_envelope_checks']),'dedicated_boot_bore_mm':v['boot_access']['bore_diameter_mm'],'physical_validation':False}
    r['oled_insertion']=v['oled_insertion']
    r['nfc_cable_route_digital_clearance_pass']=v['nfc_cable_route']['digital_clearance_pass']
    r['nominal_unloaded_height_with_feet_mm']=v['geometry_checks']['nominal_unloaded_height_with_feet_mm']

sources=[root/'cable_route.py',root/'indicators.py',root/'parameters.json',root/'build.py',root/'render.py',root/'verify_exports.py']+([target/'parameters.json'] if variant!='headless' else [])
sources += [path for path in [root/'simple_fasteners.py',root/'nfc_mount.py',root/'exploded_render.py',root/'assembly-parts.json'] if path.exists()]
for source in sources:
    r['source_sha256'][str(source.relative_to(root))]=hashlib.sha256(source.read_bytes()).hexdigest()
App.closeDocument(doc.Name)
(out/'export-verification.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
