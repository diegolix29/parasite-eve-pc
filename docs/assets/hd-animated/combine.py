"""Build a separate working pack on the immutable mod0.1 snapshot."""
from pathlib import Path
import json,hashlib,shutil,argparse
R=Path(__file__).resolve().parent;p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path,required=True);a=p.parse_args()
baseline=a.baseline.resolve();version=json.loads((baseline.parent/'mod.json').read_text());assert version['version']=='0.1'
dest=R/'pack-with-0.1';dest.mkdir(exist_ok=True);records={};sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
old=json.loads((baseline/'manifest.json').read_text());new=json.loads((R/'pack/manifest.json').read_text())
for source,entries in [(baseline,old['atlases']),(R/'pack',new['atlases'])]:
 for t in entries:
  key=t['key'];path=source/(key+'.bmp');target=dest/path.name
  # Native geometry keys can recur in later rooms. The frozen, reviewed art
  # wins over a new study for that same packet; its source reference is audited
  # independently before this union is built.
  if key in records and records[key]['pack_origin']=='mod0.1':continue
  elif not target.exists()or sha(target)!=sha(path):shutil.copy2(path,target)
  records[key]={**t,'pack_origin':'mod0.1'if source==baseline else'new-animation-study'}
for t in old['atlases']:assert sha(baseline/(t['key']+'.bmp'))==sha(dest/(t['key']+'.bmp'))
# The versioned snapshot stays immutable. Explicit later art corrections are
# a separate reviewed overlay, including corrections to baseline packets.
overrides=R/'corrections/pack-overrides';reviewed=set()
if (overrides/'manifest.json').exists():
 for t in json.loads((overrides/'manifest.json').read_text())['atlases']:
  key=t['key'];assert key in records,key
  path=overrides/(key+'.bmp');assert sha(path)==t['atlas_sha256'],key
  shutil.copy2(path,dest/path.name)
  records[key]={**records[key],**t,'pack_origin':'reviewed-correction'};reviewed.add(key)
(dest/'manifest.json').write_text(json.dumps({'model':'OpenAI built-in material anchors (exact version not exposed) + RealESRNet_x4plus original motion','baseline_version':'0.1','baseline_manifest_sha256':sha(baseline/'manifest.json'),'atlases':list(records.values()),'scope':'Working animation expansion; original controller semantics, native coverage, depth and gameplay remain authoritative.'},indent=2))
oldkeys={t['key']for t in old['atlases']};changed=oldkeys&reviewed
for key in oldkeys-changed:assert sha(baseline/(key+'.bmp'))==sha(dest/(key+'.bmp'))
(R/'baseline-preservation.json').write_text(json.dumps({'baseline_version':'0.1','unique_baseline_atlases':len(oldkeys),'all_baseline_atlases_byte_identical':not bool(changed),'intentional_reviewed_override_keys':sorted(changed),'all_other_baseline_atlases_byte_identical':True,'frozen_snapshot_unchanged':True,'combined_atlases':len(records),'baseline_overlap_keys':sorted(oldkeys&{t['key']for t in new['atlases']})},indent=2));print(len(records),'unique atlases; immutable mod0.1 plus',len(reviewed),'reviewed correction overrides')
