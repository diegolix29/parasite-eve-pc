from pathlib import Path
from PIL import Image,ImageDraw
import json,argparse
p=argparse.ArgumentParser();p.add_argument("--review",default="review");a=p.parse_args()
R=Path(__file__).resolve().parent;records=json.loads((R/a.review/'controller-index.json').read_text());index=[]
for start in range(0,len(records),4):
 group=records[start:start+4];opened=[Image.open(R/a.review/c['sheet']).convert('RGB') for c in group];heights=[max(im.height for im in opened[:2]),max((im.height for im in opened[2:]),default=0)];out=Image.new('RGB',(2560,sum(heights)))
 for i,im in enumerate(opened):out.paste(im,((i%2)*1280,0 if i<2 else heights[0]))
 path=R/a.review/f'controllers-combined-{start//4:02d}.jpg';out.save(path);index.append({'sheet':path.name,'controllers':group})
(R/a.review/'combined-controller-index.json').write_text(json.dumps(index,indent=2));print(len(index),'combined sheets')
