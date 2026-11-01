"""Audit actual packed scene sequences against original moving-layer coverage."""
from pathlib import Path
from PIL import Image
import numpy as np,json,cv2
R=Path(__file__).resolve().parent;views=json.loads((R/'views.json').read_text());tasks=json.loads((R/'tasks.json').read_text())['tasks'];records=[]
for v in views:
 w,h=v['canvas_size']
 for c in v['controllers']:
  hd=[];native=[];coverage=np.zeros((h,w),bool)
  for phase,slot in enumerate(c['slots']):
   t=next((t for t in tasks if(t['room'],t['group'],t['entry'])==(v['room'],v['group'],slot['entry'])),None);refs=np.load(R/'refs'/t['ref'])if t else []
   for (x,y),ref in zip(t['tiles']if t else [],refs):coverage[y:y+16,x:x+16]|=ref!=0
   stem=f"{v['room']}-g{v['group']:02d}-c{c['id']}-p{phase:02d}"
   native.append(np.array(Image.open(R/'composites'/f'{stem}-native.png').convert('RGB')))
   hd.append(np.array(Image.open(R/'composites'/f'{stem}-hd.png').convert('RGB')))
  covered=np.repeat(np.repeat(coverage,4,0),4,1);outside=max(int(np.any(im!=hd[0],axis=2)[~covered].sum())for im in hd)
  assert outside==0,(v['room'],c['id'],'motion outside original layer coverage',outside)
  small=[np.array(Image.fromarray(im).resize((w,h),Image.Resampling.LANCZOS)).astype('float32')for im in hd];edges=[]
  for i in range(len(hd)):
   j=(i+1)%len(hd);nd=float(np.abs(native[i].astype('float32')-native[j]).mean());dd=float(np.abs(small[i]-small[j]).mean());edges.append({'from':i,'to':j,'native_delta':nd,'hd_delta':dd,'relative_change':dd/max(nd,0.01)})
  ratios=[x['relative_change']for x in edges[:-1]if x['native_delta']>0.01];wrap=edges[-1]['relative_change']/max(float(np.median(ratios))if ratios else 1,0.01)
  records.append({'room':v['room'],'group':v['group'],'controller':c['id'],'slots':[{'entry':s['entry'],'duration':s['duration']}for s in c['slots']],'serialized_flags':c.get('flags'),'serialized_step':c.get('step'),'hd_motion_outside_original_coverage':outside,'closure_relative_to_native_and_other_edges':wrap,'closure_review':bool(ratios and edges[-1]['native_delta']>.01 and (wrap>2 or wrap<0.5)) or bool(edges[-1]['native_delta']<=.01 and edges[-1]['hd_delta']>.1),'stationary_native_edges_with_hd_change':[x for x in edges if x['native_delta']<=.01 and x['hd_delta']>.1],'edges':edges,'qualification':'Closure is a diagnostic for review, not a requirement that every script-controlled door loops or that last/first poses are identical.'})
proof={'controllers':len(records),'all_motion_confined_to_original_layer_coverage':True,'closure_review_count':sum(x['closure_review']for x in records),'sequences':records,'note':__doc__};(R/'temporal-audit.json').write_text(json.dumps(proof,indent=2));print({k:v for k,v in proof.items()if k!='sequences'})
