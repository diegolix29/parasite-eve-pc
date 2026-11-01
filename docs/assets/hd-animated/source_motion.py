"""Retain original motion where sequential generated phases missed source changes.

Same approach as the accepted M037 restoration: fixed OpenAI scene material,
original per-phase deterministic RealESRNet restoration in moving regions, one
exposure calibration for an entire view, original per-entry masks and depth.
No generated-frame interpolation, phase crossfades or additional animation.
"""
from pathlib import Path
from PIL import Image
import json,cv2,numpy as np,shutil
R=Path(__file__).resolve().parent;dest=R/'OpenAI-source-motion';dest.mkdir(exist_ok=True)
for p in (R/'OpenAI').glob('*.png'):shutil.copy2(p,dest/p.name)
tasks=json.loads((R/'tasks.json').read_text())['tasks'];views=json.loads((R/'views.json').read_text());chains=json.loads((R/'source-driven-chains.json').read_text());proof=[]
def rgb(path):return np.array(Image.open(path).convert('RGB'),dtype=np.float32)
def restored(name):
 for folder in ('source-sr','source-sr-extra'):
  p=R/folder/name
  if p.exists():return rgb(p)
 raise FileNotFoundError(name)
for room,group in sorted({(c['room'],c['group'])for c in chains}):
 v=next(v for v in views if(v['room'],v['group'])==(room,group));w,h=v['active_size'];mask=np.zeros((h,w),np.uint8)
 for c in [c for c in chains if(c['room'],c['group'])==(room,group)]:
  native=[rgb(f['source'])for f in c['frames']];mask|=np.any(np.stack(native)!=native[0],axis=(0,3)).astype('uint8')
 mask=cv2.dilate(cv2.morphologyEx(mask,cv2.MORPH_CLOSE,np.ones((5,5),np.uint8)),np.ones((3,3),np.uint8))
 high=cv2.resize(mask,(w*4,h*4),interpolation=cv2.INTER_NEAREST);alpha=np.clip(cv2.distanceTransform(np.pad(high,1),cv2.DIST_L2,5)[1:-1,1:-1]/8,0,1)[...,None]
 base=rgb(R/'generated'/v['base_input']);base=cv2.resize(base,(w*4,h*4),interpolation=cv2.INTER_LANCZOS4);source=restored(v['base_input']);valid=(high!=0)&(source.mean(2)>12)
 gain=np.clip(base[valid].sum(0)/np.maximum(source[valid].sum(0),1),.5,2)if np.any(valid)else np.ones(3)
 for t in [t for t in tasks if(t['room'],t['group'])==(room,group)]:
  full=rgb(R/'OpenAI'/t['input']);x0,y0,x1,y1=[x*4 for x in v['active_bbox']];basepart=full[y0:y1,x0:x1].copy();material=np.clip(restored(t['active_input'])*gain,0,255)
  corrected=np.uint8(np.clip(np.rint(basepart*(1-alpha)+material*alpha),0,255));assert np.array_equal(corrected[high==0],np.uint8(basepart[high==0]))
  full[y0:y1,x0:x1]=corrected;path=dest/t['input'];tmp=path.with_suffix('.tmp');Image.fromarray(np.uint8(full)).save(tmp,format='PNG');tmp.replace(path)
 Image.fromarray(mask*255).save(R/'review'/f'{room}-g{group}-source-motion-mask.png')
 proof.append({'room':room,'group':group,'gain_rgb':gain.tolist(),'moving_native_pixels':int(mask.sum()),'outside_motion_support_unchanged':True,'controller_ids':[c['controller']for c in chains if(c['room'],c['group'])==(room,group)]})
(R/'source-motion-application.json').write_text(json.dumps({'method':__doc__,'views':proof,'inference_provenance':['source-sr/inference.json','source-sr-extra/inference.json'],'generated_phase_research_preserved_in':'generated/','destination':str(dest)},indent=2));print(len(proof),'source-driven view regions; all original phases and static own-layer materials retained')
