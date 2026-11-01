from pathlib import Path
from PIL import Image
import json, numpy as np,hashlib,argparse
p=argparse.ArgumentParser();p.add_argument("--pack",default="pack");p.add_argument("--report",default="pack-audit.json");a=p.parse_args()
R=Path(__file__).resolve().parent;ts=json.loads((R/'tasks.json').read_text())['tasks'];vs=json.loads((R/'views.json').read_text());pack=R/a.pack;records=[];protected_texels=0;regions=json.loads((R/'native-detail-regions.json').read_text()) if (R/'native-detail-regions.json').exists() else []
for t in ts:
 path=pack/(t['key']+'.bmp');assert path.exists(),('missing',t['room'],t['entry'],t['key'])
 im=np.array(Image.open(path).convert('RGB'));refs=np.load(R/'refs'/t['ref']);cols=min(16,len(refs));rows=(len(refs)+cols-1)//cols;assert im.shape==(rows*136,cols*68,3)
 for i,ref in enumerate(refs):
  x=i%cols*68;y=i//cols*68+rows*68;rgb=np.stack([(ref&31)*255//31,((ref>>5)&31)*255//31,((ref>>10)&31)*255//31],axis=-1).astype('uint8');rgb=np.repeat(np.repeat(rgb,4,0),4,1);assert np.array_equal(im[y+2:y+66,x+2:x+66],rgb),(t['key'],i,'native reference mismatch')
 for i,((tx,ty),ref) in enumerate(zip(t['tiles'],refs)):
  rgb=np.stack([(ref&31)*255//31,((ref>>5)&31)*255//31,((ref>>10)&31)*255//31],axis=-1).astype('uint8');rgb=np.repeat(np.repeat(rgb,4,0),4,1);ax=i%cols*68+2;ay=i//cols*68+2
  for region in regions:
   if region['room']!=t['room']:continue
   if region.get('mode')=='source_chroma_light':continue
   x0,y0,x1,y1=region['bbox'];left,right=max(tx,x0),min(tx+16,x1);top,bottom=max(ty,y0),min(ty+16,y1)
   if left<right and top<bottom:
    ys=slice((top-ty)*4,(bottom-ty)*4);xs=slice((left-tx)*4,(right-tx)*4);assert np.array_equal(im[ay:ay+64,ax:ax+64][ys,xs],rgb[ys,xs]),(t['key'],'native indicator color mismatch');protected_texels+=(right-left)*(bottom-top)
 records.append({'key':t['key'],'room':t['room'],'entry':t['entry'],'cells':len(refs),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
lookup={(t['room'],t['group'],t['entry']):t for t in ts};phases=0
for v in vs:
 for c in v['controllers']:
  for slot in c['slots']:
   assert (v['room'],v['group'],slot['entry']) in lookup;assert slot['duration']>=0;phases+=1
proof={'atlas_count':len(records),'unique_keys':len({r['key'] for r in records}),'native_tile_cells':sum(r['cells'] for r in records),'views':len(vs),'independent_controllers':sum(len(v['controllers']) for v in vs),'serialized_phase_slots':phases,'protected_native_detail_texels':protected_texels,'all_native_reference_texels_identical':True,'original_controller_slots_preserved':True,'smooth_source_chroma_regions':[r for r in regions if r.get('mode')=='source_chroma_light'],'note':'Asset coverage/data-integrity audit, not visual phase correctness or whole-Day1 runtime proof.','atlases':records};(R/a.report).write_text(json.dumps(proof,indent=2));print({k:v for k,v in proof.items() if k!='atlases'})
