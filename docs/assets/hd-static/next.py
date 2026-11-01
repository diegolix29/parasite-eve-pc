from pathlib import Path
from PIL import Image,ImageOps,ImageDraw
import argparse,json
R=Path(__file__).resolve().parent;p=argparse.ArgumentParser();p.add_argument('--kind',choices=['base','entry'],default='base');p.add_argument('--count',type=int,default=32);p.add_argument('--batch',required=True);a=p.parse_args();J=json.loads((R/'jobs.json').read_text());busy={j['id'] for f in R.glob('batch-*.json')for j in json.loads(f.read_text()) if not (R/'metadata'/(j['id']+'.failed')).exists()};chosen=[]
for j in J:
 if j['kind']!=a.kind or j['id'] in busy or (R/'generated'/(j['id']+'.png')).exists():continue
 if j['reference'] and not(R/'generated'/(j['reference']+'.png')).exists():continue
 chosen.append(j)
 if len(chosen)>=a.count:break
(R/('batch-'+a.batch+'.json')).write_text(json.dumps(chosen,indent=2))
for page in range((len(chosen)+11)//12):
 sheet=Image.new('RGB',(960,4*260),(20,22,27));draw=ImageDraw.Draw(sheet)
 for i,j in enumerate(chosen[page*12:(page+1)*12]):
  im=Image.open(j['source']).convert('RGB');thumb=ImageOps.contain(im,(312,236));x=i%3*320;y=i//3*260;sheet.paste(thumb,(x+(320-thumb.width)//2,y+24+(236-thumb.height)//2));draw.text((x+4,y+4),' / '.join(v[0]+':'+str(v[1])for v in j['views'])[:44],fill='white')
 sheet.save(R/'review'/('sources-'+a.batch+'-'+str(page)+'.jpg'))
print(json.dumps({'batch':a.batch,'jobs':len(chosen),'source_review_pages':(len(chosen)+11)//12,'views':[j['views']for j in chosen]}))
