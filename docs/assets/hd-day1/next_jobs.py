from pathlib import Path
import json,argparse,os,time
R=Path(__file__).resolve().parent;p=argparse.ArgumentParser();p.add_argument('--kind',choices=['base','entry','any'],default='base');p.add_argument('--count',type=int,default=8);a=p.parse_args();jobs=json.loads((R/'jobs.json').read_text());out=[]
for j in jobs:
 if a.kind!='any' and j['kind']!=a.kind:continue
 if (R/'generated'/(j['id']+'.png')).exists():continue
 if j['kind']=='entry' and not (R/'generated'/(j['reference']+'.png')).exists():continue
 try:
  fd=os.open(R/'metadata'/(j['id']+'.started'),os.O_CREAT|os.O_EXCL|os.O_WRONLY);os.write(fd,str(time.time()).encode());os.close(fd)
 except FileExistsError:continue
 out.append(j)
 if len(out)>=a.count:break
(R/'current-batch.json').write_text(json.dumps(out,indent=2))
print(json.dumps(out))
