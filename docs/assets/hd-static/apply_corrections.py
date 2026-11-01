"""Install localized imagegen corrections into original per-tile atlas geometry.

Always start each atlas from its pre-correction source pack. Keep the native
reference half and every texel outside the selected material regions unchanged.
Normally restrict the canonical view to identical native layer pixels. Explicit
coherent-material regions synchronize all layers for the requested floor and
lamp-occlusion edits. Native coverage, depth and reference RGB stay unchanged.
"""
from pathlib import Path
from PIL import Image,ImageDraw
import json,hashlib,numpy as np,cv2
R=Path(__file__).resolve().parent;pack=R/'pack-with-day1'
regions=json.loads((R/'corrections/regions.json').read_text())
tasks=json.loads((R/'tasks.json').read_text())['tasks'];views=json.loads((R/'views.json').read_text())
manifest=json.loads((pack/'manifest.json').read_text());origins={t['key']:t['pack_origin']for t in manifest['atlases']}
atlases={};baselines={};allowed={};notes={}
for region in regions:
 v=next(v for v in views if (v['room'],v['group'])==(region['room'],region['group']))
 w,h=v['canvas_size'];x0,y0,x1,y1=v['active_bbox']
 enhanced=Image.new('RGB',(w*4,h*4));enhanced.paste(Image.open(R/region['image']).convert('RGB').resize(((x1-x0)*4,(y1-y0)*4),Image.Resampling.LANCZOS),(x0*4,y0*4));enhanced=np.array(enhanced)
 mask=Image.new('L',(w*4,h*4));draw=ImageDraw.Draw(mask)
 for polygon in region['polygons']:draw.polygon([((x+x0)*4,(y+y0)*4)for x,y in polygon],fill=255)
 for polygon in region.get('exclude',[]):draw.polygon([((x+x0)*4,(y+y0)*4)for x,y in polygon],fill=0)
 # Feather inward only: no modified pixels can leak outside the selected area.
 binary=np.array(mask)>0;alpha=np.clip(cv2.distanceTransform(binary.astype('uint8'),cv2.DIST_L2,5)/region.get('feather_hd',4),0,1)
 base=np.array(Image.open(R/'inputs'/v['base_canvas_input']).convert('RGB'))
 for t in [t for t in tasks if (t['room'],t['group'])==(v['room'],v['group'])]:
  key=t['key']
  if key not in atlases:
   origin=R.parent/'hd-day1/pack'if origins[key]=='reviewed-day1'else R/'pack'
   atlases[key]=np.array(Image.open(origin/(key+'.bmp')).convert('RGB'));baselines[key]=atlases[key].copy();allowed[key]=np.zeros(atlases[key].shape[:2],bool);notes[key]=[]
  original=np.array(Image.open(R/'inputs'/t['input']).convert('RGB'));same=np.repeat(np.repeat(np.all(original==base,axis=2),4,0),4,1);a=alpha*(np.ones_like(same)if region.get("coherent_all_layers")else same)
  refs=np.load(R/'refs'/t['ref']);cols=min(16,len(refs));atlas=atlases[key]
  for i,((tx,ty),ref)in enumerate(zip(t['tiles'],refs)):
   ax=i%cols*68+2;ay=i//cols*68+2;blend=a[ty*4:ty*4+64,tx*4:tx*4+64].copy();black=np.repeat(np.repeat((ref&0x7fff)==0 if region.get('preserve_black_material',True)else ref==0,4,0),4,1);blend[black]=0
   if not np.any(blend):continue
   current=atlas[ay:ay+64,ax:ax+64];candidate=enhanced[ty*4:ty*4+64,tx*4:tx*4+64];current[:]=np.rint(current*(1-blend[...,None])+candidate*blend[...,None]).astype('uint8')
   # Matching gutters are the only permitted changes outside the tile interior.
   atlas[ay-2:ay+66,ax-2:ax+66]=np.pad(current.copy(),((2,2),(2,2),(0,0)),mode='edge')
   allowed[key][ay-2:ay+66,ax-2:ax+66]|=np.pad(blend>0,((2,2),(2,2)),mode='edge')
  notes[key].append(region['reason'])
proof=[]
for key,atlas in atlases.items():
 changed=np.any(atlas!=baselines[key],axis=2);assert not np.any(changed&~allowed[key]),key
 path=pack/(key+'.bmp');temp=path.with_suffix('.tmp');Image.fromarray(atlas).save(temp,format='BMP');temp.replace(path)
 sha=hashlib.sha256(path.read_bytes()).hexdigest()
 for t in manifest['atlases']:
  if t['key']==key:t.update(atlas_sha256=sha,user_corrections=sorted(set(notes[key])))
 proof.append({'key':key,'changed_hd_texels':int(changed.sum()),'native_reference_and_unselected_texels_identical':True,'sha256':sha})
manifest['user_correction_keys']=sorted(atlases);(pack/'manifest.json').write_text(json.dumps(manifest,indent=2))
(R/'corrections/application-audit.json').write_text(json.dumps({'atlases':proof,'note':__doc__},indent=2))
print(len(proof),'localized corrected atlases; native reference and all unselected texels unchanged')

old=R.parent/'hd-day1/pack';oldkeys={t['key']for t in json.loads((old/'manifest.json').read_text())['atlases']};intentional=oldkeys&set(atlases)
assert intentional<={t['key']for t in tasks if t['room']=='M005'}
for key in oldkeys-intentional:assert hashlib.sha256((old/(key+'.bmp')).read_bytes()).hexdigest()==hashlib.sha256((pack/(key+'.bmp')).read_bytes()).hexdigest()
(R/'day1-preservation.json').write_text(json.dumps({'all_reviewed_day1_atlases_byte_identical':not bool(intentional),'unchanged_unique_atlases':len(oldkeys-intentional),'intentional_user_requested_M005_floor_correction_keys':sorted(intentional),'all_other_reviewed_day1_atlases_byte_identical':True},indent=2))
