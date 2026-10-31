from pathlib import Path
from PIL import Image,ImageDraw
import json,argparse
R=Path(__file__).resolve().parent;p=argparse.ArgumentParser();p.add_argument('--rooms',default='M012,M014,M016');a=p.parse_args();rooms=set(a.rooms.split(','));js=[j for j in json.loads((R/'local-jobs.json').read_text()) if j['views'][0][0] in rooms and (R/'source-only-generated'/(j['id']+'.png')).exists()]
for start in range(0,len(js),8):
 o=Image.new('RGB',(1280,1200));d=ImageDraw.Draw(o)
 for i,j in enumerate(js[start:start+8]):
  source=Image.open(j['local_source']).convert('RGB')
  for k,im in enumerate([source,Image.open(R/'source-only-generated'/(j['id']+'.png')).convert('RGB').resize(source.size,Image.Resampling.LANCZOS)]):
   ratio=min(310/im.width,265/im.height);im=im.resize((round(im.width*ratio),round(im.height*ratio)),Image.Resampling.NEAREST if k==0 else Image.Resampling.LANCZOS);x=i%2*640+k*320;y=i//2*300;d.text((i%2*640,y),str(j['entries'][0]),fill='white');o.paste(im,(x,y+28))
 o.save(R/f'review/corrected-{start//8:02d}.jpg')
(R/'review/corrected-index.json').write_text(json.dumps([{'id':j['id'],'entries':j['entries'],'sheet':i//8,'row':i%8//2,'column':i%2} for i,j in enumerate(js)],indent=2));print(len(js),'corrected candidates;', (len(js)+7)//8,'sheets')
