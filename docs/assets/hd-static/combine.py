"""Combine the static expansion with the reviewed Day1 pack, keeping Day1 bytes."""
from pathlib import Path
import hashlib,json,shutil
R=Path(__file__).resolve().parent
old=R.parent/'hd-day1'/'pack'
new=R/'pack'
dest=R/'pack-with-day1';dest.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
records={}
for source in (new,old):
 for t in json.loads((source/'manifest.json').read_text())['atlases']:
  key=t['key'];p=source/(key+'.bmp');target=dest/p.name
  if not target.exists() or sha(target)!=sha(p):shutil.copy2(p,target)
  records[key]={**t,'pack_origin':'reviewed-day1' if source==old else 'static-expansion'}
protected=[]
for t in json.loads((old/'manifest.json').read_text())['atlases']:
 name=t['key']+'.bmp';assert sha(old/name)==sha(dest/name)
 protected.append({'key':t['key'],'sha256':sha(dest/name)})
manifest={'model':'OpenAI built-in imagegen','scope':'Reviewed Day1 assets plus static-map expansion; original masks, depth and gameplay retained. Static expansion visual review pending.','atlases':list(records.values())}
(dest/'manifest.json').write_text(json.dumps(manifest,indent=2))
(R/'day1-preservation.json').write_text(json.dumps({'all_reviewed_day1_atlases_byte_identical':True,'records':protected},indent=2))
print(len(records),'unique atlases;',len(protected),'Day1 records preserved byte for byte')

# Apply only explicitly recorded local art corrections after the reviewed base.
if (R/"corrections/regions.json").exists():
 import subprocess,sys
 subprocess.run([sys.executable,str(R/"apply_corrections.py")],check=True)
