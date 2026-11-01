"""Return full-context enhancement to original per-entry/per-tile packets."""
from pathlib import Path
import json,argparse,hashlib,shutil
import numpy as np
from PIL import Image,ImageDraw
import cv2
p=argparse.ArgumentParser();p.add_argument('model');p.add_argument('--root',type=Path,default=Path(__file__).resolve().parent);p.add_argument('--destination',type=Path);p.add_argument('--room');p.add_argument('--entry-overrides',type=Path);p.add_argument('--no-family-lock',action='store_true');p.add_argument('--preserve-source-black',action='store_true');p.add_argument('--fallback-pack',type=Path);p.add_argument('--retain-controller',action='append',default=[]);a=p.parse_args();root=a.root.resolve();data=json.loads((root/'tasks.json').read_text());model=root/a.model;dest=a.destination.resolve() if a.destination else root/('pack-'+a.model);dest.mkdir(exist_ok=True)
detail_regions=json.loads((root/'native-detail-regions.json').read_text()) if (root/'native-detail-regions.json').exists() else []
material_regions=json.loads((root/'source-material-regions.json').read_text()) if (root/'source-material-regions.json').exists() else []
# Lock common context in each serialized controller family. Controllers remain
# independent: no combined animation/video replaces their original state.
locked={};families={}
for t in data['tasks']:
 if t['control'] is not None:families.setdefault((t['room'],t['group'],t['control']),[]).append(t)
for family,tasks in families.items():
 if a.no_family_lock or len(tasks)<2:continue
 if not all((model/t['input']).exists() for t in tasks):continue
 originals=[np.array(Image.open(root/'inputs'/t['input'])) for t in tasks]
 if len({x.shape for x in originals})!=1:continue
 changed=np.any(np.stack(originals)!=originals[0],axis=(0,3));mask=np.repeat(np.repeat(changed,4,0),4,1)
 base=np.array(Image.open(model/tasks[0]['input']))
 for t in tasks:
  locked[t['key']]=np.where(mask[...,None],np.array(Image.open(model/t['input'])),base)
  (model/'locked').mkdir(exist_ok=True)
  path=model/'locked'/(t['key']+'.png');temp=path.with_suffix('.tmp');Image.fromarray(locked[t['key']]).save(temp,format='PNG');temp.replace(path)
retain={tuple(x.split(':')) for x in a.retain_controller};fallback={}
if a.fallback_pack:
 fallback={t['key']:t for t in json.loads((a.fallback_pack/'manifest.json').read_text())['atlases']}
records=[]
for t in data['tasks']:
 if a.room and t['room']!=a.room:continue
 source=model/t['input']
 if not source.exists():continue
 if (t['room'],str(t['group']),str(t['control'])) in retain:
  assert a.fallback_pack and t['key'] in fallback
  path=dest/(t['key']+'.bmp');temp=path.with_suffix('.tmp');shutil.copyfile(a.fallback_pack/path.name,temp);temp.replace(path)
  records.append({**fallback[t['key']],'retained_controller':'Regression guard: preserve previously reviewed material; new registration changed pixels away from native motion.'})
  continue
 override=a.entry_overrides/(t['key']+'.png') if a.entry_overrides else None
 enhanced=locked.get(t['key'])
 if override and override.exists():enhanced=np.array(Image.open(override))
 if enhanced is None:enhanced=np.array(Image.open(source))
 # A reviewed material revert uses the restored original context for every
 # native layer. Never flatten its geometry using a different generated view.
 applied_materials=[]
 for region in material_regions:
  if region['room']!=t['room'] or region.get('group',t['group'])!=t['group']:continue
  restored=np.array(Image.open(root/region['source_directory']/t['input']).convert('RGB'))
  assert restored.shape==enhanced.shape,(t['key'],'source material canvas mismatch')
  mask=Image.new('L',(enhanced.shape[1],enhanced.shape[0]));draw=ImageDraw.Draw(mask)
  for polygon in region['polygons']:draw.polygon([(x*4,y*4) for x,y in polygon],fill=255)
  for polygon in region.get('exclude',[]):draw.polygon([(x*4,y*4) for x,y in polygon],fill=0)
  support=np.array(mask)>0
  distance=cv2.distanceTransform(np.pad(support.astype('uint8'),1),cv2.DIST_L2,5)[1:-1,1:-1]
  alpha=np.clip(distance/region.get('feather_hd',8),0,1)[...,None]
  enhanced=np.uint8(np.rint(enhanced*(1-alpha)+restored*alpha))
  applied_materials.append(region['reason'])
 refs=np.load(root/'refs'/t['ref']);n=len(refs);cols=min(16,n);rows=(n+cols-1)//cols;atlas=np.zeros((rows*136,cols*68,3),dtype='uint8')
 for i,((x,y),ref) in enumerate(zip(t['tiles'],refs)):
  patch=enhanced[y*4:y*4+64,x*4:x*4+64].copy();assert patch.shape==(64,64,3)
  rgb=np.stack([(ref&31)*255//31,((ref>>5)&31)*255//31,((ref>>10)&31)*255//31],axis=-1).astype('uint8');rgb=np.repeat(np.repeat(rgb,4,0),4,1)
  if a.preserve_source_black:
   black=np.repeat(np.repeat((ref&0x7fff)==0,4,0),4,1);patch[black]=0
  for region in detail_regions:
   if region['room']!=t['room']:continue
   x0,y0,x1,y1=region['bbox'];left,right=max(x,x0),min(x+16,x1);top,bottom=max(y,y0),min(y+16,y1)
   if left<right and top<bottom:
    ys=slice((top-y)*4,(bottom-y)*4);xs=slice((left-x)*4,(right-x)*4)
    if region.get('mode')=='source_chroma_light':
     native=rgb[::4,::4].astype(np.float32);chroma=native[:,:,0]-np.maximum(native[:,:,1],native[:,:,2]);alpha=np.clip((chroma-24)/96,0,1);inside=np.zeros((16,16),np.float32);inside[top-y:bottom-y,left-x:right-x]=1;alpha*=inside
     alpha=np.clip(np.array(Image.fromarray(alpha).resize((64,64),Image.Resampling.LANCZOS)),0,1)[...,None]
     smooth=np.array(Image.fromarray(rgb[::4,::4]).resize((64,64),Image.Resampling.LANCZOS)).astype(float)
     patch[ys,xs]=np.uint8(np.round(patch[ys,xs]*(1-alpha[ys,xs])+smooth[ys,xs]*alpha[ys,xs]))
    else:patch[ys,xs]=rgb[ys,xs]
  ax,ay=(i%cols)*68,(i//cols)*68
  atlas[ay:ay+68,ax:ax+68]=np.pad(patch,((2,2),(2,2),(0,0)),mode='edge')
  atlas[ay+rows*68:ay+rows*68+68,ax:ax+68]=np.pad(rgb,((2,2),(2,2),(0,0)),mode='edge')
 path=dest/(t['key']+'.bmp');temp=path.with_suffix('.tmp');Image.fromarray(atlas).save(temp,format='BMP');temp.replace(path)
 records.append({**t,'atlas_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'rgba_bytes':atlas.shape[0]*atlas.shape[1]*4,'static_family_locked':t['key'] in locked,'entry_override':str(override) if override and override.exists() else None,'source_material_restoration':applied_materials})
if a.room and (dest/'manifest.json').exists():
 previous=json.loads((dest/'manifest.json').read_text())['atlases'];records=[t for t in previous if t['room']!=a.room]+records
(dest/'manifest.json').write_text(json.dumps({'model':a.model,'scope':'Optional background PoC; original masks/STP/OT/animation timing; palette mismatch falls back to original RGB.','native_detail_regions':detail_regions,'preserve_source_black':a.preserve_source_black,'atlases':records},indent=2));print(a.model,len(records),'atlases',sum(t['rgba_bytes'] for t in records)/2**20,'MiB GPU')
