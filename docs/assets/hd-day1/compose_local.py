from pathlib import Path
from PIL import Image
import json
R=Path(__file__).resolve().parent;count=0
for j in json.loads((R/'local-jobs.json').read_text()):
 src=R/'source-only-generated'/(j['id']+'.png')
 if not src.exists():src=R/'local-generated'/(j['id']+'.png')
 dst=R/'localized'/(j['id']+'.png')
 if not src.exists():continue
 override_stamp=(R/'registered-overrides.json').stat().st_mtime if (R/'registered-overrides.json').exists() else 0
 if dst.exists() and dst.stat().st_mtime>=max(src.stat().st_mtime,(R/'generated'/(j['reference']+'.png')).stat().st_mtime,override_stamp):continue
 overrides=json.loads((R/'registered-overrides.json').read_text()) if (R/'registered-overrides.json').exists() else []
 selected=[o for o in overrides if o['id']==j['id']]
 w,h=j['size'];b=j['local_bbox'];base=Image.open(R/'generated'/(j['reference']+'.png')).convert('RGB').resize((w*4,h*4),Image.Resampling.LANCZOS);crop=Image.open(src).convert('RGB').resize(((b[2]-b[0])*4,(b[3]-b[1])*4),Image.Resampling.LANCZOS);base.paste(crop,(b[0]*4,b[1]*4))
 for o in selected:
  q=o['bbox'];im=Image.open(R/o['image']).convert('RGB').resize(((q[2]-q[0])*4,(q[3]-q[1])*4),Image.Resampling.LANCZOS);base.paste(im,(q[0]*4,q[1]*4))
 temp=dst.with_suffix('.tmp');base.save(temp,format='PNG');temp.replace(dst);count+=1
print(count,'localized active canvases composed')

js=json.loads((R/'jobs.json').read_text());status={'ready':[j['id'] for j in js if (R/'generated'/(j['id']+'.png')).exists() or (R/'localized'/(j['id']+'.png')).exists()],'localized':[j['id'] for j in js if (R/'localized'/(j['id']+'.png')).exists()],'source_only_corrections_ready':len(list((R/'source-only-generated').glob('*.png'))),'note':'Generation coverage is not phase-quality approval.'};temp=R/'progress.tmp';temp.write_text(json.dumps(status));temp.replace(R/'progress.json')
