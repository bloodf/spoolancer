"""Current LED variants: serviceable NFC seats and four nylon M3 x 12 screws.

Hardware envelopes use a 5.5 mm head/AF nut and 0.5 mm washer. Delivered
hardware, PCB margins, foam compression and printed retention remain untested.
"""
import math


def _hex(part, app, x, y, z, af, length):
    r=af/math.sqrt(3)
    points=[app.Vector(x,y+r*math.cos(math.radians(30+60*i)),z+r*math.sin(math.radians(30+60*i))) for i in range(6)]
    return part.Face(part.makePolygon(points+[points[0]])).extrude(app.Vector(length,0,0))


def _hardware(app,part,union,x,y,z,nut_x):
    v=app.Vector
    shaft=part.makeCylinder(1.5,12,v(x-12,y,z),v(1,0,0))
    head=part.makeCylinder(2.75,3,v(x,y,z),v(1,0,0))
    washer=part.makeCylinder(3.5,.5,v(x-.5,y,z),v(1,0,0)).cut(part.makeCylinder(1.6,.5,v(x-.5,y,z),v(1,0,0)))
    nut=_hex(part,app,nut_x,y,z,5.5,2.4).cut(part.makeCylinder(1.5,2.4,v(nut_x,y,z),v(1,0,0)))
    return union([shaft,head]),washer,nut


def hardware_for_setting(p,app,part,box,union,width,height):
    inner=(width+p['guide_clearance_total'])/2
    groups={}
    for side,y in [('minus',-p['nfc_mast_y']),('plus',p['nfc_mast_y'])]:
        groups['height_'+side]=_hardware(app,part,union,inner+12.5,y,height,inner+2.1)
    groups['pod_cover']=_hardware(app,part,union,inner+18.2,-27,height-18,inner+6.4)
    return groups


def make_pod(p,app,part,box,cyl,union,cut,finish,near,cable,width,height):
    v=app.Vector
    inner=(width+p['guide_clearance_total'])/2
    oy,oz=p['nfc_pod_outer'];depth=p['nfc_pod_depth'];wall=1.6
    xc=lambda r,length,x,y,z:part.makeCylinder(r,length,v(x,y,z),v(1,0,0))
    pod=box(depth,oy,oz,inner+depth/2,0,height-oz/2)
    pod=pod.cut(box(depth+1,oy-2*wall,oz-2*wall,inner+p['nfc_wall']+(depth+1)/2,0,height-oz/2+wall))
    cover=box(p['nfc_back_thickness'],oy,oz,inner+depth+p['nfc_back_thickness']/2,0,height-oz/2)
    def corner(b):return near(b.YLength,0) and near(b.ZLength,0) and near(abs(b.YMin),oy/2) and (near(b.ZMin,height-oz/2) or near(b.ZMin,height+oz/2))
    pod=finish('nfc_pod_external_corners',pod,'chamfer',p['finish_nfc_corner_chamfer'],corner)
    cover=finish('nfc_back_cover_corners',cover,'chamfer',p['finish_nfc_corner_chamfer'],corner)
    cover=finish('nfc_back_cover_outer_rim',cover,'chamfer',p['finish_nfc_back_edge_chamfer'],
        lambda b:near(b.XMin,inner+depth+p['nfc_back_thickness']) and near(b.XLength,0) and (near(b.YMin,-oy/2) or near(b.YMax,oy/2) or near(b.ZMin,height-oz/2) or near(b.ZMax,height+oz/2)))
    additions=[];cuts=[]
    for y in (-p['nfc_mast_y'],p['nfc_mast_y']):
        additions.append(box(6,12,10,inner+9,y,height-5))
        cuts.append(xc(1.7,8,inner+5,y,height))
    # Four small edge seats preserve the original 0.8 mm antenna air gap.
    # Side and end stops leave 0.3 mm nominal PCB play; there are no PCB screws.
    for y in (-20.5,20.5):
        for dz in (-15,15):additions.append(box(.8,1.7,5,inner+1.6,y,height+dz-2.5))
    for y in (-22.3,22.3):additions.append(box(4.3,1.3,30,inner+3.35,y,height-15))
    for dz in (-21.15,21.15):additions.append(box(4.3,32,1.3,inner+3.35,0,height+dz-.65))
    # Captive nut enters from the -Y outside edge. Its axial roof retains it
    # against screw pullout; the screw's 6.2 mm tip clears the PCB back at 3.6.
    ear=box(7.6,9,8.6,inner+9.8,-26.5,height-22.3)
    ear=finish('nfc_latch_ear_profile',ear,'chamfer',1.0,lambda b:b.XLength>7 and near(b.YLength,0) and near(b.ZLength,0))
    additions.append(ear)
    cuts += [xc(1.7,13,inner+4,-27,height-18),
             _hex(part,app,inner+6.3,-27,height-18,5.8,2.6),
             box(2.6,10,6.8,inner+7.6,-32,height-21.4)]
    ear=box(4.1,9,8.6,inner+15.65,-26.5,height-22.3)
    ear=finish('nfc_cover_latch_ear_profile',ear,'chamfer',1.0,lambda b:b.XLength>2 and near(b.YLength,0) and near(b.ZLength,0))
    cover=union([cover,ear])
    cover=cover.cut(xc(1.7,7,inner+12.5,-27,height-18))
    # A rigid upper tongue releases by sliding the rear cover down 4 mm, then
    # outwards. It uses no living hinge or interference snap fit.
    additions.append(box(3.6,10,1.3,inner+11.8,0,height+20.5))
    cuts.append(box(2.3,8.6,3.4,inner+8.85,0,height+20.2))
    tongue=union([box(1.7,8,4.1,inner+8.85,0,height+19),
                  box(7.2,8,1.2,inner+11.6,0,height+19)])
    cover=union([cover,tongue])
    # Rear fingers retain only the board's component-free side margins. The
    # 0.3 mm space is for compressible foam, not a plastic interference clamp.
    for y,dz in [(-19.8,13),(19.8,-13),(19.8,13)]:
        cover=union([cover,box(9.7,1.8,4,inner+8.75,y,height+dz-2)])
    pod=cut(union([pod]+additions),cuts)
    port=cable['port'](width,height)
    pod=pod.cut(port).removeSplitter()
    cover=union([cover,cable['hood'](width,height)]).cut(port).removeSplitter()
    pcb=box(p['nfc_board'][2],p['nfc_board'][0],p['nfc_board'][1],inner+2+p['nfc_board'][2]/2,0,height-p['nfc_board'][1]/2)
    pcb=cut(pcb,[xc(1.524,4,inner+1,y,height+dz) for y,dz in [(-13.9,-12.6),(13.9,12.6)]])
    components=box(2.4,28,22,inner+4.8,0,height-11)
    wires=box(7,12,8,inner+7.1,0,height-20.2)
    return pod,cover,pcb,components,wires


def verify_mounts(p,app,part,box,union,pod,guide,cable,overlap,parts=None,refs=None):
    checks=[]
    for width in (p['spool_width_min'],80,p['spool_width_max']):
        for height in (p['nfc_height_min'],p['nfc_height_reference'],p['nfc_height_max']):
            body,cover,pcb,components,wires=pod(width,height)
            solids={'pod':body,'pod_cover':cover,'PCB':pcb,'components':components,'wires':wires,'guide':guide(width,nfc=True),'cable_cover':cable['channel'](width)[1],'cable':cable['wire'](width,height)}
            hits={}
            for name,group in {**hardware_for_setting(p,app,part,box,union,width,height),'cable_cover':cable['hardware'](width)}.items():
                for kind,shape in zip(('screw','washer','nut'),group):
                    for label,solid in solids.items():
                        vol=overlap(shape,solid)
                        if vol>.01:hits[name+'/'+kind+'/'+label]=vol
            checks.append({'width_mm':width,'reader_center_mm':height,'intersections':hits})
    # One explicit maintenance pose. The cable channel cover is removed and
    # pigtail strain relief released before the pod cover is translated.
    width=50;height=p['nfc_height_reference'];inner=(width+p['guide_clearance_total'])/2
    body,cover,pcb,components,wires=pod(width,height)
    fixed={k:s for k,s in (parts or {}).items() if k in ('base','top_cover','carrier','rim_rail','rim_rail_front')}
    fixed.update({'pod':body,'PCB':pcb,'components':components,'guide':guide(width,nfc=True)})
    whole=union([body,cover,pcb,components,wires]+list(hardware_for_setting(p,app,part,box,union,width,height)['pod_cover']))
    detached={}
    for phase in ('up','out'):
        for i in range(61):
            shape=whole.copy();shape.translate(app.Vector(0,0,i) if phase=='up' else app.Vector(20*i/60,0,60))
            for label,solid in {k:s for k,s in fixed.items() if k not in ('pod','PCB','components')}.items():
                vol=overlap(shape,solid)
                if vol>.01:detached[phase+'_'+str(i)+'/'+label]=vol
    # After removing both height screws and lifting the complete pod clear of
    # the mast, open it on the bench. The fixed cassette obstructs an in-place
    # rear-cover extraction; that unavailable path is deliberately not promised.
    removal={}
    bench={'pod':body,'PCB':pcb,'components':components}
    for i in range(17):
        shape=cover.copy();shape.translate(app.Vector(0,0,-4*i/16))
        for label,solid in bench.items():
            vol=overlap(shape,solid)
            if vol>.01:removal['down_'+str(i)+'/'+label]=vol
    for i in range(1,17):
        shape=cover.copy();shape.translate(app.Vector(20*i/16,0,-4))
        for label,solid in bench.items():
            vol=overlap(shape,solid)
            if vol>.01:removal['out_'+str(i)+'/'+label]=vol
    # PCB and components withdraw straight out with the rear cover removed.
    pcb_removal={}
    for i in range(17):
        for label,original in [('PCB',pcb),('components',components)]:
            shape=original.copy();shape.translate(app.Vector(20*i/16,0,0))
            vol=overlap(shape,body)
            if vol>.01:pcb_removal[label+'_'+str(i)]=vol
    nut_loading={};pullout={};tool_hits={}
    for name,group in hardware_for_setting(p,app,part,box,union,width,height).items():
        screw,washer,nut=group
        for i in range(21):
            sh=nut.copy()
            sh.translate(app.Vector(0,-12*i/20,0) if name=='pod_cover' else app.Vector(0,0,(188-height)*i/20))
            for label,solid in {'pod':body,'guide':fixed['guide'],'PCB':pcb,'components':components}.items():
                vol=overlap(sh,solid)
                if vol>.01:nut_loading[name+'/'+str(i)+'/'+label]=vol
        pulled=nut.copy();pulled.translate(app.Vector(3,0,0))
        pullout[name]=overlap(pulled,body if name=='pod_cover' else fixed['guide'])>.01
        y=-27 if name=='pod_cover' else (-p['nfc_mast_y'] if name=='height_minus' else p['nfc_mast_y'])
        z=height-18 if name=='pod_cover' else height
        x=inner+(21.2 if name=='pod_cover' else 15.5)
        tool=part.makeCylinder(2.75,35,app.Vector(x,y,z),app.Vector(1,0,0))
        for label,solid in {**fixed,'cover':cover,'channel_cover':cable['channel'](width)[1]}.items():
            vol=overlap(tool,solid)
            if vol>.01:tool_hits[name+'/'+label]=vol
    return {'physical_validation':False,'digital_clearance_pass':not(any(c['intersections'] for c in checks) or detached or removal or pcb_removal or nut_loading or tool_hits) and all(pullout.values()),
        'hardware':'Three nylon M3 x 12, three 0.5 mm washers and three M3 captive nuts on the moving pod; the fourth identical set retains the cable cover.',
        'board_retention':'Four edge seats, 0.3 mm edge clearance and three cover fingers with 0.3 mm foam allowance. No board screws; verify delivered component-free margins.',
        'screw_dimensions_mm':{'length':12,'head_diameter':5.5,'head_height':3,'nut_af':5.5,'nut_thickness':2.4,'washer_outer_diameter':7,'washer_thickness':.5},
        'actual_hardware_cases':checks,'nut_loading_sampled_intersections':nut_loading,'nut_axial_pullout_blocked_in_geometry':pullout,'tool_access_intersections':tool_hits,'whole_pod_removal_sampled_intersections':detached,'pod_cover_removal_sampled_intersections':removal,'pcb_removal_sampled_intersections':pcb_removal,
        'service_pose':{'spool_removed':True,'width_mm':width,'reader_center_mm':height,'channel_cover_removed':True,'cable_unplugged_and_freed_from_channel':True,'height_screws_and_washers_removed':True,'height_nuts_unthreaded_retained_in_mast':True,'whole_pod_translation_mm':[[0,0,60],[20,0,0]],'reader_center_after_lift_mm':215,'pod_cover_opened_on_bench':True,'pod_cover_translation_mm':[[0,0,-4],[20,0,0]]},
        'retention':'Rigid upper tongue and lower M3 clamp; nut side-entry pocket retains axial pullout. Clearance/removal sampling is geometric evidence, not strength, fatigue or foam-pressure validation.',
        'print_note':'Print pod cover positive-Y side edge down (-90 degrees about X); use a brim and local support beneath PCB fingers and cable hood. This is not a support-free print.'}
