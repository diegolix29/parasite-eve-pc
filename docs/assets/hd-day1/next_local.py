from pathlib import Path
from PIL import Image,ImageDraw
import json,os,time,argparse
R=Path(__file__).resolve().parent;p=argparse.ArgumentParser();p.add_argument('--count',type=int,default=32);p.add_argument('--retry-full',action='store_true');a=p.parse_args();out=[]
for j in json.loads((R/'local-jobs.json').read_text()):
 if (R/'local-generated'/(j['id']+'.png')).exists():continue
 if not a.retry_full and ((R/'metadata'/(j['id']+'.started')).exists() or (R/'generated'/(j['id']+'.png')).exists()):continue
 try:
  fd=os.open(R/'metadata'/('local-'+j['id']+'.started'),os.O_CREAT|os.O_EXCL|os.O_WRONLY);os.write(fd,str(time.time()).encode());os.close(fd)
 except FileExistsError:continue
 out.append(j)
 if len(out)>=a.count:break
(R/'current-local-batch.json').write_text(json.dumps(out,indent=2))
for start in range(0,len(out),16):
 sheet=Image.new('RGB',(1280,1080));d=ImageDraw.Draw(sheet)
 for i,j in enumerate(out[start:start+16]):
  im=Image.open(j['local_source']);im.thumbnail((318,240));x=i%4*320;y=i//4*270;d.text((x,y),str(j['entries'][0]),fill='white');sheet.paste(im,(x,y+28))
 sheet.save(R/f'review/local-sources-{start//16}.jpg')
print(len(out),'selected')
