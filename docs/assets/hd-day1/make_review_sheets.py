from pathlib import Path
from PIL import Image,ImageDraw
import json,argparse
R=Path(__file__).resolve().parent;p=argparse.ArgumentParser();p.add_argument('--start',type=int,default=0);p.add_argument('--count',type=int,default=1000);a=p.parse_args();js=json.loads((R/'jobs.json').read_text());chosen=[]
for j in js:
 if j['kind']=='base':continue
 path=R/'localized'/(j['id']+'.png')
 if not path.exists():path=R/'generated'/(j['id']+'.png')
 if not path.exists():continue
 chosen.append((j,path))
records=[]
for start in range(a.start,min(len(chosen),a.start+a.count),8):
 o=Image.new('RGB',(1280,4*300));d=ImageDraw.Draw(o)
 for i,(j,path) in enumerate(chosen[start:start+8]):
  regions=j.get('changed_regions',[]);w,h=j['size'];b=[max(0,min(x[0] for x in regions)-8),max(0,min(x[1] for x in regions)-8),min(w,max(x[2] for x in regions)+8),min(h,max(x[3] for x in regions)+8)] if regions else [0,0,w,h]
  for k,pth in enumerate([Path(j['source']),path]):
   im=Image.open(pth).convert('RGB');sx=im.width/w;sy=im.height/h;im=im.crop((round(b[0]*sx),round(b[1]*sy),round(b[2]*sx),round(b[3]*sy)));ratio=min(310/im.width,265/im.height);im=im.resize((round(im.width*ratio),round(im.height*ratio)),Image.Resampling.NEAREST if k==0 else Image.Resampling.LANCZOS);x=i%2*640+k*320;y=i//2*300;d.text((i%2*640,y),str(j['entries'][0]),fill='white');o.paste(im,(x,y+28))
  records.append({'id':j['id'],'entries':j['entries'],'file':str(path.relative_to(R)),'sheet':start//8,'row':i//2,'column':i%2})
 o.save(R/f'review/all-entries-{start//8:02d}.jpg')
(R/'review/sheet-index.json').write_text(json.dumps(records,indent=2));print(len(chosen),'entry contexts ready;',len(records),'on requested sheets')
