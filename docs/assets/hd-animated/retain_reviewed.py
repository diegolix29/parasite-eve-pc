"""Keep the validated first animation batch and frozen 0.1 artwork unchanged."""
from pathlib import Path
import argparse, hashlib, json, shutil
R=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path,required=True);a=p.parse_args()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
records=json.loads((R/'pack/manifest.json').read_text());current={x['key']for x in records['atlases']};retained={}
for source,label in [(R/'pilot/pack','first animation batch'),(a.baseline,'frozen mod0.1')]:
 manifest=json.loads((source/'manifest.json').read_text())
 if label=='first animation batch':assert {t['key']for t in manifest['atlases']}<=current
 for t in manifest['atlases']:
  key=t['key']
  if key not in current:continue
  path=source/(key+'.bmp');target=R/'pack'/path.name
  if not target.exists()or sha(path)!=sha(target):shutil.copy2(path,target)
  assert sha(path)==sha(target)
  retained[key]=label
hashes={key:sha(R/'pack'/(key+'.bmp'))for key in current}
for t in records['atlases']:
 t['atlas_sha256']=hashes[t['key']]
 if t['key']in retained:
  t['atlas_sha256']=sha(R/'pack'/(t['key']+'.bmp'));t['reviewed_art_retained']=retained[t['key']]
(R/'pack/manifest.json').write_text(json.dumps(records,indent=2))
(R/'reviewed-preservation.json').write_text(json.dumps({'retained':retained,'count':len(retained),'all_retained_files_byte_identical':True},indent=2));print(len(retained),'reviewed packets retained byte for byte')
