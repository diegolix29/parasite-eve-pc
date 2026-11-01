from pathlib import Path
from PIL import Image
import json,hashlib,numpy as np,cv2
R=Path(__file__).resolve().parents[1];old={t['key']:t for t in json.loads((R/'pack-before-tester-fixes/manifest.json').read_text())['atlases']};new=json.loads((R/'pack-feedback/manifest.json').read_text())['atlases'];accepted={'M014','M017','M018','M022','M030','M031','M035','M036'};checked=[]
for t in new:
 protected=t['room'] in accepted or (t['room']=='M003' and t['control'] is not None) or (t['room']=='M006' and t['entry'] in list(range(4,9))+list(range(11,15)))
 if protected:
  sha=hashlib.sha256((R/'pack-feedback'/(t['key']+'.bmp')).read_bytes()).hexdigest();assert sha==old[t['key']]['atlas_sha256'],('accepted changed',t['room'],t['entry']);checked.append({'room':t['room'],'entry':t['entry'],'key':t['key'],'sha256':sha})
proof={'accepted_rooms':sorted(accepted),'preserved_atlas_records':len(checked),'accepted_materials_byte_identical':True,'accepted_door_opening_phases_byte_identical':True,'records':checked};(R/'feedback-fixes/preservation-audit.json').write_text(json.dumps(proof,indent=2))
views=json.loads((R/'views.json').read_text());records=[];frozen=[]
for v in views:
 if v['room'] not in ['M026','M027','M028','M029','M032','M033','M034','M037']:continue
 for c in v['controllers']:
  arrays={'native':[],'before':[],'after':[]}
  for i in range(len(c['slots'])):
   name=f"{v['room']}-g{v['group']:02d}-c{c['id']}-p{i:02d}-";n=Image.open(R/'composites-before-tester-fixes'/(name+'native.png')).convert('RGB');arrays['native'].append(np.array(n).astype(float))
   for key,folder in [('before','composites-before-tester-fixes'),('after','composites-feedback')]:arrays[key].append(np.array(Image.open(R/folder/(name+'hd.png')).convert('RGB').resize(n.size,Image.Resampling.BOX)).astype(float))
  mask=np.any(np.stack(arrays['native'])!=arrays['native'][0],axis=(0,3));mask=cv2.dilate(mask.astype('uint8'),np.ones((3,3),np.uint8)).astype(bool);edges=[]
  for i in range(len(c['slots'])):
   j=(i+1)%len(c['slots']);e={'from':i,'to':j,'wrap':j==0}
   for key,a in arrays.items():e[key+'_relative_l1']=float(abs(a[i]-a[j])[mask].mean()/max(a[0][mask].mean(),1)) if mask.any() else 0
   if e['native_relative_l1']>0 and e['after_relative_l1']==0:frozen.append((v['room'],v['group'],c['id'],i,j))
   edges.append(e)
  means={key:[float(a[mask].mean()) for a in arr] for key,arr in arrays.items()};records.append({'room':v['room'],'group':v['group'],'controller':c['id'],'mean_luminance':means,'edges':edges})
assert not frozen,frozen
p={'note':'Actual packed, native-depth/STP full-scene compositions; loop-edge metrics are not a semantic guarantee. Gate opening controller is a pose sequence, not a seamless physical loop.','frozen_native_changing_pairs':frozen,'controllers':records};(R/'feedback-fixes/cycle-audit.json').write_text(json.dumps(p,indent=2));print('Preserved',len(checked),'accepted records; native-changing cyclic edges not frozen.')
for c in records:
 if c['room']=='M037':print('M037 wrap',c['edges'][-1])
 elif c['controller']==0 or c['room'] in ['M033','M034']:
  print(c['room'],c['group'],c['controller'],'mean before',np.round(c['mean_luminance']['before'],2).tolist(),'after',np.round(c['mean_luminance']['after'],2).tolist())
