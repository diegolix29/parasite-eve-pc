"""Experimental offline animation registration; never changes original game data.
Transfer a canonical material only when native forward/backward correspondence
is reliable. Opening/disoccluded surfaces use the existing generated phase,
registered against immutable context. Keep the old pack until visual approval.
"""
from pathlib import Path
import argparse,json,hashlib
import cv2,numpy as np
from PIL import Image
R=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--rooms');a=p.parse_args();rooms=set(a.rooms.split(',')) if a.rooms else None
jobs=json.loads((R/'jobs.json').read_text());locals={j['id']:j for j in json.loads((R/'local-jobs.json').read_text())};out=R/'coherent';out.mkdir(exist_ok=True);records=[]
def read(path,size):return np.array(Image.open(path).convert('RGB').resize(size,Image.Resampling.LANCZOS))
for j in jobs:
 if rooms and not any(v[0] in rooms for v in j['views']):continue
 if j['kind']=='base':continue
 if not any(e['control'] is not None for e in j['entries']):
  stale=out/(j['id']+'.png')
  if stale.exists():stale.unlink()
  continue
 w,h=j['size'];ref=j['reference'];base=read(R/'generated'/(ref+'.png'),(w*4,h*4));s=read(Path(j['source']),(w,h));b=read(R/'active-inputs'/(ref+'.png'),(w,h));changed=np.any(s!=b,axis=2).astype('uint8')
 source=R/'localized'/(j['id']+'.png')
 if not source.exists():source=R/'generated'/(j['id']+'.png')
 phase=read(source,(w*4,h*4));mode='identical';valid_fraction=None;cc=None;transform=None
 if changed.any():
  # Close small holes in the object's source support, not the rectangular crop.
  support=cv2.morphologyEx(changed,cv2.MORPH_CLOSE,np.ones((5,5),np.uint8));support=cv2.dilate(support,np.ones((7,7),np.uint8));mask=cv2.resize(support,(w*4,h*4),interpolation=cv2.INTER_NEAREST)
  alpha=np.minimum(cv2.distanceTransform(np.pad(mask,1),cv2.DIST_L2,5)[1:-1,1:-1]/12,1)[...,None]
  animated=any(e['control'] is not None for e in j['entries'])
  use_flow=False
  if animated:
   gray=lambda v:cv2.cvtColor(cv2.resize(v,(w*2,h*2),interpolation=cv2.INTER_CUBIC),cv2.COLOR_RGB2GRAY)
   dis=cv2.DISOpticalFlow_create(cv2.DISOPTICAL_FLOW_PRESET_MEDIUM);f=dis.calc(gray(s),gray(b),None);back=dis.calc(gray(b),gray(s),None);y,x=np.mgrid[:h*2,:w*2].astype(np.float32);fb=cv2.remap(back,x+f[:,:,0],y+f[:,:,1],cv2.INTER_LINEAR);err=cv2.resize(np.linalg.norm(f+fb,axis=2),(w*4,h*4));f=cv2.resize(f,(w*4,h*4))*2;y,x=np.mgrid[:h*4,:w*4].astype(np.float32)
   lowbase=cv2.resize(b,(w*4,h*4),interpolation=cv2.INTER_CUBIC);lowphase=cv2.resize(s,(w*4,h*4),interpolation=cv2.INTER_CUBIC);match=cv2.remap(lowbase,x+f[:,:,0],y+f[:,:,1],cv2.INTER_LINEAR,borderMode=cv2.BORDER_REPLICATE);error=np.max(abs(lowphase.astype(float)-match),2);valid=(err<1.5)&(error<30)&(np.linalg.norm(f,axis=2)<48)
   core=cv2.resize(changed,(w*4,h*4),interpolation=cv2.INTER_NEAREST).astype(bool);valid_fraction=float(valid[core].mean());use_flow=valid_fraction>=.96 and float(np.percentile(np.linalg.norm(f,axis=2)[core],95))<=24
  if use_flow:
   warp=cv2.remap(base,x+f[:,:,0],y+f[:,:,1],cv2.INTER_CUBIC,borderMode=cv2.BORDER_REPLICATE)
   # Rejected correspondences retain a source-driven low-frequency delta,
   # rather than introducing a differently generated patch into the material.
   residual=base.astype(float)+lowphase.astype(float)-lowbase.astype(float)
   confidence=cv2.GaussianBlur(valid.astype(np.float32),(0,0),1)[...,None];candidate=warp*confidence+np.clip(residual,0,255)*(1-confidence);mode='native-bounded-material-transfer'
  else:
   candidate=phase.astype(float);mode='registered-generated-disocclusion'
   local=locals.get(j['id'])
   if local:
    box=[q*4 for q in local['local_bbox']];sl=np.s_[box[1]:box[3],box[0]:box[2]];aa=base[sl].copy();pp=phase[sl].copy();fixed=cv2.resize((1-support)*255,(w*4,h*4),interpolation=cv2.INTER_NEAREST)[sl].copy();warp=np.eye(2,3,dtype=np.float32)
    if np.count_nonzero(fixed)>200:
     try:
      cc,warp=cv2.findTransformECC(cv2.cvtColor(aa,cv2.COLOR_RGB2GRAY).astype('float32')/255,cv2.cvtColor(pp,cv2.COLOR_RGB2GRAY).astype('float32')/255,warp,cv2.MOTION_AFFINE,(cv2.TERM_CRITERIA_COUNT|cv2.TERM_CRITERIA_EPS,100,1e-5),fixed,5)
      corners=np.array([[0,0,1],[pp.shape[1],0,1],[0,pp.shape[0],1],[pp.shape[1],pp.shape[0],1]],np.float32);delta=np.max(np.linalg.norm(corners@warp.T-corners[:,:2],axis=1))
      if cc>=.90 and delta<=24 and np.max(abs(warp[:,:2]-np.eye(2)))<=.06:
       candidate[sl]=cv2.warpAffine(pp,warp,(pp.shape[1],pp.shape[0]),flags=cv2.INTER_CUBIC|cv2.WARP_INVERSE_MAP,borderMode=cv2.BORDER_REFLECT);transform=warp.tolist()
     except cv2.error:cc=None
   # Match only the unchanged source context in a narrow ring. This cannot
   # infer the colour of a newly revealed surface or move its geometry.
   ring=(cv2.dilate(support,np.ones((11,11),np.uint8))!=0)&(support==0);ring=cv2.resize(ring.astype('uint8'),(w*4,h*4),interpolation=cv2.INTER_NEAREST).astype(bool)
   if ring.sum()>64:
    offset=np.clip(np.median(base.astype(float)[ring]-candidate[ring],axis=0),-24,24);candidate=np.clip(candidate+offset,0,255)
  result=np.uint8(np.clip(np.round(base*(1-alpha)+candidate*alpha),0,255));outside=mask==0;assert np.array_equal(result[outside],base[outside])
 else:result=base
 path=out/(j['id']+'.png');tmp=path.with_suffix('.tmp');Image.fromarray(result).save(tmp,format='PNG');tmp.replace(path)
 records.append({'id':j['id'],'entries':j['entries'],'mode':mode,'native_correspondence_valid_fraction':valid_fraction,'immutable_context_correlation':cc,'accepted_transform':transform,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
 print(j['id'],mode,flush=True)
report=R/'coherence-report.json'
if rooms and report.exists():
 old=json.loads(report.read_text())['jobs'];ids={x['id'] for x in records};records=[x for x in old if x['id'] not in ids]+records
report.write_text(json.dumps({'experimental':True,'visual_approval_required':True,'jobs':records},indent=2));print('Prepared',len(records),'contexts; original pack unchanged.')
