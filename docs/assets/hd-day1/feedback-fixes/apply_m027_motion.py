"""Advect one fixed HD water material by original per-phase native motion.
No generated phase-detail crossfades. Original masks and controller slots retained.
"""
from pathlib import Path
import json,cv2,numpy as np
from PIL import Image
R=Path(__file__).resolve().parents[1]
c=next(c for c in json.loads((R/'feedback-fixes/temporal-jobs.json').read_text()) if c['room']=='M027')
fs=c['frames'];w,h=c['size'];S=4
native=[np.array(Image.open(f['source']).convert('RGB'),dtype=np.float32) for f in fs]
base=np.array(Image.open(fs[0]['target']).convert('RGB').resize((w*S,h*S),Image.Resampling.LANCZOS),dtype=np.float32)
changed=np.any(np.stack(native)!=native[0],axis=(0,3)).astype('uint8')
support=cv2.dilate(cv2.morphologyEx(changed,cv2.MORPH_CLOSE,np.ones((7,7),np.uint8)),np.ones((3,3),np.uint8))
mask=cv2.resize(support,(w*S,h*S),interpolation=cv2.INTER_NEAREST)
alpha=np.minimum(cv2.distanceTransform(np.pad(mask,1),cv2.DIST_L2,5)[1:-1,1:-1]/8,1)[...,None]
# Same exposure transform for all phases: dark original RGB555 water otherwise
# falls below the optical-flow solver's contrast threshold.
gray=[np.uint8(np.clip(cv2.cvtColor(x,cv2.COLOR_RGB2GRAY)*8,0,255)) for x in native]
y,x=np.mgrid[:h,:w].astype('float32');Y,X=np.mgrid[:h*S,:w*S].astype('float32');outputs=[];records=[]
for i,f in enumerate(fs):
 flow=np.zeros((h,w,2),np.float32) if i==0 else cv2.calcOpticalFlowFarneback(gray[i],gray[0],None,.5,4,15,7,5,1.1,0)
 # Backward sampling map, directly from phase i to phase zero. No cumulative drift.
 hdflow=cv2.resize(flow,(w*S,h*S),interpolation=cv2.INTER_CUBIC)*S
 material=cv2.remap(base,X+hdflow[...,0],Y+hdflow[...,1],cv2.INTER_CUBIC,borderMode=cv2.BORDER_REFLECT_101)
 warped_native=cv2.remap(native[0],x+flow[...,0],y+flow[...,1],cv2.INTER_CUBIC,borderMode=cv2.BORDER_REFLECT_101)
 # Only the source's remaining moving illumination; never blend new AI details.
 ratio=(cv2.GaussianBlur(native[i],(0,0),1.5)+4)/(cv2.GaussianBlur(warped_native,(0,0),1.5)+4)
 ratio=cv2.resize(np.clip(ratio,.6,1.67),(w*S,h*S),interpolation=cv2.INTER_CUBIC)
 out=np.uint8(np.clip(np.round(base*(1-alpha)+material*ratio*alpha),0,255));outputs.append(out)
 p=R/'OpenAI-feedback'/f['input'];tmp=p.with_suffix('.tmp');Image.fromarray(out).save(tmp,format='PNG');tmp.replace(p)
 records.append({'phase':i,'native_flow_median':float(np.median(np.linalg.norm(flow[changed!=0],axis=1))),'native_flow_p95':float(np.percentile(np.linalg.norm(flow[changed!=0],axis=1),95)),'outside_support_unchanged':bool(np.array_equal(out[mask==0],np.uint8(np.round(base[mask==0]))))})
 np.save(R/'feedback-fixes'/f'M027-phase{i}-backward-flow.npy',flow)
assert all(r['outside_support_unchanged'] for r in records)
# Compare translated high-frequency structure against the previous static-detail
# hypothesis; brightness-only/detail-strength changes cannot satisfy this check.
roi=(slice(80,440),slice(400,880));b=cv2.cvtColor(base,cv2.COLOR_RGB2GRAY);b=b-cv2.GaussianBlur(b,(0,0),3)
for i,out in enumerate(outputs):
 if not i:continue
 g=cv2.cvtColor(out.astype('float32'),cv2.COLOR_RGB2GRAY);g-=cv2.GaussianBlur(g,(0,0),3)
 flow=np.load(R/'feedback-fixes'/f'M027-phase{i}-backward-flow.npy');hf=cv2.resize(flow,(w*S,h*S))*S;warped=cv2.remap(b,X+hf[...,0],Y+hf[...,1],cv2.INTER_CUBIC,borderMode=cv2.BORDER_REFLECT_101)
 e0=float(np.mean(abs(g[roi]-b[roi])));e1=float(np.mean(abs(g[roi]-warped[roi])))
 assert e1<e0*.7,(i,e0,e1)
 records[i]['fixed_detail_error']=e0;records[i]['advected_detail_error']=e1
(R/'feedback-fixes/M027-motion-application.json').write_text(json.dumps({'method':'One fixed material, backward optical flow from each native phase to phase zero, source-only illumination residual. Four original slots, including original wrap; no cumulative or interpolated extra frames.','phases':records},indent=2)+'\n')
print(json.dumps(records))
