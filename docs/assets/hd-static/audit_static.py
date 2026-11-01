"""Validate static scope, both-disc equivalence, original black and Day1 retention."""
from pathlib import Path
from PIL import Image
import json,hashlib,numpy as np
R=Path(__file__).resolve().parent
scope=json.loads((R/'scope.json').read_text());inventory=json.loads((R/'inventory.json').read_text())
rooms=[x for x in inventory['rooms'] if x['classification']=='static_candidate']
assert len(rooms)==scope['maps']*2
for room in {x['id'] for x in rooms}:
 pair=[x for x in rooms if x['id']==room]
 assert len(pair)==2 and pair[0]['geometry_sha256']==pair[1]['geometry_sha256']
 assert pair[0]['texture_sha256']==pair[1]['texture_sha256']
jobs=json.loads((R/'jobs.json').read_text())
for j in jobs:
 assert (R/'generated'/(j['id']+'.png')).exists()
 assert (R/'metadata'/(j['id']+'.json')).exists()
views=json.loads((R/'views.json').read_text());assert len(views)==scope['views']
assert all(not v['controllers'] for v in views)
tasks=json.loads((R/'tasks.json').read_text())['tasks']
old=R.parent/'hd-day1'/'pack';pack=R/'pack-with-day1'
oldkeys={t['key'] for t in json.loads((old/'manifest.json').read_text())['atlases']}
manifest=json.loads((pack/'manifest.json').read_text());keys={t['key'] for t in manifest['atlases']}
assert keys==oldkeys|{t['key']for t in tasks}
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
intentional={t['key']for t in tasks if t['room']=='M005'}&set(manifest.get('user_correction_keys',[]))
assert (oldkeys&set(manifest.get('user_correction_keys',[])))<=intentional
for key in oldkeys-intentional:assert sha(old/(key+'.bmp'))==sha(pack/(key+'.bmp'))
black=0;seen=set();material_overrides={t['key']for t in tasks if t['room']in ('M005','M049','E15')}&set(manifest.get('user_correction_keys',[]))
for t in tasks:
 if t['key'] in oldkeys or t['key'] in seen or t['key'] in material_overrides:continue
 seen.add(t['key']);refs=np.load(R/'refs'/t['ref']);im=np.array(Image.open(pack/(t['key']+'.bmp')).convert('RGB'));cols=min(16,len(refs))
 for i,ref in enumerate(refs):
  mask=np.repeat(np.repeat((ref&0x7fff)==0,4,0),4,1);x=i%cols*68+2;y=i//cols*68+2
  assert np.all(im[y:y+64,x:x+64][mask]==0),(t['key'],i)
  black+=int(mask.sum())
proof={'scope':scope,'all_sources_and_metadata_present':True,'both_disc_source_bytes_and_geometry_identical':True,'no_serialized_animation_controllers':True,'all_static_keys_present':True,'reviewed_day1_unique_atlases_byte_identical':len(oldkeys-intentional),'user_requested_day1_correction_keys':sorted(intentional),'preserved_black_hd_texels':black,'localized_material_override_keys_verified_separately':sorted(material_overrides),'combined_unique_atlases':len(keys),'note':'Data integrity and conservative classification; not complete visual or whole-game runtime approval.'}
(R/'static-audit.json').write_text(json.dumps(proof,indent=2));print(json.dumps(proof,indent=2))
