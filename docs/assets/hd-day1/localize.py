"""Construct registered, enlarged animation-region references for imagegen edits."""
from pathlib import Path
from PIL import Image
import json
R=Path(__file__).resolve().parent;js=json.loads((R/'jobs.json').read_text());views={(v['room'],v['group']):v for v in json.loads((R/'views.json').read_text())};(R/'local-inputs').mkdir(exist_ok=True);(R/'local-references').mkdir(exist_ok=True)
families={}
for j in js:
 if j['kind']!='entry':continue
 for e in j['entries']:
  if e['control'] is not None:families.setdefault((e['room'],e['group'],e['control']),[]).extend(j.get('changed_regions',[]))
result=[]
for j in js:
 if j['kind']!='entry':continue
 regions=list(j.get('changed_regions',[]))
 for e in j['entries']:
  if e['control'] is not None:regions+=families.get((e['room'],e['group'],e['control']),[])
 if not regions:continue
 w,h=j['size'];b=[max(0,min(x[0] for x in regions)-12),max(0,min(x[1] for x in regions)-12),min(w,max(x[2] for x in regions)+12),min(h,max(x[3] for x in regions)+12)]
 src=Image.open(j['source']).convert('RGB');ref=Image.open(R/'generated'/(j['reference']+'.png')).convert('RGB').resize((w*4,h*4),Image.Resampling.LANCZOS)
 # Enlarge native original for legible local pose; never derive geometry from generated reference.
 crop=src.crop(b).resize(((b[2]-b[0])*4,(b[3]-b[1])*4),Image.Resampling.NEAREST);crop.save(R/'local-inputs'/(j['id']+'.png'));ref.crop(tuple(x*4 for x in b)).save(R/'local-references'/(j['id']+'.png'))
 result.append({**j,'local_bbox':b,'local_source':str(R/'local-inputs'/(j['id']+'.png')),'local_reference':str(R/'local-references'/(j['id']+'.png'))})
(R/'local-jobs.json').write_text(json.dumps(result,indent=2));print(len(result),'localized source contexts')
