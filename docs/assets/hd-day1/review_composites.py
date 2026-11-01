from pathlib import Path
from PIL import Image,ImageDraw
import json,math,argparse
p=argparse.ArgumentParser();p.add_argument("--composites",default="composites");p.add_argument("--review",default="review");a=p.parse_args()
R=Path(__file__).resolve().parent;(R/a.review).mkdir(exist_ok=True);vs=json.loads((R/'views.json').read_text());ts=json.loads((R/'tasks.json').read_text())['tasks'];js={j['id']:j for j in json.loads((R/'jobs.json').read_text())};records=[]
for v in vs:
 for c in v['controllers']:
  regions=[]
  for slot in c['slots']:
   t=next(t for t in ts if (t['room'],t['group'],t['entry'])==(v['room'],v['group'],slot['entry']));j=js.get(Path(t['active_input']).stem,{});regions+=j.get('changed_regions',[])
  w,h=v['active_size'];b=[max(0,min(x[0] for x in regions)-8),max(0,min(x[1] for x in regions)-8),min(w,max(x[2] for x in regions)+8),min(h,max(x[3] for x in regions)+8)] if regions else [0,0,w,h];ox,oy=v['active_bbox'][:2];b=[b[0]+ox,b[1]+oy,b[2]+ox,b[3]+oy];o=Image.new('RGB',(1280,math.ceil(len(c['slots'])/2)*300));d=ImageDraw.Draw(o)
  for phase,slot in enumerate(c['slots']):
   prefix=f"{v['room']}-g{v['group']:02d}-c{c['id']}-p{phase:02d}"
   for k,kind in enumerate(['native','hd']):
    im=Image.open(R/a.composites/(prefix+'-'+kind+'.png'));scale=4 if kind=='hd' else 1;im=im.crop(tuple(x*scale for x in b));ratio=min(310/im.width,265/im.height);im=im.resize((round(im.width*ratio),round(im.height*ratio)),Image.Resampling.NEAREST if k==0 else Image.Resampling.LANCZOS);x=phase%2*640+k*320;y=phase//2*300;o.paste(im,(x,y+28))
   d.text((phase%2*640,phase//2*300),f"{v['room']} g{v['group']} c{c['id']} phase{phase} entry{slot['entry']} duration{slot['duration']}",fill='white')
  name=f"controller-{v['room']}-g{v['group']:02d}-c{c['id']:02d}.jpg";o.save(R/a.review/name);records.append({'room':v['room'],'group':v['group'],'controller':c['id'],'phases':len(c['slots']),'sheet':name})
(R/a.review/'controller-index.json').write_text(json.dumps(records,indent=2));print(len(records),'controller phase sheets')
