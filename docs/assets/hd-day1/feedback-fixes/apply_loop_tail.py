"""Reconstruct M037 phase18 between both cyclic neighbours in HD motion space.
Preserve the other eighteen HD phases, original slots, masks and reference RGB.
"""
from pathlib import Path
from PIL import Image
import json,numpy as np,cv2
R=Path(__file__).resolve().parents[1];c=next(c for c in json.loads((R/'feedback-fixes/temporal-jobs.json').read_text()) if c['room']=='M037');fs=c['frames'];w,h=c['size'];S=4
read=lambda p,size:np.array(Image.open(p).convert('RGB').resize(size,Image.Resampling.LANCZOS),dtype=np.float32)
sources=[read(x['source'],(w,h)) for x in fs];base=read(fs[0]['target'],(w*S,h*S));target=sources[-1]
support=np.any(np.stack(sources)!=sources[0],axis=(0,3)).astype('uint8');support=cv2.morphologyEx(support,cv2.MORPH_CLOSE,np.ones((3,3),np.uint8));mask=cv2.resize(support,(w*S,h*S),interpolation=cv2.INTER_NEAREST);alpha=np.minimum(cv2.distanceTransform(np.pad(mask,1),cv2.DIST_L2,5)[1:-1,1:-1]/4,1)[...,None]
Y,X=np.mgrid[:h*S,:w*S].astype('float32');neighbours=[]
for i in [17,0]:
 full=np.array(Image.open(R/'OpenAI-feedback'/fs[i]['input']).convert('RGB'),dtype=np.float32);x0,y0,x1,y1=c['bbox'];neighbours.append(full[y0*S:y1*S,x0*S:x1*S])
gray=lambda a:np.uint8(np.clip(cv2.cvtColor(cv2.resize(a,(w,h)),cv2.COLOR_RGB2GRAY)*2,0,255))
materials=[];records=[]
for i,(a,b) in enumerate([(neighbours[0],neighbours[1]),(neighbours[1],neighbours[0])]):
 # Half a two-frame interval, estimated in the existing HD material's space.
 # Native-image motion cannot be applied directly to independently restored
 # buildings: even subpixel artwork misregistration otherwise bends the road.
 flow=cv2.calcOpticalFlowFarneback(gray(a),gray(b),None,.5,4,21,7,7,1.5,0)
 hf=cv2.resize(flow,(w*S,h*S),interpolation=cv2.INTER_CUBIC)*S
 materials.append(cv2.remap(a,X-hf[...,0]*.5,Y-hf[...,1]*.5,cv2.INTER_CUBIC,borderMode=cv2.BORDER_REFLECT_101))
 records.append({'source_phase':[17,0][i],'native_pixel_flow_median':float(np.median(np.linalg.norm(flow[support!=0],axis=1)))})
material=(materials[0]+materials[1])*.5
raw=np.clip(material,0,255);out=np.uint8(np.clip(np.round(base*(1-alpha)+raw*alpha),0,255));assert np.array_equal(out[mask==0],np.uint8(np.round(base[mask==0])))
f=fs[-1];full=Image.open(R/'OpenAI'/f['input']).convert('RGB');full.paste(Image.fromarray(out),(c['bbox'][0]*S,c['bbox'][1]*S));p=R/'OpenAI-feedback'/f['input'];tmp=p.with_suffix('.tmp');full.save(tmp,format='PNG');tmp.replace(p)
(R/'feedback-fixes/M037-tail-application.json').write_text(json.dumps({'phase':18,'previous_phase':17,'next_phase':0,'method':'Bidirectional motion-compensated midpoint between exact HD phases17 and0. Native phase18 mask retained; no generated endpoint, RGB sharpening or copied first frame.','hd_motion_sources':records,'other_18_phases_unchanged':True,'outside_original_moving_support_unchanged':True},indent=2)+'\n')
