from pathlib import Path
from PIL import Image,ImageDraw
import numpy as np,json,cv2,hashlib
R=Path('docs/assets/hd-day1');F=R/'feedback-fixes';a=np.stack([np.array(Image.open(R/'composites-m037-source-test'/f'M037-g00-c0-p{i:02d}-hd.png').convert('RGB')) for i in range(19)]);old=np.stack([np.array(Image.open(F/'M037-accepted-before-detail/composites'/f'M037-g00-c0-p{i:02d}-hd.png').convert('RGB')) for i in range(19)])
shell=np.load(F/'M037-fixed-mirror-mask.npy');refl=np.load(F/'M037-reflection-mask.npy');assert all(np.array_equal(x[:896][shell],a[0,:896][shell]) for x in a);assert np.array_equal(a[:,:896][:,refl],old[:,:896][:,refl]);assert np.array_equal(a[:,400:],old[:,400:])
pillars=Image.new('L',(1280,960));d=ImageDraw.Draw(pillars);d.polygon([(10,160),(130,150),(163,340),(80,340)],fill=255);d.polygon([(1145,155),(1260,155),(1210,339),(1120,339)],fill=255);m=np.array(pillars)>0
assert all(np.array_equal(x[m],a[0][m]) for x in a)
yellow=[]
for i in range(19):
 x=a[i,340:352,600:624].astype(float);b=old[i,340:352,600:624].astype(float);ch=lambda x:float(np.maximum(np.minimum(x[...,0],x[...,1])-x[...,2],0).mean());yellow.append({'phase':i,'before':ch(b),'after':ch(x)});assert ch(x)>ch(b)*1.1,(i,ch(x),ch(b))
# Correct phase correspondence remains true after the local detail cleanup.
c=next(c for c in json.loads((F/'temporal-jobs.json').read_text()) if c['room']=='M037');src=[np.array(Image.open(f['source']).convert('RGB'),dtype=float) for f in c['frames']];gain=np.array(json.loads((F/'M037-accepted-exposure.json').read_text())['gain_rgb']);mask=np.array(Image.open(F/'M037-street-mask.png'))>0;mask[shell|refl]=0;m=cv2.resize(mask.astype('uint8'),(320,224),interpolation=cv2.INTER_AREA)>.95
for i,x in enumerate(a):
 small=cv2.resize(x[:896].astype(float),(320,224),interpolation=cv2.INTER_AREA)/gain;e=[float(abs(cv2.GaussianBlur(small,(0,0),.7)-cv2.GaussianBlur(s,(0,0),.7))[m].mean()) for s in src];assert int(np.argmin(e))==i
count=0
for t in json.loads((F/'M037-accepted-before-detail/pack/manifest.json').read_text())['atlases']:
 if t['room']=='M037':continue
 assert hashlib.sha256((R/'pack-m037-source-test'/(t['key']+'.bmp')).read_bytes()).hexdigest()==t['atlas_sha256'];count+=1
proof={'all_19_pillar_masks_static':True,'mirror_frame_static':True,'mirror_reflection_pixels_unchanged_from_accepted_candidate':True,'cabin_pixels_unchanged_from_accepted_candidate':True,'all_19_correct_native_phase_matches':True,'yellow_chroma_restored':yellow,'other_room_records_unchanged':count,'note':'Source-driven lane color restoration and fixed window-aperture masks; no change to phase order/durations or source motion.'};(F/'M037-detail-audit.json').write_text(json.dumps(proof,indent=2)+'\n');print(proof)
