from pathlib import Path
from PIL import Image
import numpy as np,json,hashlib,shutil
R=Path(__file__).resolve().parents[1];F=R/'feedback-fixes';B=F/'M037-before-pillar-material';B.mkdir(exist_ok=True)
# Keep the accepted lane-color sequence for byte-for-byte comparison.
for p in (R/'composites').glob('M037-*.png'):
 if not (B/p.name).exists():shutil.copyfile(p,B/p.name)
if not (B/'pack-manifest.json').exists():shutil.copyfile(R/'pack/manifest.json',B/'pack-manifest.json')
a=np.stack([np.array(Image.open(R/'composites-m037-source-test'/f'M037-g00-c0-p{i:02d}-hd.png').convert('RGB')) for i in range(19)]);old=np.stack([np.array(Image.open(B/f'M037-g00-c0-p{i:02d}-hd.png').convert('RGB')) for i in range(19)])
region=np.zeros((960,1280),bool);region[:896]=np.load(F/'M037-pillar-boundary-mask.npy');assert np.array_equal(a[:,~region],old[:,~region]);assert np.array_equal(a[:,280:370,400:880],old[:,280:370,400:880])
tasks=[t for t in json.loads((R/'tasks.json').read_text())['tasks'] if t['room']=='M037'];c=next(c for c in json.loads((F/'temporal-jobs.json').read_text()) if c['room']=='M037')
def raster(t):
 out=np.zeros((240,320),np.uint16)
 for ref,(x,y) in zip(np.load(R/'refs'/t['ref']),t['tiles']):
  x0,y0=max(x,0),max(y,0);x1,y1=min(x+16,320),min(y+16,240)
  if x0>=x1 or y0>=y1:continue
  ref=ref[y0-y:y1-y,x0-x:x1-x];p=out[y0:y1,x0:x1];p[ref!=0]=ref[ref!=0]
 return out
static=raster(next(t for t in tasks if t['entry']==21));v=static;dark=(v!=0)&((v&31)<4)&(((v>>5)&31)<4)&(((v>>10)&31)<4);records=[]
for i,f in enumerate(c['frames']):
 dynamic=raster(next(t for t in tasks if t['entry']==f['entry']));visible=dark&(dynamic==0);m=np.repeat(np.repeat(visible,4,0),4,1)&region
 before=int(np.count_nonzero((old[i].max(2)>80)&m));after=int(np.count_nonzero((a[i].max(2)>80)&m));assert after==0,(i,before,after)
 records.append({'phase':i,'source_dark_foreground_visible_texels':int(m.sum()),'bright_background_pixels_before':before,'bright_background_pixels_after':after})
assert sum(x['bright_background_pixels_before'] for x in records)>0
count=0
for t in json.loads((B/'pack-manifest.json').read_text())['atlases']:
 if t['room']=='M037':continue
 assert hashlib.sha256((R/'pack-m037-source-test'/(t['key']+'.bmp')).read_bytes()).hexdigest()==t['atlas_sha256'];count+=1
proof={'cause':'Static entry21 opaque STP-black pillar texels were assigned phase-zero street RGB. Bright first-frame streaks appeared where later moving entries are transparent.','fix':'Own per-entry static foreground material, filtered original luminance; source-aligned pillar neighbourhoods.','yellow_markings_and_entire_central_road_byte_identical':True,'all_pixels_outside_pillar_neighbourhoods_byte_identical':True,'other_room_atlas_records_unchanged':count,'phase_checks':records,'pack_manifest_sha256':hashlib.sha256((R/'pack-m037-source-test/manifest.json').read_bytes()).hexdigest()};(F/'M037-pillar-material-audit.json').write_text(json.dumps(proof,indent=2)+'\n');print('All19 phases: no background-bright pixels on source-dark static pillar layer; accepted yellow road and outside regions byte-identical. Prior bright pixel count:',sum(x['bright_background_pixels_before'] for x in records))
