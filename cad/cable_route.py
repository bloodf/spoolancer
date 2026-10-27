"""Serviceable NFC cable gutter; all wire sizes/bend targets are provisional.

The removable cover opens both the rear cassette and the side of the lower
chute, so no support or wire is trapped in a closed printed tunnel.
"""
import math


def cable_system(p, app, part, box, union, cut, finish, near):
    V=app.Vector
    radius=p['nfc_cable_bundle_diameter']/2
    def line(a,b):return part.makeLine(V(*a),V(*b))
    def arc(a,m,b):return part.Arc(V(*a),V(*m),V(*b)).toShape()
    def tube(edges,r,tangent,frenet=False):
        path=part.Wire(edges)
        profile=part.Wire(part.makeCircle(r,path.Vertexes[0].Point,V(*tangent)))
        return path.makePipeShell([profile],True,frenet).removeSplitter()
    def lower_edges(inner):
        q=math.sqrt(.5)
        return [arc((inner+27.8,0,71),(inner+17.8+10*q,0,71-10*q),(inner+17.8,0,61)),
                arc((inner+17.8,0,61),(inner+17.8-10*q,0,51+10*q),(inner+7.8,0,51)),
                line((inner+7.8,0,51),(inner+7.8,0,40))]
    def pigtail_edges(inner,h):
        q=math.sqrt(.5)
        return [line((inner+7.8,0,h-17),(inner+7.8,10,h-17)),
                arc((inner+7.8,10,h-17),(inner+17.8-10*q,10+10*q,h-17),(inner+17.8,20,h-17)),
                arc((inner+17.8,20,h-17),(inner+17.8+10*q,20,h-7-10*q),(inner+27.8,20,h-7))]
    def moving_wire(width,h,r=radius):
        inner=(width+p['guide_clearance_total'])/2
        zc=172+(h-175)/2
        edges=pigtail_edges(inner,h)+[line((inner+27.8,20,h-7),(inner+27.8,20,zc)),
            arc((inner+27.8,20,zc),(inner+27.8,10,zc+10),(inner+27.8,0,zc)),
            line((inner+27.8,0,zc),(inner+27.8,0,71))]+lower_edges(inner)
        return tube(edges,r,(0,1,0))
    def lower_profile(inner,y,depth):
        pts=[(2.5,42),(12,42),(12,48),(32.9,68.9),(32.9,84),(22.7,84),(22.7,73.5),(2.5,53.3)]
        vs=[V(inner+x,y,z) for x,z in pts]
        return part.Face(part.makePolygon(vs+[vs[0]])).extrude(V(0,depth,0))
    channel_cache={}
    def channel(width):
        if width in channel_cache:return tuple(s.copy() for s in channel_cache[width])
        inner=(width+p['guide_clearance_total'])/2
        # Upper cassette opens towards +X; its entry slot accepts the moving
        # pod's overlapping hood without touching it throughout vertical travel.
        upper=box(8.4,30.5,109,inner+26.9,11.25,78)
        upper=upper.cut(box(12,27.3,105.8,inner+30.3,11.25,79.6))
        upper=upper.cut(box(4,10,100,inner+23.8,20,68))
        upper=upper.cut(box(10,10,14,inner+29.3,20,70))
        upper=upper.cut(part.makeCylinder(3,8,V(inner+27.8,0,74)))
        # Two outside ears keep cover screws away from the U-shaped cable.
        ears=[part.makeCylinder(3.5,8.4,V(inner+22.7,-8,z),V(1,0,0)) for z in (90,179)]
        ear_links=[box(8.4,5,7,inner+26.9,-5,z-3.5) for z in (90,179)]
        # Bridge runs outside the pod's maximum Y envelope. Lower gutter also
        # overlaps the existing guide web at x=inner+2.5..4, z50..60.
        bridge=union([box(23,4,3,inner+14.5,27.5,181),box(4,5,3,inner+24.7,25,181)])
        gutter=lower_profile(inner,-4,8)
        rear=box(1.6,30.5,109,inner+32.1,11.25,78)
        covers=[rear,lower_profile(inner,.2,3.8)]
        for z in (90,179):
            covers += [part.makeCylinder(3.5,1.6,V(inner+31.3,-8,z),V(1,0,0)),box(1.6,6,7,inner+32.1,-4.5,z-3.5)]
        cover=union(covers)
        # Explicit 0.2 mm split seam; a single cover exposes the whole chute.
        body=union([upper,gutter,bridge]+ears+ear_links)
        body=body.cut(cover)
        body=body.cut(lower_profile(inner,0,.2))
        cavity=union([tube(lower_edges(inner),3,(0,0,-1)),part.makeCylinder(3,15,V(inner+27.8,0,71))])
        body=body.cut(cavity);cover=cover.cut(cavity)
        cover=cover.cut(box(5.2,10,18,inner+1.6,0,49))
        pilots=[part.makeCylinder(1.05,10,V(inner+22,-8,z),V(1,0,0)) for z in (90,179)]
        clear=[part.makeCylinder(1.4,4,V(inner+30.5,-8,z),V(1,0,0)) for z in (90,179)]
        body=union([body,box(1.3,8,5,inner+24.95,0,84)])
        relief=[box(5,1.6,2,inner+24,y,85.5) for y in (-3.2,3.2)]
        body=cut(body,pilots+relief);cover=cut(cover,clear)
        # Chamfer only selected exposed straight rims. Cable bend radii remain
        # functional internal geometry, not exterior styling.
        body=finish('nfc_channel_front_vertical_bevels',body,'chamfer',p['finish_channel_chamfer'],lambda b:near(b.XMin,inner+22.7) and near(b.XLength,0) and near(b.YLength,0) and b.ZLength>50 and (near(b.YMin,-4) or near(b.YMin,26.5)))
        body=finish('nfc_channel_lower_outer_bevels',body,'chamfer',p['finish_channel_chamfer'],lambda b:near(b.YMin,-4) and near(b.YLength,0) and b.ZMax<73.6)
        cover=finish('nfc_channel_cover_vertical_bevels',cover,'chamfer',p['finish_channel_chamfer'],lambda b:near(b.XMin,inner+32.9) and near(b.XLength,0) and near(b.YLength,0) and b.ZLength>50 and (near(b.YMin,-4) or near(b.YMin,26.5)))
        cover=finish('nfc_channel_cover_lower_bevels',cover,'chamfer',p['finish_channel_chamfer'],lambda b:near(b.YMin,4) and near(b.YLength,0) and b.ZMax<73.6 and (b.XMin>=inner+11.9 or b.ZMax<48.1))
        body=finish('nfc_channel_top_rim',body,'chamfer',p['finish_channel_chamfer'],lambda b:near(b.ZMin,187) and near(b.ZMax,187) and (near(b.XMin,inner+22.7) or near(b.YMin,-4) or near(b.YMax,26.5)))
        cover=finish('nfc_channel_cover_top_rim',cover,'chamfer',p['finish_channel_chamfer'],lambda b:near(b.ZMin,187) and near(b.ZMax,187))
        result=(body.removeSplitter(),cover.removeSplitter())
        channel_cache[width]=tuple(s.copy() for s in result)
        return result
    def hood(width,h):
        inner=(width+p['guide_clearance_total'])/2
        outer=box(8.8,8,12,inner+19.6,20,h-21)
        outer=outer.cut(tube(pigtail_edges(inner,h),3,(0,1,0)))
        # Open rear and top access under the removable cassette cover; no
        # enclosed support is required inside this moving entrance hood.
        outer=outer.cut(box(10,5,9,inner+20,20,h-17))
        # Two accessible tie slots retain the pigtail at the moving cover.
        outer=cut(outer,[box(2,2,16,inner+18,y,h-23) for y in (16.8,23.2)])
        outer=finish('nfc_pigtail_hood_outer_bevels',outer,'chamfer',p['finish_channel_chamfer'],lambda b:near(b.YLength,0) and near(b.ZLength,0) and b.XLength>1 and (near(b.YMin,16) or near(b.YMin,24)) and (near(b.ZMin,h-21) or near(b.ZMin,h-9)))
        return outer.removeSplitter()
    def cover_port(width,h):
        inner=(width+p['guide_clearance_total'])/2
        return tube(pigtail_edges(inner,h),3,(0,1,0))
    def base_flex(width,r=radius):
        # A loose cable can occupy this nominal bowed path below the moving
        # chute. Width changes alter the drawn path length; it is not a kinematic
        # or force model. Extra slack must be dressed and physically zero-tested.
        inner=(width+p['guide_clearance_total'])/2;x=inner+7.8
        bow=part.BezierCurve()
        bow.setPoles([V(x,0,28),V(x,0,10),V(24,0,10),V(24,0,20)])
        edges=[line((x,0,40),(x,0,28)),bow.toBSpline().toShape(),
               arc((24,0,20),(24,5,25),(24,10,20)),line((24,10,20),(24,10,9.8))]
        return tube(edges,r,(0,0,-1),True)
    def verify(params,parts,refs,guide,pod,overlap):
        widths=[params['spool_width_min'],80,params['spool_width_max']]
        heights=[params['nfc_height_min'],params['nfc_height_reference'],params['nfc_height_max']]
        cases=[]
        fixed={k:s for k,s in parts.items() if k in ('base','top_cover','carrier','rim_rail')}
        for width in widths:
            body=guide(width,nfc=True);rear=channel(width)[1]
            for height in heights:
                podsh,back,pcb,components,_=pod(width,height)
                wire=moving_wire(width,height);tail=base_flex(width)
                checks={**fixed,'nfc_guide':body,'nfc_cable_cover':rear,'nfc_pod':podsh,'nfc_pod_cover':back,'PN532_PCB':pcb,'PN532_components':components}
                hits={label+'/'+name:overlap(shape,solid) for label,shape in [('wire',wire),('base_tail',tail)] for name,solid in checks.items() if overlap(shape,solid)>.01}
                cases.append({'width_mm':width,'reader_center_mm':height,'positive_volume_intersections':hits})
        # Conservative continuous-height envelopes are larger than the true
        # moving solids. Passing them proves clearance throughout 90..175 mm,
        # rather than merely interpolating the three drawn examples.
        inner=(80+params['guide_clearance_total'])/2
        travel=params['nfc_height_max']-params['nfc_height_min']
        h0=params['nfc_height_min'];h1=params['nfc_height_max']
        envelopes={
            'pod_body_and_back':box(15.2,49.1,travel+46.8,inner+7.6,0,h0-23.4),
            'moving_hood':box(8.8,8,travel+12,inner+19.6,20,h0-21),
            'clamp_tab_minus':box(2,12,travel+10,inner+7,-params['nfc_mast_y'],h0-5),
            'clamp_tab_plus':box(2,12,travel+10,inner+7,params['nfc_mast_y'],h0-5),
        }
        moving_hits={label+'/'+name:overlap(shape,solid) for label,shape in envelopes.items() for name,solid in [('nfc_guide',guide(80,nfc=True)),('nfc_cable_cover',channel(80)[1])] if overlap(shape,solid)>.01}
        # The U-loop always remains in the same interior rectangular sweep.
        # Each monotonic quarter-arc pigtail segment is enclosed by a box; its
        # Z span includes the entire 85 mm adjustment and the 4 mm bundle.
        cable_boxes=[box(4,4,103,inner+27.8,0,71),box(4,4,91,inner+27.8,20,83),box(4,24,56.5,inner+27.8,10,127.5),
                     box(4,14,travel+4,inner+7.8,5,h0-19)]
        for i in range(12):
            a=i*math.pi/24;b=(i+1)*math.pi/24
            xa=inner+17.8-10*math.cos(a);xb=inner+17.8-10*math.cos(b)
            ya=10+10*math.sin(a);yb=10+10*math.sin(b)
            cable_boxes.append(box(xb-xa+4,yb-ya+4,travel+4,(xa+xb)/2,(ya+yb)/2,h0-19))
            xa=inner+17.8+10*math.sin(a);xb=inner+17.8+10*math.sin(b)
            za=h0-7-10*math.cos(a);zb=h1-7-10*math.cos(b)
            cable_boxes.append(box(xb-xa+4,4,zb-za+4,(xa+xb)/2,20,za-2))
        # A 5.5 mm diameter head/tool corridor runs outward from each clamp
        # through its complete height range, with the cable cover installed.
        tool_sweeps=[box(32,5.5,travel+5.5,inner+24,y,h0-2.75) for y in (-params['nfc_mast_y'],params['nfc_mast_y'])]
        tool_hits={str(i)+'/'+name:overlap(shape,solid) for i,shape in enumerate(tool_sweeps) for name,solid in [('nfc_guide',guide(80,nfc=True)),('nfc_cable_cover',channel(80)[1])] if overlap(shape,solid)>.01}
        continuous_cable_hits={str(i)+'/'+name:overlap(shape,solid) for i,shape in enumerate(cable_boxes) for name,solid in [('nfc_guide',guide(80,nfc=True)),('nfc_cable_cover',channel(80)[1])] if overlap(shape,solid)>.01}
        # Translation over all widths: lower outlet is entirely inside the open
        # carrier bay. Fixed-base flex bounds deliberately exceed the drawn tail.
        xlo=(widths[0]+params['guide_clearance_total'])/2+7.8
        xhi=(widths[-1]+params['guide_clearance_total'])/2+7.8
        tail_envelopes={
            'outlet_vertical_sweep':box(xhi-xlo+4,4,16,(xlo+xhi)/2,0,26),
            'lower_bow_space':box(xhi-22+2,4,22.3,(22+xhi+2)/2,0,7.7),
            'fixed_end_slack_space':box(4,16,21.3,24,6,7.7),
            'noncontact_chute_sweep':box(xhi-xlo+9.5,8,14,(xlo+xhi)/2-.55,0,42),
        }
        reference_fixed={k:s for k,s in refs.items() if k in ('REF_load_cell','REF_HX711_PCB','REF_ESP32_PCB_estimate','REF_ESP32_module_estimate','REF_load_cell_wire_allowance')}
        tail_hits={label+'/'+name:overlap(shape,solid) for label,shape in tail_envelopes.items() for name,solid in {**fixed,**reference_fixed}.items() if overlap(shape,solid)>.01}
        return {'physical_validation':False,'digital_clearance_pass':not(any(c['positive_volume_intersections'] for c in cases) or moving_hits or continuous_cable_hits or tail_hits or tool_hits),
            'bundle_diameter_mm_provisional':2*radius,'moving_reader_route_bend_radius_mm_design_target':10,'illustrative_base_tail_small_arc_radius_mm':5,
            'reader_center_range_mm':[h0,h1],'spool_width_range_mm':[widths[0],widths[-1]],
            'constant_moving_route_centerline_length_mm':round(126+3*math.pi*10,6),
            'cassette_loop_centerline_length_mm':round(105+math.pi*10,6),
            'discrete_actual_path_cases':cases,'continuous_height_conservative_solid_sweep_intersections':moving_hits,
            'continuous_height_conservative_cable_sweep_intersections':continuous_cable_hits,
            'continuous_width_fixed_boundary_sweep_intersections':tail_hits,'continuous_height_clamp_access_sweep_intersections':tool_hits,'clamp_access_diameter_mm':5.5,
            'tail_continuous_bounds_method':'The cubic Bezier lies inside its control-point convex hull; control x spans the full width interval, and the 2 mm bundle radius expands that hull. The R5 semicircle and straight ends have fixed bounds. The checked boxes enclose those bounds.',
            'tail_model':'Nominal routing and a conservative free-space envelope; no constant-length flex simulation or cable spring-force measurement. Dress slack within this space and re-zero/check at every adjustment.',
            'maintenance':'Two M2.5x10 prototype cover screws; one cover exposes rear cassette and lower side gutter. Tie slots at moving cover, guide leg and fixed-base saddle.',
            'concealment':'Upper route behind guide/pod and removable cover; lower chute overlaps the open carrier bay without touching fixed lid. The flexible tail remains below the lid inside the base.',
            'no_rigid_loadcell_bypass':True,'actual_wire_stiffness_and_weighing_effect_verified':False}
    return {'channel':channel,'wire':moving_wire,'hood':hood,'port':cover_port,'base_flex':base_flex,
            'lower_edges':lower_edges,'tube':tube,'radius':radius,'verify':verify}
