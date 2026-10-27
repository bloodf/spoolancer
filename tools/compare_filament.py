"""Estimate material for both revisions with the same local OrcaSlicer settings.

Runs offline slicing only. Never sends a job to a printer. This is a comparison,
not a qualified printer profile or a measurement of printed parts.
"""
import json,subprocess,sys,zipfile,re,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PROFILES=Path('/Applications/OrcaSlicer.app/Contents/Resources/profiles/BBL')
EXE='/Applications/OrcaSlicer.app/Contents/MacOS/OrcaSlicer'
OUT=ROOT/'analysis/material-comparison'
OUT.mkdir(parents=True,exist_ok=True)
index={}
for p in PROFILES.rglob('*.json'):
    try:
        d=json.loads(p.read_text())
        if 'name' in d:index[d['name']]=(p,d)
    except (ValueError,OSError):pass

def resolve(name,seen=()):
    if name in seen:raise ValueError('Preset inheritance cycle')
    p,d=index[name]
    r=resolve(d['inherits'],seen+(name,)) if d.get('inherits') else {}
    r.update(d);r.pop('inherits',None)
    return r

machine=resolve('Bambu Lab P1S 0.4 nozzle')
process=resolve('0.20mm Standard @BBL X1C')
filament=resolve('Generic PETG @BBL P1P')
process.update(curr_bed_type='Textured PEI Plate',wall_loops='4',top_shell_layers='5',bottom_shell_layers='5',sparse_infill_density='20%',sparse_infill_pattern='gyroid',enable_support='0',brim_type='no_brim')
filament['compatible_printers']=['Bambu Lab P1S 0.4 nozzle']
for name,data in [('machine',machine),('process',process),('filament',filament)]:
    (OUT/(name+'.json')).write_text(json.dumps(data))


def slice_one(revision,name,path):
    folder=OUT/revision/name;folder.mkdir(parents=True,exist_ok=True)
    project=folder/'comparison.3mf'
    args=[EXE,'--datadir',str(OUT/'isolated-settings'),'--debug','1','--load-settings',str(OUT/'machine.json')+';'+str(OUT/'process.json'),'--load-filaments',str(OUT/'filament.json'),'--arrange','1','--orient','0','--ensure-on-bed','--slice','0','--outputdir',str(folder),'--export-3mf',project.name,str(path)]
    result=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    (folder/'slicer.log').write_text(result.stdout)
    if result.returncode:
        raise RuntimeError(f'{revision}/{name}: slicer exit {result.returncode}: {result.stdout[-1500:]}')
    with zipfile.ZipFile(project) as z:
        gcodes=[z.read(n).decode(errors='replace') for n in z.namelist() if n.endswith('.gcode')]
    if not gcodes:raise RuntimeError('No G-code in comparison project')
    grams=[];metres=[]
    for text in gcodes:
        g=re.findall(r'^; (?:total )?filament used \[g\] = ([\d., ]+)',text,re.M)
        mm=re.findall(r'^; (?:total )?filament used \[mm\] = ([\d., ]+)',text,re.M)
        if not g:raise RuntimeError('No material statistics')
        grams.append(sum(float(v) for v in g[0].split(',')))
        metres.append(sum(float(v) for v in mm[0].split(','))/1000 if mm else None)
    return {'grams':sum(grams),'metres':sum(metres) if all(m is not None for m in metres) else None}

if __name__=='__main__':
    rev=sys.argv[1] if len(sys.argv)>1 else 'v0.2'
    folders={'v0.2':'archive/v0.2-upright/cad','v0.3':'archive/v0.3-compact/cad','v0.4':'cad/variants/headless','v0.4-headless':'cad/variants/headless','v0.4-touch':'cad/variants/touch-8048'}
    if rev not in folders: raise ValueError('Unknown revision; use v0.2, v0.3, v0.4-headless or v0.4-touch')
    cad=ROOT/folders[rev]
    report=json.loads((cad/'exports/validation.json').read_text())
    results={}
    only=sys.argv[2] if len(sys.argv)>2 else None
    for name,part in report['parts'].items():
        if name.startswith('fit_') or (only and name!=only):continue
        result=slice_one(rev,name,cad/'exports'/(name+'.stl'))
        result['quantity']=part['quantity']
        result['stl_sha256']=hashlib.sha256((cad/'exports'/(name+'.stl')).read_bytes()).hexdigest()
        results[name]=result
        print(rev,name,result,flush=True)
    summary={'revision':rev,'slicer':'OrcaSlicer 2.4.1','reference_printer':'Bambu Lab P1S 0.4 nozzle','material':'Generic PETG','layer_height_mm':.2,'walls':4,'top_bottom_layers':5,'infill':'20% gyroid','supports':False,'brim':False,'physical_validation':False,'parts':results,'total_grams':round(sum(x['grams']*x['quantity'] for x in results.values()),2)}
    (OUT/(rev+('-'+only if only else '')+'.json')).write_text(json.dumps(summary,indent=2)+'\n')
    print('Total estimated grams',summary['total_grams'],flush=True)
