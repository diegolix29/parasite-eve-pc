"""Catalog native animation states and source contexts for the remaining maps."""
from pathlib import Path
from PIL import Image
import json,shutil
R=Path(__file__).resolve().parent;A=R.parent
inventory=json.loads((A/'hd-static/inventory.json').read_text())['rooms']
rooms=[x for x in inventory if x['disc']==1 and x['map']>37 and 'ui_tutorial_or_title_card'not in x['reasons'] and any(k in x['reasons']for k in ('animation_controllers','hidden_or_switchable_entries'))]
for room in rooms:
 other=next(x for x in inventory if x['disc']==2 and x['id']==room['id'])
 assert room['geometry_sha256']==other['geometry_sha256'] and room['texture_sha256']==other['texture_sha256']
(R/'inventory.json').write_text(json.dumps({'maps':len(rooms),'controllers':sum(x['controllers']for x in rooms),'entry_records':sum(len(x['entries'])for x in rooms),'both_disc_sources_identical':True,'qualification':'Serialized controllers and switchable entries; script behavior and visibility remain native.','rooms':rooms},indent=2))
for name in ('generated','metadata','review','OpenAI'):(R/name).mkdir(exist_ok=True)
tasks=json.loads((R/'tasks.json').read_text())['tasks'];views=json.loads((R/'views.json').read_text());jobs={};chains=[]
for v in views:
 key=Path(v['base_input']).stem
 jobs.setdefault(key,dict(id=key,kind='base',source=str(R/'active-inputs'/v['base_input']),views=[],reference=None,entries=[],size=v['active_size']))['views'].append([v['room'],v['group']])
for t in tasks:
 v=next(v for v in views if(v['room'],v['group'])==(t['room'],t['group']));key=Path(t['active_input']).stem
 j=jobs.setdefault(key,dict(id=key,kind='entry',source=str(R/'active-inputs'/t['active_input']),views=[],entries=[],reference=Path(v['base_input']).stem,size=v['active_size']))
 if [t['room'],t['group']]not in j['views']:j['views'].append([t['room'],t['group']])
 j['entries'].append({k:t[k]for k in ('room','group','entry','key','control')})
for v in views:
 for c in v['controllers']:
  frames=[]
  for phase,slot in enumerate(c['slots']):
   t=next((t for t in tasks if(t['room'],t['group'],t['entry'])==(v['room'],v['group'],slot['entry'])),None)
   frames.append(dict(phase=phase,id=Path(t['active_input']).stem if t else None,entry=slot['entry'],duration=slot['duration'],source=str(R/'active-inputs'/t['active_input'])if t else None,empty=t is None))
  chains.append(dict(room=v['room'],group=v['group'],controller=c['id'],base=Path(v['base_input']).stem,size=v['active_size'],frames=frames,status='pending'))
for j in jobs.values():
 dest=R/'generated'/(j['id']+'.png')
 if not dest.exists():
  for old in ('hd-day1','hd-static'):
   source=A/old/'generated'/dest.name
   if source.exists():
    shutil.copy2(source,dest);(R/'metadata'/(j['id']+'.json')).write_text(json.dumps(dict(status='reused exact source match',source=str(source)),indent=2));break
 j['status']='ready'if dest.exists()else'pending'
(R/'jobs.json').write_text(json.dumps(list(jobs.values()),indent=2));(R/'chains.json').write_text(json.dumps(chains,indent=2))
(R/'scope.json').write_text(json.dumps(dict(baseline_mod='0.1',remaining_maps=len(rooms),remaining_controllers=sum(x['controllers']for x in rooms),pilot_maps=['M038','M039','M040','M041','M042','M043'],working_maps=sorted({v['room']for v in views}),working_views=len(views),working_chains=len(chains),source_contexts=len(jobs),phase_slots=sum(len(c['frames'])for c in chains),method='Fixed OpenAI material anchor per view; original source-guided motion restored with RealESRNet_x4plus. Native controller order/durations/independence, empty states, masks and depth remain unchanged. Initial sequential-phase research retained separately. Never overwrite frozen mod0.1.'),indent=2))
print((R/'scope.json').read_text())
