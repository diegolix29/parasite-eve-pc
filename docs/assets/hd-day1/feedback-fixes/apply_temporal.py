"""Build a staged asset candidate from sequential OpenAI phases and native timing.
Only original moving support changes. No frames are inserted or reordered.
Native-identical source phases share one identical HD material.
"""
from pathlib import Path
import json,hashlib
import numpy as np,cv2
from PIL import Image
R=Path(__file__).resolve().parents[1];chains=json.loads((R/'feedback-fixes/temporal-jobs.json').read_text());tasks=json.loads((R/'tasks.json').read_text())['tasks'];report=[]
def rgb(p,size=None):
 im=Image.open(p).convert('RGB')
 if size:im=im.resize(size,Image.Resampling.LANCZOS)
 return np.array(im).astype(np.float32)
def save(a,p):
 tmp=p.with_suffix('.tmp');Image.fromarray(np.uint8(np.clip(np.round(a),0,255))).save(tmp,format='PNG');tmp.replace(p)
for c in chains:
 if c['room']=='M037':continue
 fs=c['frames'];w,h=c['size'];missing=[f['phase'] for f in fs if not Path(f['target']).exists()]
 if missing:print(c['room'],c['group'],'pending',missing);continue
 sources=[rgb(f['source']) for f in fs];base=rgb(fs[0]['target'],(w*4,h*4));native0=sources[0];changed=np.any(np.stack(sources)!=native0,axis=(0,3)).astype('uint8')
 support=cv2.morphologyEx(changed,cv2.MORPH_CLOSE,np.ones((7,7),np.uint8));support=cv2.dilate(support,np.ones((3,3),np.uint8));mask=cv2.resize(support,(w*4,h*4),interpolation=cv2.INTER_NEAREST);alpha=np.minimum(cv2.distanceTransform(np.pad(mask,1),cv2.DIST_L2,5)[1:-1,1:-1]/8,1)[...,None];active=mask!=0;measure=changed!=0
 fixed=(1-cv2.dilate(support,np.ones((7,7),np.uint8)))*255;refgray=cv2.cvtColor(cv2.resize(base,(w,h)),cv2.COLOR_RGB2GRAY)/255
 raws=[];identities={};duplicates={};near_duplicates={};registrations=[]
 for i,f in enumerate(fs):
  key=hashlib.sha256(sources[i].tobytes()).hexdigest()
  if key in identities:raws.append(raws[identities[key]].copy());duplicates[i]=identities[key];registrations.append(None);continue
  near=next((j for j in range(i) if int(np.any(sources[i]!=sources[j],axis=2).sum())<=4),None)
  if near is not None:
   tiny=cv2.resize(sources[i]-sources[near],(w*4,h*4),interpolation=cv2.INTER_CUBIC);raws.append(np.clip(raws[near]+tiny,0,255));near_duplicates[i]=near;registrations.append(None);continue
  identities[key]=i;phase=rgb(f['target'],(w*4,h*4));warp=np.eye(2,3,dtype=np.float32);fit=None
  if i and np.count_nonzero(fixed)>200:
   try:
    cc,warp=cv2.findTransformECC(refgray.astype(np.float32),(cv2.cvtColor(cv2.resize(phase,(w,h)),cv2.COLOR_RGB2GRAY)/255).astype(np.float32),warp,cv2.MOTION_AFFINE,(cv2.TERM_CRITERIA_COUNT|cv2.TERM_CRITERIA_EPS,100,1e-5),fixed,5)
    corners=np.array([[0,0,1],[w,0,1],[0,h,1],[w,h,1]],np.float32);displacement=float(np.max(np.linalg.norm(corners@warp.T-corners[:,:2],axis=1)))
    if cc>.9 and displacement<=6 and np.max(abs(warp[:,:2]-np.eye(2)))<.04:
     fit={'correlation':cc,'native_corner_displacement':displacement,'matrix':warp.tolist()};warp[:,2]*=4;phase=cv2.warpAffine(phase,warp,(w*4,h*4),flags=cv2.INTER_LANCZOS4|cv2.WARP_INVERSE_MAP,borderMode=cv2.BORDER_REPLICATE)
   except cv2.error:pass
  # Lock exposure to the accepted first image, allowing only original mean change.
  desired=base[active].mean(0)*np.clip(sources[i][measure].mean(0)/np.maximum(native0[measure].mean(0),1),.9,1.1)
  phase=np.clip(phase+(desired-phase[active].mean(0)),0,255) if i else base.copy()
  raws.append(phase);registrations.append(fit)
 # One gain for the entire cycle avoids independently increasing the last-to-first step.
 native_light=max(float(native0[measure].mean()),1);hd_light=max(float(base[active].mean()),1);gain=1.;edges=[]
 for i in range(len(fs)):
  j=(i+1)%len(fs);n=float(np.abs(sources[i]-sources[j])[measure].mean())/native_light;raw=float(np.abs(raws[i]-raws[j])[active].mean())/hd_light
  if n>0 and raw>0:gain=min(gain,n/raw)
  edges.append({'from':i,'to':j,'native_relative_l1':n,'raw_relative_l1':raw})
 finals=[]
 for i,f in enumerate(fs):
  final=np.clip(base+(raws[i]-base)*gain*alpha,0,255);final=np.uint8(np.round(final));assert np.array_equal(final[~active],np.uint8(np.round(base[~active])))
  if i in duplicates:assert np.array_equal(final,finals[duplicates[i]])
  finals.append(final);task=next(t for t in tasks if (t['room'],t['group'],t['entry'])==(c['room'],c['group'],f['entry']));out=rgb(R/'OpenAI-feedback'/task['input']);x0,y0,x1,y1=c['bbox'];out[y0*4:y1*4,x0*4:x1*4]=final;save(out,R/'OpenAI-feedback'/task['input'])
 # The same water material must also be used by the static plate beneath
 # the moving sprites. Retain its original low-frequency source illumination.
 for task in tasks:
  if (task['room'],task['group'])!=(c['room'],c['group']) or task['control'] is not None:continue
  source_full=rgb(R/'inputs'/task['input']);x0,y0,x1,y1=c['bbox'];ns=source_full[y0:y1,x0:x1]
  ratio=(cv2.GaussianBlur(ns,(0,0),1.5)+4)/(cv2.GaussianBlur(native0,(0,0),1.5)+4);ratio=cv2.resize(np.clip(ratio,.25,2),(w*4,h*4),interpolation=cv2.INTER_CUBIC)
  static_source=R/'feedback-fixes/M032-underlay-full.png' if c['room']=='M032' and task['entry']==11 else R/'OpenAI'/task['input'];out=rgb(static_source);part=out[y0*4:y1*4,x0*4:x1*4];shared=np.clip(base*ratio,0,255);out[y0*4:y1*4,x0*4:x1*4]=part*(1-alpha)+shared*alpha;save(out,R/'OpenAI-feedback'/task['input'])
 report.append({'room':c['room'],'group':c['group'],'controller':c['controller'],'gain':gain,'native_identical_phases':duplicates,'near_identical_phases':near_duplicates,'edges_including_wrap':edges,'registrations':registrations,'outside_moving_support_identical':True})
 print(c['room'],c['group'],'applied',len(finals),'phases, gain',round(gain,4),'duplicates',duplicates)
(R/'feedback-fixes/temporal-application.json').write_text(json.dumps(report,indent=2))
