"""Measure actual composite changes away from native movement; not an art score."""
from pathlib import Path
import json,numpy as np,cv2
from PIL import Image
R=Path(__file__).resolve().parent;views=json.loads((R/'views.json').read_text());records=[];OLD='composites-before-animation-fit' if (R/'composites-before-animation-fit').exists() else 'composites'
def load(folder,name):return np.array(Image.open(R/folder/name).convert('RGB'))
for v in views:
 for c in v['controllers']:
  prefix=f"{v['room']}-g{v['group']:02d}-c{c['id']}-p"
  nb=load(OLD,prefix+'00-native.png');oldbase=load(OLD,prefix+'00-hd.png');newbase=load('composites-coherent',prefix+'00-hd.png')
  for i in range(1,len(c['slots'])):
   stem=prefix+f'{i:02d}-';n=load(OLD,stem+'native.png');changing=np.any(n!=nb,2).astype('uint8');support=cv2.dilate(changing,np.ones((17,17),np.uint8));outside=cv2.resize(1-support,(newbase.shape[1],newbase.shape[0]),interpolation=cv2.INTER_NEAREST).astype(bool)
   vals={}
   for k,folder,base in [('before',OLD,oldbase),('after','composites-coherent',newbase)]:
    im=load(folder,stem+'hd.png');delta=np.max(abs(im.astype(np.int16)-base.astype(np.int16)),2);vals[k]={'outside_changed_channels_gt8':int(np.sum((delta>8)&outside)),'outside_l1':int(delta[outside].sum()),'whole_l1':int(delta.sum())}
   records.append({'room':v['room'],'group':v['group'],'controller':c['id'],'phase':i,'native_changed_pixels':int(changing.sum()),**vals})
proof={'note':__doc__,'pairs':len(records),'before_outside_l1':sum(x['before']['outside_l1'] for x in records),'after_outside_l1':sum(x['after']['outside_l1'] for x in records),'results':records};(R/'animation-fit-audit.json').write_text(json.dumps(proof,indent=2));print({k:v for k,v in proof.items() if k!='results'});print('WORST',[(x['room'],x['controller'],x['phase'],x['before']['outside_l1'],x['after']['outside_l1']) for x in sorted(records,key=lambda x:x['after']['outside_l1'],reverse=True)[:12]])
