"""Offline phase previews with source tile depth, coverage, STP and blend mode.
Does not include live actors, mirrors, scripts or camera movement.
"""
from pathlib import Path
from PIL import Image
import json,numpy as np,argparse
R=Path(__file__).resolve().parent;p=argparse.ArgumentParser();p.add_argument('--room');p.add_argument('--group',type=int,default=0);p.add_argument('--controller',type=int);p.add_argument('--pack',default='pack');p.add_argument('--output',default='composites');a=p.parse_args();data=json.loads((R/'tasks.json').read_text());vs=json.loads((R/'views.json').read_text());out=R/a.output;out.mkdir(exist_ok=True);records=[]
for view in vs:
 room,g=view['room'],view['group']
 if a.room and (a.room!=room or a.group!=g):continue
 ts=[t for t in data['tasks'] if (t['room'],t['group'])==(room,g)];desc=next(x for x in data['rooms'] if x['room']==room);controls=view['controllers'];initial={e['index'] for e in desc['entries'] if e['group']==g and e['flags']&2}
 for c in controls:
  if c['slots']:initial.add(c['slots'][0]['entry'])
 sets=[(None,0,initial.copy())]
 for c in controls:
  if a.controller is not None and a.controller!=c['id']:continue
  for phase,slot in enumerate(c['slots']):
   visible=initial-{x['entry'] for x in c['slots']};visible.add(slot['entry']);sets.append((c['id'],phase,visible))
 native={t['key']:np.load(R/'refs'/t['ref']) for t in ts};hd={}
 for t in ts:
  path=R/a.pack/(t['key']+'.bmp')
  if path.exists():hd[t['key']]=np.array(Image.open(path).convert('RGB'))
 w,h=view['canvas_size']
 for cid,phase,visible in sets:
  selected=[t for t in ts if t['entry'] in visible]
  if not all(t['key'] in hd for t in selected):continue
  tiles=[]
  for t in selected:
   for index,(xy,meta) in enumerate(zip(t['tiles'],t['tile_meta'])):tiles.append((meta,t,index,xy))
  tiles.sort(key=lambda x:(x[0]['depth'],x[0]['ordinal']),reverse=True)
  for enhanced in [False,True]:
   scale=4 if enhanced else 1;dest=np.zeros((h*scale,w*scale,3),np.float32)
   for meta,t,index,(x,y) in tiles:
    ref=native[t['key']][index];src=np.stack([(ref&31)*255//31,((ref>>5)&31)*255//31,((ref>>10)&31)*255//31],axis=-1).astype(np.float32)
    if enhanced:
     cols=min(16,len(native[t['key']]));ax=index%cols*68+2;ay=index//cols*68+2;src=hd[t['key']][ay:ay+64,ax:ax+64].astype(np.float32)
    mask=np.repeat(np.repeat(ref!=0,scale,0),scale,1);stp=np.repeat(np.repeat((ref&0x8000)!=0,scale,0),scale,1);d=dest[y*scale:(y+16)*scale,x*scale:(x+16)*scale]
    if meta['semi']:
     mode=meta['mode'];blend=(src+d)/2 if mode==0 else np.minimum(255,src+d) if mode==1 else np.maximum(0,d-src) if mode==2 else np.minimum(255,d+src/4);src=np.where(stp[...,None],blend,src)
    d[mask]=src[mask]
   name=f'{room}-g{g:02d}-c{cid if cid is not None else "base"}-p{phase:02d}-{"hd" if enhanced else "native"}.png';Image.fromarray(np.clip(dest,0,255).astype('uint8')).save(out/name)
  records.append({'room':room,'group':g,'controller':cid,'phase':phase,'visible_entries':sorted(visible)})
if a.room and (out/'manifest.json').exists():
 old=json.loads((out/'manifest.json').read_text())['composites'];records=[x for x in old if (x['room'],x['group'])!=(a.room,a.group)]+records
(out/'manifest.json').write_text(json.dumps({'note':__doc__,'composites':records},indent=2));print(len(records),'paired source/HD phase previews')
