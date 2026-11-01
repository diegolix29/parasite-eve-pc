"""M037: fixed OpenAI cabin/mirror frame, nineteen source-anchored street phases.
Uses deterministic RealESRNet x4 outputs; never invents geometry per phase.
"""
from pathlib import Path
from PIL import Image,ImageDraw
import argparse,json,numpy as np,cv2
R=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--destination',default='OpenAI-feedback');args=p.parse_args();dest=R/args.destination;dest.mkdir(exist_ok=True)
c=next(c for c in json.loads((R/'feedback-fixes/temporal-jobs.json').read_text()) if c['room']=='M037');fs=c['frames'];w,h=c['size'];S=4
rgb=lambda p:np.array(Image.open(p).convert('RGB'),dtype=np.float32)
base=rgb(fs[0]['target']);base=cv2.resize(base,(w*S,h*S),interpolation=cv2.INTER_LANCZOS4)
sources=[rgb(f['source']) for f in fs];sr=[rgb(R/'feedback-fixes/M037-source-sr'/f'p{i:02d}.png') for i in range(19)]
# Fixed HD window apertures, inset from the accepted canonical pillar edges.
# A temporal-difference mask leaves invariant road texels as OpenAI material
# (including invented white lane marks) and lets edge streaks touch the pillars.
apertures=Image.new('L',(w*S,h*S));d=ImageDraw.Draw(apertures)
d.polygon([(143,168),(486,149),(793,149),(1138,168),(1116,334),(970,334),(929,363),(350,363),(320,334),(164,334)],fill=255)
d.polygon([(0,198),(16,218),(38,274),(67,329),(0,329)],fill=255)
d.polygon([(1280,196),(1263,219),(1240,270),(1217,329),(1280,329)],fill=255)
mask=(np.array(apertures)>0).astype('uint8')
# Explicit canonical mirror silhouette, with a separate moving reflection aperture.
# Native phase masks alone cannot protect a restored edge shifted by a fraction
# of a pixel. This outline is the same fixed mask in all nineteen frames.
mirror=Image.new('L',(w*S,h*S));d=ImageDraw.Draw(mirror);d.rounded_rectangle((130*S,49*S,192*S,65*S),radius=7*S,fill=255);d.rectangle((159*S,34*S,162*S,50*S),fill=255);mirror=np.array(mirror)!=0;reflection=Image.new('L',(w*S,h*S));ImageDraw.Draw(reflection).rounded_rectangle((132*S,50*S,190*S,63*S),radius=5*S,fill=255);reflection=np.array(reflection)!=0;mirror_shell=mirror & ~reflection;mask[mirror]=0;mask[reflection]=1
alpha=np.minimum(cv2.distanceTransform(np.pad(mask,1),cv2.DIST_L2,5)[1:-1,1:-1]/3,1)[...,None];active=alpha[...,0]>.99
# One exposure calibration for the entire sequence, no per-phase matching.
gain=np.array(json.loads((R/'feedback-fixes/M037-accepted-exposure.json').read_text())['gain_rgb'])
# Pillar silhouettes come from the same original image as the moving street.
# The OpenAI plate includes bright scene pixels beside its slightly shifted
# silhouette. Insetting a window mask freezes those pixels onto the pillar.
# Replace the whole boundary neighbourhood, not just its temporal edge pixels.
pillar_region=np.zeros((h*S,w*S),np.uint8)
pillar_region[120:345,:196]=1;pillar_region[120:345,1092:]=1
pillar_alpha=np.minimum(cv2.distanceTransform(np.pad(pillar_region,1),cv2.DIST_L2,5)[1:-1,1:-1]/8,1)[...,None]
stable_dark=(~np.any(np.stack(sources)!=sources[0],axis=(0,3))) & (sources[0].max(2)<32)
stable_dark=cv2.resize(stable_dark.astype('uint8'),(w*S,h*S),interpolation=cv2.INTER_NEAREST)
stable_dark=cv2.erode(stable_dark,np.ones((5,5),np.uint8)).astype(bool)
fixed_source=np.clip(sr[0]*gain,0,255)
frames=[]
for i in range(19):
 street=np.clip(sr[i]*gain,0,255)
 # Recover the original yellow lane-paint chroma lost by SR smoothing.
 # Smooth source-driven alpha, never an opaque native-pixel patch or new stripe.
 native=sources[i];yellow=np.maximum(np.minimum(native[...,0],native[...,1])-native[...,2]-8,0)/32
 road=np.zeros((h,w),np.float32);road[78:90,142:166]=1
 yellow=cv2.resize(np.clip(yellow,0,1)*road,(w*S,h*S),interpolation=cv2.INTER_LANCZOS4);yellow=np.clip(yellow,0,1)[...,None]
 native_hd=cv2.resize(native,(w*S,h*S),interpolation=cv2.INTER_LANCZOS4)
 street=street*(1-yellow)+np.clip(native_hd*gain,0,255)*yellow
 out=base*(1-alpha)+street*alpha
 pillar_source=street.copy();pillar_source[stable_dark]=fixed_source[stable_dark]
 out=np.uint8(np.clip(np.round(out*(1-pillar_alpha)+pillar_source*pillar_alpha),0,255))
 untouched=(mask==0)&(pillar_region==0)
 assert np.array_equal(out[untouched],np.uint8(np.round(base[untouched])))
 frames.append(out)
# Prepare every M037 task, including the static plates, from the same fixed
# geometry. Runtime native masks/depth still decide which texels are visible.
tasks=[t for t in json.loads((R/'tasks.json').read_text())['tasks'] if t['room']=='M037'];mapping={f['entry']:i for i,f in enumerate(fs)}
for t in tasks:
 full=Image.open(R/'OpenAI'/t['input']).convert('RGB');phase=mapping.get(t['entry'],0);material=frames[phase].astype(np.float32)
 if t['control'] is None:
  # Static foreground texels (especially entry21's opaque STP-black pillar
  # edge) have their own material. A full phase-zero street image paints
  # frozen headlights into transparent holes of later animation entries.
  own=np.zeros((h,w,3),np.float32);coverage=np.zeros((h,w),np.uint8);refs=np.load(R/'refs'/t['ref'])
  for ref,(tx,ty) in zip(refs,t['tiles']):
   colors=np.stack([(ref&31)*255//31,((ref>>5)&31)*255//31,((ref>>10)&31)*255//31],axis=-1).astype(np.float32)
   x0,y0=max(tx,0),max(ty,0);x1,y1=min(tx+16,w),min(ty+16,h)
   if x1<=x0 or y1<=y0:continue
   v=ref[y0-ty:y1-ty,x0-tx:x1-tx]!=0;patch=own[y0:y1,x0:x1];patch[v]=colors[y0-ty:y1-ty,x0-tx:x1-tx][v];coverage[y0:y1,x0:x1]|=v
  # Dithered RGB555 near-black texels encode pillar shading, not colored
  # pinpoints. Filter their luminance before enlarging the static material.
  own_luma=cv2.cvtColor(own,cv2.COLOR_RGB2GRAY)
  own_luma=cv2.GaussianBlur(own_luma,(0,0),.65)
  own_hd=cv2.resize(own_luma,(w*S,h*S),interpolation=cv2.INTER_CUBIC)[...,None]*np.array([.98,1.,1.02],np.float32)
  material=material*(1-pillar_alpha)+np.clip(own_hd*gain,0,255)*pillar_alpha
 full.paste(Image.fromarray(np.uint8(np.clip(np.round(material),0,255))),(c['bbox'][0]*S,c['bbox'][1]*S));q=dest/t['input'];tmp=q.with_suffix('.tmp');full.save(tmp,format='PNG');tmp.replace(q)
np.save(R/'feedback-fixes/M037-pillar-boundary-mask.npy',pillar_region!=0)
np.save(R/'feedback-fixes/M037-fixed-mirror-mask.npy',mirror_shell)
np.save(R/'feedback-fixes/M037-reflection-mask.npy',reflection)
Image.fromarray(np.uint8(mask)*255).save(R/'feedback-fixes/M037-street-mask.png')
proof={'method':'Fixed OpenAI phase-zero cabin/mirror frame; all nineteen original street and mirror-reflection frames upscaled with deterministic RealESRNet_x4plus, same RGB gains for every phase. No optical-flow morphing, generated phase crossfades, sharpening cycles or endpoint interpolation.','window_apertures':'Fixed window apertures; entire pillar-boundary neighbourhood restored from original source silhouette, including static background contamination in the OpenAI plate','static_pillar_material':'Own per-entry native RGB including opaque STP-black; never first-frame background RGB','yellow_lane_paint':'Original per-phase source chroma, smooth alpha, native road region only','gain_rgb':gain.tolist(),'phases':19,'original_slots_unchanged':True,'fixed_mirror_frame_and_cabin_identical':True,'original_reflection_motion_preserved':True,'model_provenance':'M037-source-sr/inference.json','destination':args.destination}
(R/'feedback-fixes/M037-source-motion-application.json').write_text(json.dumps(proof,indent=2)+'\n');print(proof)
