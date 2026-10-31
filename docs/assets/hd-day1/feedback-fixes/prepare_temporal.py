from pathlib import Path
import json
from PIL import Image
R=Path(__file__).resolve().parents[1];D=R/'feedback-fixes/temporal';D.mkdir(exist_ok=True);tasks=json.loads((R/'tasks.json').read_text())['tasks'];views=json.loads((R/'views.json').read_text());chains=[]
for v in views:
 room,g=v['room'],v['group']
 if room not in ['M026','M027','M028','M029','M032','M033','M037']:continue
 if room=='M034':continue # Same immutable source contexts as M033.
 for c in v['controllers']:
  if room=='M029' and c['id']!=0:continue
  if room=='M032' and c['id']!=1:continue
  ts=[next(t for t in tasks if (t['room'],t['group'],t['entry'])==(room,g,s['entry'])) for s in c['slots']]
  first=ts[0];bbox=v['active_bbox'];p=D/f'{room}-g{g}-c{c["id"]}-p00.png';im=Image.open(R/'OpenAI'/first['input']).convert('RGB');im.crop(tuple(x*4 for x in bbox)).save(p)
  frames=[]
  for i,t in enumerate(ts):frames.append({'phase':i,'id':Path(t['active_input']).stem,'entry':t['entry'],'source':str(R/'active-inputs'/t['active_input']),'target':str(D/f'{room}-g{g}-c{c["id"]}-p{i:02d}.png'),'input':t['input']})
  chains.append({'room':room,'group':g,'controller':c['id'],'size':v['active_size'],'bbox':bbox,'frames':frames})
(R/'feedback-fixes/temporal-jobs.json').write_text(json.dumps(chains,indent=2));print([(c['room'],c['group'],len(c['frames'])) for c in chains])
