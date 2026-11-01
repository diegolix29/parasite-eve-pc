"""Normalize fixed native viewports; retain one shared OpenAI material in invariant areas."""
from pathlib import Path
import json,argparse
import cv2
import numpy as np
from PIL import Image
R=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--require-complete',action='store_true');p.add_argument('--coherent',action='store_true');p.add_argument('--destination',default='OpenAI');a=p.parse_args()
tasks=json.loads((R/'tasks.json').read_text())['tasks'];views=json.loads((R/'views.json').read_text());jobs=json.loads((R/'jobs.json').read_text());out=R/a.destination;out.mkdir(exist_ok=True)
missing=[j['id'] for j in jobs if not (R/'generated'/(j['id']+'.png')).exists() and not (R/'localized'/(j['id']+'.png')).exists()]
if a.require_complete:assert not missing, f'{len(missing)} missing contexts'
lettering=json.loads((R/'source-lettering.json').read_text()) if (R/'source-lettering.json').exists() else []
def restore_source_lettering(enhanced,original,room):
 # Deterministic source texture restoration, not AI OCR or invented typography.
 # The Poisson boundary follows the existing paint colour; native glyph shapes
 # and gradients replace the invented words, with no opaque rectangular paste.
 native=cv2.resize(original,(enhanced.shape[1],enhanced.shape[0]),interpolation=cv2.INTER_CUBIC)
 for detail in lettering:
  if detail['room']!=room:continue
  x0,y0,x1,y1=[v*4 for v in detail['bbox']];mask=np.zeros(enhanced.shape[:2],np.uint8);mask[y0:y1,x0:x1]=255
  enhanced=cv2.seamlessClone(native,enhanced,mask,((x0+x1)//2,(y0+y1)//2),cv2.NORMAL_CLONE)
 return enhanced
written=set();groups=[]
for v in views:
 ts=[t for t in tasks if (t['room'],t['group'])==(v['room'],v['group'])];baseid=Path(v['base_input']).stem;bp=R/'generated'/(baseid+'.png')
 if not bp.exists():continue
 bbox=v['active_bbox'];w,h=v['active_size'];canvas=v['canvas_size'];base=Image.new('RGB',(canvas[0]*4,canvas[1]*4));base.paste(Image.open(bp).convert('RGB').resize((w*4,h*4),Image.Resampling.LANCZOS),(bbox[0]*4,bbox[1]*4));base=np.array(base)
 original=np.array(Image.open(R/'inputs'/v['base_canvas_input']).convert('RGB'));common=np.ones(original.shape[:2],bool)
 for t in ts:common&=np.all(np.array(Image.open(R/'inputs'/t['input']).convert('RGB'))==original,axis=2)
 shared=np.repeat(np.repeat(common,4,0),4,1)
 for t in ts:
  if t['input'] in written:continue
  coherent=R/'coherent'/t['active_input'];use_coherent=a.coherent and coherent.exists()
  source=coherent if use_coherent else R/'localized'/t['active_input']
  if not source.exists():source=R/'generated'/t['active_input']
  if not source.exists():continue
  full=Image.new('RGB',(canvas[0]*4,canvas[1]*4));full.paste(Image.open(source).convert('RGB').resize((w*4,h*4),Image.Resampling.LANCZOS),(bbox[0]*4,bbox[1]*4))
  enhanced=np.array(full) if use_coherent else np.where(shared[...,None],base,np.array(full));enhanced=restore_source_lettering(enhanced,np.array(Image.open(R/'inputs'/t['input']).convert('RGB')),t['room']);path=out/t['input'];temp=path.with_suffix('.tmp');Image.fromarray(enhanced).save(temp,format='PNG');temp.replace(path);written.add(t['input'])
 groups.append({'room':v['room'],'group':v['group'],'shared_native_pixels':int(common.sum()),'total_native_pixels':int(common.size)})
(R/('normalization-coherent.json' if a.coherent else 'normalization.json')).write_text(json.dumps({'completed_contexts':len(jobs)-len(missing),'total_contexts':len(jobs),'missing':missing,'written_full_canvases':len(written),'groups':groups,'method':'Fixed per-view native coverage bounds; exact 4x full canvas; only all-source-invariant pixels share canonical OpenAI material. Original changing pixels remain per phase. No optical flow or restoration-model mix.'},indent=2))
print(len(written),'normalized canvases;',len(missing),'generation jobs pending')
