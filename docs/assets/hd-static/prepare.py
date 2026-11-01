"""Prepare independent source-authoritative restoration jobs without replacing Day1."""
from pathlib import Path
from PIL import Image
import json,shutil
R=Path(__file__).resolve().parent; D=R.parent/'hd-day1'
T=json.loads((R/'tasks.json').read_text()); V=json.loads((R/'views.json').read_text()); jobs={}
for folder in ('generated','metadata','OpenAI','review','pack'):(R/folder).mkdir(exist_ok=True)
for v in V:
 key=Path(v['base_input']).stem
 j=jobs.setdefault(key,dict(id=key,kind='base',source=str(R/'active-inputs'/v['base_input']),views=[],entries=[],reference=None,size=v['active_size']))
 j['views'].append([v['room'],v['group']])
byview={(v['room'],v['group']):v for v in V}
for t in T['tasks']:
 key=Path(t['active_input']).stem; v=byview[t['room'],t['group']]
 j=jobs.setdefault(key,dict(id=key,kind='entry',source=str(R/'active-inputs'/t['active_input']),views=[],entries=[],reference=Path(v['base_input']).stem,size=v['active_size']))
 if [t['room'],t['group']] not in j['views']:j['views'].append([t['room'],t['group']])
 j['entries'].append(dict(room=t['room'],group=t['group'],entry=t['entry'],key=t['key']))
old={j['id']:j for j in json.loads((D/'jobs.json').read_text())}
for j in jobs.values():
 dest=R/'generated'/(j['id']+'.png')
 if j['id'] in old and (D/'generated'/dest.name).exists():
  if not dest.exists():shutil.copy2(D/'generated'/dest.name,dest)
  j['status']='reused_day1';j['reused_from']=str(D/'generated'/dest.name)
  (R/'metadata'/f"{j['id']}.json").write_text(json.dumps(j,indent=2))
 else:
  image=Image.open(j['source']).convert('RGB');extrema=image.getextrema()
  if all(lo==hi for lo,hi in extrema):
   if not dest.exists():image.resize((image.width*4,image.height*4),Image.Resampling.NEAREST).save(dest)
   j['status']='uniform_source_preserved';(R/'metadata'/f"{j['id']}.json").write_text(json.dumps(j,indent=2))
  else:j['status']='generated' if dest.exists() else 'pending'
(R/'jobs.json').write_text(json.dumps(list(jobs.values()),indent=2))
summary=dict(maps=len(T['rooms']),views=len(V),atlases=len(T['tasks']),unique_sources=len(jobs),statuses={s:sum(j['status']==s for j in jobs.values()) for s in sorted({j['status']for j in jobs.values()})},scope='World maps and encounter/EX backgrounds on both discs. Excludes animated/switchable rooms, title cards, tutorials and Game Over. Both-disc geometry and complete source texture bytes checked. Classification is not a visual approval.')
(R/'scope.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2))
