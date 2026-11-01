"""Register generated material to native coordinates; retain raw outputs/provenance.

One global affine correction handles model output aspect/size differences. This
is not optical flow and never deforms animation or alters native layer geometry.
Uncovered edges use deterministic source restoration. Uncertain fits are flagged
for visual review, never described as validated registration.
"""
from pathlib import Path
from PIL import Image
import cv2, json, shutil, numpy as np
R=Path(__file__).resolve().parent;out=R/'registered-anchors';out.mkdir(exist_ok=True)
jobs=json.loads((R/'anchor-jobs.json').read_text());sift=cv2.SIFT_create(nfeatures=4000);clahe=cv2.createCLAHE(2,(8,8));records=[]
for j in jobs:
 p=R/'generated'/(j['id']+'.png');q=R/'source-sr-full'/p.name
 if j.get('source_only')and q.exists()and not p.exists():
  shutil.copy2(q,p)
  (R/'metadata'/(j['id']+'.json')).write_text(json.dumps({'id':j['id'],'source_only':True,'source':j['source'],'reason':j.get('source_only_reason','Text-heavy original UI'),'method':'RealESRNet_x4plus source preservation','weights_sha256':'a820b9bde89a874d7599d545567308ce6c128fc8754a53208eda016d40aa81df'},indent=2))
 if not p.exists()or not q.exists():continue
 source=np.array(Image.open(j['source']).convert('RGB'));h,w=source.shape[:2];image=np.array(Image.open(p).convert('RGB').resize((w*4,h*4),Image.Resampling.LANCZOS));small=cv2.resize(image,(w,h),interpolation=cv2.INTER_AREA)
 if j.get('source_only'):
  fixed=np.array(Image.open(q).convert('RGB'));rec={'id':j['id'],'views':j['views'],'status':'source preserved'}
 else:
  ka,da=sift.detectAndCompute(clahe.apply(cv2.cvtColor(source,cv2.COLOR_RGB2GRAY)),None);kb,db=sift.detectAndCompute(clahe.apply(cv2.cvtColor(small,cv2.COLOR_RGB2GRAY)),None);rec={'id':j['id'],'views':j['views'],'status':'uncertain fit'};fixed=image
  if da is not None and db is not None:
   pairs=cv2.BFMatcher().knnMatch(db,da,k=2);good=[m for pair in pairs if len(pair)==2 for m,n in[pair]if m.distance<.72*n.distance]
   if len(good)>=12:
    src=np.float32([kb[m.queryIdx].pt for m in good]);dst=np.float32([ka[m.trainIdx].pt for m in good]);matrix,inliers=cv2.estimateAffine2D(src,dst,method=cv2.RANSAC,ransacReprojThreshold=1.5,maxIters=6000)
    if matrix is not None:
     count=int(inliers.sum());corners=np.float32([[0,0],[w,0],[0,h],[w,h]]);moved=corners@matrix[:,:2].T+matrix[:,2];displacement=float(np.max(np.linalg.norm(moved-corners,axis=1)));singular=np.linalg.svd(matrix[:,:2],compute_uv=False);error=np.linalg.norm(src@matrix[:,:2].T+matrix[:,2]-dst,axis=1);rec.update(inliers=count,matrix_native=matrix.tolist(),corner_displacement=displacement,median_inlier_error=float(np.median(error[inliers[:,0]!=0])))
     if count>=12 and np.all((singular>.85)&(singular<1.18))and displacement<.14*np.hypot(w,h):
      high=matrix.copy();high[:,2]*=4
      warped=cv2.warpAffine(image,high,(w*4,h*4),flags=cv2.INTER_LANCZOS4)
      valid=cv2.warpAffine(np.ones((h*4,w*4),np.uint8),high,(w*4,h*4),flags=cv2.INTER_NEAREST)
      # Inward feather only at the edge introduced by the global transform.
      alpha=np.clip(cv2.distanceTransform(np.pad(valid,1),cv2.DIST_L2,5)[1:-1,1:-1]/8,0,1)[...,None]
      restored=np.array(Image.open(q).convert('RGB'));fixed=np.uint8(np.clip(np.rint(warped*alpha+restored*(1-alpha)),0,255));rec.update(status='global affine registered',source_edge_pixels=int((valid==0).sum()))
 path=out/p.name;tmp=path.with_suffix('.tmp');Image.fromarray(fixed).save(tmp,format='PNG');tmp.replace(path);records.append(rec)
(R/'anchor-alignment.json').write_text(json.dumps({'method':__doc__,'anchors':records},indent=2));print(len(records),'anchors;',sum(x['status']=='uncertain fit'for x in records),'uncertain fits',flush=True)
