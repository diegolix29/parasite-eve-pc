"""Reproducible tester corrections, staging only; accepted rooms are immutable."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFilter
import json,shutil,numpy as np,cv2
R=Path(__file__).resolve().parents[1];D=R/'feedback-fixes';out=R/'OpenAI-feedback';out.mkdir(exist_ok=True);tasks=json.loads((R/'tasks.json').read_text())['tasks'];reported={'M003','M006','M013','M016','M020','M021','M026','M027','M028','M029','M032','M033','M034','M037'}
def task(room,e):return next(t for t in tasks if t['room']==room and t['entry']==e)
def read(t,folder='OpenAI'):return Image.open(R/folder/t['input']).convert('RGB')
def save(im,path):
 temp=path.with_suffix('.tmp');im.save(temp,format='PNG');temp.replace(path)
for name in {t['input'] for t in tasks if t['room'] in reported}:
 p=out/name;tmp=p.with_suffix('.tmp');shutil.copyfile(R/'OpenAI'/name,tmp);tmp.replace(p)
# Only the uncovered floor plate changes; every door atlas remains identical.
t=task('M003',13);base=read(t);patch=Image.open(D/'M003-carpet-generated.png').convert('RGB').resize((540,400),Image.Resampling.LANCZOS);old=np.array(base).astype(float);new=np.array(patch).astype(float);gain=old[420:480,600:760,0].mean()/max(new[320:380,200:360,0].mean(),1);patch=Image.fromarray(np.uint8(np.clip(np.round(new*gain),0,255)));layer=base.copy();layer.paste(patch,(400,100));mask=Image.new('L',base.size);ImageDraw.Draw(mask).polygon([(584,136),(760,136),(840,496),(492,496)],fill=255);mask=mask.filter(ImageFilter.GaussianBlur(8));save(Image.composite(layer,base,mask),out/t['input'])
# Closed front-door material differs natively at one pixel, not in geometry.
a,b=task('M006',2),task('M006',9);s=np.array(Image.open(R/'inputs'/b['input'])).astype(np.float32);n=np.array(Image.open(R/'inputs'/a['input'])).astype(np.float32);hd=np.array(read(a)).astype(np.float32);delta=cv2.resize(s-n,(hd.shape[1],hd.shape[0]),interpolation=cv2.INTER_CUBIC);save(Image.fromarray(np.uint8(np.clip(np.round(hd+delta),0,255))),out/b['input'])
# The first two poses of each door are visually closed. Reuse one HD
# material there; the original per-pose alpha/positions still determine edges.
for entry,reference in [(3,2),(10,9)]:
 t=task('M006',entry);a=task('M006',reference);save(read(a,'OpenAI-feedback'),out/t['input'])
# A static plate must agree with the canonical scene wherever its own source
# did not change, regardless of what other animation phases do there.
def lock_static(room,entry):
 a,b=task(room,0),task(room,entry);s=np.array(Image.open(R/'inputs'/b['input']));n=np.array(Image.open(R/'inputs'/a['input']));hd=np.array(read(b));canonical=np.array(read(a));mask=np.repeat(np.repeat(np.all(s==n,2),4,0),4,1);hd[mask]=canonical[mask];save(Image.fromarray(hd),out/b['input'])
lock_static('M016',23)
# Keep source-only metal poses, without the earlier support/closed-pose blend
# which pulled floor pixels into a newly exposed rotating door panel.
for t in tasks:
 if t['room']=='M021' and t['control'] is not None:
  p=R/'localized'/t['active_input']
  if p.exists():
   a=task('M021',0);full=read(a);layer=full.copy();layer.paste(Image.open(p).convert('RGB'),(0,0))
   source=np.array(Image.open(R/'inputs'/t['input']));native0=np.array(Image.open(R/'inputs'/a['input']));mask=np.any(source!=native0,2).astype(np.uint8);ys,xs=np.nonzero(mask)
   if len(xs):
    hull=cv2.convexHull(np.column_stack([xs,ys]).astype('int32'));mask[:]=0;cv2.fillConvexPoly(mask,hull,255);alpha=Image.fromarray(mask).resize(full.size,Image.Resampling.LANCZOS);full=Image.composite(layer,full,alpha)
   save(full,out/t['input'])
for e in [1,2,51]:lock_static('M021',e)
# Separate static passage and moving metal. The original first gate mask gives
# the opening's convex silhouette, including gaps between the metal slats.
t,gate=task('M032',11),task('M032',0);base=read(t);layer=base.copy();patch=Image.open(D/'M032-underlay-generated.png').convert('RGB').resize((372,452),Image.Resampling.LANCZOS);layer.paste(patch,(140,68));mask=np.zeros((336,512),np.uint8)
for (x,y),ref in zip(gate['tiles'],np.load(R/'refs'/gate['ref'])):mask[y:y+16,x:x+16]=np.maximum(mask[y:y+16,x:x+16],np.uint8(ref!=0)*255)
ys,xs=np.nonzero(mask);hull=cv2.convexHull(np.column_stack([xs,ys]).astype('int32'));mask[:]=0;cv2.fillConvexPoly(mask,hull,255);alpha=Image.fromarray(mask).resize(base.size,Image.Resampling.LANCZOS);underlay=Image.composite(layer,base,alpha);save(underlay,D/'M032-underlay-full.png');save(underlay,out/t['input']);save(alpha,D/'M032-opening-mask.png')
print('Structural source-material corrections staged; no original masks or timing modified.')
