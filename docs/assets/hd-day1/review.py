from pathlib import Path
import json,cv2,numpy as np
from PIL import Image,ImageDraw
R=Path(__file__).resolve().parent;jobs=json.loads((R/'jobs.json').read_text());reports=[];done=[]
for job in jobs:
 path=R/'generated'/(job['id']+'.png')
 if not path.exists():continue
 source=Image.open(job['source']).convert('RGB');out=Image.open(path).convert('RGB');record={'id':job['id'],'kind':job['kind'],'views':job['views'],'source_size':source.size,'generated_size':out.size,'aspect_error':abs((out.width/out.height)/(source.width/source.height)-1)}
 if job['kind']=='base':
  a=np.array(source);b=np.array(out.resize(source.size,Image.Resampling.BOX));
  def gray(x):return cv2.GaussianBlur(cv2.cvtColor(x,cv2.COLOR_RGB2GRAY).astype('float32')/255,(0,0),1)
  try:
   corr,W=cv2.findTransformECC(gray(a),gray(b),np.eye(2,3,dtype='float32'),cv2.MOTION_AFFINE,(cv2.TERM_CRITERIA_COUNT|cv2.TERM_CRITERIA_EPS,100,1e-6),None,5);w,h=source.size;pts=np.array([[w*.2,h*.2,1],[w*.8,h*.2,1],[w*.5,h*.5,1],[w*.2,h*.8,1],[w*.8,h*.8,1]],np.float32);record.update(ecc=corr,affine=W.tolist(),diagnostic_anchor_displacement_native=np.linalg.norm(pts@W.T-pts[:,:2],axis=1).tolist())
  except cv2.error:record['ecc']='failed'
  done.append(job)
 reports.append(record)
(R/'review/automatic.json').write_text(json.dumps({'note':'Global native-grayscale affine diagnostics; not proof of HR geometry or semantic accuracy. Human/agent visual review required.','jobs':reports},indent=2))
for start in range(0,len(done),4):
 sheet=Image.new('RGB',(640,4*340),(16,16,16));draw=ImageDraw.Draw(sheet)
 for n,job in enumerate(done[start:start+4]):
  label=' '.join(f'{r}/g{g}' for r,g in job['views']);draw.text((5,n*340+5),label,fill='white')
  for col,path in enumerate([Path(job['source']),R/'generated'/(job['id']+'.png')]):
   im=Image.open(path).convert('RGB');ratio=min(310/im.width,310/im.height);im=im.resize((round(im.width*ratio),round(im.height*ratio)),Image.Resampling.NEAREST if col==0 else Image.Resampling.LANCZOS);sheet.paste(im,(col*320,n*340+25))
 sheet.save(R/f'review/base-{start//4:02d}.jpg')
print(json.dumps([{k:v for k,v in r.items() if k in ['id','views','aspect_error','ecc','diagnostic_anchor_displacement_native']} for r in reports if r['kind']=='base'],indent=2))
