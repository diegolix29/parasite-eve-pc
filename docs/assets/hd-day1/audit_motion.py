from pathlib import Path
from PIL import Image
import json,numpy as np,argparse
p=argparse.ArgumentParser();p.add_argument("--composites",default="composites");p.add_argument("--report",default="motion-audit.json");opts=p.parse_args()
R=Path(__file__).resolve().parent;vs=json.loads((R/'views.json').read_text());results=[];frozen=[]
for v in vs:
 for c in v['controllers']:
  pairs=[];prev=None
  for i in range(len(c['slots'])):
   pre=f"{v['room']}-g{v['group']:02d}-c{c['id']}-p{i:02d}-";native=Image.open(R/opts.composites/(pre+'native.png')).convert('RGB');hd=Image.open(R/opts.composites/(pre+'hd.png')).convert('RGB').resize(native.size,Image.Resampling.BOX);a=np.array(native).astype(np.int16);b=np.array(hd).astype(np.int16)
   if prev is not None:
    na=int(np.abs(a-prev[0]).sum());hb=int(np.abs(b-prev[1]).sum());pairs.append({'phase_from':i-1,'phase_to':i,'native_l1':na,'hd_l1':hb})
    if na and not hb:frozen.append({'room':v['room'],'group':v['group'],'controller':c['id'],'pair':pairs[-1]})
   prev=a,b
  results.append({'room':v['room'],'group':v['group'],'controller':c['id'],'pairs':pairs})
proof={'controllers':len(results),'frozen_native_changing_pairs':frozen,'note':'Nonzero motion audit only; not proof of correct pose, motion direction, amplitude or temporal texture consistency.','results':results};(R/opts.report).write_text(json.dumps(proof,indent=2));print('controllers',len(results),'frozen pairs',frozen)
