"""One material reference per view, with links between near-identical cameras."""
from pathlib import Path
from PIL import Image,ImageOps,ImageDraw
import numpy as np,cv2,json
R=Path(__file__).resolve().parent;views=json.loads((R/'views.json').read_text());tasks=json.loads((R/'tasks.json').read_text())['tasks'];jobs={};mapping=[];vectors=[]
def load(name):return np.array(Image.open(R/'active-inputs'/name).convert('RGB'))
for v in views:
 name=v['base_input'];a=load(name);canvas=v['base_canvas_input'];candidates=[t for t in tasks if(t['room'],t['group'])==(v['room'],v['group'])]
 if float(np.mean(np.any(a!=0,axis=2)))<.1:
  best=max(candidates,key=lambda t:float(np.mean(np.any(load(t['active_input'])!=0,axis=2))))
  b=load(best['active_input'])
  if np.mean(np.any(b!=0,axis=2))>np.mean(np.any(a!=0,axis=2))*2:name=best['active_input'];canvas=best['input'];a=b
 key=Path(name).stem;j=jobs.setdefault(key,dict(id=key,kind='base',source=str(R/'active-inputs'/name),views=[],size=list(Image.open(R/'active-inputs'/name).size),reference=None,anchor_canvas_input=canvas))
 j['views'].append([v['room'],v['group']]);mapping.append(dict(room=v['room'],group=v['group'],id=key,canvas_input=canvas,initial_base_input=v['base_input']))
for j in jobs.values():
 a=load(j['id']+'.png');gray=cv2.resize(cv2.cvtColor(a,cv2.COLOR_RGB2GRAY),(96,96)).astype('float32');edge=np.stack([cv2.Sobel(gray,cv2.CV_32F,1,0),cv2.Sobel(gray,cv2.CV_32F,0,1)]).reshape(-1);edge/=max(float(np.linalg.norm(edge)),1)
 best=None;score=.94
 for p,vector in vectors:
  if abs(j['size'][0]/j['size'][1]-p['size'][0]/p['size'][1])>.015:continue
  value=float(edge@vector)
  if value>score:best=p;score=value
 if best:j.update(reference=best['id'],reference_source=best['source'],reference_similarity=score)
 vectors.append((j,edge));dest=R/'generated'/(j['id']+'.png')
 if not dest.exists()and np.all(a==a[0,0]):
  Image.fromarray(a).resize((a.shape[1]*4,a.shape[0]*4),Image.Resampling.NEAREST).save(dest);(R/'metadata'/(j['id']+'.json')).write_text(json.dumps({'status':'Uniform native material preserved exactly; no generated objects','source':j['source']},indent=2))
 j['status']='ready'if dest.exists()else'pending'
(R/'anchor-jobs.json').write_text(json.dumps(list(jobs.values()),indent=2));(R/'anchor-views.json').write_text(json.dumps(mapping,indent=2));pending=[j for j in jobs.values()if j['status']=='pending']
for start in range(0,len(pending),36):
 sheet=Image.new('RGB',(1200,6*180),(16,19,24));draw=ImageDraw.Draw(sheet)
 for i,j in enumerate(pending[start:start+36]):
  im=ImageOps.contain(Image.open(j['source']).convert('RGB'),(195,150));x=i%6*200;y=i//6*180;sheet.paste(im,(x,y+25));draw.text((x,y),' '.join(v[0]+':'+str(v[1])for v in j['views'])[:27],fill='white')
 sheet.save(R/'review'/f'full-anchor-sources-{start//36}.jpg')
print(json.dumps({'views':len(mapping),'unique_anchors':len(jobs),'new_anchors':len(pending),'linked_material_references':sum(j['reference']is not None for j in jobs.values()),'source_review_sheets':(len(pending)+35)//36,'largest_aspect_ratio':max(max(j['size'])/min(j['size'])for j in jobs.values())}))
