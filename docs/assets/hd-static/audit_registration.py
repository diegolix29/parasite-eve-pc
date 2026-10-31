"""Measure registration evidence; low feature counts are uncertain, not proof of failure."""
from pathlib import Path
from PIL import Image
import cv2,numpy as np,json,math,argparse
p=argparse.ArgumentParser();p.add_argument("--normalized",action="store_true");args=p.parse_args()
R=Path(__file__).resolve().parent;J=json.loads((R/'jobs.json').read_text());records=[];tasks=json.loads((R/'tasks.json').read_text())['tasks'];views=json.loads((R/'views.json').read_text());sift=cv2.SIFT_create(nfeatures=3000);clahe=cv2.createCLAHE(clipLimit=2.0,tileGridSize=(8,8))
for j in J:
 p=R/'generated'/(j['id']+'.png')
 if not p.exists():continue
 a=np.array(Image.open(j['source']).convert('RGB')); im=Image.open(p).convert('RGB')
 if args.normalized and j['kind']=='entry':
  t=next(t for t in tasks if Path(t['active_input']).stem==j['id']);v=next(v for v in views if (v['room'],v['group'])==(t['room'],t['group']));q=R/'OpenAI'/t['input']
  if not q.exists():continue
  im=Image.open(q).convert('RGB').crop(tuple(x*4 for x in v['active_bbox']))
 b=np.array(im.resize((a.shape[1],a.shape[0]),Image.Resampling.LANCZOS));ag=clahe.apply(cv2.cvtColor(a,cv2.COLOR_RGB2GRAY));bg=clahe.apply(cv2.cvtColor(b,cv2.COLOR_RGB2GRAY));ka,da=sift.detectAndCompute(ag,None);kb,db=sift.detectAndCompute(bg,None); rec=dict(id=j['id'],views=j['views'],kind=j['kind'],source_size=j['size'],generated_size=list(im.size),luma_ratio=float(b.mean()/max(1,a.mean())),status='insufficient_features')
 if da is not None and db is not None:
  pairs=cv2.BFMatcher().knnMatch(da,db,k=2);good=[m for pair in pairs if len(pair)==2 for m,n in [pair] if m.distance<0.72*n.distance];rec['matches']=len(good)
  if len(good)>=8:
   src=np.float32([ka[m.queryIdx].pt for m in good]);dst=np.float32([kb[m.trainIdx].pt for m in good]);M,mask=cv2.estimateAffinePartial2D(src,dst,method=cv2.RANSAC,ransacReprojThreshold=2,maxIters=4000)
   if M is not None:
    rec.update(inliers=int(mask.sum()),scale=float(math.hypot(M[0,0],M[1,0])),rotation_degrees=float(math.degrees(math.atan2(M[1,0],M[0,0]))),translation_native=[float(M[0,2]),float(M[1,2])]); rec['status']='insufficient_features' if rec['inliers']<8 else 'registration_consistent' if rec['inliers']>=8 and abs(rec['scale']-1)<0.025 and abs(rec['rotation_degrees'])<1 and max(abs(x)for x in rec['translation_native'])<4 else 'registration_review'
 records.append(rec)
proof=dict(note=__doc__,complete_sources=len(records),statuses={s:sum(r['status']==s for r in records)for s in sorted({r['status']for r in records})},results=records);(R/('registration-normalized-audit.json' if args.normalized else 'registration-audit.json')).write_text(json.dumps(proof,indent=2));print({k:v for k,v in proof.items()if k!='results'})
